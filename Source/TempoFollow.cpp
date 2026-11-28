#include "TempoFollow.h"
#include "Loops.h"
#include "Takes.h"
#include "AudioEdit.h"
#include <tracktion_engine/3rd_party/signalsmith-stretch/signalsmith-stretch.h>

namespace TempoFollow
{
    namespace
    {
        const juce::Identifier modeId ("gpsTempoRecordings");
        const juce::Identifier srcId ("gpsRecSource"), bpmId ("gpsRecBpm"), madeId ("gpsRecMade"),
                               startId ("gpsRecStartBeat"), endId ("gpsRecEndBeat"), offsetId ("gpsRecOffset"),
                               waitingId ("gpsRecWaiting");

        juce::AudioFormatManager& formats()
        {
            static juce::AudioFormatManager fm;
            static bool done = false;
            if (! done) { fm.registerBasicFormats(); done = true; }
            return fm;
        }

        double songBpm (te::Edit& edit)
        {
            return edit.tempoSequence.getTempoAt (te::TimePosition()).getBpm();
        }

        juce::File tempoFolderFor (const juce::File& source)
        {
            auto dir = source.getParentDirectory();
            for (auto* name : { "Timed", "Following", "Harmony", "Stems", "Tempo" })
                if (dir.getFileName() == name) dir = dir.getParentDirectory();
            return dir.getChildFile ("Tempo");
        }

        // Stretched copies live in the song's own Audio/Tempo folder (never next to a file outside the song)
        juce::File folderFor (te::Edit& edit, const juce::File& source)
        {
            if (edit.editFileRetriever)
                if (auto f = edit.editFileRetriever(); f != juce::File())
                    return f.getParentDirectory().getChildFile ("Audio").getChildFile ("Tempo");
            return tempoFolderFor (source);
        }

        // Only files this made: "<name> @ 100 (from 80).wav"
        bool isStretchedCopy (const juce::File& f)
        {
            const auto n = f.getFileNameWithoutExtension();
            return n.contains (" @ ") && n.contains ("(from ") && n.endsWithChar (')');
        }

        juce::String bpmText (double bpm)
        {
            return std::abs (bpm - std::round (bpm)) < 0.01 ? juce::String (juce::roundToInt (bpm)) : juce::String (bpm, 2);
        }
    }

    Mode getMode (te::Edit& edit)
    {
        const auto m = edit.state[modeId].toString();
        return m == "stretch" ? Mode::stretch : m == "keep" ? Mode::keep : Mode::ask;
    }

    void setMode (te::Edit& edit, Mode m)
    {
        edit.state.setProperty (modeId, m == Mode::stretch ? "stretch" : m == Mode::keep ? "keep" : "", nullptr);
    }

    bool applies (te::WaveAudioClip& c)
    {
        return ! c.getAutoTempo() && ! Loops::isFollowing (c) && Takes::count (c) <= 1 && c.getOriginalFile().existsAsFile();
    }

    juce::Array<te::WaveAudioClip*> recordings (te::Edit& edit)
    {
        juce::Array<te::WaveAudioClip*> out;
        for (auto* t : te::getAudioTracks (edit))
            for (auto* c : t->getClips())
                if (auto* w = dynamic_cast<te::WaveAudioClip*> (c); w != nullptr && applies (*w))
                    out.add (w);
        return out;
    }

    void prepareForChange (te::Edit& edit)
    {
        auto& ts = edit.tempoSequence;
        const auto bpm = songBpm (edit);
        for (auto* c : recordings (edit))
        {
            auto& s = c->state;
            // Already waiting for a stretch: its place in beats is still the one to keep
            if ((bool) s.getProperty (waitingId, false))
                continue;

            const auto current = c->getOriginalFile();
            const bool known = s.hasProperty (srcId)
                               && (current == juce::File (s[madeId].toString()) || current == juce::File (s[srcId].toString()));
            if (! known)
            {
                // A new recording, or its sound was replaced since (timing correction, a take, normalise...):
                // what plays now is the original, at the current tempo
                s.setProperty (srcId, current.getFullPathName(), nullptr);
                s.setProperty (bpmId, bpm, nullptr);
                s.setProperty (madeId, current.getFullPathName(), nullptr);
            }
            // Where it sits now, in beats (the user may have moved or trimmed it)
            const auto pos = c->getPosition();
            const auto ratioNow = (double) s[bpmId] / bpm;            // length of the playing file / original
            s.setProperty (startId, ts.toBeats (pos.getStart()).inBeats(), nullptr);
            s.setProperty (endId, ts.toBeats (pos.getEnd()).inBeats(), nullptr);
            s.setProperty (offsetId, pos.getOffset().inSeconds() / juce::jmax (1.0e-6, ratioNow), nullptr);
        }
    }

    void setTempoKeepingRecordings (te::Edit& edit, double newBpm)
    {
        auto* tempo = edit.tempoSequence.getTempo (0);
        if (tempo == nullptr || std::abs (tempo->getBpm() - newBpm) < 1.0e-6)
            return;

        // Recordings: kept in seconds while the tempo changes (MIDI, chords and loops follow the beat)
        std::vector<std::pair<te::Clip::Ptr, int>> previous;
        for (auto* c : recordings (edit))
        {
            previous.push_back ({ c, (int) c->getSyncType() });
            c->state.setProperty (te::IDs::sync, (int) te::Clip::syncAbsolute, nullptr);
        }
        tempo->setBpm (newBpm);
        for (auto& [c, sync] : previous)
            c->state.setProperty (te::IDs::sync, sync, nullptr);

        if (getMode (edit) != Mode::keep)
            for (auto& [c, sync] : previous)
                c->state.setProperty (waitingId, true, nullptr);
    }

    void sourceReplaced (te::WaveAudioClip& c)
    {
        auto& s = c.state;
        if (! s.hasProperty (srcId)) return;
        const auto now = c.getOriginalFile();
        if (now == juce::File (s[madeId].toString()) || now == juce::File (s[srcId].toString()))
            return;
        // Where it sits, in beats, before its length changes
        auto& ts = c.edit.tempoSequence;
        const auto pos = c.getPosition();
        s.setProperty (startId, ts.toBeats (pos.getStart()).inBeats(), nullptr);
        s.setProperty (endId, ts.toBeats (pos.getEnd()).inBeats(), nullptr);
        s.setProperty (offsetId, pos.getOffset().inSeconds() / juce::jmax (1.0e-6, (double) s[bpmId] / songBpm (c.edit)), nullptr);
        s.setProperty (srcId, now.getFullPathName(), nullptr);
        s.setProperty (madeId, now.getFullPathName(), nullptr);
        s.setProperty (waitingId, true, nullptr);
    }

    void forget (te::Edit& edit)
    {
        for (auto* c : recordings (edit))
            for (auto id : { srcId, bpmId, madeId, startId, endId, offsetId, waitingId })
                c->state.removeProperty (id, nullptr);
    }

    int numWaiting (te::Edit& edit)
    {
        int n = 0;
        for (auto* c : recordings (edit))
            if ((bool) c->state.getProperty (waitingId, false)) ++n;
        return n;
    }

    //==========================================================================
    std::vector<Job> jobsFor (te::Edit& edit)
    {
        std::vector<Job> jobs;
        const auto bpm = songBpm (edit);
        for (auto* c : recordings (edit))
        {
            if (! (bool) c->state.getProperty (waitingId, false)) continue;
            Job j;
            j.clip = c->itemID;
            j.source = juce::File (c->state[srcId].toString());
            j.ratio = (double) c->state[bpmId] / bpm;
            if (std::abs (j.ratio - 1.0) < 1.0e-4)
                j.dest = j.source;                                    // back at its own tempo: the original itself
            else
                j.dest = folderFor (edit, j.source).getChildFile (j.source.getFileNameWithoutExtension()
                                                                  + " @ " + bpmText (bpm) + " (from " + bpmText ((double) c->state[bpmId]) + ").wav");
            jobs.push_back (j);
        }
        return jobs;
    }

    bool render (const Job& job, const std::function<bool()>& keepGoing)
    {
        if (job.dest == job.source || job.dest.existsAsFile())
            return job.source.existsAsFile();
        std::unique_ptr<juce::AudioFormatReader> rd (formats().createReaderFor (job.source));
        if (rd == nullptr) return false;
        const int n = (int) rd->lengthInSamples;
        const int ch = (int) juce::jlimit (1u, 2u, rd->numChannels);
        const double sr = rd->sampleRate;
        juce::AudioBuffer<float> in (ch, n);
        rd->read (&in, 0, n, 0, true, ch > 1);
        const int outLen = juce::jmax (1, (int) std::lround (n * job.ratio));

        // Padding before and after, so the very start and end of the recording are stretched cleanly
        const int pad = (int) (sr * 0.2);
        std::vector<std::vector<float>> src ((size_t) ch, std::vector<float> ((size_t) (n + 2 * pad), 0.0f));
        for (int c = 0; c < ch; ++c)
            std::copy (in.getReadPointer (c), in.getReadPointer (c) + n, src[(size_t) c].begin() + pad);
        const int padOut = (int) std::lround (pad * job.ratio);
        const int total = outLen + 2 * padOut;
        std::vector<std::vector<float>> dst ((size_t) ch, std::vector<float> ((size_t) total, 0.0f));
        if (keepGoing && ! keepGoing()) return false;

        signalsmith::stretch::SignalsmithStretch<float> stretch;
        stretch.presetDefault (ch, (float) sr);
        stretch.exact (src, n + 2 * pad, dst, total);
        if (keepGoing && ! keepGoing()) return false;

        juce::AudioBuffer<float> out (ch, outLen);
        for (int c = 0; c < ch; ++c)
            std::copy (dst[(size_t) c].begin() + padOut, dst[(size_t) c].begin() + padOut + outLen, out.getWritePointer (c));

        job.dest.getParentDirectory().createDirectory();
        auto tmp = job.dest.withFileExtension (".part");
        tmp.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (tmp);
            auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (ch).withBitsPerSample (24));
            if (w == nullptr || ! w->writeFromAudioSampleBuffer (out, 0, outLen)) return false;
        }
        return tmp.moveFileTo (job.dest);
    }

    bool finish (te::Edit& edit, const Job& job)
    {
        auto* c = dynamic_cast<te::WaveAudioClip*> (te::findClipForID (edit, job.clip));
        if (c == nullptr || juce::File (c->state[srcId].toString()) != job.source || ! job.dest.existsAsFile())
            return false;
        auto& ts = edit.tempoSequence;
        const auto bpm = songBpm (edit);
        const auto ratio = (double) c->state[bpmId] / bpm;
        if (std::abs (ratio - job.ratio) > 1.0e-6)
            return false;                                             // the tempo changed again since: a newer job follows

        if (c->getOriginalFile() != job.dest)
            c->getSourceFileReference().setToFile (job.dest, te::SourceFileReference::PathStyle::chooseBest, false);
        const auto start = ts.toTime (te::BeatPosition::fromBeats ((double) c->state[startId]));
        const auto end = ts.toTime (te::BeatPosition::fromBeats ((double) c->state[endId]));
        const auto offset = te::TimeDuration::fromSeconds ((double) c->state[offsetId] * ratio);
        c->setPosition ({ { start, end }, offset });
        c->state.setProperty (madeId, job.dest.getFullPathName(), nullptr);
        c->state.removeProperty (waitingId, nullptr);
        AudioEdit::prepare (*c);
        return true;
    }

    int refreshNow (te::Edit& edit)
    {
        int n = 0;
        for (auto& j : jobsFor (edit))
            if (render (j) && finish (edit, j)) ++n;
        return n;
    }

    int tidyUp (te::Edit& edit)
    {
        juce::Array<juce::File> used, folders;
        for (auto* t : te::getAudioTracks (edit))
            for (auto* c : t->getClips())
                if (auto* w = dynamic_cast<te::WaveAudioClip*> (c))
                {
                    used.add (w->getOriginalFile());
                    // Any file the clip remembers (its original, a timing correction's source...) stays
                    for (int i = 0; i < w->state.getNumProperties(); ++i)
                    {
                        const auto v = w->state[w->state.getPropertyName (i)].toString();
                        if (juce::File::isAbsolutePath (v)) used.add (juce::File (v));
                    }
                    if (w->getOriginalFile().getParentDirectory().getFileName() == "Tempo")
                        folders.addIfNotAlreadyThere (w->getOriginalFile().getParentDirectory());
                }
        if (edit.editFileRetriever && edit.editFileRetriever() != juce::File())
            folders.addIfNotAlreadyThere (edit.editFileRetriever().getParentDirectory().getChildFile ("Audio").getChildFile ("Tempo"));
        int removed = 0;
        for (auto& folder : folders)
            if (folder.isDirectory() && folder.getFileName() == "Tempo")
                for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav"))
                    if (isStretchedCopy (f) && ! used.contains (f) && f.deleteFile()) ++removed;
        return removed;
    }
}
