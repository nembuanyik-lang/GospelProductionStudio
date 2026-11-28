#include "AudioEdit.h"
#include "WavFiles.h"
#include "Takes.h"
#include "RenderTest.h"
#include "Learn.h"
#include "Loops.h"
#include "Timing.h"
#include "Harmony.h"
#include "Backing.h"
#include "GuitarPlugin.h"
#include "Percussion.h"
#include "ChordDetect.h"
#include "Stems.h"
#include "Help.h"
#include "TempoFollow.h"
#include "BackgroundWork.h"
#include "SongFiles.h"
#include "RealFeel.h"
#include "LatencyCheck.h"
#include "SongSafety.h"
#include "Sfz.h"
#include "StudioEngine.h"
#include "ChordTrack.h"
#include "DeEsserPlugin.h"
#include "AnalyserPlugin.h"
#include "Analysis.h"
#include "Release.h"
#include "Mp3Encoder.h"
#include "MySounds.h"
#include "OrganPlugin.h"
#include "SamplerPlugin.h"
#include "StudioKeysPlugin.h"
#include "Grooves.h"
#include "PluginHost.h"
#include "Mixing.h"
#include "Automation.h"
#include "Exporter.h"
#include "MasterPlugin.h"
#include "VocalTunePlugin.h"
#include "Loudness.h"

namespace RenderTest
{
    namespace
    {
        void addChord (te::MidiList& seq, std::initializer_list<int> notes, double start, double length, int velocity = 96)
        {
            for (auto n : notes)
                seq.addNote (n, te::BeatPosition::fromBeats (start), te::BeatDuration::fromBeats (length), velocity, 0, nullptr);
        }

        bool render (te::Engine& engine, const juce::File& out, const juce::String& pluginType,
                     std::function<void (te::Plugin&)> configure,
                     std::function<void (te::MidiList&)> notes, double seconds)
        {
            auto edit = te::createEmptyEdit (engine, out.withFileExtension ("tracktionedit"));
            edit->tempoSequence.getTempos()[0]->setBpm (90.0);
            edit->ensureNumberOfAudioTracks (1);
            auto* track = te::getAudioTracks (*edit)[0];

            auto plugin = edit->getPluginCache().createNewPlugin (pluginType, {});
            if (plugin == nullptr)
                return false;
            track->pluginList.insertPlugin (plugin, 0, nullptr);
            configure (*plugin);

            // Wait for samples to load
            if (auto* s = dynamic_cast<SamplerPlugin*> (plugin.get()))
                for (int i = 0; i < 300 && s->getStatus() == SamplerPlugin::Status::loading; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

            auto clip = track->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (seconds)), nullptr);
            notes (clip->getSequence());

            out.deleteFile();
            const auto ok = te::Renderer::renderToFile (*edit, out, false);
            std::cout << out.getFileName() << (ok ? " rendered" : " FAILED");
            if (auto* s = dynamic_cast<SamplerPlugin*> (plugin.get()))
                std::cout << "  (sampler: " << s->getStatusText() << ", " << s->getMemoryBytes() / 1024 << " KB)";
            std::cout << std::endl;
            return ok;
        }
    }

    namespace
    {
        void waitForPlugin (te::Plugin& p)
        {
            if (auto* ep = dynamic_cast<te::ExternalPlugin*> (&p))
                for (int i = 0; i < 250 && ep->isInitialisingAsync(); ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }

        // Like turning a knob in the plugin's own window
        void turnKnob (te::Plugin& p, const juce::String& name, float normalised)
        {
            if (auto* ep = dynamic_cast<te::ExternalPlugin*> (&p))
                if (auto* pi = ep->getAudioPluginInstance())
                    for (auto* param : pi->getParameters())
                        if (param->getName (64).equalsIgnoreCase (name))
                            param->setValueNotifyingHost (normalised);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);   // the host hears about it
        }

        void pluginTests (te::Engine& engine, const juce::File& dir, const juce::File& folder)
        {
            auto& scanner = PluginHost::getScanner (engine);
            const auto start = juce::Time::getMillisecondCounter();
            const auto found = scanner.scanFolderNow (dir);
            std::cout << "plugin scan: " << found << " files ok in " << (juce::Time::getMillisecondCounter() - start) << " ms, "
                      << PluginHost::getInstruments (engine).size() << " instruments, "
                      << PluginHost::getEffects (engine).size() << " effects" << std::endl;
            for (auto& f : scanner.getLastFailures())
                std::cout << "  failed: " << f << std::endl;

            const auto instruments = PluginHost::getInstruments (engine);
            const auto effects = PluginHost::getEffects (engine);
            if (instruments.isEmpty() || effects.isEmpty())
            {
                PluginHost::releaseScanner();
                return;
            }

            for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "plugin*.wav"))
                f.deleteFile();      // the renderer adds to an existing file
            const auto editFile = folder.getChildFile ("plugins.tracktionedit");
            editFile.deleteFile();
            {
                auto edit = te::createEmptyEdit (engine, editFile);
                edit->editFileRetriever = [editFile] { return editFile; };
                edit->ensureNumberOfAudioTracks (1);
                auto* track = te::getAudioTracks (*edit)[0];

                auto synth = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, instruments.getReference (0));
                synth->state.setProperty ("gpsInstrument", true, nullptr);
                track->pluginList.insertPlugin (synth, 0, nullptr);
                waitForPlugin (*synth);
                std::cout << "instrument: " << PluginHost::describe (*synth) << "  synth " << (int) synth->isSynth() << std::endl;

                auto clip = track->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2.0)), nullptr);
                clip->getSequence().addNote (69, te::BeatPosition(), te::BeatDuration::fromBeats (3.0), 100, 0, nullptr);

                auto out = folder.getChildFile ("plugin_synth.wav");
                std::cout << out.getFileName() << (te::Renderer::renderToFile (*edit, out, false) ? " rendered" : " FAILED") << std::endl;

                auto gain = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, effects.getReference (0));
                track->pluginList.insertPlugin (gain, 1, nullptr);
                waitForPlugin (*gain);
                std::cout << "effect: " << PluginHost::describe (*gain) << std::endl;
                out = folder.getChildFile ("plugin_synth_gain.wav");
                std::cout << out.getFileName() << (te::Renderer::renderToFile (*edit, out, false) ? " rendered" : " FAILED") << std::endl;

                // Gain to 1.5 (normalised 0.75), then save the song
                turnKnob (*gain, "Gain", 0.75f);
                out = folder.getChildFile ("plugin_knob.wav");
                std::cout << out.getFileName() << (te::Renderer::renderToFile (*edit, out, false) ? " rendered" : " FAILED") << std::endl;
                te::EditFileOperations (*edit).save (true, true, false);
            }
            {
                // Reopen: the plugin settings come back from the song
                auto edit = te::loadEditFromFile (engine, editFile);
                for (auto* p : te::getAllPlugins (*edit, false))
                    waitForPlugin (*p);
                auto out = folder.getChildFile ("plugin_reload.wav");
                std::cout << out.getFileName() << (te::Renderer::renderToFile (*edit, out, false) ? " rendered" : " FAILED") << std::endl;
            }
            {
                // A song made on another computer: the plugin is not installed here
                auto text = editFile.loadFileAsString();
                text = text.replace (instruments.getReference (0).fileOrIdentifier, "/nowhere/Missing Synth.vst3")
                           .replace (instruments.getReference (0).name, "Missing Synth")
                           .replace (juce::String::toHexString (instruments.getReference (0).uniqueId), "1234abcd")
                           .replace (juce::String::toHexString (instruments.getReference (0).deprecatedUid), "4321dcba");
                auto missingFile = folder.getChildFile ("plugins_missing.tracktionedit");
                missingFile.replaceWithText (text);
                auto edit = te::loadEditFromFile (engine, missingFile);
                auto* track = te::getAudioTracks (*edit)[0];
                for (auto* p : track->pluginList.getPlugins())
                {
                    waitForPlugin (*p);
                    if (PluginHost::isPlugin (p))
                        std::cout << "missing test: " << PluginHost::describe (*p) << "  flagged instrument "
                                  << (int) (bool) p->state.getProperty ("gpsInstrument", false) << std::endl;
                }
                auto out = folder.getChildFile ("plugin_missing.wav");
                const auto ok = te::Renderer::renderToFile (*edit, out, false);
                std::cout << "missing test: song opened and played without the plugin (" << (ok ? "rendered" : "silent, as expected") << ")" << std::endl;
            }
            PluginHost::releaseScanner();
        }
    }

    namespace
    {
        void mixerTests (te::Engine& engine, const juce::File& folder)
        {
            for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "mix_*.wav"))
                f.deleteFile();

            auto editFile = folder.getChildFile ("mixer.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->ensureNumberOfAudioTracks (1);
            auto* track = te::getAudioTracks (*edit)[0];
            Mixing::prepareSong (*edit);
            std::cout << "mixer: song tracks " << Mixing::getSongTracks (*edit).size()
                      << ", all tracks " << te::getAudioTracks (*edit).size() << std::endl;

            auto organ = edit->getPluginCache().createNewPlugin (OrganPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (organ, 0, nullptr);
            auto clip = track->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (4.0)), nullptr);
            addChord (clip->getSequence(), { 60, 64, 67 }, 0.0, 0.5);     // short chord, then silence

            auto render = [&] (const char* name)
            {
                auto out = folder.getChildFile (name);
                std::cout << out.getFileName() << (te::Renderer::renderToFile (*edit, out, false) ? " rendered" : " FAILED") << std::endl;
            };

            render ("mix_dry.wav");
            Mixing::setSendDb (*track, Mixing::reverbBus, 0.0f);
            std::cout << "  send now " << Mixing::getSendDb (*track, Mixing::reverbBus) << " dB" << std::endl;
            render ("mix_reverb.wav");
            Mixing::setSendDb (*track, Mixing::reverbBus, Mixing::sendOffDb);
            Mixing::setSendDb (*track, Mixing::delayBus, -3.0f);
            render ("mix_delay.wav");
            Mixing::setSendDb (*track, Mixing::delayBus, Mixing::sendOffDb);

            // Fader and pan
            track->getVolumePlugin()->setVolumeDb (-6.0f);
            track->getVolumePlugin()->setPan (-1.0f);
            render ("mix_fader_left.wav");
            track->getVolumePlugin()->setVolumeDb (0.0f);
            track->getVolumePlugin()->setPan (0.0f);

            // Every built-in effect can be created; EQ and compressor in the chain
            for (auto& b : Mixing::builtInEffects())
                std::cout << "  built-in " << b.name << (Mixing::createBuiltIn (*edit, b.type) != nullptr ? " ok" : " MISSING") << std::endl;
            for (auto* type : { te::EqualiserPlugin::xmlTypeName, te::CompressorPlugin::xmlTypeName })
                track->pluginList.insertPlugin (Mixing::createBuiltIn (*edit, type), 1, nullptr);
            render ("mix_eq_comp.wav");

            // Master effect
            Mixing::addMasterEffect (*edit, Mixing::createBuiltIn (*edit, te::ReverbPlugin::xmlTypeName));
            std::cout << "  master effects " << Mixing::getMasterEffects (*edit).size() << std::endl;
        }
    }

    namespace
    {
        // A held organ chord: volume fades out over 4 s, pan sweeps left to right,
        // and an EQ setting changes half way through
        void automationTests (te::Engine& engine, const juce::File& folder)
        {
            for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "auto_*.wav"))
                f.deleteFile();

            auto editFile = folder.getChildFile ("automation.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->ensureNumberOfAudioTracks (1);
            auto* track = te::getAudioTracks (*edit)[0];
            Mixing::prepareSong (*edit);

            track->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (OrganPlugin::xmlTypeName, {}), 0, nullptr);
            auto clip = track->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (4.0)), nullptr);
            addChord (clip->getSequence(), { 60, 64, 67 }, 0.0, 6.0);

            auto* vol = Automation::resolve (*track, "vol");
            auto& vc = vol->getCurve();
            vc.addPoint (te::TimePosition::fromSeconds (0.5), te::decibelsToVolumeFaderPosition (0.0f), 0.0f, nullptr);
            vc.addPoint (te::TimePosition::fromSeconds (3.5), te::decibelsToVolumeFaderPosition (-30.0f), 0.0f, nullptr);
            std::cout << "automation: volume points " << vc.getNumPoints()
                      << ", at 2 s " << vol->valueToString (vc.getValueAt (te::TimePosition::fromSeconds (2.0), 0.0f)) << std::endl;

            auto render = [&] (const char* name)
            {
                auto out = folder.getChildFile (name);
                std::cout << out.getFileName() << (te::Renderer::renderToFile (*edit, out, false) ? " rendered" : " FAILED") << std::endl;
            };
            render ("auto_fade.wav");

            vc.clear (nullptr);
            auto* pan = Automation::resolve (*track, "pan");
            pan->getCurve().addPoint (te::TimePosition::fromSeconds (0.0), -1.0f, 0.0f, nullptr);
            pan->getCurve().addPoint (te::TimePosition::fromSeconds (4.0), 1.0f, 0.0f, nullptr);
            render ("auto_pan.wav");

            // A send that only opens in the second half
            pan->getCurve().clear (nullptr);
            if (auto* send = Automation::resolve (*track, "send0", true))
            {
                send->getCurve().addPoint (te::TimePosition::fromSeconds (1.9), 0.0f, 0.0f, nullptr);
                send->getCurve().addPoint (te::TimePosition::fromSeconds (2.0), te::decibelsToVolumeFaderPosition (0.0f), 0.0f, nullptr);
                std::cout << "  send automated: " << Automation::describe (*track, "send0") << std::endl;
            }
            std::cout << "  automated settings on the track: " << Automation::countAutomated (*track) << std::endl;

            // Saved and reloaded with the song
            te::EditFileOperations (*edit).save (true, true, false);
            auto reloaded = te::loadEditFromFile (engine, editFile);
            auto* t2 = te::getAudioTracks (*reloaded)[0];
            std::cout << "  after reopening: " << Automation::countAutomated (*t2) << " automated" << std::endl;
        }
    }

    namespace
    {
        // A short song (organ chord + piano notes, reverb on the organ), exported three ways, then frozen
        void exportTests (te::Engine& engine, const juce::File& folder)
        {
            auto out = folder.getChildFile ("exports");
            out.deleteRecursively();

            auto editFile = folder.getChildFile ("export.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            edit->ensureNumberOfAudioTracks (3);
            Mixing::prepareSong (*edit);
            auto tracks = Mixing::getSongTracks (*edit);
            tracks[0]->setName ("Organ");
            tracks[1]->setName ("Keys");
            tracks[2]->setName ("Empty");

            tracks[0]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (OrganPlugin::xmlTypeName, {}), 0, nullptr);
            tracks[1]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (StudioKeysPlugin::xmlTypeName, {}), 0, nullptr);
            auto c0 = tracks[0]->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (4.0)), nullptr);
            addChord (c0->getSequence(), { 60, 64, 67 }, 0.0, 6.0);
            auto c1 = tracks[1]->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (4.0)), nullptr);
            for (int b = 0; b < 8; ++b)
                addChord (c1->getSequence(), { 72 + (b % 4) * 2 }, b, 0.5);
            Mixing::setSendDb (*tracks[0], Mixing::reverbBus, 0.0f);
            edit->getTransport().setLoopRange (te::TimeRange (te::TimePosition::fromSeconds (1.0), te::TimePosition::fromSeconds (2.0)));

            std::cout << "export: song length " << Exporter::songRange (*edit).getLength().inSeconds()
                      << " s, stem tracks " << Exporter::stemTracks (*edit).size() << std::endl;

            auto run = [&] (Exporter::What what, Exporter::Format format, const char* name, bool fx = true, bool loud = true)
            {
                Exporter::Options o;
                o.stemsWithSharedFx = fx;
                o.loudForSharing = loud;
                o.what = what;
                o.format = format;
                o.title = "Test Song";
                o.artist = "Test Choir";
                o.folder = out;
                bool done = false;
                Exporter::Job::Callbacks cb;
                cb.finished = [&] (juce::Result r, const juce::Array<juce::File>& files, const juce::StringArray& warnings)
                {
                    for (auto& w : warnings)
                        std::cout << "  WARNING: " << w << std::endl;
                    std::cout << "  " << name << ": " << (r.wasOk() ? "ok" : r.getErrorMessage());
                    for (auto& f : files)
                        std::cout << "  [" << f.getFileName() << " " << f.getSize() << " bytes]";
                    std::cout << std::endl;
                    done = true;
                };
                Exporter::Job job (*edit, o, cb);
                const auto t0 = juce::Time::getMillisecondCounter();
                job.start();
                while (! done && juce::Time::getMillisecondCounter() - t0 < 60000)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            };

            run (Exporter::What::mix, Exporter::Format::mp3High, "mix mp3");
            run (Exporter::What::mix, Exporter::Format::wav24, "mix wav24");
            run (Exporter::What::stems, Exporter::Format::wav24, "stems wav24");
            run (Exporter::What::loop, Exporter::Format::wav16, "loop wav16");
            if (juce::SystemStats::getEnvironmentVariable ("GPS_LEVELS", {}).isNotEmpty())
            {
                out = folder.getChildFile ("exports_dry");
                run (Exporter::What::stems, Exporter::Format::wav24, "stems without shared fx", false);
                run (Exporter::What::mix, Exporter::Format::wav24, "mix as mixed", true, false);
                tracks[0]->getVolumePlugin()->setVolumeDb (6.0f);
                tracks[0]->getVolumePlugin()->setVolumeDb (6.0f);
                Mixing::setSendDb (*tracks[0], Mixing::reverbBus, 6.0f);
                run (Exporter::What::stems, Exporter::Format::wav24, "stems too loud");
                tracks[0]->getVolumePlugin()->setVolumeDb (0.0f);
                Mixing::setSendDb (*tracks[0], Mixing::reverbBus, 0.0f);
                out = folder.getChildFile ("exports");
            }

            // Cancel half way
            {
                Exporter::Options o;
                o.what = Exporter::What::stems; o.format = Exporter::Format::mp3Small; o.title = "Cancelled"; o.folder = out;
                bool done = false;
                Exporter::Job::Callbacks cb;
                cb.finished = [&] (juce::Result r, const juce::Array<juce::File>&, const juce::StringArray&) { std::cout << "  cancel: " << r.getErrorMessage() << std::endl; done = true; };
                Exporter::Job job (*edit, o, cb);
                job.start();
                job.cancel();
                const auto t0 = juce::Time::getMillisecondCounter();
                while (! done && juce::Time::getMillisecondCounter() - t0 < 20000)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                std::cout << "  files named Cancelled left: " << out.findChildFiles (juce::File::findFiles, false, "Cancelled*").size() << std::endl;
            }

            // Freeze the organ track and render again: should sound the same
            te::EditFileOperations (*edit).save (true, true, false);
            const auto t0 = juce::Time::getMillisecondCounter();
            const auto r = tracks[0]->setFrozen (true, te::Track::individualFreeze);
            std::cout << "  freeze: " << (r.wasOk() ? juce::String ("ok") : r.getErrorMessage())
                      << " in " << (int) (juce::Time::getMillisecondCounter() - t0) << " ms, frozen "
                      << (int) tracks[0]->isFrozen (te::Track::individualFreeze) << std::endl;
            run (Exporter::What::mix, Exporter::Format::wav24, "mix after freeze");
            const auto r2 = tracks[0]->setFrozen (false, te::Track::individualFreeze);
            std::cout << "  unfreeze: " << (r2.wasOk() ? "ok" : r2.getErrorMessage()) << std::endl;
        }
    }

    namespace
    {
        void masteringTests (te::Engine& engine, const juce::File& folder)
        {
            for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "master_*.wav"))
                f.deleteFile();

            auto editFile = folder.getChildFile ("mastering.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->ensureNumberOfAudioTracks (2);
            Mixing::prepareSong (*edit);
            auto tracks = Mixing::getSongTracks (*edit);
            tracks[0]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (OrganPlugin::xmlTypeName, {}), 0, nullptr);
            tracks[1]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (StudioKeysPlugin::xmlTypeName, {}), 0, nullptr);
            auto c0 = tracks[0]->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (12.0)), nullptr);
            for (int b = 0; b < 24; b += 4)
                addChord (c0->getSequence(), { 48, 55, 60, 64, 67 }, b, 4.0);
            auto c1 = tracks[1]->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (12.0)), nullptr);
            for (int b = 0; b < 24; ++b)
                addChord (c1->getSequence(), { 72 + (b % 5) * 2 }, b, 0.5, 70 + (b % 4) * 15);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto render = [&] (const char* name)
            {
                te::Renderer::Parameters p (*edit);
                p.time = Exporter::songRange (*edit);
                p.endAllowance = te::TimeDuration::fromSeconds (2.0);
                p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
                p.bitDepth = 32;
                p.useMasterPlugins = true;
                p.canRenderInMono = false;
                p.destFile = folder.getChildFile (name);
                p.destFile.deleteFile();
                bool finished = false;
                auto handle = te::EditRenderer::render (p, [&finished] (tl::expected<juce::File, std::string>) { finished = true; });
                for (int i = 0; i < 3000 && ! finished; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
                handle.reset();
                const auto written = p.destFile;
                if (! written.existsAsFile())
                {
                    std::cout << "  " << name << ": FAILED to render" << std::endl;
                    return -100.0f;
                }
                // Measure with our own meter (compared with ffmpeg afterwards)
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (p.destFile));
                LoudnessMeter m; m.prepare (r->sampleRate, 60.0);
                juce::AudioBuffer<float> b (2, (int) r->lengthInSamples);
                r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
                m.process (b.getReadPointer (0), b.getReadPointer (1), b.getNumSamples());
                std::cout << "  " << name << ": " << juce::String (m.getIntegrated(), 1) << " LUFS (our meter), peak "
                          << juce::String (juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples())), 2) << " dB" << std::endl;
                return m.getIntegrated();
            };

            std::cout << "mastering:" << std::endl;
            tracks[0]->setMute (true);
            render ("master_keys_off.wav");
            tracks[0]->setMute (false);
            render ("master_off.wav");

            auto* mp = MasterPlugin::addTo (*edit);
            mp->warmth = 0.0f; mp->clarity = 0.0f; mp->air = 0.0f; mp->glue = 0.0f; mp->width = 1.0f; mp->push = 0.0f; mp->lowCut = false;
            render ("master_flat.wav");                       // should match "off" (no added delay)
            tracks[0]->setMute (true);
            render ("master_keys_flat.wav");
            tracks[0]->setMute (false);

            mp->push = 18.0f;
            render ("master_push18.wav");                     // peaks must stay under -1 dB

            mp->applyPreset (MasterPlugin::presets()[1]);     // Uptempo praise
            mp->push = 0.0f;
            // What "Set loudness" does: measure, adjust Push, again
            for (int pass = 0; pass < 3; ++pass)
            {
                const auto measured = render ("master_auto.wav");
                const auto diff = mp->targetLufs.get() - measured;
                std::cout << "    pass " << pass + 1 << ": aim " << mp->targetLufs.get() << ", off by " << juce::String (diff, 2) << " dB" << std::endl;
                if (std::abs (diff) <= 0.4f) break;
                mp->push = juce::jlimit (-12.0f, 18.0f, mp->push.get() + diff);
            }

            te::EditFileOperations (*edit).save (true, true, false);
            auto reloaded = te::loadEditFromFile (engine, editFile);
            if (auto* m2 = MasterPlugin::findIn (*reloaded))
                std::cout << "  after reopening: preset " << m2->presetName.get() << ", push " << juce::String (m2->push.get(), 1) << " dB" << std::endl;
        }
    }

    namespace
    {
        // A sung line that is a little out of tune (A major), through vocal tuning
        void vocalTuneTests (te::Engine& engine, const juce::File& folder, const juce::File& voice)
        {
            std::cout << "vocal tuning:" << std::endl;
            for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "tune_*.wav"))
                f.deleteFile();

            auto editFile = folder.getChildFile ("tune.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            te::EditFileOperations (*edit).save (true, true, false);      // so the voice file is found next to it
            edit->ensureNumberOfAudioTracks (1);
            auto* track = te::getAudioTracks (*edit)[0];
            auto copy = folder.getChildFile ("tune_source.wav");
            voice.copyFileTo (copy);
            track->insertWaveClip ("voice", copy, { { te::TimePosition(), te::TimePosition::fromSeconds (3.35) }, {} }, false);

            auto render = [&] (const char* name)
            {
                te::Renderer::Parameters p (*edit);
                p.time = { te::TimePosition(), te::TimePosition::fromSeconds (3.35) };
                p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
                p.bitDepth = 24;
                p.canRenderInMono = false;
                p.destFile = folder.getChildFile (name);
                p.destFile.deleteFile();
                bool finished = false;
                auto handle = te::EditRenderer::render (p, [&finished] (tl::expected<juce::File, std::string>) { finished = true; });
                for (int i = 0; i < 3000 && ! finished; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
                std::cout << "  " << name << (p.destFile.existsAsFile() ? " rendered" : " FAILED") << std::endl;
            };

            render ("tune_off.wav");
            auto p = edit->getPluginCache().createNewPlugin (VocalTunePlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (p, 0, nullptr);
            auto* vt = dynamic_cast<VocalTunePlugin*> (p.get());
            vt->keyOverride = 9;          // A major
            vt->minorOverride = false;
            vt->scaleMode = 0;
            vt->strength = 1.0f;
            vt->speedMs = 25.0f;
            vt->range = 1;
            std::cout << "  key " << vt->describeKey() << ", latency " << juce::String (vt->getLatencySeconds() * 1000.0, 1) << " ms" << std::endl;
            render ("tune_on.wav");
            vt->speedMs = 200.0f;
            vt->strength = 0.5f;
            render ("tune_gentle.wav");
        }
    }

    //==========================================================================
    //  Step 18: loops follow the tempo, and My Sounds play
    //==========================================================================
    namespace
    {
        void writeClickLoop (const juce::File& f, double bpm, int beats, double sr = 44100.0)
        {
            const auto len = (int) std::round (beats * 60.0 / bpm * sr);
            juce::AudioBuffer<float> b (2, len);
            b.clear();
            for (int beat = 0; beat < beats; ++beat)
            {
                const int start = (int) std::round (beat * 60.0 / bpm * sr);
                const double freq = beat % 4 == 0 ? 1760.0 : 880.0;
                for (int i = 0; i < (int) (0.03 * sr) && start + i < len; ++i)
                {
                    const auto v = (float) (0.8 * std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr) * std::exp (-i / (0.008 * sr)));
                    b.setSample (0, start + i, v);
                    b.setSample (1, start + i, v);
                }
            }
            f.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (f, sr, (int) (2), 24).release());
            w->writeFromAudioSampleBuffer (b, 0, len);
        }

        void writeTone (const juce::File& f, double freq, double seconds, double sr = 44100.0)
        {
            const auto len = (int) (seconds * sr);
            juce::AudioBuffer<float> b (1, len);
            for (int i = 0; i < len; ++i)
            {
                const auto t = i / sr;
                double v = 0.0;
                for (int h = 1; h <= 4; ++h) v += std::sin (2.0 * juce::MathConstants<double>::pi * freq * h * t) / h;
                b.setSample (0, i, (float) (0.3 * v * std::min (1.0, t * 50.0) * std::exp (-t * 0.8)));
            }
            f.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (f, sr, (int) (1), 24).release());
            w->writeFromAudioSampleBuffer (b, 0, len);
        }

        // Times (seconds) where the sound jumps up from silence
        std::vector<double> onsets (const juce::File& f)
        {
            std::vector<double> out;
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
            if (r == nullptr) return out;
            juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
            r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
            const int hop = 64;
            bool quiet = true;
            int quietRun = 0;
            for (int i = 0; i + hop < b.getNumSamples(); i += hop)
            {
                const auto m = b.getMagnitude (0, i, hop);
                if (quiet && m > 0.1f) { out.push_back ((i + b.getNumSamples() * 0) / r->sampleRate); quiet = false; quietRun = 0; }
                else if (! quiet) { quietRun = m < 0.02f ? quietRun + 1 : 0; if (quietRun > 40) quiet = true; }
            }
            return out;
        }

        void loopTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "loops:" << std::endl;

            // Names
            auto nameCheck = [] (const char* n, double bpm, int root)
            {
                const auto t = Loops::tempoFromName (n);
                const auto r = Loops::rootFromName (n);
                const bool ok = (juce::exactlyEqual (bpm, 0.0) ? ! t.has_value() : (t.has_value() && std::abs (*t - bpm) < 0.01)) && r == root;
                std::cout << "  name \"" << n << "\": " << (t ? juce::String (*t) : juce::String ("?")) << " BPM, root " << r
                          << (ok ? "  ok" : "  FAILED") << std::endl;
            };
            nameCheck ("Shaker Loop 96bpm", 96.0, -1);
            nameCheck ("Gospel_Guitar_Riff_G_88_BPM", 88.0, 7);
            nameCheck ("Organ swell Ebm 72", 72.0, 3);
            nameCheck ("Clap loop", 0.0, -1);
            nameCheck ("Bass_F#min_100bpm", 100.0, 6);

            // A 2-bar click loop at 90 BPM, played in a song at 120 BPM, repeated twice
            const auto src = folder.getChildFile ("loop_clicks_90bpm.wav");
            writeClickLoop (src, 90.0, 8);
            auto editFile = folder.getChildFile ("loops.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            edit->tempoSequence.getTempos()[0]->setBpm (120.0);
            te::EditFileOperations (*edit).save (true, true, false);
            edit->ensureNumberOfAudioTracks (1);
            auto* track = te::getAudioTracks (*edit)[0];

            const auto g = Loops::guess (engine, src, 120.0, 4, true);
            std::cout << "  guess: " << g.bpm << " BPM, " << g.beats << " beats (" << g.how << ")"
                      << (std::abs (g.bpm - 90.0) < 0.1 && std::abs (g.beats - 8.0) < 0.01 ? "  ok" : "  FAILED") << std::endl;

            // Length only (no tempo in the name): 5.33 s in a 120 BPM song -> 2 bars at 90
            const auto plain = folder.getChildFile ("loop_plain.wav");
            src.copyFileTo (plain);
            const auto g2 = Loops::guess (engine, plain, 100.0, 4, true);
            std::cout << "  guess from length: " << g2.bpm << " BPM, " << g2.beats << " beats"
                      << (std::abs (g2.bpm - 90.0) < 0.1 ? "  ok" : "  FAILED") << std::endl;

            const te::ClipPosition pos { { te::TimePosition::fromSeconds (0.5), te::TimePosition::fromSeconds (0.5 + 16.0 / 3.0) }, {} };
            auto clip = track->insertWaveClip ("loop", src, pos, false);
            const bool followed = Loops::follow (*clip, g, 2);
            std::cout << "  follow: " << (followed ? "yes" : "no") << ", clip " << juce::String (clip->getPosition().getLength().inSeconds(), 3)
                      << " s (want 8.000)" << (std::abs (clip->getPosition().getLength().inSeconds() - 8.0) < 0.01 ? "  ok" : "  FAILED") << std::endl;
            std::cout << "  " << Loops::describe (*clip) << std::endl;
            std::cout << "  start " << clip->getPosition().getStart().inSeconds() << " offset " << clip->getPosition().getOffset().inSeconds()
                      << " loop " << clip->getLoopRange().getStart().inSeconds() << ".." << clip->getLoopRange().getEnd().inSeconds()
                      << " file " << clip->getOriginalFile().getFileName() << std::endl;

            auto renderTo = [&] (const juce::File& out, double seconds)
            {
                te::Renderer::Parameters p (*edit);
                p.time = { te::TimePosition(), te::TimePosition::fromSeconds (seconds) };
                p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
                p.bitDepth = 24;
                p.canRenderInMono = false;
                p.destFile = out;
                out.deleteFile();
                bool finished = false;
                auto handle = te::EditRenderer::render (p, [&finished] (tl::expected<juce::File, std::string> r)
                {
                    if (! r) std::cout << "  render error: " << r.error() << std::endl;
                    finished = true;
                });
                for (int i = 0; i < 3000 && ! finished; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
            };

            const auto out = folder.getChildFile ("loop_follow_120.wav");
            renderTo (out, 9.0);
            auto inClip = [&] (std::vector<double> o)
            {
                // Onsets inside the clip (a stretched click can start a hair before the end)
                const auto end = clip->getPosition().getEnd().inSeconds() - 0.02;
                o.erase (std::remove_if (o.begin(), o.end(), [end] (double t) { return t > end; }), o.end());
                return o;
            };
            auto on = inClip (onsets (out));
            double worst = 0.0;
            for (size_t i = 0; i < on.size(); ++i)
                worst = std::max (worst, std::abs (on[i] - (0.5 + 0.5 * (double) i)));
            std::cout << "  played at 120: " << on.size() << " clicks (want 16), worst timing error "
                      << juce::String (worst * 1000.0, 1) << " ms" << (on.size() == 16 && worst < 0.015 ? "  ok" : "  FAILED") << std::endl;

            // Tempo change: the loop keeps following
            edit->tempoSequence.getTempos()[0]->setBpm (100.0);
            const auto remade = Loops::refreshAll (*edit);
            std::cout << "  tempo changed to 100: " << remade << " loop remade, clip " << juce::String (clip->getPosition().getLength().inSeconds(), 3)
                      << " s (want 9.600)" << (remade == 1 && std::abs (clip->getPosition().getLength().inSeconds() - 9.6) < 0.01 ? "  ok" : "  FAILED") << std::endl;
            const auto out2 = folder.getChildFile ("loop_follow_100.wav");
            renderTo (out2, 11.0);
            on = inClip (onsets (out2));
            worst = 0.0;
            const auto start100 = clip->getPosition().getStart().inSeconds();      // the clip stays on its beat: 0.6 s
            for (size_t i = 0; i < on.size(); ++i)
                worst = std::max (worst, std::abs (on[i] - (start100 + 0.6 * (double) i)));
            std::cout << "  played at 100: " << on.size() << " clicks (want 16), worst timing error "
                      << juce::String (worst * 1000.0, 1) << " ms" << (on.size() == 16 && worst < 0.015 ? "  ok" : "  FAILED") << std::endl;

            // Key: a riff in G, in a song in Ab, follows the key (+1 semitone)
            {
                const auto riff = folder.getChildFile ("Riff G 100bpm.wav");
                {
                    const double sr = 44100.0;
                    const int len = (int) (2.4 * sr);
                    juce::AudioBuffer<float> b (1, len);
                    for (int i = 0; i < len; ++i)
                        b.setSample (0, i, (float) (0.4 * std::sin (2.0 * juce::MathConstants<double>::pi * 196.0 * i / sr)));
                    riff.deleteFile();
                    juce::WavAudioFormat wav;
                    std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (riff, sr, (int) (1), 24).release());
                    w->writeFromAudioSampleBuffer (b, 0, len);
                }
                edit->tempoSequence.getTempos()[0]->setBpm (100.0);
                edit->pitchSequence.getPitch (0)->setPitch (68);       // Ab
                edit->ensureNumberOfAudioTracks (2);
                auto* t2 = te::getAudioTracks (*edit)[1];
                auto rc = t2->insertWaveClip ("riff", riff, { { te::TimePosition(), te::TimePosition::fromSeconds (2.4) }, {} }, false);
                auto rg = Loops::guess (engine, riff, 100.0, 4, true);
                std::cout << "  riff: " << rg.bpm << " BPM, root " << rg.root << (rg.root == 7 ? "  ok" : "  FAILED") << std::endl;
                Loops::follow (*rc, rg, 1);
                Loops::setFollowKey (*rc, true);
                clip->setMuted (true);
                const auto keyOut = folder.getChildFile ("loop_key.wav");
                renderTo (keyOut, 2.4);
                clip->setMuted (false);
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (keyOut));
                double freq = 0.0;
                if (r != nullptr)
                {
                    juce::AudioBuffer<float> b (1, 8192);
                    r->read (&b, 0, 8192, 22050, true, false);
                    int crossings = 0;
                    for (int i = 1; i < 8192; ++i)
                        if (b.getSample (0, i - 1) < 0.0f && b.getSample (0, i) >= 0.0f) ++crossings;
                    freq = crossings * r->sampleRate / 8192.0;
                }
                std::cout << "  follows the key: " << juce::String (freq, 1) << " Hz (want 207.7)"
                          << (std::abs (freq - 207.65) < 3.0 ? "  ok" : "  FAILED") << "  " << Loops::describe (*rc) << std::endl;
                rc->removeFromParent();
            }

            // Stop following: back to 90 BPM, played once
            Loops::stopFollowing (*clip);
            std::cout << "  stop following: clip " << juce::String (clip->getPosition().getLength().inSeconds(), 3) << " s (want 5.333)"
                      << (std::abs (clip->getPosition().getLength().inSeconds() - 16.0 / 3.0) < 0.01 ? "  ok" : "  FAILED") << std::endl;
        }

        //======================================================================
        //  Step 12: de-esser and vocal chains
        //======================================================================
        double bandDb (const juce::File& f, double fromSec, double toSec, double lowHz, double highHz)
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
            if (r == nullptr) return -200.0;
            const int a = (int) (fromSec * r->sampleRate), n = (int) ((toSec - fromSec) * r->sampleRate);
            juce::AudioBuffer<float> b (1, a + n);
            r->read (&b, 0, a + n, 0, true, false);
            juce::IIRFilter hp1, hp2, lp1, lp2;
            hp1.setCoefficients (juce::IIRCoefficients::makeHighPass (r->sampleRate, lowHz));
            hp2.setCoefficients (juce::IIRCoefficients::makeHighPass (r->sampleRate, lowHz));
            lp1.setCoefficients (juce::IIRCoefficients::makeLowPass (r->sampleRate, highHz));
            lp2.setCoefficients (juce::IIRCoefficients::makeLowPass (r->sampleRate, highHz));
            auto* d = b.getWritePointer (0);
            hp1.processSamples (d, a + n); hp2.processSamples (d, a + n);
            lp1.processSamples (d, a + n); lp2.processSamples (d, a + n);
            double sum = 0.0;
            for (int i = a; i < a + n; ++i) sum += d[i] * d[i];
            return 10.0 * std::log10 (sum / juce::jmax (1, n) + 1.0e-20);
        }

        void deEsserTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "de-esser and vocal chain:" << std::endl;

            // A "voice": a vowel (200 Hz with harmonics), with a hissing "s" from 1.0 to 1.3 s
            const auto src = folder.getChildFile ("deess_source.wav");
            {
                const double sr = 44100.0;
                const int len = (int) (2.0 * sr);
                juce::AudioBuffer<float> b (1, len);
                juce::Random rnd (7);
                juce::IIRFilter hp1, hp2;
                hp1.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 5500.0));
                hp2.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 5500.0));
                for (int i = 0; i < len; ++i)
                {
                    const auto t = i / sr;
                    double v = 0.0;
                    for (int h = 1; h <= 12; ++h) v += std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * h * t) / (h * h);
                    float hiss = (rnd.nextFloat() * 2.0f - 1.0f);
                    hiss = hp2.processSingleSampleRaw (hp1.processSingleSampleRaw (hiss));
                    const bool s = t >= 1.0 && t < 1.3;
                    b.setSample (0, i, (float) (0.3 * v) + (s ? 0.6f * hiss : 0.0f));
                }
                src.deleteFile();
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (src, sr, (int) (1), 24).release());
                w->writeFromAudioSampleBuffer (b, 0, len);
            }

            auto editFile = folder.getChildFile ("deess.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            te::EditFileOperations (*edit).save (true, true, false);
            edit->ensureNumberOfAudioTracks (1);
            auto* track = te::getAudioTracks (*edit)[0];
            track->insertWaveClip ("voice", src, { { te::TimePosition(), te::TimePosition::fromSeconds (2.0) }, {} }, false);

            auto renderTo = [&] (const juce::File& out)
            {
                te::Renderer::Parameters p (*edit);
                p.time = { te::TimePosition(), te::TimePosition::fromSeconds (2.0) };
                p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
                p.bitDepth = 24;
                p.canRenderInMono = false;
                p.destFile = out;
                out.deleteFile();
                bool finished = false;
                auto handle = te::EditRenderer::render (p, [&finished] (tl::expected<juce::File, std::string>) { finished = true; });
                for (int i = 0; i < 3000 && ! finished; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
            };

            const auto dry = folder.getChildFile ("deess_off.wav");
            renderTo (dry);

            auto p = edit->getPluginCache().createNewPlugin (DeEsserPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (p, 0, nullptr);
            auto* de = dynamic_cast<DeEsserPlugin*> (p.get());
            Mixing::applyPreset (*p, "Lead vocal: any voice");
            const auto wet = folder.getChildFile ("deess_on.wav");
            renderTo (wet);

            const auto sBefore = bandDb (dry, 1.05, 1.25, 6000.0, 16000.0), sAfter = bandDb (wet, 1.05, 1.25, 6000.0, 16000.0);
            const auto vBefore = bandDb (dry, 0.3, 0.9, 80.0, 3000.0),      vAfter = bandDb (wet, 0.3, 0.9, 80.0, 3000.0);
            const auto sBody   = bandDb (wet, 1.05, 1.25, 80.0, 3000.0) - bandDb (dry, 1.05, 1.25, 80.0, 3000.0);
            std::cout << "  the S: " << juce::String (sAfter - sBefore, 1) << " dB (want -5 or less)"
                      << (sAfter - sBefore <= -5.0 ? "  ok" : "  FAILED") << std::endl;
            std::cout << "  the vowel: " << juce::String (vAfter - vBefore, 2) << " dB (want about 0)"
                      << (std::abs (vAfter - vBefore) < 0.5 ? "  ok" : "  FAILED") << std::endl;
            std::cout << "  the voice under the S: " << juce::String (sBody, 2) << " dB (want about 0)"
                      << (std::abs (sBody) < 0.5 ? "  ok" : "  FAILED") << std::endl;
            std::cout << "  most turned down: " << juce::String (de->getMaxReductionDb(), 1) << " dB (range 8)" << std::endl;

            // Vocal chain: the three effects in order, with the sends
            track->pluginList.getPlugins()[0]->deleteFromParent();
            const bool ok = Mixing::applyVocalChain (*track, 0);
            juce::StringArray names;
            for (auto* q : track->pluginList.getPlugins())
                if (! Mixing::isMixerPlugin (*q))
                    names.add (q->getName());
            std::cout << "  chain \"" << Mixing::currentVocalChain (*track) << "\": " << names.joinIntoString (", ")
                      << ", reverb send " << Mixing::getSendDb (*track, Mixing::reverbBus) << " dB"
                      << (ok && names.size() == 3 ? "  ok" : "  FAILED") << std::endl;
            Mixing::applyVocalChain (*track, 4);    // choir: replaces it, no second set
            int count = 0;
            for (auto* q : track->pluginList.getPlugins())
                if (! Mixing::isMixerPlugin (*q)) ++count;
            std::cout << "  changed to \"" << Mixing::currentVocalChain (*track) << "\": " << count << " effects, delay send "
                      << Mixing::getSendDb (*track, Mixing::delayBus) << " dB" << (count == 3 ? "  ok" : "  FAILED") << std::endl;
            const auto chainOut = folder.getChildFile ("deess_chain.wav");
            renderTo (chainOut);
            std::cout << "  chain renders: " << juce::String (bandDb (chainOut, 0.3, 0.9, 80.0, 3000.0), 1) << " dB in the voice band" << std::endl;
            Mixing::removeVocalChain (*track);
            std::cout << "  removed: " << (Mixing::currentVocalChain (*track).isEmpty() ? "ok" : "FAILED") << std::endl;
        }

        //======================================================================
        //  Plan Step 13: timing correction
        //======================================================================
        void timingTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "timing correction:" << std::endl;
            const double sr = 44100.0, bpm = 120.0, eighth = 0.25;
            const double offsMs[] = { 30, -25, 18, -35, 40, 0, -15, 22, 0, 28, -20, 12, -30, 35, 0, -10 };
            const int count = (int) std::size (offsMs);
            std::vector<double> actual;

            const auto src = folder.getChildFile ("timing_source.wav");
            {
                const int len = (int) ((1.0 + count * eighth + 1.0) * sr);
                juce::AudioBuffer<float> b (1, len);
                b.clear();
                for (int k = 0; k < count; ++k)
                {
                    const double start = 1.0 + k * eighth + offsMs[k] / 1000.0;
                    actual.push_back (start);
                    const double f0 = k % 3 == 0 ? 220.0 : (k % 3 == 1 ? 247.0 : 196.0);
                    const int s0 = (int) (start * sr);
                    for (int i = 0; i < (int) (0.2 * sr) && s0 + i < len; ++i)
                    {
                        const double t = i / sr;
                        double v = 0.0;
                        for (int h = 1; h <= 8; ++h) v += std::sin (2.0 * juce::MathConstants<double>::pi * f0 * h * t) / h;
                        const double env = std::min (1.0, t / 0.008) * std::exp (-t * 6.0) * std::min (1.0, (0.2 - t) / 0.04);
                        b.addSample (0, s0 + i, (float) (0.25 * v * env));
                    }
                }
                src.deleteFile();
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (src, sr, (int) (1), 24).release());
                w->writeFromAudioSampleBuffer (b, 0, len);
            }

            auto compare = [&] (const std::vector<double>& found, const std::vector<double>& want, double& worst)
            {
                int matched = 0;
                worst = 0.0;
                for (auto w : want)
                {
                    double best = 1.0e9;
                    for (auto f : found) best = std::min (best, std::abs (f - w));
                    if (best < 0.03) { ++matched; worst = std::max (worst, best); }
                }
                return matched;
            };

            const auto found = Timing::findNoteStarts (src, 1);
            if (juce::SystemStats::getEnvironmentVariable ("GPS_TIMING_DEBUG", {}).isNotEmpty())
            {
                std::cout << "  actual:";
                for (auto t : actual) std::cout << " " << juce::String (t, 3);
                std::cout << std::endl << "  found: ";
                for (auto t : found) std::cout << " " << juce::String (t, 3);
                std::cout << std::endl;
            }
            double worst = 0.0;
            const auto matched = compare (found, actual, worst);
            std::cout << "  note starts found: " << found.size() << " (want " << count << "), matched " << matched
                      << ", worst " << juce::String (worst * 1000.0, 1) << " ms"
                      << (matched == count && (int) found.size() == count && worst < 0.006 ? "  ok" : "  FAILED") << std::endl;

            auto editFile = folder.getChildFile ("timing.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            edit->tempoSequence.getTempos()[0]->setBpm (bpm);
            te::EditFileOperations (*edit).save (true, true, false);
            edit->ensureNumberOfAudioTracks (1);
            auto* track = te::getAudioTracks (*edit)[0];
            const auto lenSec = (1.0 + count * eighth + 1.0);
            auto clip = track->insertWaveClip ("voice", src, { { te::TimePosition(), te::TimePosition::fromSeconds (lenSec) }, {} }, false);

            auto check = [&] (float strength, const char* name)
            {
                Timing::Settings s;
                s.gridBeats = 0.5;          // 1/8 at 120 BPM = 0.25 s
                s.strength = strength;
                s.toleranceMs = 0.0;
                juce::String problem;
                if (juce::SystemStats::getEnvironmentVariable ("GPS_TIMING_DEBUG", {}).isNotEmpty())
                {
                    std::cout << "  plan:";
                    for (auto& nn : Timing::analyse (*clip, s).notes)
                        std::cout << " " << juce::String (nn.sourceSec, 3) << ">" << juce::String (nn.newSourceSec, 3) << (nn.moves ? "" : "(stay)");
                    std::cout << std::endl;
                }
                const bool ok = Timing::apply (*clip, s, problem);
                // Measure where each note now starts: the biggest jump in loudness near where it should be
                std::vector<double> now;
                {
                    juce::AudioFormatManager fm2; fm2.registerBasicFormats();
                    std::unique_ptr<juce::AudioFormatReader> rr (fm2.createReaderFor (clip->getOriginalFile()));
                    juce::AudioBuffer<float> bb (1, (int) rr->lengthInSamples);
                    rr->read (&bb, 0, (int) rr->lengthInSamples, 0, true, false);
                    auto energy = [&] (int at) { double e = 1.0e-9; for (int j = 0; j < 64; ++j) e += bb.getSample (0, at + j) * bb.getSample (0, at + j); return e; };
                    for (int k = 0; k < count; ++k)
                    {
                        const double w0 = actual[(size_t) k] + (1.0 + k * eighth - actual[(size_t) k]) * strength;
                        double best = 0.0; int at = 0;
                        for (int i = (int) ((w0 - 0.045) * sr); i < (int) ((w0 + 0.045) * sr); i += 16)
                        {
                            const auto rise = energy (i) / energy (i - 64);
                            if (rise > best) { best = rise; at = i; }
                        }
                        now.push_back (at / sr);
                    }
                }
                std::vector<double> want;
                for (int k = 0; k < count; ++k)
                    want.push_back (actual[(size_t) k] + (1.0 + k * eighth - actual[(size_t) k]) * strength);
                double w = 0.0;
                const auto m = compare (now, want, w);
                if (juce::SystemStats::getEnvironmentVariable ("GPS_TIMING_DEBUG", {}).isNotEmpty())
                {
                    std::cout << "  want:";
                    for (auto t : want) std::cout << " " << juce::String (t, 3);
                    std::cout << std::endl << "  got: ";
                    for (auto t : now) std::cout << " " << juce::String (t, 3);
                    std::cout << std::endl;
                }
                std::cout << "  " << name << ": " << (ok ? "applied" : problem) << ", " << m << " of " << count
                          << " where they should be, worst " << juce::String (w * 1000.0, 1) << " ms"
                          << (ok && m == count && w < 0.006 ? "  ok" : "  FAILED") << std::endl;
            };
            check (1.0f, "strength 100 % (on the grid)");
            check (0.5f, "strength 50 % (half way)");

            // Length unchanged, sound unchanged between note starts (level of the 1st note)
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> a (fm.createReaderFor (src)), b (fm.createReaderFor (clip->getOriginalFile()));
            std::cout << "  length kept: " << (a != nullptr && b != nullptr && a->lengthInSamples == b->lengthInSamples ? "ok" : "FAILED") << std::endl;

            Timing::remove (*clip);
            std::cout << "  removed: " << (clip->getOriginalFile() == src && ! Timing::isCorrected (*clip) ? "ok" : "FAILED") << std::endl;

            // A clip played faster is refused with a clear reason
            clip->setSpeedRatio (1.5);
            const auto refused = Timing::analyse (*clip, {});
            std::cout << "  sped-up clip refused: " << (! refused.ok && refused.problem.isNotEmpty() ? "ok" : "FAILED") << std::endl;
        }


        //======================================================================
        //  Plan Step 19: harmony generator and choir stacks
        //======================================================================
        void harmonyTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "harmony and choir:" << std::endl;

            // 1. Choosing notes (C major; C, C/E, F and G chords)
            {
                Theory::Key c;                           // C major
                auto span = [] (std::vector<int> pcs, int bass) { Harmony::ChordSpan s; s.upper = pcs; s.bass = bass; return s; };
                const auto C = span ({ 0, 4, 7 }, 0), CoverE = span ({ 0, 4, 7 }, 4), F = span ({ 5, 9, 0 }, 5), G = span ({ 7, 11, 2 }, 7);
                struct Case { int part, melody; const Harmony::ChordSpan* chord; int want; const char* what; };
                const Case cases[] = {
                    { 0, 67, &C, 64, "alto under G over C = E" },
                    { 1, 67, &C, 60, "tenor = C" },
                    { 2, 67, &C, 48, "bass = C an octave+ below" },
                    { 2, 67, &CoverE, 52, "bass follows the slash chord C/E = E" },
                    { 3, 67, &C, 72, "soprano above G = C" },
                    { 4, 64, &C, 67, "third above E = G" },
                    { 4, 67, &C, 72, "third above G over C = C (not the clashing B)" },
                    { 5, 72, &C, 67, "third below C over C = G (not A)" },
                    { 6, 64, &C, 55, "sixth below E = G" },
                    { 0, 69, &F, 65, "alto under A over F = F" },
                    { 1, 69, &F, 60, "tenor = C" },
                    { 0, 74, &G, 71, "alto under D over G = B" },
                    { 0, 74, &C, 71, "passing note D over C: a third below = B" },
                    { 0, 67, nullptr, 64, "no chords: a third below in the key" },
                    { 7, 67, &C, 55, "octave below" },
                };
                int bad = 0;
                for (auto& k : cases)
                {
                    const auto got = Harmony::harmonyNote (k.part, k.melody, k.chord, c, 0);
                    if (got != k.want) { ++bad; std::cout << "  " << k.what << ": got " << got << " want " << k.want << "  FAILED" << std::endl; }
                }
                Theory::Key am; am.tonic = 9; am.minor = true;
                const auto Am = span ({ 9, 0, 4 }, 9);
                if (Harmony::harmonyNote (0, 72, &Am, am, 0) != 69 || Harmony::harmonyNote (1, 72, &Am, am, 0) != 64) { ++bad; std::cout << "  A minor  FAILED" << std::endl; }
                std::cout << "  choosing notes: " << (int) std::size (cases) + 1 << " cases" << (bad == 0 ? "  ok" : "  FAILED") << std::endl;
            }

            // 2. A sung melody (with vibrato and vowels), 8 notes at 120 BPM over C C F F C C G C
            const double sr = 44100.0;
            const int melody[] = { 67, 64, 69, 65, 67, 72, 74, 72 };
            const int chordRoots[] = { 0, 0, 5, 5, 0, 0, 7, 0 };
            const int count = 8;
            const auto src = folder.getChildFile ("harmony_lead.wav");
            {
                const int len = (int) ((0.5 + count + 0.5) * sr);
                juce::AudioBuffer<float> b (1, len);
                b.clear();
                juce::Random rng (7);
                for (int k = 0; k < count; ++k)
                {
                    const int s0 = (int) ((0.5 + k) * sr), sl = (int) (0.9 * sr);
                    const double f0 = 440.0 * std::pow (2.0, (melody[k] - 69) / 12.0);
                    double phase = 0.0;
                    for (int i = 0; i < sl; ++i)
                    {
                        const double t = i / sr;
                        const double f = f0 * std::pow (2.0, 0.3 / 12.0 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.5 * t) * std::min (1.0, t / 0.3));
                        phase += 2.0 * juce::MathConstants<double>::pi * f / sr;
                        double v = 0.0;
                        for (int h = 1; h <= 20 && f * h < 10000.0; ++h)
                        {
                            const double fh = f * h;
                            const double formant = 1.0 + 3.0 * std::exp (-std::pow ((fh - 700.0) / 150.0, 2.0)) + 2.0 * std::exp (-std::pow ((fh - 1200.0) / 200.0, 2.0));
                            v += std::sin (phase * h) * formant / h;
                        }
                        const double env = std::min (1.0, t / 0.03) * std::min (1.0, (0.9 - t) / 0.05);
                        b.addSample (0, s0 + i, (float) (0.12 * v * env + 0.002 * rng.nextFloat()));
                    }
                }
                src.deleteFile();
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (src, sr, (int) (1), 24).release());
                w->writeFromAudioSampleBuffer (b, 0, len);
            }

            const auto found = Harmony::findMelody (src);
            {
                int right = 0;
                double worstStart = 0.0;
                for (int k = 0; k < count && k < (int) found.notes.size(); ++k)
                {
                    if (found.notes[(size_t) k].note == melody[k]) ++right;
                    worstStart = std::max (worstStart, std::abs (found.notes[(size_t) k].start - (0.5 + k)));
                }
                std::cout << "  melody found: " << found.notes.size() << " notes (want " << count << "), " << right << " right, starts within "
                          << juce::roundToInt (worstStart * 1000.0) << " ms"
                          << ((int) found.notes.size() == count && right == count && worstStart < 0.04 ? "  ok" : "  FAILED") << std::endl;
            }

            Harmony::Context ctx;
            for (int k = 0; k < count; ++k)
            {
                Harmony::ChordSpan s;
                s.start = 0.5 + k; s.end = 1.5 + k;
                const int r = chordRoots[k];
                s.upper = { r, (r + 4) % 12, (r + 7) % 12 };
                s.bass = r;
                ctx.chords.push_back (s);
            }

            // Measure the pitch and start of each note in a rendered part
            auto measure = [&] (const juce::File& f, int part, int singers, double& worstCents, double& worstMs, int& right)
            {
                const auto plan = Harmony::planPart (part, found, ctx);
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
                juce::AudioBuffer<float> bb ((int) rd->numChannels, (int) rd->lengthInSamples);
                rd->read (&bb, 0, bb.getNumSamples(), 0, true, true);
                bool finite = true;
                for (int c = 0; c < bb.getNumChannels(); ++c)
                    for (int i = 0; i < bb.getNumSamples(); ++i)
                        finite = finite && std::isfinite (bb.getSample (c, i));
                const auto got = Harmony::findMelody (bb, rd->sampleRate);
                worstCents = 0.0; worstMs = 0.0; right = 0;
                for (int k = 0; k < count; ++k)
                {
                    const double mid = 0.95 + k;
                    // pitch: median of frames in the middle of the note
                    std::vector<float> v;
                    for (size_t fr = 0; fr < got.pitch.size(); ++fr)
                    {
                        const auto t = got.firstFrameSec + (double) fr * got.frameSec;
                        if (std::abs (t - mid) < 0.25 && got.pitch[fr] > 0.0f) v.push_back (got.pitch[fr]);
                    }
                    if (v.empty()) continue;
                    std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
                    const double cents = std::abs (v[v.size() / 2] - plan[(size_t) k]) * 100.0;
                    if (cents < 40.0) ++right;
                    worstCents = std::max (worstCents, cents);
                    for (auto& n : got.notes)
                        if (n.start < mid && n.end > mid)
                            worstMs = std::max (worstMs, std::abs (n.start - (0.5 + k)) * 1000.0);
                }
                juce::ignoreUnused (singers);
                return finite;
            };

            // Step 33: notes moved by hand
            {
                Harmony::Edits e;
                e.set (0, found.notes[3], Harmony::planPart (0, found, ctx)[3] + 2);       // alto, 4th note, up a tone
                e.set (2, found.notes[1], 40);                                             // bass, 2nd note
                const auto again = Harmony::Edits::fromString (e.toString());
                Harmony::Note nearly = found.notes[3];
                nearly.start += 0.003;                                                      // found again 3 ms later: still the same note
                const bool stored = again.count() == 2 && again.find (0, nearly).has_value() && ! again.find (1, found.notes[3]).has_value();

                const auto autoPlan = Harmony::planPart (0, found, ctx);
                const auto edited = Harmony::planPart (0, found, ctx, again);
                int differ = 0;
                for (size_t i = 0; i < autoPlan.size(); ++i) differ += autoPlan[i] != edited[i] ? 1 : 0;
                const bool planned = differ == 1 && edited[3] == autoPlan[3] + 2;

                Harmony::ChordSpan cMajor; cMajor.upper = { 0, 4, 7 }; cMajor.bass = 0;
                Theory::Key keyC;
                const bool steps = Harmony::chordToneStep (64, &cMajor, keyC, 1) == 67 && Harmony::chordToneStep (64, &cMajor, keyC, -1) == 60
                                && Harmony::chordToneStep (64, nullptr, keyC, 1) == 65 && Harmony::chordToneStep (60, nullptr, keyC, -1) == 59;
                std::cout << "  moved notes: kept with the clip (\"" << e.toString() << "\"), only that note changes, chord-note steps E4 -> G4 / C4"
                          << (stored && planned && steps ? "  ok" : "  FAILED") << std::endl;

                // Sung with the change: the moved note comes out at its new pitch, the others as before
                const auto out = folder.getChildFile ("harmony_edited_alto.wav");
                const bool ok = Harmony::renderPart (src, found, ctx, 0, 1, 0.0f, out, {}, again);
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> rd (ok ? fm.createReaderFor (out) : nullptr);
                int right = 0;
                double movedCents = 1000.0;
                if (rd != nullptr)
                {
                    juce::AudioBuffer<float> bb ((int) rd->numChannels, (int) rd->lengthInSamples);
                    rd->read (&bb, 0, bb.getNumSamples(), 0, true, true);
                    const auto got = Harmony::findMelody (bb, rd->sampleRate);
                    for (int k = 0; k < count; ++k)
                    {
                        std::vector<float> v;
                        for (size_t fr = 0; fr < got.pitch.size(); ++fr)
                        {
                            const auto t = got.firstFrameSec + (double) fr * got.frameSec;
                            if (std::abs (t - (0.95 + k)) < 0.25 && got.pitch[fr] > 0.0f) v.push_back (got.pitch[fr]);
                        }
                        if (v.empty()) continue;
                        std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
                        const double cents = std::abs (v[v.size() / 2] - edited[(size_t) k]) * 100.0;
                        if (cents < 40.0) ++right;
                        if (k == 3) movedCents = cents;
                    }
                }
                std::cout << "  sung with the change: moved note " << juce::roundToInt (movedCents) << " cents from its new pitch, "
                          << right << " of " << count << " notes right" << (ok && movedCents < 30.0 && right >= count - 1 ? "  ok" : "  FAILED") << std::endl;
            }

            struct PartCase { int part, singers; float loose; const char* name; double maxCents, maxMs; int minRight; };
            const PartCase pcs[] = {
                { 0, 1, 0.0f, "alto",  25.0, 40.0, 8 },
                { 1, 1, 0.0f, "tenor", 25.0, 40.0, 8 },
                { 2, 1, 0.0f, "bass (shifted down up to 19 semitones)", 30.0, 45.0, 8 },
                { 4, 1, 0.0f, "third above", 25.0, 40.0, 8 },
                { 0, 3, 1.0f, "alto, choir of 3 singers (loose)", 1000.0, 1000.0, 7 },
            };
            for (auto& pc : pcs)
            {
                const auto out = folder.getChildFile (juce::String ("harmony_part_") + juce::String (pc.part) + "_" + juce::String (pc.singers) + ".wav");
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                const bool ok = Harmony::renderPart (src, found, ctx, pc.part, pc.singers, pc.loose, out);
                const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
                double cents = 0.0, startMs = 0.0;
                int right = 0;
                const bool finite = ok && measure (out, pc.part, pc.singers, cents, startMs, right);
                std::cout << "  " << pc.name << ": " << right << " of " << count << " notes in tune, worst " << juce::roundToInt (cents)
                          << " cents, starts within " << juce::roundToInt (startMs) << " ms, " << juce::String (9.5 * pc.singers * 1000.0 / ms, 1) << "x real time"
                          << (finite && right >= pc.minRight && cents <= pc.maxCents && startMs <= pc.maxMs ? "  ok" : "  FAILED") << std::endl;

                if (pc.singers > 1)
                {
                    juce::AudioFormatManager fm; fm.registerBasicFormats();
                    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (out));
                    juce::AudioBuffer<float> bb (2, (int) rd->lengthInSamples);
                    rd->read (&bb, 0, bb.getNumSamples(), 0, true, true);
                    double lr = 0.0, ll = 0.0, rr = 0.0;
                    for (int i = 0; i < bb.getNumSamples(); ++i)
                    {
                        const double l = bb.getSample (0, i), r = bb.getSample (1, i);
                        lr += l * r; ll += l * l; rr += r * r;
                    }
                    const auto corr = lr / std::sqrt (ll * rr + 1.0e-12);
                    const auto peak = bb.getMagnitude (0, bb.getNumSamples());
                    std::cout << "  choir stack: stereo " << rd->numChannels << " ch, left/right likeness " << juce::String (corr, 2)
                              << ", peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dB"
                              << (rd->numChannels == 2 && corr < 0.97 && corr > 0.3 && peak < 1.0f ? "  ok" : "  FAILED") << std::endl;
                }
            }

            // 3. In a song: tracks under the lead, a Choir bus, again = same tracks, remove
            {
                auto editFile = folder.getChildFile ("harmony.tracktionedit");
                editFile.deleteFile();
                auto edit = te::createEmptyEdit (engine, editFile);
                edit->editFileRetriever = [editFile] { return editFile; };
                edit->tempoSequence.getTempos()[0]->setBpm (120.0);
                te::EditFileOperations (*edit).save (true, true, false);
                edit->ensureNumberOfAudioTracks (2);
                auto* lead = te::getAudioTracks (*edit)[0];
                lead->setName ("Lead");
                lead->getVolumePlugin()->setVolumeDb (-8.0f);
                te::getAudioTracks (*edit)[1]->setName ("Keys");
                auto clip = lead->insertWaveClip ("lead", src, { { te::TimePosition(), te::TimePosition::fromSeconds (9.5) }, {} }, false);
                for (int k = 0; k < count; ++k)
                {
                    Theory::Chord ch; ch.root = chordRoots[k];
                    ChordTrack::add (*edit, ch, 1.0 + 2.0 * k, 2.0);
                }
                const auto info = Harmony::prepare (*clip, Theory::Key());
                bool chordsOk = info.ok && info.context.chords.size() == (size_t) count
                                && std::abs (info.context.chords[2].start - 2.5) < 0.001 && info.context.chords[2].bass == 5;

                Harmony::Settings s;
                s.parts = { 0, 1, 2 };
                const auto problem = Harmony::generate (*clip, Theory::Key(), s);
                auto tracks = te::getAudioTracks (*edit);
                juce::StringArray names;
                for (auto* t : tracks) names.add (t->getName());
                auto* bus = Mixing::getBusOf (*tracks[1]);
                const bool placed = problem.isEmpty() && names[1] == "Alto" && names[2] == "Tenor" && names[3] == "Bass" && names[4] == "Keys"
                                    && bus != nullptr && Mixing::getBusOf (*tracks[3]) == bus && Mixing::getBusOf (*lead) == nullptr
                                    && tracks[1]->getClips().size() == 1 && Harmony::findHarmonyTracks (*clip).size() == 3;
                std::cout << "  in a song: chords read " << (chordsOk ? "ok" : "FAILED") << ", tracks " << names.joinIntoString (", ")
                          << (placed ? "  ok" : "  FAILED") << (problem.isNotEmpty() ? " (" + problem + ")" : juce::String()) << std::endl;

                // Again, after changing a chord: the same tracks are used, the new chord is followed
                ChordTrack::clearRange (*edit, 1.0, 3.0);
                { Theory::Chord ch; ch.root = 9; ch.quality = "m"; ChordTrack::add (*edit, ch, 1.0, 2.0); }     // Am under the first G
                s.parts = { 0, 1, 2, 4 };
                Harmony::generate (*clip, Theory::Key(), s);
                const auto again = Harmony::findHarmonyTracks (*clip);
                int clipsOnAlto = 0;
                for (auto* t : again) if (t->getName() == "Alto") clipsOnAlto = t->getClips().size();
                int buses = 0;
                for (auto* t : te::getAudioTracks (*edit)) if (Mixing::isGroupBus (*t)) ++buses;
                const auto plan = Harmony::planPart (0, found, Harmony::prepare (*clip, Theory::Key()).context);
                std::cout << "  again: " << again.size() << " harmony tracks (want 4), " << clipsOnAlto << " clip on the alto, " << buses << " choir bus, "
                          << "alto under G over Am = " << Theory::midiNoteName (plan[0], {})
                          << (again.size() == 4 && clipsOnAlto == 1 && buses == 1 && plan[0] == 64 ? "  ok" : "  FAILED") << std::endl;

                // The song plays with the harmonies (not silent, no clipping)
                auto out = folder.getChildFile ("harmony_song.wav");
                out.deleteFile();
                te::Renderer::renderToFile (*edit, out, false);
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (out));
                float lo = 0, hi = 0, lo2 = 0, hi2 = 0;
                if (rd != nullptr) rd->readMaxLevels (0, rd->lengthInSamples, lo, hi, lo2, hi2);
                const auto peak = juce::jmax (std::abs (lo), hi, std::abs (lo2), hi2);
                std::cout << "  song with the choir: peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dB"
                          << (peak > 0.05f && peak < 1.0f ? "  ok" : "  FAILED") << std::endl;

                // Opening the song again tidies up parts from the first run (nothing uses them now)
                {
                    const auto removedFiles = Harmony::tidyUp (*edit);
                    bool kept = true;
                    for (auto* t : Harmony::findHarmonyTracks (*clip))
                        for (auto* c : t->getClips())
                            if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) kept = kept && w->getOriginalFile().existsAsFile();
                    const auto left = Harmony::folderFor (src).findChildFiles (juce::File::findFiles, false, "*.wav").size();
                    std::cout << "  tidy up: " << removedFiles << " old part files deleted, " << left << " in use kept"
                              << (removedFiles >= 3 && left == 4 && kept ? "  ok" : "  FAILED") << std::endl;
                }

                Harmony::removeHarmonies (*clip);
                std::cout << "  removed: " << (Harmony::findHarmonyTracks (*clip).isEmpty() ? "ok" : "FAILED") << std::endl;

                clip->setSpeedRatio (1.5);
                const auto refused = Harmony::prepare (*clip, Theory::Key());
                std::cout << "  sped-up clip refused: " << (! refused.ok && refused.problem.isNotEmpty() ? "ok" : "FAILED") << std::endl;
            }
        }


        //======================================================================
        //  Plan Step 20: backing players and the guitar
        //======================================================================
        void backingTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "backing band:" << std::endl;

            // 1. The guitar is in tune, and letting go mutes the string
            {
                const auto out = folder.getChildFile ("guitar_notes.wav");
                const int notes[] = { 45, 64, 76 };
                render (engine, out, GuitarPlugin::xmlTypeName, [] (te::Plugin&) {},
                        [&] (te::MidiList& seq)
                        {
                            for (int i = 0; i < 3; ++i)
                                seq.addNote (notes[i], te::BeatPosition::fromBeats (i * 1.5), te::BeatDuration::fromBeats (1.2), 100, 0, nullptr);
                            seq.addNote (57, te::BeatPosition::fromBeats (4.5), te::BeatDuration::fromBeats (0.15), 100, 0, nullptr);    // a short "chick"
                        }, 5.0);
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (out));
                juce::AudioBuffer<float> b (2, (int) rd->lengthInSamples);
                rd->read (&b, 0, b.getNumSamples(), 0, true, true);
                const double sr = rd->sampleRate;
                const auto mel = Harmony::findMelody (b, sr);
                double worst = 0.0;
                int found = 0;
                for (int i = 0; i < 3; ++i)
                {
                    std::vector<float> v;
                    for (size_t f = 0; f < mel.pitch.size(); ++f)
                    {
                        const auto t = mel.firstFrameSec + (double) f * mel.frameSec;
                        if (t > i * 1.0 + 0.15 && t < i * 1.0 + 0.6 && mel.pitch[f] > 0.0f) v.push_back (mel.pitch[f]);
                    }
                    if (v.empty()) continue;
                    std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
                    worst = std::max (worst, std::abs (v[v.size() / 2] - notes[i]) * 100.0);
                    ++found;
                }
                auto rms = [&] (double from, double to)
                {
                    double e = 1.0e-12; int n = 0;
                    for (int i = (int) (from * sr); i < (int) (to * sr) && i < b.getNumSamples(); ++i, ++n) e += b.getSample (0, i) * b.getSample (0, i);
                    return juce::Decibels::gainToDecibels ((float) std::sqrt (e / juce::jmax (1, n)));
                };
                const auto ring = rms (3.02, 3.1), muted = rms (3.35, 3.45), peak = b.getMagnitude (0, b.getNumSamples());
                bool finite = true;
                for (int i = 0; i < b.getNumSamples(); ++i) finite = finite && std::isfinite (b.getSample (0, i));
                std::cout << "  guitar: " << found << " of 3 notes in tune (worst " << juce::roundToInt (worst) << " cents), muted after letting go: "
                          << juce::roundToInt (ring - muted) << " dB quieter, peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dB"
                          << (finite && found == 3 && worst < 15.0 && ring - muted > 25.0 && peak < 1.0f && peak > 0.05f ? "  ok" : "  FAILED") << std::endl;
            }

            // 2. Every style: patterns fit their bar, the drum groove exists
            {
                int bad = 0;
                for (auto& st : Backing::styles())
                {
                    const int steps = st.numerator * st.stepsPerBeat;
                    bool grooveOk = false;
                    for (auto& g : Grooves::all()) grooveOk = grooveOk || juce::String (g.name) == st.groove;
                    bool ok = grooveOk;
                    for (int p = 0; p < 4; ++p)
                        for (int v = 0; v < 2; ++v)
                            ok = ok && (int) std::strlen (st.patterns[p][v]) == steps;
                    if (! ok) { ++bad; std::cout << "  style " << st.name << ": FAILED" << std::endl; }
                }
                std::cout << "  styles: " << Backing::styles().size() << " checked" << (bad == 0 ? "  ok" : "  FAILED") << std::endl;
            }

            // 3. Praise in C: Verse (bars 1-4), Chorus (bars 5-8); C F G Am | C F G7 C(ending)
            Theory::Key key;
            auto chord = [] (int root, const char* q) { Theory::Chord c; c.root = root; c.quality = q; return c; };
            std::vector<ChordTrack::Event> events;
            const Theory::Chord prog[] = { chord (0, ""), chord (5, ""), chord (7, ""), chord (9, "m"), chord (0, ""), chord (5, ""), chord (7, "7"), chord (0, "") };
            for (int i = 0; i < 8; ++i) { ChordTrack::Event e; e.chord = prog[i]; e.start = i * 4.0; e.length = 4.0; events.push_back (e); }
            std::vector<Backing::SectionSpan> sections { { 0.0, 16.0, "Verse" }, { 16.0, 32.0, "Chorus" } };
            Backing::Settings s;
            s.humanise = 0.0f;
            const auto parts = Backing::arrange (events, sections, key, 4.0, s);

            auto chordAt = [&] (double beat) { int i = 0; for (int k = 0; k < 8; ++k) if (events[(size_t) k].start <= beat + 1.0e-6) i = k; return i; };
            auto inChord = [&] (int pitch, int ci, bool gospelColours)
            {
                auto u = events[(size_t) ci].chord; u.bass = -1;
                auto pcs = Theory::chordPitchClasses (u, key);
                if (gospelColours)          // the piano's gospel voicing adds 9ths and 6ths on purpose
                    for (auto v : Theory::voice (events[(size_t) ci].chord, key, Theory::Voicing::gospel, Theory::LeftHand::root, 0))
                        pcs.push_back (v % 12);
                return std::find (pcs.begin(), pcs.end(), ((pitch % 12) + 12) % 12) != pcs.end();
            };
            const char* names[] = { "bass", "piano", "organ", "guitar", "drums" };
            const int lo[] = { 28, 36, 48, 52, 35 }, hi[] = { 52, 96, 90, 96, 60 };
            for (auto& part : parts)
            {
                int wrong = 0, outOfRange = 0, rootsOk = 0, roots = 0, verse = 0, chorus = 0;
                double verseVel = 0.0, chorusVel = 0.0;
                for (auto& n : part.notes)
                {
                    if (n.pitch < lo[part.player] || n.pitch > hi[part.player]) ++outOfRange;
                    if (n.start >= 14.0 - 1.0e-6 && n.start < 16.0) continue;       // the fill
                    if (n.start < 16.0) { ++verse; verseVel += n.velocity; } else if (n.start < 28.0) { ++chorus; chorusVel += n.velocity; }
                    if (part.player == Backing::drums) continue;
                    const int ci = chordAt (n.start + 0.02), cn = juce::jmin (7, ci + 1);
                    if (part.player == Backing::bass)
                    {
                        if (std::abs (n.start - events[(size_t) ci].start) < 0.02)
                        {
                            ++roots;
                            if (((n.pitch % 12) + 12) % 12 == (events[(size_t) ci].chord.root % 12)) ++rootsOk;
                        }
                        continue;
                    }
                    const bool g = part.player == Backing::keys;
                    if (! inChord (n.pitch, ci, g) && ! inChord (n.pitch, cn, g)) ++wrong;
                }
                const bool ok = part.notes.size() > 10 && wrong == 0 && outOfRange == 0 && (part.player != Backing::bass || (roots >= 7 && rootsOk == roots))
                                && (part.player == Backing::organ || part.player == Backing::drums || chorus * 12 > verse * 12 * 1)
                                && (part.player != Backing::drums || chorusVel / juce::jmax (1, chorus) > verseVel / juce::jmax (1, verse) + 8.0);
                std::cout << "  " << names[part.player] << ": " << part.notes.size() << " notes, " << wrong << " outside the chord, " << outOfRange << " out of range"
                          << (part.player == Backing::bass ? ", roots on chord changes " + juce::String (rootsOk) + "/" + juce::String (roots) : juce::String())
                          << ", verse " << verse << " / chorus " << chorus << " notes" << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // Fill before the chorus, crash on it; the ending
            {
                auto find = [&] (int player) -> const Backing::Part* { for (auto& p : parts) if (p.player == player) return &p; return nullptr; };
                std::vector<int> walk, run;
                bool toms = false, crash = false;
                for (auto& n : find (Backing::bass)->notes)  if (n.start >= 14.0 - 1.0e-6 && n.start < 16.0) walk.push_back (n.pitch);
                for (auto& n : find (Backing::keys)->notes)  if (n.start >= 14.0 - 1.0e-6 && n.start < 16.0) run.push_back (n.pitch);
                for (auto& n : find (Backing::drums)->notes)
                {
                    if (n.start >= 14.0 && n.start < 16.0 && (n.pitch == 50 || n.pitch == 47 || n.pitch == 45 || n.pitch == 43)) toms = true;
                    if (std::abs (n.start - 16.0) < 0.01 && n.pitch == 49) crash = true;
                }
                const bool walkUp = walk.size() == 4 && std::is_sorted (walk.begin(), walk.end()) && walk.back() < 40 && (36 - walk.back()) <= 2;
                const bool runUp = run.size() == 8 && std::is_sorted (run.begin(), run.end());
                std::cout << "  fill before the chorus: bass walk-up " << (walkUp ? "yes" : "no") << ", piano run " << (runUp ? "yes" : "no")
                          << ", drum fill " << (toms ? "yes" : "no") << ", crash on the chorus " << (crash ? "yes" : "no")
                          << (walkUp && runUp && toms && crash ? "  ok" : "  FAILED") << std::endl;

                int together = 0;
                for (auto& p : parts)
                    for (auto& n : p.notes)
                        if (std::abs (n.start - 28.0) < 0.05 && (p.player == Backing::drums || n.length > 3.5)) { ++together; break; }
                std::cout << "  ending: " << together << " of 5 players on the last chord" << (together == 5 ? "  ok" : "  FAILED") << std::endl;
            }

            // 4. Every style in its own time signature: notes for all players, inside the chords
            {
                int bad = 0;
                for (int si = 0; si < (int) Backing::styles().size(); ++si)
                {
                    const auto& st = Backing::styles()[(size_t) si];
                    const double bpb = st.numerator;
                    std::vector<ChordTrack::Event> ev;
                    for (int i = 0; i < 6; ++i) { ChordTrack::Event e; e.chord = prog[i]; e.start = i * bpb; e.length = bpb; ev.push_back (e); }
                    Backing::Settings ss;
                    ss.style = si;
                    const auto pp = Backing::arrange (ev, { { 0.0, 3 * bpb, "Verse" }, { 3 * bpb, 6 * bpb, "Chorus" } }, key, bpb, ss);
                    bool ok = pp.size() == 5;
                    for (auto& p : pp)
                    {
                        ok = ok && ! p.notes.empty();
                        for (auto& n : p.notes)
                            ok = ok && n.start >= 0.0 && n.start < 6 * bpb && n.length > 0.0 && n.start + n.length <= 6 * bpb + 0.05;
                    }
                    if (! ok) { ++bad; std::cout << "  " << st.name << ": FAILED" << std::endl; }
                }
                std::cout << "  every style plays: " << (bad == 0 ? "ok" : "FAILED") << std::endl;
            }

            // 5. In a song: the right tracks, again = replaced, remove; it plays
            {
                auto editFile = folder.getChildFile ("band.tracktionedit");
                editFile.deleteFile();
                auto edit = te::createEmptyEdit (engine, editFile);
                edit->editFileRetriever = [editFile] { return editFile; };
                edit->tempoSequence.getTempos()[0]->setBpm (110.0);
                edit->ensureNumberOfAudioTracks (1);
                auto* keysTrack = te::getAudioTracks (*edit)[0];
                keysTrack->setName ("Keys");
                if (auto p = edit->getPluginCache().createNewPlugin (StudioKeysPlugin::xmlTypeName, {}))
                    keysTrack->pluginList.insertPlugin (p, 0, nullptr);
                keysTrack->insertMIDIClip ("Chords (from Chord Track)", te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (4.0)), nullptr);
                for (int i = 0; i < 8; ++i) ChordTrack::add (*edit, prog[i], i * 4.0, 4.0);
                edit->getArrangerTrack()->insertNewClip (te::TrackItem::Type::arranger, "Verse",
                    te::TimeRange (te::TimePosition(), edit->tempoSequence.toTime (te::BeatPosition::fromBeats (16.0))), nullptr);
                edit->getArrangerTrack()->insertNewClip (te::TrackItem::Type::arranger, "Chorus",
                    te::TimeRange (edit->tempoSequence.toTime (te::BeatPosition::fromBeats (16.0)), edit->tempoSequence.toTime (te::BeatPosition::fromBeats (32.0))), nullptr);

                const auto report = Backing::apply (*edit, key, 4.0, s);
                auto countBand = [&]
                {
                    int n = 0, chordsToNotes = 0;
                    for (auto* t : te::getAudioTracks (*edit))
                        for (auto* c : t->getClips())
                        {
                            if (Backing::isBandClip (*c)) ++n;
                            if (c->getName() == "Chords (from Chord Track)") ++chordsToNotes;
                        }
                    return std::make_pair (n, chordsToNotes);
                };
                juce::StringArray trackNames;
                for (auto* t : te::getAudioTracks (*edit)) trackNames.add (t->getName());
                const auto first = countBand();
                const bool keysUsed = Backing::findTrackFor (*edit, Backing::keys) == keysTrack;
                std::cout << "  in a song: tracks " << trackNames.joinIntoString (", ") << "; " << first.first << " band clips"
                          << (trackNames.size() == 5 && first.first == 5 && first.second == 0 && keysUsed ? "  ok" : "  FAILED") << std::endl;

                Backing::Settings s2 = s;
                s2.style = 4;                 // Makossa
                Backing::apply (*edit, key, 4.0, s2);
                const auto again = countBand();
                std::cout << "  again with another style: " << te::getAudioTracks (*edit).size() << " tracks, " << again.first << " band clips"
                          << (te::getAudioTracks (*edit).size() == 5 && again.first == 5 ? "  ok" : "  FAILED") << std::endl;

                auto out = folder.getChildFile ("band_song.wav");
                out.deleteFile();
                te::Renderer::renderToFile (*edit, out, false);
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (out));
                float l1 = 0, h1 = 0, l2 = 0, h2 = 0;
                if (rd != nullptr) rd->readMaxLevels (0, rd->lengthInSamples, l1, h1, l2, h2);
                const auto peak = juce::jmax (std::abs (l1), h1, std::abs (l2), h2);
                std::cout << "  the band plays: " << juce::String (rd != nullptr ? rd->lengthInSamples / rd->sampleRate : 0.0, 1) << " s, peak "
                          << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dB" << (peak > 0.02f ? "  ok" : "  FAILED") << std::endl;

                const auto removed = Backing::remove (*edit);
                std::cout << "  removed: " << removed << (removed == 5 && countBand().first == 0 ? "  ok" : "  FAILED") << std::endl;
                std::cout << "  report: " << report << std::endl;
            }
        }


        //======================================================================
        //  Plan Step 21: African percussion and the groove pack
        //======================================================================
        void africanTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "african groove pack:" << std::endl;
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            auto load = [&] (const juce::File& f, double& sr)
            {
                std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
                juce::AudioBuffer<float> b (2, rd != nullptr ? (int) rd->lengthInSamples : 0);
                if (rd != nullptr) rd->read (&b, 0, b.getNumSamples(), 0, true, true);
                sr = rd != nullptr ? rd->sampleRate : 44100.0;
                return b;
            };
            auto rmsDb = [] (const juce::AudioBuffer<float>& b, double sr, double from, double to)
            {
                double e = 1.0e-12; int n = 0;
                for (int i = (int) (from * sr); i < (int) (to * sr) && i < b.getNumSamples(); ++i, ++n) e += b.getSample (0, i) * b.getSample (0, i);
                return juce::Decibels::gainToDecibels ((float) std::sqrt (e / juce::jmax (1, n)));
            };
            // Strongest frequency between 'from' and 'to' seconds (in Hz), searched in [lo, hi]
            auto pitchHz = [] (const juce::AudioBuffer<float>& b, double sr, double from, double to, double lo, double hi)
            {
                double best = 0.0, bestF = 0.0;
                const int i0 = (int) (from * sr), i1 = juce::jmin (b.getNumSamples(), (int) (to * sr));
                for (double f = lo; f <= hi; f *= 1.002)
                {
                    double re = 0.0, im = 0.0;
                    for (int i = i0; i < i1; ++i)
                    {
                        const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * (i - i0) / juce::jmax (1, i1 - i0));
                        re += b.getSample (0, i) * w * std::cos (2.0 * juce::MathConstants<double>::pi * f * i / sr);
                        im += b.getSample (0, i) * w * std::sin (2.0 * juce::MathConstants<double>::pi * f * i / sr);
                    }
                    const auto m = re * re + im * im;
                    if (m > best) { best = m; bestF = f; }
                }
                return bestF;
            };

            // 1. Every percussion sound: heard, not too loud, dies away
            {
                int bad = 0;
                const auto out = folder.getChildFile ("percussion_all.wav");
                const auto& list = Percussion::sounds();
                render (engine, out, PercussionPlugin::xmlTypeName, [] (te::Plugin&) {},
                        [&] (te::MidiList& seq)
                        {
                            for (size_t i = 0; i < list.size(); ++i)          // one sound per 1.5 beats (1 s at 90 BPM)
                                seq.addNote (list[i].note, te::BeatPosition::fromBeats (i * 1.5), te::BeatDuration::fromBeats (0.2), 110, 0, nullptr);
                        }, (double) list.size() * 1.0 + 1.0);
                double sr = 44100.0;
                const auto b = load (out, sr);
                bool finite = true;
                for (int i = 0; i < b.getNumSamples(); ++i) finite = finite && std::isfinite (b.getSample (0, i)) && std::isfinite (b.getSample (1, i));
                for (size_t i = 0; i < list.size(); ++i)
                {
                    const double t = (double) i;
                    const auto hit = rmsDb (b, sr, t, t + 0.05), tail = rmsDb (b, sr, t + 0.9, t + 0.99);
                    juce::Range<float> range = b.findMinMax (0, (int) (t * sr), juce::jmin ((int) sr, b.getNumSamples() - (int) (t * sr)));
                    const auto peak = juce::jmax (std::abs (range.getStart()), std::abs (range.getEnd()));
                    if (hit < -36.0f || hit - tail < 12.0f || peak >= 0.9f)
                    {
                        ++bad;
                        std::cout << "  " << list[i].name << ": hit " << juce::String (hit, 1) << " dB, tail " << juce::String (tail, 1) << " dB, peak " << peak << "  FAILED" << std::endl;
                    }
                }
                std::cout << "  percussion sounds: " << list.size() << " heard and dying away" << (finite && bad == 0 ? "  ok" : "  FAILED") << std::endl;

                // Conga open around 330 Hz; the talking drum really rises and falls
                auto indexOf = [&] (int note) { for (size_t i = 0; i < list.size(); ++i) if (list[i].note == note) return (double) i; return 0.0; };
                const auto conga = pitchHz (b, sr, indexOf (Percussion::congaOpen) + 0.03, indexOf (Percussion::congaOpen) + 0.2, 200, 500);
                const auto riseA = pitchHz (b, sr, indexOf (Percussion::talkRise) + 0.0, indexOf (Percussion::talkRise) + 0.05, 120, 400);
                const auto riseB = pitchHz (b, sr, indexOf (Percussion::talkRise) + 0.25, indexOf (Percussion::talkRise) + 0.4, 120, 400);
                const auto fallA = pitchHz (b, sr, indexOf (Percussion::talkFall) + 0.0, indexOf (Percussion::talkFall) + 0.05, 120, 400);
                const auto fallB = pitchHz (b, sr, indexOf (Percussion::talkFall) + 0.25, indexOf (Percussion::talkFall) + 0.4, 120, 400);
                std::cout << "  conga " << juce::roundToInt (conga) << " Hz; talking drum rises " << juce::roundToInt (riseA) << " > " << juce::roundToInt (riseB)
                          << " Hz, falls " << juce::roundToInt (fallA) << " > " << juce::roundToInt (fallB) << " Hz"
                          << (std::abs (conga - 330.0) < 15.0 && riseB > riseA * 1.2 && fallB < fallA / 1.2 ? "  ok" : "  FAILED") << std::endl;
            }

            // 2. The balafon is in tune
            {
                const auto out = folder.getChildFile ("balafon.wav");
                const int notes[] = { 55, 60, 67, 72 };
                render (engine, out, PercussionPlugin::xmlTypeName, [] (te::Plugin& p) { dynamic_cast<PercussionPlugin&> (p).mode = 1; },
                        [&] (te::MidiList& seq) { for (int i = 0; i < 4; ++i) seq.addNote (notes[i], te::BeatPosition::fromBeats (i * 1.5), te::BeatDuration::fromBeats (0.5), 100, 0, nullptr); }, 5.0);
                double sr = 44100.0;
                const auto b = load (out, sr);
                double worst = 0.0;
                for (int i = 0; i < 4; ++i)
                {
                    const double want = 440.0 * std::pow (2.0, (notes[i] - 69) / 12.0);
                    const auto f = pitchHz (b, sr, i + 0.02, i + 0.3, want * 0.8, want * 1.2);
                    worst = std::max (worst, std::abs (1200.0 * std::log2 (f / want)));
                }
                std::cout << "  balafon: 4 notes, worst " << juce::roundToInt (worst) << " cents" << (worst < 10.0 ? "  ok" : "  FAILED") << std::endl;
            }

            // 3. The Drum Kit plays the percussion notes it has no sample for
            {
                const auto out = folder.getChildFile ("kit_with_percussion.wav");
                render (engine, out, SamplerPlugin::xmlTypeName,
                        [] (te::Plugin& p) { dynamic_cast<SamplerPlugin&> (p).instrumentId = "drums"; },
                        [] (te::MidiList& seq)
                        {
                            seq.addNote (Percussion::congaOpen, te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (0.3), 110, 0, nullptr);
                            seq.addNote (Percussion::shaker, te::BeatPosition::fromBeats (1.5), te::BeatDuration::fromBeats (0.3), 110, 0, nullptr);
                        }, 3.0);
                double sr = 44100.0;
                const auto b = load (out, sr);
                const auto conga = rmsDb (b, sr, 0.0, 0.1), shaker = rmsDb (b, sr, 1.0, 1.08), between = rmsDb (b, sr, 0.85, 0.95);
                const auto f = pitchHz (b, sr, 0.03, 0.2, 200, 500);
                std::cout << "  drum kit plays congas and shakers: conga " << juce::String (conga, 1) << " dB (" << juce::roundToInt (f) << " Hz), shaker "
                          << juce::String (shaker, 1) << " dB" << (conga > -36.0f && shaker > -40.0f && between < conga - 20.0f && std::abs (f - 330.0) < 15.0 ? "  ok" : "  FAILED") << std::endl;
            }

            // 4. The groove pack: every groove has whole bars, the African ones use the percussion
            {
                int bad = 0, african = 0, cameroon = 0;
                for (auto& g : Grooves::all())
                {
                    const bool whole = Grooves::numSteps (g) == g.numerator * g.stepsPerBeat;
                    for (auto& l : g.lanes)
                        if ((int) std::strlen (l.steps) != Grooves::numSteps (g)) ++bad;
                    if (! whole) { ++bad; std::cout << "  groove " << g.name << ": not one bar  FAILED" << std::endl; }
                    const juce::String cat (g.category);
                    if (cat == "Cameroon" || cat == "West Africa" || cat == "Central Africa")
                    {
                        ++african;
                        if (cat == "Cameroon") ++cameroon;
                        if (! Grooves::usesPercussion (g) && cat != "Cameroon") { ++bad; std::cout << "  " << g.name << ": no percussion  FAILED" << std::endl; }
                    }
                }
                std::cout << "  groove pack: " << Grooves::all().size() << " grooves, " << african << " African (" << cameroon << " from Cameroon)"
                          << (bad == 0 && cameroon >= 7 && african >= 14 ? "  ok" : "  FAILED") << std::endl;
            }

            // 5. A band style from home: fills use djembe and talking drum
            {
                int si = -1;
                for (int i = 0; i < (int) Backing::styles().size(); ++i)
                    if (juce::String (Backing::styles()[(size_t) i].name) == "Assiko praise") si = i;
                Theory::Key key;
                std::vector<ChordTrack::Event> ev;
                for (int i = 0; i < 8; ++i) { ChordTrack::Event e; e.chord.root = (i % 2) * 7; e.start = i * 4.0; e.length = 4.0; ev.push_back (e); }
                Backing::Settings s;
                s.style = si;
                s.humanise = 0.0f;
                const auto parts = Backing::arrange (ev, { { 0.0, 16.0, "Verse" }, { 16.0, 32.0, "Chorus" } }, key, 4.0, s);
                bool talking = false, bottle = false;
                for (auto& p : parts)
                    if (p.player == Backing::drums)
                        for (auto& n : p.notes)
                        {
                            if (n.start >= 14.0 && n.start < 16.0 && (n.pitch == 78 || n.pitch == 79)) talking = true;
                            if (n.pitch == Percussion::claves) bottle = true;
                        }
                int african = 0;
                for (auto& st : Backing::styles())
                    if (Grooves::find (st.groove) != nullptr && Grooves::usesPercussion (*Grooves::find (st.groove))) ++african;
                std::cout << "  band styles from home: " << african << " with African percussion; Assiko plays the bottle " << (bottle ? "yes" : "no")
                          << ", talking drum fill " << (talking ? "yes" : "no") << (si >= 0 && african >= 10 && talking && bottle ? "  ok" : "  FAILED") << std::endl;
            }
        }


        //======================================================================
        //  Plan Step 22: chord detection, stems and help
        //======================================================================
        void step22Tests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "chords, stems and help:" << std::endl;
            const double sr = 44100.0, bpm = 100.0, beatSec = 60.0 / bpm;
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            auto writeWav = [] (const juce::File& f, const juce::AudioBuffer<float>& b, double rate)
            {
                f.deleteFile();
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (f, rate, (int) (b.getNumChannels()), 24).release());
                w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            };
            // A piano-like note (harmonics, decaying) added into a buffer
            auto addNote = [&] (juce::AudioBuffer<float>& b, int midi, double start, double length, float level, float pan)
            {
                const double f0 = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
                const int s0 = (int) (start * sr), len = (int) (length * sr);
                for (int i = 0; i < len && s0 + i < b.getNumSamples(); ++i)
                {
                    const double t = i / sr;
                    double v = 0.0;
                    for (int h = 1; h <= 8 && f0 * h < 8000.0; ++h) v += std::sin (2.0 * juce::MathConstants<double>::pi * f0 * h * t) / (h * h * 0.7 + 0.3);
                    const double env = std::min (1.0, t / 0.005) * std::exp (-t * 1.2) * std::min (1.0, (length - t) / 0.03);
                    b.addSample (0, s0 + i, (float) (v * env * level * (1.0f - pan) ));
                    if (b.getNumChannels() > 1) b.addSample (1, s0 + i, (float) (v * env * level * (1.0f + pan)));
                }
            };

            // 1. Chords from a recording: C Am F G | C/E F G C7 | F Fm C G7 | C (key of C), 2 bars each... one bar each, 100 BPM
            struct Want { int root; const char* quality; int bass; };
            const Want prog[] = { { 0, "", -1 }, { 9, "m", -1 }, { 5, "", -1 }, { 7, "", -1 }, { 0, "", 4 }, { 5, "", -1 }, { 7, "", -1 }, { 0, "7", -1 },
                                  { 5, "", -1 }, { 5, "m", -1 }, { 0, "", -1 }, { 7, "7", -1 }, { 0, "", -1 } };
            const int bars = (int) std::size (prog);
            const auto src = folder.getChildFile ("chords_song.wav");
            {
                juce::AudioBuffer<float> b (2, (int) ((bars * 4 + 1) * beatSec * sr));
                b.clear();
                for (int k = 0; k < bars; ++k)
                {
                    const auto& w = prog[k];
                    std::vector<int> iv;
                    for (auto i : Theory::findQuality (w.quality).intervals) iv.push_back (i);
                    const double t0 = k * 4 * beatSec;
                    // Piano: the chord on beats 1 and 3 (a gospel-ish voicing around middle C)
                    for (int beat : { 0, 2 })
                        for (auto i : iv)
                        {
                            int note = 60 + ((w.root + i) % 12);
                            if (note > 67) note -= 12;
                            addNote (b, note, t0 + beat * beatSec, 2 * beatSec, 0.05f, 0.0f);
                        }
                    // Bass: root (or slash bass) on 1, fifth on 3
                    const int bassPc = w.bass >= 0 ? w.bass : w.root;
                    addNote (b, 36 + bassPc, t0, 2 * beatSec, 0.12f, 0.0f);
                    addNote (b, 36 + ((w.root + 7) % 12), t0 + 2 * beatSec, 2 * beatSec, 0.1f, 0.0f);
                }
                // A hi-hat on every eighth, for the tempo guess
                juce::Random rng (3);
                for (int e = 0; e < bars * 8; ++e)
                    for (int i = 0; i < (int) (0.03 * sr); ++i)
                    {
                        const int at = (int) (e * beatSec / 2.0 * sr) + i;
                        if (at >= b.getNumSamples()) break;
                        const float v = (rng.nextFloat() * 2.0f - 1.0f) * 0.08f * std::exp (-(float) i / (float) (0.006 * sr)) * (e % 2 == 0 ? 1.0f : 0.6f);
                        b.addSample (0, at, v); b.addSample (1, at, v);
                    }
                writeWav (src, b, sr);
            }

            auto editFile = folder.getChildFile ("chords.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            edit->tempoSequence.getTempos()[0]->setBpm (bpm);
            te::EditFileOperations (*edit).save (true, true, false);
            edit->ensureNumberOfAudioTracks (2);
            auto* track = te::getAudioTracks (*edit)[0];
            const auto lenSec = bars * 4 * beatSec;
            auto clip = track->insertWaveClip ("song", src, { { te::TimePosition(), te::TimePosition::fromSeconds (lenSec) }, {} }, false);

            const auto r = ChordDetect::detect (*clip, 4.0);
            Theory::Key c;
            int right = 0, total = 0;
            juce::StringArray got;
            for (int k = 0; k < bars; ++k)
            {
                for (int beat = 0; beat < 4; ++beat)
                {
                    const double at = k * 4 + beat + 0.5;
                    const ChordDetect::Detected* d = nullptr;
                    for (auto& x : r.chords) if (at >= x.start && at < x.start + x.length) d = &x;
                    ++total;
                    if (d != nullptr && d->rootPc == prog[k].root && d->quality == prog[k].quality && d->bassPc == prog[k].bass) ++right;
                }
            }
            for (auto& d : r.chords) got.add (ChordDetect::chordName (d, c));
            std::cout << "  recording: " << got.joinIntoString (" ") << (r.ok ? juce::String() : "  (" + r.problem + ")") << std::endl;
            std::cout << "  chords found: " << right << " of " << total << " beats right (" << juce::roundToInt (100.0 * right / total) << " %), key "
                      << Theory::noteName (r.key.tonic, true) << (r.key.minor ? " minor" : " major") << ", tempo about " << juce::String (r.tempoGuess, 1) << " BPM"
                      << (right >= total * 85 / 100 && r.key.tonic == 0 && ! r.key.minor && std::abs (r.tempoGuess - bpm) < 2.0 ? "  ok" : "  FAILED") << std::endl;

            // In another song key (Ab): the chord track stores numbers relative to the key, names stay right
            Theory::Key ab = Theory::keyFromSong (8, false, "Ab major");
            const auto n = ChordDetect::writeToChordTrack (*edit, r, ab);
            const auto events = ChordTrack::getEvents (*edit);
            bool namesOk = n == (int) events.size() && n > 0;
            for (size_t i = 0; i < events.size() && namesOk; ++i)
                namesOk = Theory::nameLabel (events[i].chord, ab) == ChordDetect::chordName (r.chords[i], ab);
            std::cout << "  on the Chord Track: " << n << " chords, first " << (events.empty() ? juce::String() : Theory::nameLabel (events[0].chord, ab))
                      << " = number " << (events.empty() ? juce::String() : Theory::numberLabel (events[0].chord)) << " in Ab" << (namesOk ? "  ok" : "  FAILED") << std::endl;

            // 2. From a MIDI clip (exact notes): every chord right
            {
                auto* mt = te::getAudioTracks (*edit)[1];
                auto mc = mt->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (lenSec)), nullptr);
                for (int k = 0; k < bars; ++k)
                {
                    for (auto i : Theory::findQuality (prog[k].quality).intervals)
                        mc->getSequence().addNote (60 + (prog[k].root + i) % 12, te::BeatPosition::fromBeats (k * 4.0), te::BeatDuration::fromBeats (3.9), 90, 0, nullptr);
                    mc->getSequence().addNote (36 + (prog[k].bass >= 0 ? prog[k].bass : prog[k].root), te::BeatPosition::fromBeats (k * 4.0), te::BeatDuration::fromBeats (3.9), 90, 0, nullptr);
                }
                const auto rm = ChordDetect::detect (*mc, 4.0);
                int ok = 0;
                for (int k = 0; k < bars; ++k)
                    for (auto& d : rm.chords)
                        if (d.start <= k * 4 + 0.5 && d.start + d.length > k * 4 + 0.5
                            && d.rootPc == prog[k].root && d.quality == prog[k].quality && d.bassPc == prog[k].bass) ++ok;
                juce::StringArray mnames;
                for (auto& d : rm.chords) mnames.add (juce::String (d.start) + ":" + ChordDetect::chordName (d, c));
                std::cout << "  midi: " << mnames.joinIntoString (" ") << std::endl;
                std::cout << "  from a MIDI clip: " << ok << " of " << bars << " chords right" << (ok == bars ? "  ok" : "  FAILED") << std::endl;
            }

            // 3. Stems: a mix of voice (middle), drums, bass and keys (wide) split back
            {
                const double secs = 8.0;
                const int len = (int) (secs * sr);
                juce::AudioBuffer<float> voice (2, len), drums (2, len), bassB (2, len), keys (2, len);
                for (auto* b : { &voice, &drums, &bassB, &keys }) b->clear();
                // Voice: a sung line in the middle, with vibrato
                {
                    double phase = 0.0;
                    const int melody[] = { 67, 69, 71, 72, 71, 69, 67, 64 };
                    for (int i = 0; i < len; ++i)
                    {
                        const double t = i / sr;
                        const int idx = juce::jmin (7, (int) t);
                        const double f = 440.0 * std::pow (2.0, (melody[idx] - 69) / 12.0) * std::pow (2.0, 0.25 / 12.0 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t));
                        phase += 2 * juce::MathConstants<double>::pi * f / sr;
                        double v = 0;
                        for (int h = 1; h <= 10; ++h) v += std::sin (phase * h) * (h == 2 || h == 3 ? 0.8 : 1.0) / h;
                        const double tin = t - idx;
                        const float env = (float) (std::min (1.0, tin / 0.05) * std::min (1.0, (1.0 - tin) / 0.08));
                        voice.setSample (0, i, (float) v * 0.12f * env);
                        voice.setSample (1, i, (float) v * 0.12f * env);
                    }
                }
                // Drums: kick and snare-like hits
                {
                    juce::Random rng (5);
                    for (int hit = 0; hit < (int) (secs / 0.3); ++hit)
                    {
                        const int s0 = (int) (hit * 0.3 * sr);
                        const bool kickHit = hit % 2 == 0;
                        for (int i = 0; i < (int) (0.15 * sr) && s0 + i < len; ++i)
                        {
                            const double t = i / sr;
                            const float v = kickHit ? (float) (std::sin (2 * juce::MathConstants<double>::pi * (60 + 80 * std::exp (-t * 30)) * t) * std::exp (-t * 25) * 0.5)
                                                    : (rng.nextFloat() * 2 - 1) * (float) std::exp (-t * 40) * 0.3f;
                            drums.addSample (0, s0 + i, v); drums.addSample (1, s0 + i, v);
                        }
                    }
                }
                // Bass: long low notes
                for (int i = 0; i < len; ++i)
                {
                    const double t = i / sr;
                    const double f = ((int) (t / 2.0) % 2 == 0) ? 55.0 : 73.4;
                    const float v = (float) (std::sin (2 * juce::MathConstants<double>::pi * f * t) * 0.3 + std::sin (4 * juce::MathConstants<double>::pi * f * t) * 0.08);
                    bassB.setSample (0, i, v); bassB.setSample (1, i, v);
                }
                // Keys: chords spread wide (left and right different)
                for (int k = 0; k < 4; ++k)
                {
                    addNote (keys, 64, k * 2.0, 2.0, 0.05f, -0.8f);
                    addNote (keys, 67, k * 2.0, 2.0, 0.05f, 0.8f);
                    addNote (keys, 72, k * 2.0, 2.0, 0.04f, -0.6f);
                }
                juce::AudioBuffer<float> mix (2, len);
                mix.clear();
                for (auto* b : { &voice, &drums, &bassB, &keys })
                    for (int ch = 0; ch < 2; ++ch) mix.addFrom (ch, 0, *b, ch, 0, len);
                const auto mixFile = folder.getChildFile ("stems_mix.wav");
                writeWav (mixFile, mix, sr);

                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                const auto res = Stems::separate (mixFile, Stems::folderFor (mixFile));
                const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
                std::array<juce::AudioBuffer<float>, Stems::numStems> stems;
                bool loaded = res.ok;
                for (int s = 0; s < Stems::numStems && loaded; ++s)
                {
                    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (res.files[(size_t) s]));
                    loaded = rd != nullptr && rd->lengthInSamples == len;
                    if (loaded) { stems[(size_t) s].setSize (2, len); rd->read (&stems[(size_t) s], 0, len, 0, true, true); }
                }
                // The four add up to the mix
                double err = 0.0, ref = 1.0e-12;
                if (loaded)
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < len; ++i)
                        {
                            float sum = 0;
                            for (auto& st : stems) sum += st.getSample (ch, i);
                            err += (sum - mix.getSample (ch, i)) * (sum - mix.getSample (ch, i));
                            ref += mix.getSample (ch, i) * mix.getSample (ch, i);
                        }
                const auto nullDb = 10.0 * std::log10 (err / ref + 1.0e-30);
                // How much of each stem is its own part: correlation with the true part
                auto corr = [&] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
                {
                    double cross = 0, aa = 1.0e-12, bb = 1.0e-12;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < len; ++i) { cross += a.getSample (ch, i) * b.getSample (ch, i); aa += a.getSample (ch, i) * a.getSample (ch, i); bb += b.getSample (ch, i) * b.getSample (ch, i); }
                    return cross / std::sqrt (aa * bb);
                };
                const double cv = loaded ? corr (stems[Stems::voice], voice) : 0, cd = loaded ? corr (stems[Stems::drums], drums) : 0,
                             cb = loaded ? corr (stems[Stems::bass], bassB) : 0, co = loaded ? corr (stems[Stems::other], keys) : 0;
                std::cout << "  stems: add up to the mix within " << juce::roundToInt (nullDb) << " dB; likeness to the real part: voice " << juce::String (cv, 2)
                          << ", drums " << juce::String (cd, 2) << ", bass " << juce::String (cb, 2) << ", other " << juce::String (co, 2)
                          << "; " << juce::String (secs * 1000.0 / ms, 1) << "x real time"
                          << (loaded && nullDb < -60.0 && cv > 0.7 && cd > 0.6 && cb > 0.9 && co > 0.5 ? "  ok" : "  FAILED") << std::endl;

                // On tracks under the clip; the original muted; again = same tracks
                auto* t3 = te::getAudioTracks (*edit)[0];
                auto mixClip = t3->insertWaveClip ("mix", mixFile, { { te::TimePosition::fromSeconds (100.0), te::TimePosition::fromSeconds (108.0) }, {} }, false);
                Stems::placeStems (*mixClip, res, true);
                Stems::placeStems (*mixClip, res, true);
                juce::StringArray names;
                for (auto* t : te::getAudioTracks (*edit)) names.add (t->getName());
                std::cout << "  stem tracks: " << names.joinIntoString (", ") << (t3->isMuted (false) ? " (original muted)" : "")
                          << (Stems::findStemTracks (*mixClip).size() == 4 && t3->isMuted (false) && names[1] == "Voice (stem)" ? "  ok" : "  FAILED") << std::endl;
            }

            // 4. Help: questions in everyday words (and French) find the right answer
            {
                struct Q { const char* question; const char* expect; };
                const Q qs[] = {
                    { "how do I record my voice", "record my voice" },
                    { "comment enregistrer ma voix", "record my voice" },
                    { "makossa", "makossa" },
                    { "find the chords of a song", "find the chords" },
                    { "accords", "chords" },
                    { "my voice is out of tune", "in tune" },
                    { "remove the vocals karaoke", "stems" },
                    { "distrokid", "DistroKid" },
                    { "there is a delay when I sing", "latency" },
                    { "backing band", "backing band" },
                    { "mp3", "MP3" },
                    { "talking drum", "talking drum" },
                };
                int ok = 0;
                for (auto& q : qs)
                {
                    const auto res = Help::search (q.question);
                    const bool hit = ! res.empty() && Help::topics()[(size_t) res[0]].question.containsIgnoreCase (q.expect);
                    if (hit) ++ok; else std::cout << "  help \"" << q.question << "\" gave: " << (res.empty() ? juce::String ("nothing") : Help::topics()[(size_t) res[0]].question) << std::endl;
                }
                std::cout << "  help: " << Help::topics().size() << " answers; " << ok << " of " << (int) std::size (qs) << " questions found the right one"
                          << (ok == (int) std::size (qs) ? "  ok" : "  FAILED") << std::endl;
            }
        }


        //======================================================================
        //  Plan Step 23: recordings follow tempo changes
        //======================================================================
        void tempoFollowTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "recordings and tempo changes:" << std::endl;
            const double sr = 44100.0;
            const auto src = folder.getChildFile ("tempo_take.wav");
            {
                juce::AudioBuffer<float> b (1, (int) (6.0 * sr));
                b.clear();
                for (double t : { 1.0, 2.0, 3.0, 4.0, 5.0 })                    // a sharp hit every second
                    for (int i = 0; i < 400; ++i)
                        b.setSample (0, (int) (t * sr) + i, (float) (0.8 * std::exp (-i / 60.0) * std::sin (i * 0.3)));
                src.deleteFile();
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (src, sr, (int) (1), 24).release());
                w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            }
            auto hits = [&] (const juce::File& f)
            {
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
                std::vector<double> out;
                if (rd == nullptr) return out;
                juce::AudioBuffer<float> b (1, (int) rd->lengthInSamples);
                rd->read (&b, 0, b.getNumSamples(), 0, true, false);
                float level = 0.0f;
                int last = -100000;
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    level = std::max (std::abs (b.getSample (0, i)), level * 0.999f);
                    if (level > 0.3f && i - last > (int) (0.3 * sr)) { out.push_back (i / sr); last = i; }
                }
                return out;
            };
            auto lengthOf = [&] (const juce::File& f)
            {
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
                return rd != nullptr ? rd->lengthInSamples / rd->sampleRate : 0.0;
            };

            auto editFile = folder.getChildFile ("tempo.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            edit->tempoSequence.getTempos()[0]->setBpm (80.0);
            te::EditFileOperations (*edit).save (true, true, false);
            edit->ensureNumberOfAudioTracks (2);
            auto take = te::getAudioTracks (*edit)[0]->insertWaveClip ("take", src, { { te::TimePosition::fromSeconds (24.0), te::TimePosition::fromSeconds (29.0) },
                                                                                         te::TimeDuration::fromSeconds (1.0) }, false);
            auto midi = te::getAudioTracks (*edit)[1]->insertMIDIClip (te::TimeRange (te::TimePosition::fromSeconds (24.0), te::TimePosition::fromSeconds (30.0)), nullptr);
            TempoFollow::setMode (*edit, TempoFollow::Mode::stretch);

            auto change = [&] (double bpm)
            {
                TempoFollow::prepareForChange (*edit);
                TempoFollow::setTempoKeepingRecordings (*edit, bpm);
            };
            auto pos = [&] { return take->getPosition(); };
            auto near = [] (double a, double b, double tol = 0.002) { return std::abs (a - b) <= tol; };

            // 80 -> 100 BPM: in between nothing drifts, then the take is stretched to 0.8 of its length
            change (100.0);
            const bool held = near (pos().getStart().inSeconds(), 24.0) && TempoFollow::numWaiting (*edit) == 1 && near (midi->getPosition().getStart().inSeconds(), 19.2);
            const auto made = TempoFollow::refreshNow (*edit);
            const auto file100 = take->getOriginalFile();
            const auto h = hits (file100);
            double worst = 0.0;
            for (size_t i = 0; i < 5 && i < h.size(); ++i) worst = std::max (worst, std::abs (h[i] - (i + 1) * 0.8));
            const bool ok100 = made == 1 && file100 != src && near (pos().getStart().inSeconds(), 19.2) && near (pos().getLength().inSeconds(), 4.0)
                               && near (pos().getOffset().inSeconds(), 0.8) && near (lengthOf (file100), 4.8, 0.01) && h.size() == 5 && worst < 0.005
                               && TempoFollow::numWaiting (*edit) == 0;
            std::cout << "  80 to 100 BPM: kept in place until stretched " << (held ? "yes" : "no") << "; now starts " << juce::String (pos().getStart().inSeconds(), 3)
                      << " s, " << juce::String (pos().getLength().inSeconds(), 3) << " s long, hits within " << juce::roundToInt (worst * 1000.0) << " ms of the beat"
                      << (held && ok100 ? "  ok" : "  FAILED") << std::endl;

            // On the beat with the MIDI: the take's first hit (1 s into the original = beat 33.33 at 80) is still on that beat
            const auto hitTime = pos().getStart().inSeconds() + (h.size() > 1 ? h[1] : 0.0) - pos().getOffset().inSeconds();
            const auto hitBeat = edit->tempoSequence.toBeats (te::TimePosition::fromSeconds (hitTime)).inBeats();
            // (the clip starts 1 s into the take, at 24 s = beat 32; the hit 2 s into the take plays at 25 s = beat 33.333)
            std::cout << "  the second hit stays on beat " << juce::String (hitBeat, 3) << " (want 33.333)" << (near (hitBeat, 33.333, 0.01) ? "  ok" : "  FAILED") << std::endl;

            // 100 -> 90 -> 95 quickly (no stretch in between), then the stretch: from the original, once
            change (90.0);
            change (95.0);
            TempoFollow::refreshNow (*edit);
            const bool ok95 = near (pos().getStart().inSeconds(), 24.0 * 80.0 / 95.0, 0.003) && near (pos().getLength().inSeconds(), 5.0 * 80.0 / 95.0, 0.003)
                              && near (lengthOf (take->getOriginalFile()), 6.0 * 80.0 / 95.0, 0.01);
            std::cout << "  100 to 90 to 95 BPM: starts " << juce::String (pos().getStart().inSeconds(), 3) << " s, "
                      << juce::String (pos().getLength().inSeconds(), 3) << " s long" << (ok95 ? "  ok" : "  FAILED") << std::endl;

            // Back to 80: the untouched original again
            change (80.0);
            TempoFollow::refreshNow (*edit);
            const bool back = take->getOriginalFile() == src && near (pos().getStart().inSeconds(), 24.0) && near (pos().getLength().inSeconds(), 5.0) && near (pos().getOffset().inSeconds(), 1.0);
            std::cout << "  back to 80 BPM: the original recording, at 24.000 s" << (back ? "  ok" : "  FAILED") << std::endl;

            // "Leave them": the take stays exactly where it is in time; the MIDI follows the beat
            TempoFollow::setMode (*edit, TempoFollow::Mode::keep);
            TempoFollow::setTempoKeepingRecordings (*edit, 120.0);
            TempoFollow::forget (*edit);
            TempoFollow::refreshNow (*edit);
            const bool kept = take->getOriginalFile() == src && near (pos().getStart().inSeconds(), 24.0) && near (pos().getLength().inSeconds(), 5.0)
                              && near (midi->getPosition().getStart().inSeconds(), 16.0);
            std::cout << "  \"leave them\" at 120 BPM: take still at " << juce::String (pos().getStart().inSeconds(), 3) << " s, MIDI moved to "
                      << juce::String (midi->getPosition().getStart().inSeconds(), 3) << " s" << (kept ? "  ok" : "  FAILED") << std::endl;

            // Loops that follow the tempo have their own way: not touched here
            take->setAutoTempo (true);
            const bool loopSkipped = ! TempoFollow::applies (*take);
            take->setAutoTempo (false);
            std::cout << "  tempo-following loops left alone: " << (loopSkipped ? "ok" : "FAILED") << std::endl;

            // A timing-corrected take, stretched, then "Remove timing correction": the uncorrected take, stretched
            {
                TempoFollow::setMode (*edit, TempoFollow::Mode::stretch);
                edit->tempoSequence.getTempos()[0]->setBpm (80.0);
                take->setPosition ({ { te::TimePosition::fromSeconds (24.0), te::TimePosition::fromSeconds (29.0) }, te::TimeDuration::fromSeconds (1.0) });
                Timing::Settings ts;
                ts.gridBeats = 0.5; ts.strength = 1.0f; ts.toleranceMs = 0.0;
                juce::String problem;
                const bool corrected = Timing::apply (*take, ts, problem);
                change (100.0);
                TempoFollow::refreshNow (*edit);
                const auto stretchedTimed = take->getOriginalFile();
                Timing::remove (*take);
                const bool waiting = TempoFollow::numWaiting (*edit) == 1;
                TempoFollow::refreshNow (*edit);
                const auto now = take->getOriginalFile();
                const bool ok = corrected && waiting && now != src && now != stretchedTimed && near (lengthOf (now), 4.8, 0.01)
                                && near (pos().getStart().inSeconds(), 19.2) && ! Timing::isCorrected (*take);
                std::cout << "  timing correction removed after a tempo change: the uncorrected take, stretched to 100 BPM"
                          << (ok ? "  ok" : "  FAILED") << (corrected ? juce::String() : "  (" + problem + ")") << std::endl;
            }

            // Stretched copies no clip uses are deleted when the song opens; the original never is
            TempoFollow::setMode (*edit, TempoFollow::Mode::stretch);
            const auto removed = TempoFollow::tidyUp (*edit);
            std::cout << "  tidy up: " << removed << " unused stretched copies deleted, original kept"
                      << (src.existsAsFile() && removed >= 1 ? "  ok" : "  FAILED") << std::endl;
        }


        //======================================================================
        //  Plan Step 24: background work listed, stopped and waited for
        //======================================================================
        void backgroundWorkTests (const juce::File& folder, bool alsoShutdown)
        {
            std::cout << "background work and closing:" << std::endl;
            auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };

            // A long job is listed while it runs, and stops quickly when asked
            std::atomic<bool> finished { false }, sawStop { false };
            BackgroundWork::run ("Making the harmonies", [&] (const std::atomic<bool>& stop)
            {
                for (int i = 0; i < 500 && ! stop.load(); ++i) juce::Thread::sleep (10);
                sawStop = stop.load();
                finished = true;
            });
            pump (100);
            const auto busy = BackgroundWork::whatIsBusy();
            const auto t0 = juce::Time::getMillisecondCounter();
            BackgroundWork::stopAll();
            while (! finished && juce::Time::getMillisecondCounter() - t0 < 3000) pump (10);
            const auto stopMs = juce::Time::getMillisecondCounter() - t0;
            pump (50);
            std::cout << "  listed while working: \"" << busy.joinIntoString (", ") << "\", stopped in " << (int) stopMs << " ms, then idle "
                      << (BackgroundWork::isIdle() ? "yes" : "no")
                      << (busy.contains ("Making the harmonies") && sawStop && stopMs < 500 && BackgroundWork::isIdle() ? "  ok" : "  FAILED") << std::endl;

            // Quiet work (loading a reference song) is not asked about; tokens for exports and downloads are
            std::atomic<bool> quietDone { false };
            BackgroundWork::run ("Loading the reference song", [&] (const std::atomic<bool>& stop)
            {
                for (int i = 0; i < 30 && ! stop.load(); ++i) juce::Thread::sleep (10);
                quietDone = true;
            }, false);
            bool stopped = false;
            auto token = BackgroundWork::busy ("Exporting the song", [&] { stopped = true; });
            pump (50);
            const auto important = BackgroundWork::whatIsBusy (true), all = BackgroundWork::whatIsBusy (false);
            BackgroundWork::stopAll();
            token.reset();
            for (int i = 0; i < 100 && ! quietDone; ++i) pump (10);
            std::cout << "  asks about the export, not about loading a reference: "
                      << (important.contains ("Exporting the song") && ! important.contains ("Loading the reference song")
                          && all.contains ("Loading the reference song") && stopped ? "ok" : "FAILED") << std::endl;

            // Results come back on the message thread
            std::atomic<int> got { 0 };
            BackgroundWork::run ("Finding the chords", [&] (const std::atomic<bool>&) { BackgroundWork::onMessageThread ([&] { got = 42; }); }, false);
            for (int i = 0; i < 100 && got == 0; ++i) pump (10);
            std::cout << "  results come back to the studio: " << (got == 42 ? "ok" : "FAILED") << std::endl;

            if (! alsoShutdown)
                return;

            // Closing in the middle of splitting stems: waits, stops, no half-written file is left
            const auto src = folder.getChildFile ("stems_mix.wav");
            const auto out = folder.getChildFile ("ShutdownStems");
            out.deleteRecursively();
            std::atomic<bool> started { false };
            BackgroundWork::run ("Splitting into stems", [&] (const std::atomic<bool>& stop)
            {
                for (int round = 0; round < 50 && ! stop.load(); ++round)
                    Stems::separate (src, out, [&] (float) { started = true; return ! stop.load(); });
            });
            for (int i = 0; i < 300 && ! started; ++i) pump (10);
            const auto s0 = juce::Time::getMillisecondCounter();
            BackgroundWork::shutdown();
            const auto ms = juce::Time::getMillisecondCounter() - s0;
            bool lateCallRan = false;
            BackgroundWork::onMessageThread ([&] { lateCallRan = true; });
            BackgroundWork::run ("Too late", [&] (const std::atomic<bool>&) { lateCallRan = true; });
            pump (100);
            const auto parts = out.findChildFiles (juce::File::findFiles, true, "*.part").size();
            std::cout << "  closing while splitting stems: stopped and waited in " << (int) ms << " ms, " << parts
                      << " half-written files, nothing runs after closing" << (started && ms < 3000 && parts == 0 && ! lateCallRan ? "  ok" : "  FAILED") << std::endl;
        }








        //======================================================================
        //  Plan Step 34: a livelier band
        //======================================================================
        void livelyBandTests (te::Engine& engine)
        {
            std::cout << "livelier band:" << std::endl;
            Theory::Key key;                                                   // C major
            auto chord = [] (int root, const char* q) { Theory::Chord c; c.root = root; c.quality = q; return c; };
            const Theory::Chord prog[] = { chord (0, ""), chord (5, ""), chord (7, ""), chord (9, "m") };
            std::vector<ChordTrack::Event> events;
            for (int i = 0; i < 32; ++i) { ChordTrack::Event e; e.chord = prog[i % 4]; e.start = i * 4.0; e.length = 4.0; events.push_back (e); }
            // Four sections of 8 bars
            std::vector<Backing::SectionSpan> sections { { 0.0, 32.0, "Verse 1" }, { 32.0, 64.0, "Chorus" }, { 64.0, 96.0, "Verse 2" }, { 96.0, 128.0, "Chorus" } };
            auto find = [] (const std::vector<Backing::Part>& parts, int player) -> const Backing::Part*
            {
                for (auto& p : parts) if (p.player == player) return &p;
                return nullptr;
            };
            auto count = [] (const Backing::Part* p, double from, double to, std::function<bool (const Backing::Note&)> what)
            {
                int c = 0;
                if (p != nullptr) for (auto& n : p->notes) if (n.start >= from && n.start < to && what (n)) ++c;
                return c;
            };
            auto meanVelocity = [] (const std::vector<Backing::Part>& parts, double from, double to)
            {
                double sum = 0.0; int c = 0;
                for (auto& p : parts) for (auto& n : p.notes) if (n.start >= from && n.start < to) { sum += n.velocity; ++c; }
                return c > 0 ? sum / c : 0.0;
            };
            auto any = [] (const Backing::Note&) { return true; };

            Backing::Settings s;
            s.humanise = 0.0f;
            const auto parts = Backing::arrange (events, sections, key, 4.0, s);
            Backing::Settings plain = s;
            plain.answer = plain.drumVariation = plain.growth = false;
            const auto plainParts = Backing::arrange (events, sections, key, 4.0, plain);

            // 0. The piano stays in its middle over a long song (it used to climb an octave every 8 bars)
            {
                int lo = 200, hi = 0;
                for (auto* pp : { &parts, &plainParts })
                    if (auto* k0 = find (*pp, Backing::keys))
                        for (auto& n : k0->notes)
                            if (n.start >= 96 && n.start < 124) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
                int hi1 = 0;
                if (auto* k0 = find (plainParts, Backing::keys))
                    for (auto& n : k0->notes) if (n.start < 28) hi1 = juce::jmax (hi1, n.pitch);
                const bool ok = hi <= 96 && hi1 >= 70 && lo >= 30;
                std::cout << "  the piano stays in the middle of the keyboard: last chorus " << lo << "-" << hi << ", first verse up to " << hi1
                          << (ok ? "  ok" : "  FAILED") << std::endl;
            }
            // 1. The song grows
            {
                const auto* g = find (parts, Backing::guitar);
                const auto* o = find (parts, Backing::organ);
                const auto* k = find (parts, Backing::keys);
                const int gV1 = count (g, 0, 30, any) + count (o, 0, 30, any), gV2 = count (g, 64, 94, any) + count (o, 64, 94, any);
                const auto v1 = meanVelocity (parts, 32, 62), v2 = meanVelocity (parts, 96, 126);
                int highest1 = 0, highest2 = 0;
                if (k != nullptr) for (auto& n : k->notes)
                {
                    if (n.start >= 32 && n.start < 62) highest1 = juce::jmax (highest1, n.pitch);
                    if (n.start >= 96 && n.start < 126) highest2 = juce::jmax (highest2, n.pitch);
                }
                const bool ok = gV1 == 0 && gV2 > 10 && v2 > v1 + 4.0 && highest2 >= highest1 + 5;
                std::cout << "  the song grows: guitar+organ notes verse 1 " << gV1 << " / verse 2 " << gV2 << ", choruses "
                          << juce::String (v1, 0) << " -> " << juce::String (v2, 0) << " loudness, piano reaches " << highest1 << " -> " << highest2
                          << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 2. Drum variety
            {
                const auto* d = find (parts, Backing::drums);
                bool crashes = true;
                for (double at : { 32.0, 64.0, 96.0 })
                    crashes = crashes && count (d, at - 0.01, at + 0.05, [] (const Backing::Note& n) { return n.pitch == 49; }) > 0;
                const int open = count (d, 0, 128, [] (const Backing::Note& n) { return n.pitch == 46; });
                auto ghost = [] (const Backing::Note& n) { return n.pitch == 38 && n.velocity < 45; };
                const auto* d0 = find (plainParts, Backing::drums);
                const int ghostChorus = count (d, 32, 64, ghost) + count (d, 96, 128, ghost) - count (d0, 32, 64, ghost) - count (d0, 96, 128, ghost),
                          ghostVerse  = count (d, 0, 32, ghost) + count (d, 64, 96, ghost) - count (d0, 0, 32, ghost) - count (d0, 64, 96, ghost);
                const bool ok = crashes && open >= 4 && ghostChorus > 0 && ghostVerse == 0;
                std::cout << "  drum variety: crash on every new section, " << open << " open hi-hats at phrase ends, ghost notes added chorus "
                          << ghostChorus << " / verse " << ghostVerse << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 3. Answers at the ends of 4-bar phrases (no singer yet): a pentatonic run ending on a chord note
            {
                const auto windows = Backing::answerWindows ({}, sections, 0.0, 124.0, 4.0);
                const auto* k = find (parts, Backing::keys);
                const auto [ws, we] = windows.empty() ? std::pair<double, double> { 0.0, 0.0 } : windows.front();
                std::vector<int> run;
                if (k != nullptr) for (auto& n : k->notes) if (n.start >= ws - 1.0e-6 && n.start < we) run.push_back (n.pitch);
                bool penta = ! run.empty();
                for (auto pch : run) { const int pc = ((pch % 12) + 12) % 12; penta = penta && (pc == 0 || pc == 2 || pc == 4 || pc == 7 || pc == 9); }
                const int lastPc = run.empty() ? -1 : run.back() % 12;
                const bool chordNote = lastPc == 9 || lastPc == 0 || lastPc == 4;     // the chord there is A minor
                const bool ok = windows.size() == 4 && std::abs (ws - 14.5) < 1.0e-6 && run.size() >= 4 && penta && chordNote;
                std::cout << "  answers without a singer: " << windows.size() << " phrase ends, piano plays " << (int) run.size()
                          << " pentatonic notes at beat " << ws << ", ending on a chord note" << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 4. Answers where the singer rests, and nowhere else
            {
                Backing::Settings sung = s;
                sung.singing = { { 0.0, 5.5 }, { 8.0, 13.5 }, { 16.0, 31.0 } };     // rests at 5.5-8 and 13.5-16
                const auto windows = Backing::answerWindows (sung.singing, sections, 0.0, 124.0, 4.0);
                const auto withSinger = Backing::arrange (events, sections, key, 4.0, sung);
                const auto* k = find (withSinger, Backing::keys);
                const auto* k0 = find (parts, Backing::keys);
                const int inRest = count (k, 5.5, 7.8, [] (const Backing::Note& n) { return n.pitch >= 70; });
                const bool ok = windows.size() == 2 && std::abs (windows[0].first - 5.5) < 1.0e-6 && std::abs (windows[0].second - 7.5) < 1.0e-6
                             && inRest >= 4 && count (k, 1.0, 5.0, any) == count (k0, 1.0, 5.0, any);
                std::cout << "  answers where the singer rests: " << windows.size() << " rests found (first at beat " << (windows.empty() ? 0.0 : windows[0].first)
                          << "), piano answers there with " << inRest << " notes, untouched while singing" << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 5. Finding the singer: a MIDI melody on a track called "Melody"
            {
                auto edit = te::createEmptyEdit (engine, juce::File());
                edit->ensureNumberOfAudioTracks (2);
                auto tracks = te::getAudioTracks (*edit);
                tracks[1]->setName ("Melody");
                const auto range = te::TimeRange (te::TimePosition(), edit->tempoSequence.toTime (te::BeatPosition::fromBeats (16.0)));
                if (auto mc = tracks[1]->insertMIDIClip (range, nullptr))
                {
                    mc->getSequence().addNote (60, te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (3.0), 100, 0, nullptr);
                    mc->getSequence().addNote (62, te::BeatPosition::fromBeats (6.0), te::BeatDuration::fromBeats (2.0), 100, 0, nullptr);
                }
                const auto found = Backing::findSinging (*edit);
                const bool ok = found.size() == 2 && std::abs (found[1].first - 6.0) < 1.0e-3 && std::abs (found[1].second - 8.0) < 1.0e-3;
                std::cout << "  the singer is found on the Melody track: " << found.size() << " notes" << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 6. All three switched off: none of it
            {
                const auto* d = find (plainParts, Backing::drums);
                const int open = count (d, 0, 128, [] (const Backing::Note& n) { return n.pitch == 46; });
                const bool ok = open == 0 && count (find (plainParts, Backing::guitar), 0, 30, any) > 0;
                std::cout << "  switched off: no open hi-hats, the guitar plays the first verse" << (ok ? "  ok" : "  FAILED") << std::endl;
            }
        }


        //======================================================================
        //  The Learn course (inside the app)
        //======================================================================
        void learnTests (const juce::File& folder)
        {
            std::cout << "learn course:" << std::endl;

            // 1. The lessons themselves
            {
                const auto problems = Learn::checkCourse();
                int steps = 0, tasks = 0;
                for (auto& m : Learn::course()) for (auto& l : m.lessons) for (auto& st : l.steps) { ++steps; if (st.task.isNotEmpty()) ++tasks; }
                const bool ok = problems.isEmpty() && Learn::course().size() >= 4 && Learn::lessonCount() >= 12 && tasks >= 25;
                std::cout << "  " << Learn::course().size() << " modules, " << Learn::lessonCount() << " lessons, " << steps << " steps, " << tasks
                          << " tasks that tick themselves" << (problems.isEmpty() ? juce::String() : ": " + problems.joinIntoString ("; ")) << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 2. Finding buttons by their ID, text or tooltip (what "Show me" makes glow)
            {
                juce::Component root;
                root.setBounds (0, 0, 400, 300);
                root.setVisible (true);
                juce::TextButton tone ("Test Tone  (A 440 Hz)"), hidden ("Make the band"), shown ("Make the band"), play;
                juce::ComboBox key;
                play.setTooltip ("Play / Pause (Space)");
                key.setComponentID ("studio.key");
                juce::Component inner;
                inner.setBounds (0, 100, 400, 200);
                for (auto* c : { (juce::Component*) &tone, (juce::Component*) &hidden, (juce::Component*) &play, (juce::Component*) &key })
                {
                    root.addAndMakeVisible (c);
                    c->setBounds (10, 10, 80, 30);
                }
                root.addAndMakeVisible (inner);
                inner.addAndMakeVisible (shown);
                shown.setBounds (5, 5, 90, 30);
                hidden.setVisible (false);
                const bool ok = Learn::findIn (root, "Test Tone (A 440 Hz)") == &tone          // spaces do not matter
                             && Learn::findIn (root, "id:studio.key") == &key
                             && Learn::findIn (root, "tip:Play / Pause") == &play
                             && Learn::findIn (root, "Make the band") == &shown                 // the hidden one is skipped
                             && Learn::findIn (root, "Nothing like this") == nullptr
                             && Learn::findIn (root, {}) == nullptr;
                std::cout << "  finding buttons by ID, text and tooltip, hidden ones skipped" << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 3. Progress is remembered
            {
                const auto before = Learn::isDone ("1.1");
                Learn::setDone ("1.1", true);
                const auto n = Learn::doneCount();
                Learn::setDone ("1.1", false);
                const bool ok = n >= 1 && ! Learn::isDone ("1.1") && Learn::doneCount() == n - 1;
                Learn::setDone ("1.1", before);
                std::cout << "  lesson ticks are remembered and can be cleared" << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 4. The handbook has every lesson and every step
            {
                const auto file = Learn::writeHandbook (folder.getChildFile ("Handbook.html"), "test");
                const auto text = file.loadFileAsString();
                int missing = 0, tries = 0;
                for (auto& m : Learn::course())
                    for (auto& l : m.lessons)
                    {
                        if (! text.contains (l.title.replace ("&", "&amp;"))) ++missing;
                        for (auto& st : l.steps) if (st.task.isNotEmpty()) ++tries;
                    }
                const int shown = juce::StringArray::fromTokens (text, "\n", {}).size() > 0 ? text.length() - text.replace ("Try it:", "").length() : 0;
                const bool ok = file.existsAsFile() && missing == 0 && shown / 7 == tries && text.contains ("Quick reference") && text.contains ("Ctrl+P");
                std::cout << "  the handbook: " << (int) (file.getSize() / 1024) << " KB, every lesson, " << shown / 7 << " \"Try it\" tasks, shortcuts" << (ok ? "  ok" : "  FAILED") << std::endl;
            }
        }

        //======================================================================
        //  Plan Steps 43-44: versions, repairs and recovery
        //======================================================================
        void songSafetyTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "never lose a song:" << std::endl;
            const auto song = folder.getChildFile ("SafetySong");
            song.deleteRecursively();
            song.createDirectory();
            const auto editFile = song.getChildFile ("project.tracktionedit");
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            edit->ensureNumberOfAudioTracks (2);
            te::EditFileOperations (*edit).save (true, true, false);

            // 1. Versions: a copy only when something changed, named ones always
            const bool first = SongSafety::snapshot (editFile, "opened");
            const bool same = SongSafety::snapshot (editFile, "auto");
            edit->ensureNumberOfAudioTracks (4);
            te::EditFileOperations (*edit).save (true, true, false);
            const bool changed = SongSafety::snapshot (editFile, "auto");
            const bool named = SongSafety::snapshot (editFile, "named", "Before the choir");
            auto versions = SongSafety::list (editFile);
            bool namedFound = false;
            for (auto& v : versions) namedFound = namedFound || (v.kind == "named" && v.name == "Before the choir");
            std::cout << "  versions: " << versions.size() << " kept (opened, changed, named \"Before the choir\"), none when nothing changed"
                      << (first && ! same && changed && named && versions.size() == 3 && namedFound ? "  ok" : "  FAILED") << std::endl;

            // 2. Going back: the song as it was, and the present kept as a version
            {
                juce::File oldest;
                for (auto& v : versions) if (v.kind == "opened") oldest = v.file;
                SongSafety::Version v; v.file = oldest; v.kind = "opened";
                const bool ok = SongSafety::restore (editFile, v) && editFile.hasIdenticalContentTo (oldest);
                int restoreCopies = 0;
                for (auto& w : SongSafety::list (editFile)) restoreCopies += w.kind == "restore" ? 1 : 0;
                const auto back = te::loadEditFromFile (engine, editFile);
                std::cout << "  going back: the song has " << te::getAudioTracks (*back).size() << " tracks again, the present kept as a version"
                          << (ok && restoreCopies == 1 && te::getAudioTracks (*back).size() == 2 ? "  ok" : "  FAILED") << std::endl;
            }

            // 3. Old automatic versions are thinned out; named ones stay
            {
                const auto vf = SongSafety::versionsFolder (editFile);
                vf.deleteRecursively();
                vf.createDirectory();
                const auto now = juce::Time::getCurrentTime();
                const auto content = editFile.loadFileAsString();
                auto make = [&] (juce::Time t, const juce::String& kind)
                {
                    vf.getChildFile (t.formatted ("%Y-%m-%d %H-%M-%S") + " ~ " + kind + ".tracktionedit").replaceWithText (content);
                };
                for (int i = 0; i < 30; ++i) make (now - juce::RelativeTime::minutes (i + 1), "auto");                     // today
                for (int d = 5; d < 15; ++d) make (now - juce::RelativeTime::days (d), "auto");                            // 10 older days
                for (int d = 40; d < 45; ++d) make (now - juce::RelativeTime::days (d), "auto");                           // more than a month
                make (now - juce::RelativeTime::days (60), "named ~ Wedding version");
                make (now - juce::RelativeTime::days (61), "named ~ First mix");
                const auto removed = SongSafety::prune (editFile, now);
                const auto left = SongSafety::list (editFile).size();
                std::cout << "  thinning: " << removed << " old copies removed, " << left << " kept (20 newest, one a day for a month, named ones)"
                          << (left == 32 && removed == 15 ? "  ok" : "  FAILED") << std::endl;
            }

            // 4. A song file the app was saving at the moment of a power cut
            {
                const auto good = editFile.loadFileAsString();
                const auto temp = editFile.getSiblingFile (".tmp_" + editFile.getFileNameWithoutExtension());
                editFile.deleteFile();
                temp.replaceWithText (good);
                const auto a = SongSafety::prepareSongFile (editFile);
                const bool fromTemp = a.changed && SongSafety::isReadableSong (editFile);
                temp.deleteFile();

                editFile.replaceWithText (good.substring (0, good.length() / 2));           // cut off halfway
                const auto b = SongSafety::prepareSongFile (editFile);
                const bool fromVersion = b.changed && SongSafety::isReadableSong (editFile)
                                      && editFile.getSiblingFile ("project (damaged).tracktionedit").existsAsFile();
                const auto fresh = folder.getChildFile ("SafetyNewSong").getChildFile ("project.tracktionedit");
                fresh.getParentDirectory().deleteRecursively();
                fresh.getParentDirectory().createDirectory();
                const auto c = SongSafety::prepareSongFile (fresh);
                std::cout << "  damaged song file: repaired from the half-saved copy, then from the newest version; a new song is left alone"
                          << (fromTemp && fromVersion && ! c.changed && c.message.isEmpty() ? "  ok" : "  FAILED") << std::endl;
            }

            // 5. The clean-up keeps files an old version uses
            {
                const auto stems = song.getChildFile ("Audio").getChildFile ("Stems");
                stems.createDirectory();
                auto makeWav = [] (const juce::File& f)
                {
                    juce::AudioBuffer<float> b (1, 4410); b.clear();
                    if (auto w = WavFiles::writer (f, 44100.0, 1, 16)) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                };
                const auto oldStem = stems.getChildFile ("Lead - Voice (old split).wav");
                const auto junk = stems.getChildFile ("Lead - Voice (nobody uses).wav");
                makeWav (oldStem); makeWav (junk);
                SongSafety::versionsFolder (editFile).getChildFile ("2026-01-01 10-00-00 ~ named ~ With old stems.tracktionedit")
                    .replaceWithText ("<EDIT><TRACK><AUDIOCLIP source=\"Audio/Stems/Lead - Voice (old split).wav\"/></TRACK></EDIT>");
                auto e2 = te::loadEditFromFile (engine, editFile);
                e2->editFileRetriever = [editFile] { return editFile; };
                SongFiles::tidy (*e2);
                std::cout << "  clean-up: keeps the stem an old version uses, removes the one nothing uses"
                          << (oldStem.existsAsFile() && ! junk.existsAsFile() ? "  ok" : "  FAILED") << std::endl;
            }

            // 6. A recording cut off by a power cut: found, repaired and put back in the song
            {
                const auto marker = folder.getChildFile ("session-test.json");
                marker.deleteFile();
                SongSafety::Session::appStarted (marker);
                const bool cleanBefore = ! SongSafety::Session::previousEndedUnexpectedly().has_value();
                SongSafety::Session::songOpened ("safety-song", editFile);
                SongSafety::Session::recordingStarted (editFile, 12.5);

                // The recording being written when the power went: its header still says "empty"
                const auto take = song.getChildFile ("project_Lead Vocal_Take_1.wav");
                {
                    juce::AudioBuffer<float> b (1, 44100);
                    for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, 0.3f * std::sin ((float) i * 0.06f));
                    if (auto w = WavFiles::writer (take, 44100.0, 1, 16)) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                    juce::FileOutputStream out (take);
                    out.setPosition (4);  out.writeInt (0);                    // RIFF size never written
                    out.setPosition (40); out.writeInt (0);                    // data size never written
                }
                // ... the app stops here without closing; next start:
                SongSafety::Session::appStarted (marker);
                const auto prev = SongSafety::Session::previousEndedUnexpectedly();
                const bool noticed = prev.has_value() && prev->wasRecording && prev->songId == "safety-song" && std::abs (prev->recordStartSeconds - 12.5) < 1.0e-6;

                auto e3 = te::loadEditFromFile (engine, editFile);
                e3->editFileRetriever = [editFile] { return editFile; };
                const auto found = prev ? SongSafety::Session::findCutOffRecordings (*prev, e3.get()).size() : 0;
                const auto rescued = prev ? SongSafety::Session::rescueRecordings (*e3, *prev) : 0;
                te::Clip* clip = nullptr;
                for (auto* t : te::getAudioTracks (*e3))
                    if (t->getName() == "Rescued recording" && ! t->getClips().isEmpty()) clip = t->getClips().getFirst();
                const bool placed = clip != nullptr && std::abs (clip->getPosition().getStart().inSeconds() - 12.5) < 1.0e-3
                                 && std::abs (clip->getPosition().getLength().inSeconds() - 1.0) < 0.01;
                const auto againFound = prev ? SongSafety::Session::findCutOffRecordings (*prev, e3.get()).size() : 9;

                SongSafety::Session::appClosedNormally();
                SongSafety::Session::appStarted (marker);
                const bool cleanAfter = ! SongSafety::Session::previousEndedUnexpectedly().has_value();
                SongSafety::Session::appClosedNormally();
                std::cout << "  power cut while recording: noticed at the next start, " << found << " recording found, repaired and placed at 12.5 s ("
                          << (clip != nullptr ? juce::String (clip->getPosition().getLength().inSeconds(), 2) : juce::String ("?")) << " s long); a proper close leaves nothing to recover"
                          << (cleanBefore && noticed && found == 1 && rescued == 1 && placed && againFound == 0 && cleanAfter ? "  ok" : "  FAILED") << std::endl;
            }
        }

        //======================================================================
        //  Plan Step 28: measuring the recording delay (a stand-in interface with a known delay)
        //======================================================================
        void latencyTests()
        {
            std::cout << "recording delay check:" << std::endl;
            // Runs the probe through a pretend device: what goes out comes back `delay` samples later
            auto simulate = [] (double rate, int delay, float gain, bool cable, int block)
            {
                LatencyCheck::Probe probe;
                probe.prepare (rate, 2);
                std::vector<float> line ((size_t) (delay + block + 8), 0.0f);
                size_t w = 0;
                juce::Random noise (7);
                std::vector<float> in0 ((size_t) block), in1 ((size_t) block), out0 ((size_t) block), out1 ((size_t) block);
                juce::AudioIODeviceCallbackContext ctx;
                for (int done = 0; ! probe.isFinished() && done < 10'000'000; done += block)
                {
                    // Input = output from `delay` samples ago (through the cable), plus hiss
                    for (int i = 0; i < block; ++i)
                    {
                        const auto past = line[(w + (size_t) i) % line.size()];
                        in0[(size_t) i] = (cable ? juce::jlimit (-1.0f, 1.0f, past * gain) : 0.0f) + (noise.nextFloat() - 0.5f) * 0.01f;
                        in1[(size_t) i] = (noise.nextFloat() - 0.5f) * 0.01f;
                    }
                    const float* ins[] = { in0.data(), in1.data() };
                    float* outs[] = { out0.data(), out1.data() };
                    probe.audioDeviceIOCallbackWithContext (ins, 2, outs, 2, block, ctx);
                    // What went out now comes back `delay` samples later
                    for (int i = 0; i < block; ++i)
                        line[(w + (size_t) i + (size_t) delay) % line.size()] = out0[(size_t) i];
                    for (int i = 0; i < block; ++i)
                        line[(w + (size_t) i) % line.size()] = line[(w + (size_t) i) % line.size()];
                    w = (w + (size_t) block) % line.size();
                }
                return LatencyCheck::analyse (probe.getRecording(), rate, 10.0);
            };

            bool allOk = true;
            juce::String found;
            for (auto [rate, delay, block] : { std::tuple<double, int, int> { 48000.0, 1301, 256 }, { 44100.0, 237, 64 }, { 48000.0, 4800, 512 } })
            {
                const auto r = simulate (rate, delay, 0.6f, true, block);
                const bool ok = r.ok && r.roundTripSamples == delay && r.inputChannel == 0
                             && std::abs (r.correctionMs - (1000.0 * delay / rate - 10.0)) < 0.01 && ! r.clipped;
                allOk = allOk && ok;
                found << juce::String (delay) << (r.ok ? " -> " + juce::String (r.roundTripSamples) : juce::String (" -> failed")) << ", ";
            }
            std::cout << "  delays found exactly (samples): " << found.dropLastCharacters (2) << ", on input 1, correction = measured - reported"
                      << (allOk ? "  ok" : "  FAILED") << std::endl;

            const auto none = simulate (48000.0, 1000, 0.6f, false, 256);
            std::cout << "  no cable: \"" << none.problem.upToFirstOccurrenceOf (".", true, false) << "\"" << (! none.ok ? "  ok" : "  FAILED") << std::endl;

            const auto loud = simulate (48000.0, 900, 6.0f, true, 256);
            std::cout << "  input overloaded: still measured " << loud.roundTripSamples << " samples, and says to turn the gain down"
                      << (loud.ok && loud.roundTripSamples == 900 && loud.clipped ? "  ok" : "  FAILED") << std::endl;
        }

        //======================================================================
        //  Plan Step 32: real-drummer feel, and the Drum Kit's recorded percussion
        //======================================================================
        void realFeelTests (const juce::File& folder)
        {
            std::cout << "real drummers:" << std::endl;

            // 1. Placing hits with a feel (a made-up style: snare late, kick early, accents on the beat)
            {
                RealFeel::Style style { "Test", "test", 1, {}, { { 100.0f, 100.0f, 80.0f, 80.0f, 90.0f } } };
                for (auto& c : style.cells) c = { 20, 0.0f, 0.0f, 80.0f };
                for (int s = 0; s < 16; ++s)
                {
                    style.cells[(size_t) (RealFeel::snare * 16 + s)] = { 20, 0.16f, 0.0f, 100.0f };
                    style.cells[(size_t) (RealFeel::kick * 16 + s)]  = { 20, -0.08f, 0.0f, 100.0f };
                    style.cells[(size_t) (RealFeel::hiHat * 16 + s)] = { 20, 0.0f, 0.0f, s % 4 == 0 ? 104.0f : 60.0f };
                }
                juce::Random r (1);
                const auto sn = RealFeel::place (style, 38, 1.0, 100, r);
                const auto kk = RealFeel::place (style, 36, 0.0, 100, r);
                const auto onBeat = RealFeel::place (style, 42, 2.0, 90, r);
                const auto between = RealFeel::place (style, 42, 2.5, 90, r);
                const auto shaker = RealFeel::place (style, 70, 0.5, 77, r);
                const bool ok = std::abs (sn.offsetBeats - 0.04) < 1.0e-6 && std::abs (kk.offsetBeats + 0.02) < 1.0e-6
                             && onBeat.velocity > between.velocity + 15 && shaker.velocity == 77;
                std::cout << "  feel: snare " << juce::String (sn.offsetBeats * 500.0, 0) << " ms late, kick " << juce::String (-kk.offsetBeats * 500.0, 0)
                          << " ms early (120 BPM), hi-hat " << onBeat.velocity << " on the beat / " << between.velocity << " between, shaker keeps its strength"
                          << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 2. The measured data (only in builds that include it)
            if (RealFeel::fromRealDrummers())
            {
                bool sane = ! RealFeel::styles().empty();
                for (auto& st : RealFeel::styles())
                    for (auto& c : st.cells)
                        sane = sane && std::abs (c.offset) <= 0.45f && c.spread >= 0.0f && c.velocity >= 1.0f && c.velocity <= 127.0f;
                int hits = 0;
                for (auto& g : RealFeel::grooves())
                {
                    sane = sane && g.hits.size() >= 16 && g.bpm >= 60;
                    for (auto& h : g.hits)
                        sane = sane && h.beat >= 0.0 && h.beat < g.lengthBeats && h.velocity >= 1 && h.velocity <= 127 && RealFeel::kindOf (h.note) >= 0;
                    hits += (int) g.hits.size();
                }
                const auto* gospel = RealFeel::findStyle ("Gospel");
                std::cout << "  " << RealFeel::styles().size() << " feels from real drummers (" << (gospel != nullptr ? juce::String (gospel->performances) : juce::String ("no"))
                          << " gospel performances), " << RealFeel::grooves().size() << " grooves, " << hits << " hits, all in range"
                          << (sane && gospel != nullptr ? "  ok" : "  FAILED") << std::endl;
            }
            else
                std::cout << "  this build has no real-drummer data (the GitHub build adds it)" << std::endl;

            // 3. Drum Kit + recorded percussion: the kit keeps its own sounds, percussion fills the rest
            {
                const auto dir = folder.getChildFile ("kit_plus_percussion");
                dir.deleteRecursively();
                dir.createDirectory();
                auto wav = [&] (const juce::String& name, double hz)
                {
                    juce::AudioBuffer<float> b (1, 4410);
                    for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * hz * i / 44100.0)));
                    if (auto w = WavFiles::writer (dir.getChildFile (name), 44100.0, 1, 16)) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                };
                wav ("kick.wav", 60.0); wav ("snare.wav", 200.0); wav ("pclap.wav", 900.0); wav ("conga.wav", 300.0);
                dir.getChildFile ("Kit.sfz").replaceWithText ("<region> sample=kick.wav key=36\n<region> sample=snare.wav key=38\n");
                dir.getChildFile ("Perc.sfz").replaceWithText ("<region> sample=pclap.wav key=38\n<region> sample=conga.wav key=63\n");

                Sfz::InstrumentPtr result;
                bool done = false;
                Sfz::Library::getInstance().request (dir.getChildFile ("Kit.sfz"), [&] (Sfz::InstrumentPtr i) { result = i; done = true; },
                                                    dir.getChildFile ("Perc.sfz"));
                for (int i = 0; i < 500 && ! done; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
                int at38 = 0, at63 = 0;
                bool sampleOk = true;
                if (result != nullptr)
                    for (auto& r : result->regions)
                    {
                        if (r.lokey <= 38 && r.hikey >= 38) ++at38;
                        if (r.lokey <= 63 && r.hikey >= 63) { ++at63; sampleOk = r.sampleIndex == 2 && result->samples.size() == 3; }
                    }
                const bool ok = result != nullptr && result->regions.size() == 3 && at38 == 1 && at63 == 1 && sampleOk;
                std::cout << "  Drum Kit + recorded percussion: kit snare kept, conga added (" << (result != nullptr ? (int) result->regions.size() : 0)
                          << " regions, " << (result != nullptr ? (int) result->samples.size() : 0) << " samples)" << (ok ? "  ok" : "  FAILED") << std::endl;
                result.reset();
                Sfz::Library::getInstance().forget (dir.getChildFile ("Kit.sfz"));
                dir.deleteRecursively();
            }
        }

        //======================================================================
        //  Plan Step 30: recorded guitars (sound packs), built-in guitar until downloaded
        //======================================================================
        float pitchOf (const juce::AudioBuffer<float>& b, int from, int len, double sr)
        {
            // Autocorrelation over 70..1000 Hz
            const int minLag = (int) (sr / 1000.0), maxLag = (int) (sr / 70.0);
            const auto* x = b.getReadPointer (0);
            std::vector<double> corr ((size_t) maxLag + 2, 0.0);
            double e0 = 1.0e-12;
            for (int i = from; i < from + len && i < b.getNumSamples(); ++i) e0 += x[i] * x[i];
            double best = 0.0;
            for (int lag = minLag; lag <= maxLag + 1; ++lag)
            {
                double c = 0.0, e = 1.0e-12;
                for (int i = from; i < from + len && i + lag < b.getNumSamples(); ++i) { c += x[i] * x[i + lag]; e += x[i + lag] * x[i + lag]; }
                corr[(size_t) lag] = c / std::sqrt (e * e0);
                best = std::max (best, corr[(size_t) lag]);
            }
            // The first strong peak is the period (later peaks are whole multiples of it)
            for (int lag = minLag + 1; lag <= maxLag; ++lag)
                if (corr[(size_t) lag] >= 0.9 * best && corr[(size_t) lag] >= corr[(size_t) lag - 1] && corr[(size_t) lag] >= corr[(size_t) lag + 1])
                    return (float) (sr / lag);
            return 0.0f;
        }

        void guitarPackTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "recorded guitars:" << std::endl;
            const auto sr = 44100.0;
            auto load = [] (const juce::File& f)
            {
                juce::AudioBuffer<float> b;
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                if (std::unique_ptr<juce::AudioFormatReader> r { fm.createReaderFor (f) })
                {
                    b.setSize ((int) r->numChannels, (int) r->lengthInSamples);
                    r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
                }
                return b;
            };
            auto peakDb = [] (const juce::AudioBuffer<float>& b) { return juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()), -120.0f); };
            auto rmsDb  = [] (const juce::AudioBuffer<float>& b) { return juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, b.getNumSamples()), -120.0f); };
            auto oneNote = [] (te::MidiList& seq) { seq.addNote (52, te::BeatPosition(), te::BeatDuration::fromBeats (2.0), 100, 0, nullptr); };   // E3, 164.8 Hz

            // 1. Songs from before: the built-in guitar track becomes the sampler, same settings
            {
                auto edit = te::createEmptyEdit (engine, juce::File());
                edit->ensureNumberOfAudioTracks (2);
                auto tracks = te::getAudioTracks (*edit);
                auto g = edit->getPluginCache().createNewPlugin (GuitarPlugin::xmlTypeName, {});
                dynamic_cast<GuitarPlugin&> (*g).sound = 1;
                dynamic_cast<GuitarPlugin&> (*g).volumeDb = -3.0f;
                tracks[1]->pluginList.insertPlugin (g, 0, nullptr);
                const auto changed = StudioEngine::upgradeBuiltInInstruments (*edit);
                auto* s = dynamic_cast<SamplerPlugin*> (StudioEngine::getInstrumentPlugin (*tracks[1]));
                const bool ok = changed == 1 && s != nullptr && s->instrumentId.get() == "guitarNylon" && std::abs (s->volumeDb.get() + 3.0f) < 0.01f
                             && StudioEngine::getInstrumentId (*tracks[1]) == "guitarNylon" && StudioEngine::upgradeBuiltInInstruments (*edit) == 0;
                std::cout << "  old song: built-in nylon guitar (-3 dB) now " << (s != nullptr ? s->getName() : juce::String ("?"))
                          << (s != nullptr ? " at " + juce::String (s->volumeDb.get(), 1) + " dB" : juce::String()) << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 2. Pack not downloaded: the sampler plays the built-in guitar, as loud as before
            const bool realPack = SoundPacks::isInstalled ("guitar-electric");
            if (! realPack)
            {
                const auto viaSampler = folder.getChildFile ("guitar_builtin_sampler.wav");
                const auto viaPlugin  = folder.getChildFile ("guitar_builtin_plugin.wav");
                // Four plucks: each pick is a little random, so the two renders can differ by up to about 2.5 dB
                // (3 dB is still "as loud as before" to the ear; a real mistake would be 6 dB or more)
                auto fourNotes = [] (te::MidiList& seq)
                {
                    for (int i = 0; i < 4; ++i)
                        seq.addNote (52, te::BeatPosition::fromBeats (i * 1.5), te::BeatDuration::fromBeats (1.2), 100, 0, nullptr);
                };
                render (engine, viaSampler, SamplerPlugin::xmlTypeName, [] (te::Plugin& p) { dynamic_cast<SamplerPlugin&> (p).instrumentId = "guitar"; }, fourNotes, 5.0);
                render (engine, viaPlugin, GuitarPlugin::xmlTypeName, [] (te::Plugin&) {}, fourNotes, 5.0);
                const auto a = load (viaSampler), b = load (viaPlugin);
                const auto f = a.getNumSamples() > 0 ? pitchOf (a, 4410, 8192, sr) : 0.0f;
                const bool ok = a.getNumSamples() > 0 && peakDb (a) > -30.0f && std::abs (rmsDb (a) - rmsDb (b)) < 3.0f
                             && std::abs (12.0 * std::log2 (juce::jmax (1.0f, f) / 164.81)) < 0.5;
                std::cout << "  not downloaded: built-in guitar, " << juce::String (rmsDb (a), 1) << " dB (plugin " << juce::String (rmsDb (b), 1)
                          << " dB), E3 heard at " << juce::String (f, 1) << " Hz" << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            // 3. With the pack: recorded samples. A small test pack is made if the real one is not installed.
            const auto packFolder = SoundPacks::getPackFolder ("guitar-electric");
            if (! realPack)
            {
                packFolder.getChildFile ("samples").createDirectory();
                juce::String sfz ("<control> default_path=\n");
                for (int key : { 40, 52, 64 })
                {
                    const auto f = 440.0 * std::pow (2.0, (key - 69) / 12.0);
                    juce::AudioBuffer<float> b (2, (int) (sr * 2.0));
                    for (int i = 0; i < b.getNumSamples(); ++i)
                    {
                        double v = 0.0;
                        for (int k = 1; k <= 6; ++k) v += (0.5 / k) * std::sin (juce::MathConstants<double>::twoPi * f * k * i / sr) * std::exp (-i / sr * (2.0 + k));
                        b.setSample (0, i, (float) v); b.setSample (1, i, (float) v);
                    }
                    const auto wavFile = packFolder.getChildFile ("samples").getChildFile ("n" + juce::String (key) + ".wav");
                    if (auto w = WavFiles::writer (wavFile, sr, 2, 16))
                        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                    sfz << "<region> sample=samples/n" << key << ".wav lokey=" << (key - 6) << " hikey=" << (key + 5)
                        << " pitch_keycenter=" << key << " ampeg_release=0.2\n";
                }
                packFolder.getChildFile ("Clean.sfz").replaceWithText (sfz);
                packFolder.getChildFile ("pack.json").replaceWithText ("{ \"id\": \"guitar-electric\", \"version\": \"test\" }");
            }
            {
                const auto out = folder.getChildFile ("guitar_recorded.wav");
                juce::String status;
                juce::int64 memory = 0;
                render (engine, out, SamplerPlugin::xmlTypeName, [&] (te::Plugin& p) { dynamic_cast<SamplerPlugin&> (p).instrumentId = "guitar"; },
                        oneNote, 3.0);
                {
                    // Status after loading
                    auto edit = te::createEmptyEdit (engine, juce::File());
                    edit->ensureNumberOfAudioTracks (1);
                    auto p = edit->getPluginCache().createNewPlugin (SamplerPlugin::xmlTypeName, {});
                    auto& s = dynamic_cast<SamplerPlugin&> (*p);
                    s.instrumentId = "guitar";
                    te::getAudioTracks (*edit)[0]->pluginList.insertPlugin (p, 0, nullptr);
                    for (int i = 0; i < 300 && s.getStatus() == SamplerPlugin::Status::loading; ++i)
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                    status = s.getStatus() == SamplerPlugin::Status::ready ? "recorded samples" : s.getStatusText();
                    memory = s.getMemoryBytes();
                }
                const auto a = load (out);
                const auto f = a.getNumSamples() > 0 ? pitchOf (a, 4410, 8192, sr) : 0.0f;
                const bool ok = status == "recorded samples" && memory > 0 && a.getNumSamples() > 0 && peakDb (a) > -30.0f
                             && std::abs (12.0 * std::log2 (juce::jmax (1.0f, f) / 164.81)) < 0.5;
                std::cout << "  " << (realPack ? "guitar pack" : "test pack") << " installed: " << status << ", " << memory / 1024 << " KB, E3 heard at "
                          << juce::String (f, 1) << " Hz" << (ok ? "  ok" : "  FAILED") << std::endl;
            }
            if (! realPack)
                packFolder.deleteRecursively();

            // 4. The band's guitar uses the sampler now
            {
                auto edit = te::createEmptyEdit (engine, juce::File());
                const bool usesSampler = Instruments::find ("guitar") != nullptr && Instruments::find ("guitar")->kind == Instruments::Kind::sampler
                                      && juce::String (Instruments::find ("guitarNylon")->pack) == "guitar-nylon";
                std::cout << "  Clean Electric and Nylon guitars play from the sound packs" << (usesSampler ? "  ok" : "  FAILED") << std::endl;
            }
        }

        //======================================================================
        //  Plan Step 31: recorded African percussion (sound pack), built-in where it has no recording
        //======================================================================
        void percussionPackTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "recorded percussion:" << std::endl;
            const auto sr = 44100.0;
            auto load = [] (const juce::File& f)
            {
                juce::AudioBuffer<float> b;
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                if (std::unique_ptr<juce::AudioFormatReader> r { fm.createReaderFor (f) })
                {
                    b.setSize ((int) r->numChannels, (int) r->lengthInSamples);
                    r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
                }
                return b;
            };
            auto rmsDb = [] (const juce::AudioBuffer<float>& b, int from, int len)
            {
                if (from + len > b.getNumSamples()) return -120.0f;
                return juce::Decibels::gainToDecibels (b.getRMSLevel (0, from, len), -120.0f);
            };
            // Conga open at 0 s, djembe slap at 1 s (90 BPM: 1.5 beats a second)
            auto twoHits = [] (te::MidiList& seq)
            {
                seq.addNote (Percussion::congaOpen,  te::BeatPosition(), te::BeatDuration::fromBeats (0.5), 110, 0, nullptr);
                seq.addNote (Percussion::djembeSlap, te::BeatPosition::fromBeats (1.5), te::BeatDuration::fromBeats (0.5), 110, 0, nullptr);
            };

            // 1. Old songs: built-in percussion and balafon tracks move to the sampler
            {
                auto edit = te::createEmptyEdit (engine, juce::File());
                edit->ensureNumberOfAudioTracks (2);
                auto tracks = te::getAudioTracks (*edit);
                for (int i = 0; i < 2; ++i)
                {
                    auto p = edit->getPluginCache().createNewPlugin (PercussionPlugin::xmlTypeName, {});
                    dynamic_cast<PercussionPlugin&> (*p).mode = i;
                    dynamic_cast<PercussionPlugin&> (*p).volumeDb = -2.0f;
                    tracks[i]->pluginList.insertPlugin (p, 0, nullptr);
                }
                const auto changed = StudioEngine::upgradeBuiltInInstruments (*edit);
                auto* a = dynamic_cast<SamplerPlugin*> (StudioEngine::getInstrumentPlugin (*tracks[0]));
                auto* b = dynamic_cast<SamplerPlugin*> (StudioEngine::getInstrumentPlugin (*tracks[1]));
                const bool ok = changed == 2 && a != nullptr && b != nullptr && a->instrumentId.get() == "percussion" && b->instrumentId.get() == "balafon"
                             && std::abs (a->volumeDb.get() + 2.0f) < 0.01f;
                std::cout << "  old song: percussion and balafon tracks now play through the sampler" << (ok ? "  ok" : "  FAILED") << std::endl;
            }

            const bool realPack = SoundPacks::isInstalled ("percussion-african");

            // 2. Not downloaded: exactly the built-in percussion, as loud as before
            if (! realPack)
            {
                const auto viaSampler = folder.getChildFile ("perc_builtin_sampler.wav");
                const auto viaPlugin  = folder.getChildFile ("perc_builtin_plugin.wav");
                render (engine, viaSampler, SamplerPlugin::xmlTypeName, [] (te::Plugin& p) { dynamic_cast<SamplerPlugin&> (p).instrumentId = "percussion"; }, twoHits, 2.0);
                render (engine, viaPlugin, PercussionPlugin::xmlTypeName, [] (te::Plugin&) {}, twoHits, 2.0);
                const auto a = load (viaSampler), b = load (viaPlugin);
                const auto conga = rmsDb (a, 0, 8820), slap = rmsDb (a, 44100, 8820);
                const bool ok = conga > -40.0f && slap > -40.0f && std::abs (conga - rmsDb (b, 0, 8820)) < 2.0f && std::abs (slap - rmsDb (b, 44100, 8820)) < 2.0f;
                std::cout << "  not downloaded: conga " << juce::String (conga, 1) << " dB, djembe slap " << juce::String (slap, 1)
                          << " dB, same as before" << (ok ? "  ok" : "  FAILED") << std::endl;

                const auto bal = folder.getChildFile ("balafon_builtin_sampler.wav");
                render (engine, bal, SamplerPlugin::xmlTypeName, [] (te::Plugin& p) { dynamic_cast<SamplerPlugin&> (p).instrumentId = "balafon"; },
                        [] (te::MidiList& seq) { seq.addNote (64, te::BeatPosition(), te::BeatDuration::fromBeats (1.0), 100, 0, nullptr); }, 2.0);
                const auto c = load (bal);
                const auto f = c.getNumSamples() > 0 ? pitchOf (c, 2205, 4096, sr) : 0.0f;
                const bool balOk = rmsDb (c, 0, 22050) > -45.0f && std::abs (12.0 * std::log2 (juce::jmax (1.0f, f) / 329.63)) < 0.6;
                std::cout << "  not downloaded: balafon E4 heard at " << juce::String (f, 1) << " Hz" << (balOk ? "  ok" : "  FAILED") << std::endl;
            }

            // 3. With the pack: the conga is the recording, the djembe (no recording) stays built-in
            const auto packFolder = SoundPacks::getPackFolder ("percussion-african");
            if (! realPack)
            {
                packFolder.getChildFile ("samples").createDirectory();
                // A recognisable "recording": a pure 523 Hz tone (no synth conga sounds like this)
                juce::AudioBuffer<float> b (2, (int) (sr * 0.5));
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    const auto v = (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * 523.25 * i / sr) * std::exp (-i / sr * 4.0));
                    b.setSample (0, i, v); b.setSample (1, i, v);
                }
                if (auto w = WavFiles::writer (packFolder.getChildFile ("samples").getChildFile ("conga.wav"), sr, 2, 16))
                    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                packFolder.getChildFile ("Percussion.sfz").replaceWithText ("<control> default_path=\n<region> sample=samples/conga.wav lokey=63 hikey=63 pitch_keycenter=63\n");
                packFolder.getChildFile ("Balafon.sfz").replaceWithText ("<control> default_path=\n<region> sample=samples/conga.wav lokey=72 hikey=72 pitch_keycenter=72\n");
                packFolder.getChildFile ("pack.json").replaceWithText ("{ \"id\": \"percussion-african\", \"version\": \"test\" }");
            }
            if (! realPack)
            {
                const auto out = folder.getChildFile ("perc_recorded.wav");
                render (engine, out, SamplerPlugin::xmlTypeName, [] (te::Plugin& p) { dynamic_cast<SamplerPlugin&> (p).instrumentId = "percussion"; }, twoHits, 2.0);
                const auto a = load (out);
                const auto f = a.getNumSamples() > 0 ? pitchOf (a, 1000, 4096, sr) : 0.0f;
                const auto slap = rmsDb (a, 44100, 8820);
                const bool ok = std::abs (12.0 * std::log2 (juce::jmax (1.0f, f) / 523.25)) < 0.3 && slap > -40.0f;
                std::cout << "  test pack installed: conga plays the recording (" << juce::String (f, 1) << " Hz), djembe slap stays built-in ("
                          << juce::String (slap, 1) << " dB)" << (ok ? "  ok" : "  FAILED") << std::endl;

                // Balafon: the recorded note, and a note the pack does not have still sounds (built-in)
                const auto bal = folder.getChildFile ("balafon_recorded.wav");
                render (engine, bal, SamplerPlugin::xmlTypeName, [] (te::Plugin& p) { dynamic_cast<SamplerPlugin&> (p).instrumentId = "balafon"; },
                        [] (te::MidiList& seq)
                        {
                            seq.addNote (72, te::BeatPosition(), te::BeatDuration::fromBeats (0.5), 100, 0, nullptr);
                            seq.addNote (64, te::BeatPosition::fromBeats (1.5), te::BeatDuration::fromBeats (0.5), 100, 0, nullptr);
                        }, 2.0);
                const auto c = load (bal);
                const auto f1 = c.getNumSamples() > 0 ? pitchOf (c, 1000, 4096, sr) : 0.0f;
                const auto f2 = c.getNumSamples() > 0 ? pitchOf (c, 44100 + 2205, 4096, sr) : 0.0f;
                const bool balOk = std::abs (12.0 * std::log2 (juce::jmax (1.0f, f1) / 523.25)) < 0.3 && std::abs (12.0 * std::log2 (juce::jmax (1.0f, f2) / 329.63)) < 0.6;
                std::cout << "  balafon: recorded C5 at " << juce::String (f1, 1) << " Hz, built-in E4 at " << juce::String (f2, 1) << " Hz" << (balOk ? "  ok" : "  FAILED") << std::endl;
                packFolder.deleteRecursively();
            }
        }

        //======================================================================
        //  Plan Step 26: track names follow the instrument
        //======================================================================
        void trackNameTests (te::Engine& engine)
        {
            std::cout << "track names:" << std::endl;
            auto edit = te::createEmptyEdit (engine, juce::File());
            edit->ensureNumberOfAudioTracks (5);
            auto tracks = te::getAudioTracks (*edit);
            tracks[0]->setName ("Organ");
            tracks[1]->setName ("Instrument 6");
            tracks[2]->setName ("Sister Grace solo");
            tracks[3]->setName ("Piano 2");
            tracks[4]->setName ("Track 5");

            const bool names = StudioEngine::trackNameFor ("grand") == "Piano" && StudioEngine::trackNameFor ("bassPick") == "Bass"
                            && StudioEngine::trackNameFor ("percussion") == "Percussion" && StudioEngine::trackNameFor ("guitarNylon") == "Guitar";
            std::cout << "  instrument names: Piano, Bass, Percussion, Guitar" << (names ? "  ok" : "  FAILED") << std::endl;

            const bool automatic = StudioEngine::hasAutomaticName (*tracks[0]) && StudioEngine::hasAutomaticName (*tracks[1])
                                && ! StudioEngine::hasAutomaticName (*tracks[2]) && StudioEngine::hasAutomaticName (*tracks[3])
                                && StudioEngine::hasAutomaticName (*tracks[4]);
            std::cout << "  \"Organ\", \"Instrument 6\", \"Piano 2\" are automatic; \"Sister Grace solo\" is yours" << (automatic ? "  ok" : "  FAILED") << std::endl;

            StudioEngine::nameAfterInstrument (*tracks[1], "Organ");     // another organ: "Organ 2"
            StudioEngine::nameAfterInstrument (*tracks[2], "Bass");      // your own name stays
            StudioEngine::nameAfterInstrument (*tracks[3], "Guitar");    // piano changed to guitar
            StudioEngine::nameAfterInstrument (*tracks[0], "Organ");     // same instrument: unchanged
            const bool renamed = tracks[0]->getName() == "Organ" && tracks[1]->getName() == "Organ 2"
                              && tracks[2]->getName() == "Sister Grace solo" && tracks[3]->getName() == "Guitar";
            std::cout << "  renamed: " << tracks[0]->getName() << ", " << tracks[1]->getName() << ", " << tracks[2]->getName()
                      << ", " << tracks[3]->getName() << (renamed ? "  ok" : "  FAILED") << std::endl;

            const bool unique = StudioEngine::uniqueTrackName (*edit, "Organ") == "Organ 3" && StudioEngine::uniqueTrackName (*edit, "Drums") == "Drums";
            std::cout << "  a new organ track: " << StudioEngine::uniqueTrackName (*edit, "Organ") << (unique ? "  ok" : "  FAILED") << std::endl;
        }

        //======================================================================
        //  Plan Step 25: disk clean-up when a song opens
        //======================================================================
        void songFilesTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "song files clean-up:" << std::endl;
            const auto song = folder.getChildFile ("CleanupSong");
            song.deleteRecursively();
            const auto audio = song.getChildFile ("Audio");
            auto make = [&] (const juce::File& f, int seconds)
            {
                f.getParentDirectory().createDirectory();
                juce::AudioBuffer<float> b (2, 44100 * seconds);
                b.clear();
                for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, 0.1f * std::sin (i * 0.05f));
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (f);
                auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (44100.0).withNumChannels (2).withBitsPerSample (24));
                w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                return f;
            };
            const auto take = make (audio.getChildFile ("Lead take.wav"), 2);
            const auto imported = make (audio.getChildFile ("Imported song (not in any clip).wav"), 1);
            const auto loopCopy = make (audio.getChildFile ("Following").getChildFile ("Loop - 100bpm.wav"), 1);
            juce::Array<juce::File> stemsNow, stemsOld;
            for (auto* n : { "Voice", "Drums", "Bass", "Other" })
            {
                stemsNow.add (make (audio.getChildFile ("Stems").getChildFile (juce::String ("Lead take - ") + n + "2.wav"), 2));
                stemsOld.add (make (audio.getChildFile ("Stems").getChildFile (juce::String ("Lead take - ") + n + ".wav"), 2));
            }
            const auto oldHarmony = make (audio.getChildFile ("Harmony").getChildFile ("Lead take - Alto.wav"), 1);
            const auto nestedOld = make (audio.getChildFile ("Timed").getChildFile ("Harmony").getChildFile ("Lead take - timed - Tenor.wav"), 1);
            const auto timedNow = make (audio.getChildFile ("Timed").getChildFile ("Lead take - timed.wav"), 2);
            const auto stretchedOriginal = make (audio.getChildFile ("Tempo").getChildFile ("Lead take - timed @ 100 (from 80).wav"), 2);

            auto editFile = song.getChildFile ("project.tracktionedit");
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            te::EditFileOperations (*edit).save (true, true, false);
            edit->ensureNumberOfAudioTracks (5);
            auto tracks = te::getAudioTracks (*edit);
            // The lead: timing-corrected (plays the Timed file, remembers the original take)
            auto lead = tracks[0]->insertWaveClip ("lead", timedNow, { { te::TimePosition(), te::TimePosition::fromSeconds (2.0) }, {} }, false);
            lead->state.setProperty ("gpsTimingSource", take.getFullPathName(), nullptr);
            lead->state.setProperty ("gpsRecMade", stretchedOriginal.getFullPathName(), nullptr);
            for (int i = 0; i < 4; ++i)
                tracks[i + 1]->insertWaveClip ("stem", stemsNow[i], { { te::TimePosition(), te::TimePosition::fromSeconds (2.0) }, {} }, false);

            juce::int64 expectBytes = 0;
            for (auto& f : stemsOld) expectBytes += f.getSize();
            expectBytes += oldHarmony.getSize() + nestedOld.getSize();

            const auto r = SongFiles::tidy (*edit);
            bool keptAll = take.existsAsFile() && imported.existsAsFile() && loopCopy.existsAsFile() && timedNow.existsAsFile() && stretchedOriginal.existsAsFile();
            for (auto& f : stemsNow) keptAll = keptAll && f.existsAsFile();
            bool goneAll = ! oldHarmony.existsAsFile() && ! nestedOld.existsAsFile();
            for (auto& f : stemsOld) goneAll = goneAll && ! f.existsAsFile();
            const bool emptyFoldersGone = ! audio.getChildFile ("Harmony").exists() && ! audio.getChildFile ("Timed").getChildFile ("Harmony").exists();
            std::cout << "  deleted " << r.files << " unused files (" << SongFiles::describeSize (r.bytes) << "), kept every file in use, the take, imported files and loops"
                      << (r.files == 6 && r.bytes == expectBytes && keptAll && goneAll && emptyFoldersGone ? "  ok" : "  FAILED") << std::endl;

            // Opening again: nothing more to clean
            const auto again = SongFiles::tidy (*edit);
            std::cout << "  opening again: " << again.files << " files" << (again.files == 0 ? "  ok" : "  FAILED") << std::endl;
        }

        //======================================================================
        //  Plan Step 14: group buses and sidechain ducking
        //======================================================================
        void busTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "buses and sidechain:" << std::endl;
            const double sr = 44100.0, seconds = 4.0;

            auto writeWav = [&] (const juce::File& f, std::function<float (int)> fn)
            {
                const int len = (int) (seconds * sr);
                juce::AudioBuffer<float> b (1, len);
                for (int i = 0; i < len; ++i) b.setSample (0, i, fn (i));
                f.deleteFile();
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (f, sr, (int) (1), 24).release());
                w->writeFromAudioSampleBuffer (b, 0, len);
            };
            const auto twoPi = juce::MathConstants<double>::twoPi;
            const auto alto = folder.getChildFile ("bus_alto.wav"), tenor = folder.getChildFile ("bus_tenor.wav");
            const auto bass = folder.getChildFile ("duck_bass.wav"), kick = folder.getChildFile ("duck_kick.wav");
            writeWav (alto,  [&] (int i) { return (float) (0.2 * std::sin (twoPi * 440.0 * i / sr)); });
            writeWav (tenor, [&] (int i) { return (float) (0.2 * std::sin (twoPi * 330.0 * i / sr)); });
            writeWav (bass,  [&] (int i) { return (float) (0.3 * std::sin (twoPi * 80.0 * i / sr)); });
            juce::Random rnd (3);
            writeWav (kick,  [&] (int i)
            {
                const double t = std::fmod (i / sr, 0.5);           // a hit every half second
                return t < 0.12 ? (float) ((rnd.nextFloat() * 2.0f - 1.0f) * 0.7 * std::exp (-t * 25.0)) : 0.0f;
            });

            auto editFile = folder.getChildFile ("buses.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            te::EditFileOperations (*edit).save (true, true, false);
            edit->ensureNumberOfAudioTracks (4);
            auto tracks = te::getAudioTracks (*edit);
            const te::ClipPosition whole { { te::TimePosition(), te::TimePosition::fromSeconds (seconds) }, {} };
            tracks[0]->insertWaveClip ("alto", alto, whole, false);
            tracks[1]->insertWaveClip ("tenor", tenor, whole, false);

            auto renderTo = [&] (const juce::File& out, juce::Array<te::Track*> only = {})
            {
                te::Renderer::Parameters p (*edit);
                p.time = { te::TimePosition(), te::TimePosition::fromSeconds (seconds) };
                p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
                p.bitDepth = 24;
                p.canRenderInMono = false;
                p.destFile = out;
                if (! only.isEmpty()) p.tracksToDo = te::toBitSet (only);
                out.deleteFile();
                bool finished = false;
                auto handle = te::EditRenderer::render (p, [&finished] (tl::expected<juce::File, std::string>) { finished = true; });
                for (int i = 0; i < 3000 && ! finished; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
            };

            const auto before = folder.getChildFile ("bus_before.wav");
            renderTo (before);
            const auto lBefore = bandDb (before, 0.5, 3.5, 200.0, 1000.0);

            // An empty bus, both voices into it: the same sound
            auto* bus = Mixing::createGroupBus (*edit, (int) Mixing::busKinds().size() - 1);
            Mixing::routeToBus (*tracks[0], bus);
            Mixing::routeToBus (*tracks[1], bus);
            std::cout << "  bus \"" << bus->getName() << "\" has " << Mixing::getBusMembers (*bus).size() << " tracks; song tracks listed: "
                      << Mixing::getSongTracks (*edit).size() << (Mixing::getBusMembers (*bus).size() == 2 && Mixing::getSongTracks (*edit).size() == 4 ? "  ok" : "  FAILED") << std::endl;
            const auto through = folder.getChildFile ("bus_through.wav");
            renderTo (through);
            const auto lThrough = bandDb (through, 0.5, 3.5, 200.0, 1000.0);
            std::cout << "  through the bus: " << juce::String (lThrough - lBefore, 2) << " dB (want 0)"
                      << (std::abs (lThrough - lBefore) < 0.2 ? "  ok" : "  FAILED") << std::endl;

            // The bus fader moves both voices
            bus->getVolumePlugin()->setVolumeDb (-6.0f);
            const auto down = folder.getChildFile ("bus_down.wav");
            renderTo (down);
            const auto lDown = bandDb (down, 0.5, 3.5, 200.0, 1000.0);
            std::cout << "  bus fader -6 dB: " << juce::String (lDown - lBefore, 2) << " dB"
                      << (std::abs (lDown - lBefore + 6.0) < 0.3 ? "  ok" : "  FAILED") << std::endl;

            // Stem of the bus (as the export does it)
            juce::Array<te::Track*> stem { bus };
            for (auto* m : Mixing::getBusMembers (*bus)) stem.add (m);
            const auto stemFile = folder.getChildFile ("bus_stem.wav");
            renderTo (stemFile, stem);
            const auto lStem = bandDb (stemFile, 0.5, 3.5, 200.0, 1000.0);
            std::cout << "  bus as a stem: " << juce::String (lStem - lDown, 2) << " dB against the mix"
                      << (std::abs (lStem - lDown) < 0.3 ? "  ok" : "  FAILED") << std::endl;

            // A preset bus gets its effects; deleting a bus sends its tracks back to the master
            auto* choir = Mixing::createGroupBus (*edit, 0);
            int fx = 0;
            for (auto* q : choir->pluginList.getPlugins()) if (! Mixing::isMixerPlugin (*q)) ++fx;
            std::cout << "  \"" << choir->getName() << "\" comes with " << fx << " effects" << (fx == 2 ? "  ok" : "  FAILED") << std::endl;
            Mixing::deleteGroupBus (*bus);
            std::cout << "  bus deleted: tracks back to the master "
                      << (Mixing::getBusOf (*tracks[0]) == nullptr && Mixing::getBusOf (*tracks[1]) == nullptr ? "ok" : "FAILED") << std::endl;
            Mixing::deleteGroupBus (*choir);

            // Sidechain: the kick pushes the bass down
            tracks[0]->setMute (true);
            tracks[1]->setMute (true);
            tracks[2]->insertWaveClip ("bass", bass, whole, false);
            tracks[3]->insertWaveClip ("kick", kick, whole, false);
            auto dip = [&] (const juce::File& f)
            {
                // Bass level during the kicks (first 60 ms of each) against between them
                double on = 0.0, off = 0.0;
                for (int k = 1; k < 7; ++k)
                {
                    on  += bandDb (f, k * 0.5 + 0.01, k * 0.5 + 0.06, 40.0, 120.0);
                    off += bandDb (f, k * 0.5 + 0.30, k * 0.5 + 0.45, 40.0, 120.0);
                }
                return (on - off) / 6.0;
            };
            const auto dry = folder.getChildFile ("duck_off.wav");
            renderTo (dry);
            Mixing::setDucking (*tracks[2], tracks[3], 1);
            const auto ducked = folder.getChildFile ("duck_on.wav");
            renderTo (ducked);
            const auto dipOff = dip (dry), dipOn = dip (ducked);
            std::cout << "  bass under the kick: " << juce::String (dipOff, 1) << " dB without, " << juce::String (dipOn, 1)
                      << " dB with ducking" << (dipOn < dipOff - 3.0 ? "  ok" : "  FAILED") << std::endl;
            std::cout << "  ducking source: " << (Mixing::getDuckingSource (*tracks[2]) == tracks[3] ? "ok" : "FAILED")
                      << ", kind \"" << Mixing::duckKinds()[(size_t) Mixing::getDuckingKind (*tracks[2])].name << "\"" << std::endl;
            Mixing::setDucking (*tracks[2], nullptr, 0);
            std::cout << "  ducking off: " << (Mixing::findDucker (*tracks[2]) == nullptr ? "ok" : "FAILED") << std::endl;
        }

        //======================================================================
        //  Plan Step 16: analysers and the reference track
        //======================================================================
        void analyserTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "analyser and reference:" << std::endl;
            const double sr = 44100.0, seconds = 6.0;
            const int len = (int) (sr * seconds);
            const auto twoPi = juce::MathConstants<double>::twoPi;

            auto writeStereo = [&] (const juce::File& f, std::function<std::pair<float, float> (int)> fn)
            {
                juce::AudioBuffer<float> b (2, len);
                for (int i = 0; i < len; ++i) { auto [l, r] = fn (i); b.setSample (0, i, l); b.setSample (1, i, r); }
                f.deleteFile();
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (f, sr, (int) (2), 24).release());
                w->writeFromAudioSampleBuffer (b, 0, len);
            };

            // A 1 kHz tone: the spectrum peaks at 1 kHz, mono, centred
            const auto tone = folder.getChildFile ("an_tone.wav");
            writeStereo (tone, [&] (int i) { const auto v = (float) (0.5 * std::sin (twoPi * 1000.0 * i / sr)); return std::pair { v, v }; });
            auto a = Analysis::analyseFile (tone);
            int peakBand = 0;
            for (int b = 1; b < (int) a.spectrum.db.size(); ++b) if (a.spectrum.db[(size_t) b] > a.spectrum.db[(size_t) peakBand]) peakBand = b;
            const auto peakHz = Analysis::bandCentres()[(size_t) peakBand];
            std::cout << "  1 kHz tone: spectrum peak at " << juce::roundToInt (peakHz) << " Hz, phase " << juce::String (a.correlation, 2)
                      << ", width " << juce::String (a.width, 2) << ", " << juce::String (a.lufs, 1) << " LUFS"
                      << (std::abs (peakHz - 1000.0f) < 120.0f && a.correlation > 0.99f && a.width < 0.01f ? "  ok" : "  FAILED") << std::endl;

            // Out of phase and wide
            const auto anti = folder.getChildFile ("an_antiphase.wav");
            writeStereo (anti, [&] (int i) { const auto v = (float) (0.3 * std::sin (twoPi * 300.0 * i / sr)); return std::pair { v, -v }; });
            const auto b = Analysis::analyseFile (anti);
            std::cout << "  out of phase: phase " << juce::String (b.correlation, 2) << (b.correlation < -0.99f ? "  ok" : "  FAILED") << std::endl;

            // Two noises: a "reference" (flat) and a "mix" with 6 dB more bass and quieter
            juce::Random rnd (11);
            const auto refFile = folder.getChildFile ("an_reference.wav"), mixFile = folder.getChildFile ("an_mix.wav");
            std::vector<float> noiseL ((size_t) len), noiseR ((size_t) len);
            for (int i = 0; i < len; ++i) { noiseL[(size_t) i] = rnd.nextFloat() * 2.0f - 1.0f; noiseR[(size_t) i] = rnd.nextFloat() * 2.0f - 1.0f; }
            writeStereo (refFile, [&] (int i) { return std::pair { 0.25f * noiseL[(size_t) i], 0.25f * noiseR[(size_t) i] }; });
            {
                juce::IIRFilter shelfL, shelfR;
                shelfL.setCoefficients (juce::IIRCoefficients::makeLowShelf (sr, 120.0, 0.7, juce::Decibels::decibelsToGain (8.0f)));
                shelfR.setCoefficients (juce::IIRCoefficients::makeLowShelf (sr, 120.0, 0.7, juce::Decibels::decibelsToGain (8.0f)));
                writeStereo (mixFile, [&] (int i)
                {
                    return std::pair { 0.08f * shelfL.processSingleSampleRaw (noiseL[(size_t) i]), 0.08f * shelfR.processSingleSampleRaw (noiseR[(size_t) i]) };
                });
            }
            const auto ref = Analysis::analyseFile (refFile), mix = Analysis::analyseFile (mixFile);
            const auto lines = Analysis::describeDifferences (mix, ref);
            std::cout << "  noise with more bass against flat noise:" << std::endl;
            for (auto& l : lines) std::cout << "    " << l << std::endl;
            const bool saysBass = lines[0].startsWith ("Bass:") && lines[0].contains ("more");
            bool saysQuieter = false;
            for (auto& l : lines) saysQuieter = saysQuieter || l.contains ("quieter");
            std::cout << "  finds the extra bass and the lower loudness: " << (saysBass && saysQuieter ? "ok" : "FAILED")
                      << "  (wide noise: phase " << juce::String (ref.correlation, 2) << ", width " << juce::String (ref.width, 2) << ")" << std::endl;

            // The plugin: exports never contain the reference
            auto editFile = folder.getChildFile ("analyser.tracktionedit");
            editFile.deleteFile();
            auto edit = te::createEmptyEdit (engine, editFile);
            edit->editFileRetriever = [editFile] { return editFile; };
            te::EditFileOperations (*edit).save (true, true, false);
            edit->ensureNumberOfAudioTracks (1);
            te::getAudioTracks (*edit)[0]->insertWaveClip ("mix", mixFile, { { te::TimePosition(), te::TimePosition::fromSeconds (seconds) }, {} }, false);

            auto renderTo = [&] (const juce::File& out)
            {
                te::Renderer::Parameters p (*edit);
                p.time = { te::TimePosition(), te::TimePosition::fromSeconds (seconds) };
                p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
                p.bitDepth = 24;
                p.canRenderInMono = false;
                p.useMasterPlugins = true;
                p.destFile = out;
                out.deleteFile();
                bool finished = false;
                auto handle = te::EditRenderer::render (p, [&finished] (tl::expected<juce::File, std::string>) { finished = true; });
                for (int i = 0; i < 3000 && ! finished; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
            };
            const auto without = folder.getChildFile ("an_export_without.wav");
            renderTo (without);
            auto* an = AnalyserPlugin::addTo (*edit);
            an->loadReference (refFile);
            for (int i = 0; i < 500 && ! an->isReferenceReady(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
            an->referenceGainDb = mix.lufs - ref.lufs;
            an->listenToReference = true;
            const auto with = folder.getChildFile ("an_export_with_reference_on.wav");
            renderTo (with);
            const auto d = bandDb (with, 0.5, 5.5, 20.0, 20000.0) - bandDb (without, 0.5, 5.5, 20.0, 20000.0);
            std::cout << "  reference loaded: " << (an->isReferenceReady() ? "yes" : "no") << ", " << juce::String (an->getReferenceLength(), 1)
                      << " s; export with B switched on is unchanged: " << juce::String (d, 3) << " dB" << (an->isReferenceReady() && std::abs (d) < 0.01 ? "  ok" : "  FAILED") << std::endl;

            // Live: with B on, the plugin plays the reference at the matched level
            an->initialise ({ te::TimePosition(), sr, 512 });
            juce::AudioBuffer<float> block (2, 512);
            double heard = 0.0, refPart = 0.0;
            for (int k = 0; k < 400; ++k)
            {
                block.clear();          // the mix is silent here: what comes out is the reference
                const auto t0 = 1.0 + k * 512 / sr;
                te::PluginRenderContext ctx (&block, juce::AudioChannelSet::stereo(), 0, 512, nullptr, 0.0,
                                             { te::TimePosition::fromSeconds (t0), te::TimePosition::fromSeconds (t0 + 512 / sr) },
                                             true, false, false, false);
                an->applyToBuffer (ctx);
                if (k > 20) { heard += block.getRMSLevel (0, 0, 512); refPart += 1.0; }
            }
            const auto heardDb = juce::Decibels::gainToDecibels ((float) (heard / refPart));
            const auto expectDb = juce::Decibels::gainToDecibels (0.25f / std::sqrt (3.0f)) + an->referenceGainDb.load();
            std::cout << "  B while playing: " << juce::String (heardDb, 1) << " dB (want " << juce::String (expectDb, 1) << ")"
                      << (std::abs (heardDb - expectDb) < 0.5f ? "  ok" : "  FAILED") << std::endl;
            an->listenToReference = false;
            for (int k = 0; k < 10; ++k)
            {
                block.clear();
                te::PluginRenderContext ctx (&block, juce::AudioChannelSet::stereo(), 0, 512, nullptr, 0.0,
                                             { te::TimePosition::fromSeconds (3.0), te::TimePosition::fromSeconds (3.0 + 512 / sr) }, true, false, false, false);
                an->applyToBuffer (ctx);
            }
            std::cout << "  back to A: " << (block.getMagnitude (0, 0, 512) < 1.0e-6f ? "ok" : "FAILED") << std::endl;
            float tmpL[64], tmpR[64];
            std::cout << "  live sound reaches the screen: " << (an->readLive (tmpL, tmpR, 64) == 64 ? "ok" : "FAILED") << std::endl;
        }

        //======================================================================
        //  Plan Step 18: release package
        //======================================================================
        juce::StringArray id3Frames (const juce::File& mp3)
        {
            juce::StringArray frames;
            juce::MemoryBlock data;
            if (! mp3.loadFileAsData (data) || data.getSize() < 10) return frames;
            const auto* b = static_cast<const juce::uint8*> (data.getData());
            if (b[0] != 'I' || b[1] != 'D' || b[2] != '3') return frames;
            const int version = b[3];
            const auto size = (b[6] << 21) | (b[7] << 14) | (b[8] << 7) | b[9];
            size_t pos = 10;
            while (pos + 10 < (size_t) size + 10 && pos + 10 < data.getSize())
            {
                const juce::String id (reinterpret_cast<const char*> (b + pos), 4);
                if (b[pos] == 0) break;
                const auto fs = version == 4 ? (juce::uint32) ((b[pos + 4] << 21) | (b[pos + 5] << 14) | (b[pos + 6] << 7) | b[pos + 7])
                                             : (juce::uint32) ((b[pos + 4] << 24) | (b[pos + 5] << 16) | (b[pos + 6] << 8) | b[pos + 7]);
                frames.add (id);
                pos += 10 + fs;
            }
            return frames;
        }

        void releaseTests (const juce::File& folder)
        {
            std::cout << "release package:" << std::endl;
            juce::String clean;
            const bool isrcOk = Release::checkIsrc ("CM-ABC-26-00001", clean) && clean == "CMABC2600001"
                                && Release::checkIsrc ("USRC17607839", clean) && ! Release::checkIsrc ("CM-ABC-26-0001", clean)
                                && ! Release::checkIsrc ("12ABC2600001", clean);
            std::cout << "  ISRC check: " << (isrcOk ? "ok" : "FAILED") << std::endl;
            const bool upcOk = Release::checkUpc ("036000291452", clean) && Release::checkUpc ("4006381333931", clean)
                               && ! Release::checkUpc ("036000291453", clean) && ! Release::checkUpc ("12345", clean);
            std::cout << "  UPC check digit: " << (upcOk ? "ok" : "FAILED") << std::endl;

            // Covers
            auto makeImage = [&] (const juce::String& name, int w, int h, bool png)
            {
                juce::Image img (juce::Image::RGB, w, h, true);
                juce::Graphics g (img);
                g.setGradientFill (juce::ColourGradient (juce::Colours::gold, 0, 0, juce::Colours::darkblue, (float) w, (float) h, false));
                g.fillAll();
                const auto f = folder.getChildFile (name);
                f.deleteFile();
                juce::FileOutputStream out (f);
                if (png) juce::PNGImageFormat().writeImageToStream (img, out);
                else     juce::JPEGImageFormat().writeImageToStream (img, out);
                return f;
            };
            const auto good = makeImage ("cover_3000.jpg", 3000, 3000, false);
            const auto big = makeImage ("cover_4000.png", 4000, 4000, true);
            const auto wide = makeImage ("cover_wide.jpg", 2000, 1000, false);
            const auto small = makeImage ("cover_small.jpg", 1000, 1000, false);
            const bool coversOk = Release::checkCover (good).usable && Release::checkCover (big).usable
                                  && ! Release::checkCover (wide).usable && ! Release::checkCover (small).usable;
            std::cout << "  cover checks (3000 ok, 4000 ok, not square refused, too small refused): " << (coversOk ? "ok" : "FAILED") << std::endl;
            juce::String problem;
            const auto out = folder.getChildFile ("cover_out.jpg");
            const bool wrote = Release::writeCover (big, out, problem);
            const auto outInfo = Release::checkCover (out);
            std::cout << "  4000 x 4000 PNG saved as " << outInfo.width << " x " << outInfo.height << " " << outInfo.format
                      << (wrote && outInfo.width == 3000 && outInfo.height == 3000 && outInfo.format == "jpg" ? "  ok" : "  FAILED") << std::endl;

            // Details and checks
            Song song;
            song.title = "Jehovah Is Faithful";
            song.key = "Ab major";
            song.tempo = 76;
            song.sections.push_back ({ "Verse", "You are faithful, Lord\nYou never fail" });
            song.sections.push_back ({ "Chorus", "Jehovah is faithful" });
            auto info = Release::Info::fromSong (song, "Minister Kemamah");
            std::cout << "  filled from the song: \"" << info.title << "\", " << info.artist << ", lyrics " << juce::StringArray::fromLines (info.lyrics).size()
                      << " lines, " << info.pLine << (info.title == song.title && info.lyrics.contains ("faithful") && info.pLine.contains ("Minister Kemamah") ? "  ok" : "  FAILED") << std::endl;
            info.title = "Jehovah Is Faithful feat. Choir";
            info.coverPath = good.getFullPathName();
            info.songwriters = "Johnathan Nembuanyi";
            auto checks = Release::check (info);
            std::cout << "  \"feat.\" in the title is refused: " << (Release::hasProblems (checks) ? "ok" : "FAILED") << std::endl;
            info.title = "Jehovah Is Faithful";
            info.featured = "Choir";
            checks = Release::check (info);
            std::cout << "  fixed: " << (Release::hasProblems (checks) ? "FAILED" : "ok (no red lines)") << std::endl;

            Analysis::FileAnalysis master;
            master.ok = true; master.seconds = 245; master.lufs = -9.5f; master.truePeakDb = 0.3f;
            checks = Release::check (info, &master, 3.5);
            int problems = 0, warnings = 0;
            for (auto& c : checks) { if (c.level == Release::Level::problem) ++problems; if (c.level == Release::Level::warning) ++warnings; }
            std::cout << "  clipping master and 3.5 s of silence are caught: " << problems << " red, " << warnings << " orange"
                      << (problems >= 1 && warnings >= 1 ? "  ok" : "  FAILED") << std::endl;

            const auto saved = Release::Info::fromSong ([&] { auto s2 = song; s2.release = info.toVar(); return Song::fromVar (s2.toVar()); }(), "Someone");
            std::cout << "  details saved with the song: " << (saved.featured == "Choir" && saved.songwriters == info.songwriters && saved.artist == "Minister Kemamah" ? "ok" : "FAILED") << std::endl;
            const auto details = Release::detailsText (info, master, song);
            std::cout << "  details sheet: " << juce::StringArray::fromLines (details).size() << " lines"
                      << (details.contains ("Songwriters") && details.contains ("Ab major") ? "  ok" : "  FAILED") << std::endl;

            // MP3 with the details and the cover inside (accents included)
            if (Mp3Encoder::isAvailable())
            {
                const auto wav = folder.getChildFile ("release_tone.wav");
                {
                    juce::AudioBuffer<float> b (2, 44100 * 2);
                    for (int i = 0; i < b.getNumSamples(); ++i)
                        b.setSample (0, i, (float) (0.3 * std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / 44100.0))), b.setSample (1, i, b.getSample (0, i));
                    wav.deleteFile();
                    juce::WavAudioFormat fmt;
                    std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (wav, 44100.0, (int) (2), 24).release());
                    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                }
                Mp3Encoder::Tags tags;
                tags.title = juce::String::fromUTF8 ("J\xc3\xa9sus est l\xc3\xa0");
                tags.artist = "Minister Kemamah";
                tags.year = "2026";
                tags.genre = "Gospel";
                tags.composer = "Johnathan Nembuanyi";
                tags.copyright = juce::String::fromUTF8 ("\xc2\xa9 2026 Minister Kemamah");
                tags.isrc = "CMABC2600001";
                out.loadFileAsData (tags.coverArt);
                const auto mp3 = folder.getChildFile ("release_tone.mp3");
                const auto r = Mp3Encoder::encode (wav, mp3, 320, tags);
                const auto frames = id3Frames (mp3);
                bool all = r.wasOk();
                for (auto f : { "TIT2", "TPE1", "TCOM", "TCOP", "TSRC", "APIC", "TCON" })
                    all = all && frames.contains (f);
                std::cout << "  MP3 tags: " << frames.joinIntoString (" ") << (all ? "  ok" : "  FAILED") << std::endl;
            }
            else
                std::cout << "  MP3 tags: skipped (no MP3 encoder in this build)" << std::endl;
        }

        void mySoundsTests (te::Engine& engine, const juce::File& folder)
        {
            std::cout << "my sounds:" << std::endl;
            const auto tone = folder.getChildFile ("mysound_a3.wav");     // 220 Hz = A3 (57)
            writeTone (tone, 220.0, 1.5);
            const auto clicks = folder.getChildFile ("mysound_hit.wav");
            writeClickLoop (clicks, 120.0, 1);
            const auto root = MySounds::detectRootNote (tone);
            std::cout << "  detected note of the tone: " << MySounds::keyName (root) << (root == 57 ? "  ok" : "  FAILED") << std::endl;
            const auto noRoot = MySounds::detectRootNote (clicks);
            std::cout << "  click (no clear note): " << noRoot << std::endl;

            render (engine, folder.getChildFile ("mysounds.wav"), SamplerPlugin::xmlTypeName,
                    [&] (te::Plugin& p)
                    {
                        auto& s = dynamic_cast<SamplerPlugin&> (p);
                        s.instrumentId = "mysounds";
                        auto hit = MySounds::add (s.state, clicks, nullptr);
                        auto mel = MySounds::add (s.state, tone, nullptr);
                        MySounds::setMode (mel, MySounds::Mode::melodic, nullptr);
                        std::cout << "  hit on " << MySounds::keyName ((int) hit["key"]) << ", melodic from "
                                  << MySounds::keyName ((int) mel["key"]) << std::endl;
                        // Give the loader time
                        for (int i = 0; i < 200 && s.getStatus() != SamplerPlugin::Status::ready; ++i)
                            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
                        std::cout << "  sampler: " << (s.getStatus() == SamplerPlugin::Status::ready ? "ready" : "FAILED to load") << std::endl;
                    },
                    [] (te::MidiList& seq)
                    {
                        seq.addNote (48, te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (0.25), 110, 0, nullptr);  // the hit (C3)
                        seq.addNote (69, te::BeatPosition::fromBeats (2.0), te::BeatDuration::fromBeats (1.5), 100, 0, nullptr);   // A4: an octave up
                        seq.addNote (64, te::BeatPosition::fromBeats (4.0), te::BeatDuration::fromBeats (1.5), 100, 0, nullptr);   // E4: a fifth up
                    }, 5.0);
        }
    }

    //==========================================================================
    //  Step 27: everything the tests print also goes to test-results.txt in the
    //  test folder (a Windows app has no console), with a summary at the end.
    //==========================================================================
    namespace
    {
        int lastFailures = 0;

        struct ResultsFile : std::streambuf
        {
            explicit ResultsFile (const juce::File& f)
                : file (f), screen (std::cout.rdbuf())
            {
                file.deleteFile();
                stream = std::make_unique<juce::FileOutputStream> (file);
                std::cout.rdbuf (this);
            }

            ~ResultsFile() override
            {
                flushLine();
                std::cout.rdbuf (screen);
                lastFailures = failed;
                const auto summary = "Summary: " + juce::String (passed) + " ok, " + juce::String (failed) + " FAILED";
                std::cout << summary << std::endl;
                if (stream != nullptr && stream->openedOk())
                {
                    stream->writeText (summary + "\n", false, false, nullptr);
                    stream->flush();
                }
            }

            int_type overflow (int_type c) override
            {
                if (c == traits_type::eof())
                    return traits_type::not_eof (c);
                screen->sputc ((char) c);
                if (c == '\n')
                    flushLine();
                else
                    line += (char) c;
                return c;
            }

            int sync() override
            {
                screen->pubsync();
                if (stream != nullptr) stream->flush();
                return 0;
            }

        private:
            void flushLine()
            {
                if (line.empty())
                    return;
                const juce::String text (juce::CharPointer_UTF8 (line.c_str()));
                if (text.contains ("FAILED"))                    ++failed;
                else if (text.contains ("  ok") || text.trimEnd().endsWith (" ok")) ++passed;
                if (stream != nullptr && stream->openedOk())
                {
                    stream->writeText (text + "\n", false, false, nullptr);
                    stream->flush();          // keep what was written if the app crashes
                }
                line.clear();
            }

            juce::File file;
            std::streambuf* screen;
            std::unique_ptr<juce::FileOutputStream> stream;
            std::string line;
            int passed = 0, failed = 0;
        };
    }

    int failuresInLastRun()     { return lastFailures; }

    bool runIfRequested (const juce::String& commandLine)
    {
        if (! commandLine.contains ("--render-test"))
            return false;

        const auto folder = juce::File (commandLine.fromFirstOccurrenceOf ("--render-test", false, false).trim().unquoted());
        folder.createDirectory();
        ResultsFile results (folder.getChildFile ("test-results.txt"));

        // To check that a failure shows as a red mark on GitHub: GPS_TEST_FAIL_ON_PURPOSE=1
        if (juce::SystemStats::getEnvironmentVariable ("GPS_TEST_FAIL_ON_PURPOSE", {}).isNotEmpty())
            std::cout << "deliberate failure (GPS_TEST_FAIL_ON_PURPOSE)  FAILED" << std::endl;

        te::Engine engine { "GospelProductionStudioRenderTest" };
        engine.getPluginManager().createBuiltInType<OrganPlugin>();
        engine.getPluginManager().createBuiltInType<GuitarPlugin>();
        engine.getPluginManager().createBuiltInType<PercussionPlugin>();
        engine.getPluginManager().createBuiltInType<SamplerPlugin>();
        engine.getPluginManager().createBuiltInType<StudioKeysPlugin>();
        engine.getPluginManager().createBuiltInType<MasterPlugin>();
        engine.getPluginManager().createBuiltInType<VocalTunePlugin>();
        engine.getPluginManager().createBuiltInType<DeEsserPlugin>();
        engine.getPluginManager().createBuiltInType<AnalyserPlugin>();

        // Organ: gospel chord voicing, with the Leslie switched to fast half way (mod wheel)
        render (engine, folder.getChildFile ("organ_full.wav"), OrganPlugin::xmlTypeName,
                [] (te::Plugin& p) { auto& o = dynamic_cast<OrganPlugin&> (p); o.applyPreset (OrganPlugin::presets()[0]); },
                [] (te::MidiList& seq)
                {
                    addChord (seq, { 44, 56, 60, 63, 67 }, 0.0, 4.0);
                    addChord (seq, { 49, 56, 60, 65, 68 }, 4.0, 4.0);
                    seq.addControllerEvent (te::BeatPosition::fromBeats (4.0), 1, 127 << 7, nullptr);
                    addChord (seq, { 51, 58, 62, 65, 70 }, 8.0, 4.0);
                }, 9.0);

        render (engine, folder.getChildFile ("organ_classic_perc.wav"), OrganPlugin::xmlTypeName,
                [] (te::Plugin& p) { auto& o = dynamic_cast<OrganPlugin&> (p); o.applyPreset (OrganPlugin::presets()[1]); },
                [] (te::MidiList& seq)
                {
                    const int line[] = { 68, 70, 72, 75, 77, 75, 72, 70 };
                    for (int i = 0; i < 8; ++i)
                        addChord (seq, { line[i] }, i * 0.5, 0.45);
                    addChord (seq, { 56, 60, 63 }, 0.0, 4.0);
                }, 4.0);

        // Sampler instruments (silent fallback if packs are not installed)
        const struct { const char* id; std::initializer_list<int> notes; } samplerTests[] = {
            { "grand",      { 44, 56, 60, 63, 67 } },
            { "upright",    { 44, 56, 60, 63, 67 } },
            { "bassFinger", { 32 } },
            { "drums",      { 36 } },
        };

        for (const auto& t : samplerTests)
        {
            const juce::String id (t.id);
            render (engine, folder.getChildFile ("sampler_" + id + ".wav"), SamplerPlugin::xmlTypeName,
                    [id] (te::Plugin& p) { dynamic_cast<SamplerPlugin&> (p).instrumentId = id; },
                    [id, notes = std::vector<int> (t.notes)] (te::MidiList& seq)
                    {
                        if (id == "drums")
                        {
                            Grooves::writeTo (seq, Grooves::all()[1], 0.0, 4.0, 4.0);
                            return;
                        }
                        for (int v : { 40, 80, 120 })
                        {
                            const auto start = (v / 40 - 1) * 2.0;
                            for (auto n : notes)
                                seq.addNote (n, te::BeatPosition::fromBeats (start), te::BeatDuration::fromBeats (1.5), v, 0, nullptr);
                        }
                        seq.addControllerEvent (te::BeatPosition::fromBeats (6.0), 64, 127 << 7, nullptr);
                        for (auto n : notes)
                            seq.addNote (n + 2, te::BeatPosition::fromBeats (6.0), te::BeatDuration::fromBeats (0.5), 90, 0, nullptr);
                        seq.addControllerEvent (te::BeatPosition::fromBeats (9.0), 64, 0, nullptr);
                    }, 8.0);
        }

        // Takes: a clip with two takes plays the one chosen
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            auto makeTone = [&] (const juce::File& f, double hz)
            {
                f.deleteFile();
                juce::WavAudioFormat wav;
                auto w = WavFiles::writer (f, 44100.0, 1, 16);
                if (w == nullptr) return;
                juce::AudioBuffer<float> b (1, 44100 * 2);
                for (int i = 0; i < b.getNumSamples(); ++i)
                    b.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / 44100.0));
                w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            };
            const auto take1 = folder.getChildFile ("take1.wav"), take2 = folder.getChildFile ("take2.wav");
            makeTone (take1, 440.0);
            makeTone (take2, 660.0);

            for (int which = 0; which < 2; ++which)
            {
                auto edit = te::createEmptyEdit (engine, folder.getChildFile ("takes.tracktionedit"));
                edit->ensureNumberOfAudioTracks (1);
                te::EditFileOperations (*edit).save (true, true, false);     // like a real song: the file exists
                auto* track = te::getAudioTracks (*edit)[0];
                const te::ClipPosition pos { { te::TimePosition(), te::TimePosition::fromSeconds (2.0) }, {} };
                auto clip = track->insertWaveClip ("Vocal", take1, pos, false);
                clip->addTake (take1);
                clip->addTake (take2);
                Takes::select (*clip, which);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
                const auto out = folder.getChildFile ("takes_" + juce::String (which + 1) + ".wav");
                out.deleteFile();
                const auto ok = te::Renderer::renderToFile (*edit, out, false);
                std::cout << out.getFileName() << (ok ? " rendered" : " FAILED") << "  (takes: " << Takes::count (*clip)
                          << ", current " << Takes::current (*clip) + 1 << ")" << std::endl;
            }
        }

        // Audio editing: split, normalise, pitch, speed, fade, comp
        {
            const auto take1 = folder.getChildFile ("take1.wav"), take2 = folder.getChildFile ("take2.wav");
            auto editTest = [&] (const juce::String& name, std::function<void (te::WaveAudioClip&, te::AudioTrack&)> change)
            {
                auto edit = te::createEmptyEdit (engine, folder.getChildFile (name + ".tracktionedit"));
                edit->ensureNumberOfAudioTracks (1);
                te::EditFileOperations (*edit).save (true, true, false);
                auto* track = te::getAudioTracks (*edit)[0];
                const te::ClipPosition pos { { te::TimePosition(), te::TimePosition::fromSeconds (2.0) }, {} };
                auto clip = track->insertWaveClip ("Vocal", take1, pos, false);
                change (*clip, *track);
                for (int i = 0; i < 100; ++i)      // give stretched clips time to prepare
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
                const auto out = folder.getChildFile ("edit_" + name + ".wav");
                out.deleteFile();
                const auto ok = te::Renderer::renderToFile (*edit, out, false);
                std::cout << out.getFileName() << (ok ? " rendered" : " FAILED") << "  clips " << track->getClips().size()
                          << "  length " << edit->getLength().inSeconds() << std::endl;
            };

            editTest ("split", [] (te::WaveAudioClip& c, te::AudioTrack&) { AudioEdit::splitAt (c, te::TimePosition::fromSeconds (1.0)); });
            editTest ("normalise", [] (te::WaveAudioClip& c, te::AudioTrack&)
            {
                std::cout << "  peak before " << AudioEdit::measurePeakDb (c) << " dB" << std::endl;
                AudioEdit::normalise (c, -1.0f);
                std::cout << "  clip gain " << c.getGainDB() << " dB" << std::endl;
            });
            editTest ("pitch12", [] (te::WaveAudioClip& c, te::AudioTrack&) { AudioEdit::setPitch (c, 12.0f); });
            editTest ("slow_keep", [] (te::WaveAudioClip& c, te::AudioTrack&) { AudioEdit::setSpeed (c, 0.5, true); });
            editTest ("fast_tape", [] (te::WaveAudioClip& c, te::AudioTrack&) { AudioEdit::setSpeed (c, 2.0, false); });
            editTest ("fadein", [] (te::WaveAudioClip& c, te::AudioTrack&) { c.setFadeIn (te::TimeDuration::fromSeconds (0.5)); });

            std::vector<Comp::Segment> segs { { 0.0, 2.0, 0 } };
            Comp::assign (segs, 1.0, 2.0, 1);
            const auto r = Comp::render ({ take1, take2 }, segs, 0.02, folder.getChildFile ("comp.wav"));
            std::cout << "comp.wav " << (r.wasOk() ? "rendered" : r.getErrorMessage()) << "  segments " << (int) segs.size() << std::endl;
        }

        // Mixer (Step 12): a short note dry, then with the shared reverb, then through EQ and compressor
        if (juce::SystemStats::getEnvironmentVariable ("GPS_BENCH", {}).isNotEmpty())
        {
            // How much computer power the organ and piano take (offline, one core),
            // and how clean the organ's overdrive is on a big chord (Leslie and vibrato off)
            struct Case { const char* type; const char* name; float drive; bool dry; };
            for (auto c : { Case { StudioKeysPlugin::xmlTypeName, "keys", 0, false },
                            Case { OrganPlugin::xmlTypeName, "organ", -1, false },
                            Case { OrganPlugin::xmlTypeName, "organ_dry_nodrive", 0.0f, true },
                            Case { OrganPlugin::xmlTypeName, "organ_dry_drive", -1, true },
                            Case { OrganPlugin::xmlTypeName, "organ_dry_drive_max", 1.0f, true } })
            {
                auto edit = te::createEmptyEdit (engine, folder.getChildFile ("bench.tracktionedit"));
                edit->ensureNumberOfAudioTracks (1);
                auto* track = te::getAudioTracks (*edit)[0];
                auto plugin = edit->getPluginCache().createNewPlugin (c.type, {});
                if (c.drive >= 0) plugin->state.setProperty ("drive", c.drive, nullptr);
                if (c.dry) { plugin->state.setProperty ("leslie", 0, nullptr); plugin->state.setProperty ("vibrato", 0, nullptr); plugin->state.setProperty ("click", 0.0f, nullptr); }
                track->pluginList.insertPlugin (plugin, 0, nullptr);
                auto clip = track->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (20.0)), nullptr);
                for (int b = 0; b < 36; b += 4)
                    addChord (clip->getSequence(), { 48, 55, 60, 64, 67, 72 }, b, 4.0);
                auto out = folder.getChildFile (juce::String ("bench_") + c.name + ".wav");
                out.deleteFile();
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                te::Renderer::renderToFile (*edit, out, false);
                const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
                std::cout << "bench " << c.name << ": 20 s of 6-note chords took " << (int) ms << " ms ("
                          << juce::String (20000.0 / ms, 1) << "x real time)" << std::endl;
            }
            return true;
        }

        if (auto v = juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_VOICE", {}); v.isNotEmpty())
        {
            vocalTuneTests (engine, folder, juce::File (v));
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_RELEASE", {}).isNotEmpty())
        {
            releaseTests (folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_ANALYSER", {}).isNotEmpty())
        {
            analyserTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_BUSES", {}).isNotEmpty())
        {
            busTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_LEARN", {}).isNotEmpty())
        {
            learnTests (folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_LIVELY", {}).isNotEmpty())
        {
            livelyBandTests (engine);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_SAFETY", {}).isNotEmpty())
        {
            songSafetyTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_DELAY", {}).isNotEmpty())
        {
            latencyTests();
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_FEEL", {}).isNotEmpty())
        {
            realFeelTests (folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_PERC", {}).isNotEmpty())
        {
            percussionPackTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_GUITAR", {}).isNotEmpty())
        {
            guitarPackTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_NAMES", {}).isNotEmpty())
        {
            trackNameTests (engine);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_FILES", {}).isNotEmpty())
        {
            songFilesTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_CLOSING", {}).isNotEmpty())
        {
            backgroundWorkTests (folder, true);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_TEMPO", {}).isNotEmpty())
        {
            tempoFollowTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_STEP22", {}).isNotEmpty())
        {
            step22Tests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_AFRICA", {}).isNotEmpty())
        {
            africanTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_BAND", {}).isNotEmpty())
        {
            backingTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_HARMONY", {}).isNotEmpty())
        {
            harmonyTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_TIMING", {}).isNotEmpty())
        {
            timingTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_DEESS", {}).isNotEmpty())
        {
            deEsserTests (engine, folder);
            return true;
        }

        if (juce::SystemStats::getEnvironmentVariable ("GPS_ONLY_LOOPS", {}).isNotEmpty())
        {
            loopTests (engine, folder);
            mySoundsTests (engine, folder);
            return true;
        }

        // Drum grooves (Step 17): every groove writes notes inside its bars
        {
            int bad = 0;
            for (const auto& g : Grooves::all())
            {
                te::MidiList seq;
                const auto bar = Grooves::numSteps (g) / (double) g.stepsPerBeat;
                Grooves::writeTo (seq, g, 0.0, bar * 2.0, bar, nullptr);
                bool ok = seq.getNotes().size() > 0;
                for (auto* n : seq.getNotes())
                    ok = ok && n->getStartBeat().inBeats() < bar * 2.0 - 1.0e-6;
                for (const auto& l : g.lanes)
                    ok = ok && (int) std::strlen (l.steps) == Grooves::numSteps (g);
                if (! ok) { ++bad; std::cout << "groove " << g.name << ": FAILED" << std::endl; }
            }
            std::cout << "grooves: " << Grooves::all().size() << " checked, " << (bad == 0 ? "ok" : "FAILED") << std::endl;

            render (engine, folder.getChildFile ("drums_praise_shuffle.wav"), SamplerPlugin::xmlTypeName,
                    [] (te::Plugin& p) { dynamic_cast<SamplerPlugin&> (p).instrumentId = "drums"; },
                    [] (te::MidiList& seq)
                    {
                        for (const auto& g : Grooves::all())
                            if (juce::String (g.name) == "Praise shuffle")
                                Grooves::writeTo (seq, g, 0.0, 8.0, 4.0, nullptr);
                    }, 6.0);
        }

        loopTests (engine, folder);
        mySoundsTests (engine, folder);
        deEsserTests (engine, folder);
        timingTests (engine, folder);
        harmonyTests (engine, folder);
        backingTests (engine, folder);
        africanTests (engine, folder);
        step22Tests (engine, folder);
        tempoFollowTests (engine, folder);
        backgroundWorkTests (folder, false);
        songFilesTests (engine, folder);
        trackNameTests (engine);
        guitarPackTests (engine, folder);
        percussionPackTests (engine, folder);
        realFeelTests (folder);
        latencyTests();
        songSafetyTests (engine, folder);
        livelyBandTests (engine);
        learnTests (folder);
        busTests (engine, folder);
        analyserTests (engine, folder);
        releaseTests (folder);

        mixerTests (engine, folder);
        automationTests (engine, folder);
        exportTests (engine, folder);
        masteringTests (engine, folder);
        if (auto v = juce::SystemStats::getEnvironmentVariable ("GPS_TEST_VOICE", {}); v.isNotEmpty())
            vocalTuneTests (engine, folder, juce::File (v));

        // Plugins (Step 11): only when a folder of test VST3 plugins is given
        if (auto dir = juce::SystemStats::getEnvironmentVariable ("GPS_TEST_VST3_DIR", {}); dir.isNotEmpty())
            pluginTests (engine, juce::File (dir), folder);

        return true;
    }
}
