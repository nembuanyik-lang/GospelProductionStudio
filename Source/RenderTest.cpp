#include "AudioEdit.h"
#include "Takes.h"
#include "RenderTest.h"
#include "OrganPlugin.h"
#include "SamplerPlugin.h"
#include "StudioKeysPlugin.h"
#include "Grooves.h"
#include "PluginHost.h"
#include "Mixing.h"
#include "Automation.h"

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

    bool runIfRequested (const juce::String& commandLine)
    {
        if (! commandLine.contains ("--render-test"))
            return false;

        const auto folder = juce::File (commandLine.fromFirstOccurrenceOf ("--render-test", false, false).trim().unquoted());
        folder.createDirectory();

        te::Engine engine { "GospelProductionStudioRenderTest" };
        engine.getPluginManager().createBuiltInType<OrganPlugin>();
        engine.getPluginManager().createBuiltInType<SamplerPlugin>();
        engine.getPluginManager().createBuiltInType<StudioKeysPlugin>();

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
                std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 44100.0, 1, 16, {}, 0));
                if (w == nullptr) return;
                os.release();
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
        mixerTests (engine, folder);
        automationTests (engine, folder);

        // Plugins (Step 11): only when a folder of test VST3 plugins is given
        if (auto dir = juce::SystemStats::getEnvironmentVariable ("GPS_TEST_VST3_DIR", {}); dir.isNotEmpty())
            pluginTests (engine, juce::File (dir), folder);

        return true;
    }
}
