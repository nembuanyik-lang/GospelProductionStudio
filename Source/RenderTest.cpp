#include "Takes.h"
#include "RenderTest.h"
#include "OrganPlugin.h"
#include "SamplerPlugin.h"
#include "StudioKeysPlugin.h"
#include "Grooves.h"

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

        return true;
    }
}
