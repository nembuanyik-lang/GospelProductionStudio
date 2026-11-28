#include "Exporter.h"
#include "Mixing.h"
#include "Mp3Encoder.h"
#include "MasterPlugin.h"

namespace Exporter
{
juce::String formatName (Format f)
{
    switch (f)
    {
        case Format::mp3High:  return "MP3 320 kbps (best for sharing)";
        case Format::mp3Small: return "MP3 192 kbps (smaller, for WhatsApp)";
        case Format::wav16:    return "WAV 16-bit (CD quality)";
        case Format::wav24:    return "WAV 24-bit (studio master)";
    }
    return {};
}

juce::String extension (Format f)     { return isMp3 (f) ? ".mp3" : ".wav"; }
bool isMp3 (Format f)                 { return f == Format::mp3High || f == Format::mp3Small; }

te::TimeRange songRange (te::Edit& edit)
{
    auto end = te::TimePosition();
    for (auto* t : Mixing::getSongTracks (edit))
        for (auto* c : t->getClips())
            end = juce::jmax (end, c->getPosition().getEnd());
    return { te::TimePosition(), end };
}

juce::File defaultFolder (const juce::String& songTitle)
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
              .getChildFile ("Gospel Production Studio").getChildFile ("Exports")
              .getChildFile (juce::File::createLegalFileName (songTitle.isEmpty() ? "Song" : songTitle));
}

juce::Array<te::AudioTrack*> stemTracks (te::Edit& edit)
{
    // Tracks in a group bus come out together, as the bus (a "Choir" stem)
    juce::Array<te::AudioTrack*> result;
    for (auto* t : Mixing::getSongTracks (edit))
        if (! t->isMuted (false) && ! t->getClips().isEmpty() && Mixing::getBusOf (*t) == nullptr)
            result.add (t);
    for (auto* bus : Mixing::getGroupBuses (edit))
    {
        bool hasSound = false;
        for (auto* m : Mixing::getBusMembers (*bus))
            hasSound = hasSound || (! m->isMuted (false) && ! m->getClips().isEmpty());
        if (hasSound && ! bus->isMuted (false))
            result.add (bus);
    }
    return result;
}

namespace
{
    // "Song.mp3", or "Song (2).mp3" if that name is taken
    juce::File freeName (const juce::File& folder, const juce::String& name, const juce::String& ext)
    {
        auto f = folder.getChildFile (juce::File::createLegalFileName (name) + ext);
        for (int i = 2; f.exists(); ++i)
            f = folder.getChildFile (juce::File::createLegalFileName (name) + " (" + juce::String (i) + ")" + ext);
        return f;
    }
}

//==============================================================================
Job::Job (te::Edit& e, Options o, Callbacks c) : edit (e), options (std::move (o)), callbacks (std::move (c)) {}

Job::~Job()
{
    busyToken.reset();
    alive->store (false);
    cancel();
    stopTimer();
    renderHandle.reset();          // cancels and waits for the render thread
    if (encoder != nullptr && encoder->joinable())
        encoder->join();
    tempFile.deleteFile();
}

void Job::start()
{
    if (running)
        return;

    busyToken = BackgroundWork::busy ("Exporting the song", [this] { cancel(); });
    range = options.what == What::loop ? edit.getTransport().getLoopRange() : songRange (edit);
    if (range.getLength() <= te::TimeDuration::fromSeconds (0.05))
    {
        finishAll (juce::Result::fail (options.what == What::loop ? "The loop range is empty. Set a loop first."
                                                                  : "There is nothing in the song to export yet."));
        return;
    }

    if (! options.folder.createDirectory())
    {
        finishAll (juce::Result::fail ("Could not create the folder " + options.folder.getFullPathName()));
        return;
    }

    steps.clear();
    const auto ext = extension (options.format);
    const auto title = options.title.isEmpty() ? juce::String ("Song") : options.title;

    if (options.what == What::stems)
    {
        juce::Array<te::Track*> returns;
        if (options.stemsWithSharedFx)
            for (int bus = 0; bus < Mixing::numBuses; ++bus)
                if (auto* r = Mixing::findReturn (edit, bus))
                    returns.add (r);

        for (auto* t : stemTracks (edit))
        {
            Step s;
            s.label = t->getName();
            s.tracks.add (t);
            if (Mixing::isGroupBus (*t))
                for (auto* m : Mixing::getBusMembers (*t))
                    s.tracks.add (m);
            s.tracks.addArray (returns);
            s.isMix = false;
            s.finalFile = freeName (options.folder, title + " - " + t->getName(), ext);
            steps.push_back (s);
        }
        if (steps.empty())
        {
            finishAll (juce::Result::fail ("No track has anything on it (or every track is muted)."));
            return;
        }
    }
    else
    {
        Step s;
        s.label = options.what == What::loop ? "the loop" : "the whole song";
        s.finalFile = freeName (options.folder, options.what == What::loop ? title + " (loop)" : title, ext);
        steps.push_back (s);
    }

    edit.getTransport().stop (false, true);
    running = true;
    cancelled = false;
    current = 0;
    written.clear();
    warnings.clear();
    startTimerHz (10);
    startStep();
}

void Job::cancel()
{
    cancelled = true;
    if (renderHandle != nullptr)
        renderHandle->cancel();
}

void Job::startStep()
{
    if (current >= steps.size())
    {
        finishAll (juce::Result::ok());
        return;
    }

    auto& step = steps[current];
    const auto mp3 = isMp3 (options.format);

    te::Renderer::Parameters p (edit);
    p.tracksToDo = step.tracks.isEmpty() ? juce::BigInteger() : te::toBitSet (step.tracks);
    p.time = range;
    p.endAllowance = options.what == What::loop ? te::TimeDuration() : te::TimeDuration::fromSeconds (6.0);   // reverb and echo tails
    p.sampleRateForAudio = options.sampleRate;
    p.blockSizeForAudio = 512;
    p.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = mp3 ? 32 : (options.format == Format::wav16 ? 16 : 24);
    p.ditheringEnabled = ! mp3;
    p.usePlugins = true;
    p.useMasterPlugins = step.isMix;            // master effects belong to the mix, not to stems
    p.canRenderInMono = false;
    p.checkNodesForAudio = false;

    // With mastering on, the mastering sets the loudness: no second change here
    const auto mastered = step.isMix && MasterPlugin::isActive (edit);
    if (auto* m = MasterPlugin::findIn (edit))
        m->compare = false;

    if (step.isMix && options.loudForSharing && ! mastered)
    {
        p.shouldNormaliseByLUFS = true;
        p.normaliseToLevelDb = -14.0f;
        p.limitTruePeak = true;
        p.truePeakCeilingDb = -1.0f;
    }

    if (! mp3)
    {
        p.metadata.set (juce::WavAudioFormat::bwavDescription, options.title);
        p.metadata.set (juce::WavAudioFormat::bwavOriginator, options.artist);
    }

    tempFile = mp3 ? juce::File::createTempFile (".wav") : juce::File();
    p.destFile = mp3 ? tempFile : step.finalFile;
    p.destFile.deleteFile();                   // the renderer adds to an existing file

    phase = Phase::rendering;
    auto a = alive;
    renderHandle = te::EditRenderer::render (p, [this, a] (tl::expected<juce::File, std::string> result)
    {
        const bool ok = result.has_value();
        const auto error = ok ? juce::String() : juce::String (result.error());
        const auto file = ok ? *result : juce::File();
        juce::MessageManager::callAsync ([this, a, ok, error, file]
        {
            if (a->load())
                renderFinished (ok, error, file);
        });
    });
}

void Job::renderFinished (bool ok, const juce::String& error, juce::File rendered)
{
    juce::ignoreUnused (rendered);
    renderHandle.reset();

    if (cancelled)        { finishAll (juce::Result::fail ("Stopped.")); return; }
    if (! ok)             { finishAll (juce::Result::fail ("The render did not work: " + error)); return; }

    auto& step = steps[current];
    checkForClipping (isMp3 (options.format) ? tempFile : step.finalFile, step.isMix ? juce::String ("The song") : step.label);
    if (! isMp3 (options.format))
    {
        finishStep (juce::Result::ok());
        return;
    }

    // Encode the MP3 on a background thread
    phase = Phase::encoding;
    encodeProgress = 0.0f;
    Mp3Encoder::Tags tags;
    tags.title  = step.isMix ? options.title : options.title + " - " + step.label;
    tags.artist = options.artist;
    tags.album  = options.title;
    const auto kbps = options.format == Format::mp3High ? 320 : 192;
    const auto in = tempFile, out = step.finalFile;
    auto a = alive;

    encoder = std::make_unique<std::thread> ([this, a, in, out, kbps, tags]
    {
        const auto r = Mp3Encoder::encode (in, out, kbps, tags,
                                           [this] (float f) { encodeProgress = f; },
                                           [this] { return cancelled.load(); });
        juce::MessageManager::callAsync ([this, a, r]
        {
            if (! a->load())
                return;
            if (encoder != nullptr && encoder->joinable())
                encoder->join();
            encoder.reset();
            tempFile.deleteFile();
            finishStep (r);
        });
    });
}

// Too loud? The file is kept, but you are told which part to turn down
void Job::checkForClipping (const juce::File& f, const juce::String& label)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr)
        return;

    juce::Range<float> ranges[2];
    reader->readMaxLevels (0, reader->lengthInSamples, ranges, (int) juce::jmin (2u, reader->numChannels));
    float peak = 0.0f;
    for (auto& r : ranges)
        peak = juce::jmax (peak, std::abs (r.getStart()), std::abs (r.getEnd()));

    if (peak >= 0.9995f)
        warnings.add (label + " is so loud that it clips (distorts) at its loudest moments. "
                      "Lower its fader (or the Master) a few dB and export again"
                      + juce::String (isMp3 (options.format) || steps[current].isMix ? ", or tick \"Ready to share\"." : "."));
}

void Job::finishStep (juce::Result r)
{
    if (r.failed() || cancelled)
    {
        finishAll (cancelled ? juce::Result::fail ("Stopped.") : r);
        return;
    }

    written.add (steps[current].finalFile);
    ++current;
    startStep();
}

void Job::finishAll (juce::Result r)
{
    stopTimer();
    phase = Phase::idle;
    running = false;
    busyToken.reset();
    tempFile.deleteFile();

    if (r.failed() && current < steps.size())
        steps[current].finalFile.deleteFile();          // no half-written files left behind

    if (callbacks.finished)
        callbacks.finished (r, written, warnings);
}

void Job::timerCallback()
{
    if (! running || current >= steps.size() || callbacks.progress == nullptr)
        return;

    const auto n = (float) steps.size();
    float stepProgress = 0.0f;
    juce::String what;

    if (phase == Phase::rendering && renderHandle != nullptr)
    {
        stepProgress = renderHandle->getProgress() * (isMp3 (options.format) ? 0.7f : 1.0f);
        what = "Rendering ";
    }
    else if (phase == Phase::encoding)
    {
        stepProgress = 0.7f + 0.3f * encodeProgress.load();
        what = "Making the MP3 of ";
    }

    auto status = what + steps[current].label;
    if (steps.size() > 1)
        status << "  (" << (int) current + 1 << " of " << (int) steps.size() << ")";
    callbacks.progress (status, ((float) current + juce::jlimit (0.0f, 1.0f, stepProgress)) / n);
}
}
