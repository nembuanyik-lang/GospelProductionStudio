#include "Stems.h"
#include "WavFiles.h"
#include "Mixing.h"
#include "AudioEdit.h"
#include "StudioLookAndFeel.h"
#include "BackgroundWork.h"

namespace Stems
{
    namespace
    {
        const juce::Identifier ofId ("gpsStemOf"), partId ("gpsStemPart");

        juce::AudioFormatManager& formats()
        {
            static juce::AudioFormatManager fm;
            static bool done = false;
            if (! done) { fm.registerBasicFormats(); done = true; }
            return fm;
        }

        float median (float* v, int n)
        {
            std::nth_element (v, v + n / 2, v + n);
            return v[n / 2];
        }
    }

    juce::String stemName (int s)
    {
        static const char* names[] = { "Voice", "Drums", "Bass", "Other" };
        return names[juce::jlimit (0, numStems - 1, s)];
    }

    juce::File folderFor (const juce::File& source)
    {
        auto dir = source.getParentDirectory();
        for (auto* name : { "Timed", "Following", "Harmony", "Stems" })
            if (dir.getFileName() == name) dir = dir.getParentDirectory();
        return dir.getChildFile ("Stems");
    }

    //==========================================================================
    Result separate (const juce::File& source, const juce::File& folder, const std::function<bool (float)>& progress)
    {
        Result result;
        std::unique_ptr<juce::AudioFormatReader> rd (formats().createReaderFor (source));
        if (rd == nullptr) { result.problem = "The recording could not be read."; return result; }
        const double sr = rd->sampleRate;
        if (rd->lengthInSamples > (juce::int64) (sr * 60.0 * 20.0)) { result.problem = "The recording is longer than 20 minutes."; return result; }
        const int n = (int) rd->lengthInSamples;
        if (n < 8192) { result.problem = "The recording is too short."; return result; }

        constexpr int order = 12, N = 1 << order, hop = N / 4, bins = N / 2 + 1;
        constexpr int medianFrames = 17, medianBins = 17;

        // Stereo input, padded by one window at both ends
        const int padded = n + 2 * N;
        juce::AudioBuffer<float> in (2, padded);
        in.clear();
        {
            juce::AudioBuffer<float> tmp ((int) juce::jlimit (1u, 2u, rd->numChannels), n);
            rd->read (&tmp, 0, n, 0, true, true);
            for (int c = 0; c < 2; ++c)
                in.copyFrom (c, N, tmp, juce::jmin (c, tmp.getNumChannels() - 1), 0, n);
        }

        juce::dsp::FFT fft (order);
        std::vector<float> window ((size_t) N);
        for (int i = 0; i < N; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);
        const int frames = (padded - N) / hop + 1;

        // ---- Pass 1: how loud each frequency is in each slice (mono) ----------
        std::vector<float> mag ((size_t) frames * bins);
        std::vector<float> buf ((size_t) N * 2);
        for (int f = 0; f < frames; ++f)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            const float* L = in.getReadPointer (0, f * hop);
            const float* R = in.getReadPointer (1, f * hop);
            for (int i = 0; i < N; ++i) buf[(size_t) i] = 0.5f * (L[i] + R[i]) * window[(size_t) i];
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);
            std::copy (buf.begin(), buf.begin() + bins, mag.begin() + (long) f * bins);
            if (progress && f % 256 == 0 && ! progress (0.3f * (float) f / (float) frames)) return result;
        }

        // ---- The masks: per frequency, how much goes to each stem ----------------
        std::vector<float> bassWeight ((size_t) bins), voiceBand ((size_t) bins);
        for (int k = 0; k < bins; ++k)
        {
            const double f = k * sr / N;
            bassWeight[(size_t) k] = f <= 150.0 ? 1.0f : f >= 300.0 ? 0.0f : (float) (0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * (f - 150.0) / 150.0));
            float v = f < 120.0 ? 0.0f : f < 250.0 ? (float) ((f - 120.0) / 130.0) : 1.0f;
            if (f > 6000.0) v = (float) juce::jmax (0.3, 1.0 - (f - 6000.0) / 4000.0 * 0.7);
            voiceBand[(size_t) k] = v;
        }

        // ---- Pass 2: split every slice and put the four stems back together -------
        folder.createDirectory();
        const auto base = source.getFileNameWithoutExtension();
        std::array<std::unique_ptr<juce::AudioFormatWriter>, numStems> writers;
        std::array<juce::File, numStems> temps;
        for (int s = 0; s < numStems; ++s)
        {
            result.files[(size_t) s] = folder.getNonexistentChildFile (base + " - " + stemName (s), ".wav", false);
            temps[(size_t) s] = result.files[(size_t) s].withFileExtension (".part");
            temps[(size_t) s].deleteFile();
            juce::WavAudioFormat wav;
            writers[(size_t) s].reset (WavFiles::writer (temps[(size_t) s], sr, (int) (2), 24).release());
            if (writers[(size_t) s] == nullptr) { result.problem = "The stems could not be written (is the disk full?)."; return result; }
        }

        // Overlap-add: Hann in and out with a quarter-window hop adds up to 1.5
        const float olaScale = 1.0f / 1.5f;
        std::array<juce::AudioBuffer<float>, numStems> acc;
        for (auto& a : acc) { a.setSize (2, N); a.clear(); }
        std::vector<float> spec[2] { std::vector<float> ((size_t) N * 2), std::vector<float> ((size_t) N * 2) };
        std::vector<float> stemSpec ((size_t) N * 2);
        std::vector<float> col (medianFrames), row (medianBins);
        std::vector<float> masks ((size_t) numStems * bins);
        int written = 0;                       // samples of the padded timeline already written (minus the padding)

        for (int f = 0; f < frames; ++f)
        {
            for (int c = 0; c < 2; ++c)
            {
                auto& sp = spec[c];
                std::fill (sp.begin(), sp.end(), 0.0f);
                const float* x = in.getReadPointer (c, f * hop);
                for (int i = 0; i < N; ++i) sp[(size_t) i] = x[i] * window[(size_t) i];
                fft.performRealOnlyForwardTransform (sp.data(), true);
            }

            const float* m = mag.data() + (long) f * bins;
            for (int k = 0; k < bins; ++k)
            {
                for (int j = 0; j < medianFrames; ++j)
                    col[(size_t) j] = mag[(size_t) juce::jlimit (0, frames - 1, f + j - medianFrames / 2) * bins + (size_t) k];
                int cnt = 0;
                for (int j = 0; j < medianBins; ++j)
                {
                    const int kk = k + j - medianBins / 2;
                    if (kk >= 0 && kk < bins) row[(size_t) cnt++] = m[kk];
                }
                const float H = median (col.data(), medianFrames), P = median (row.data(), cnt);
                const float wp = (P * P) / (H * H + P * P + 1.0e-12f);
                const float wh = 1.0f - wp;

                // How "in the middle" this frequency is: 1 when left and right are the same
                const float lr = spec[0][(size_t) (2 * k)], li = spec[0][(size_t) (2 * k + 1)];
                const float rr = spec[1][(size_t) (2 * k)], ri = spec[1][(size_t) (2 * k + 1)];
                const float sumR = lr + rr, sumI = li + ri;
                const float centre = (sumR * sumR + sumI * sumI) / (2.0f * (lr * lr + li * li + rr * rr + ri * ri) + 1.0e-12f);
                const float c = juce::jlimit (0.0f, 1.0f, (centre - 0.75f) / 0.2f);

                const float b = bassWeight[(size_t) k];
                const float v = c * voiceBand[(size_t) k];
                masks[(size_t) (drums * bins + k)] = wp;
                masks[(size_t) (bass * bins + k)]  = wh * b;
                masks[(size_t) (voice * bins + k)] = wh * (1.0f - b) * v;
                masks[(size_t) (other * bins + k)] = wh * (1.0f - b) * (1.0f - v);
            }

            for (int s = 0; s < numStems; ++s)
                for (int c = 0; c < 2; ++c)
                {
                    const auto& sp = spec[c];
                    for (int k = 0; k < bins; ++k)
                    {
                        const float g = masks[(size_t) (s * bins + k)];
                        stemSpec[(size_t) (2 * k)] = sp[(size_t) (2 * k)] * g;
                        stemSpec[(size_t) (2 * k + 1)] = sp[(size_t) (2 * k + 1)] * g;
                    }
                    std::fill (stemSpec.begin() + 2 * bins, stemSpec.end(), 0.0f);
                    fft.performRealOnlyInverseTransform (stemSpec.data());
                    float* a = acc[(size_t) s].getWritePointer (c);
                    for (int i = 0; i < N; ++i) a[i] += stemSpec[(size_t) i] * window[(size_t) i] * olaScale;
                }

            // The first 'hop' samples are complete now: write them (skipping the padding) and move on
            const int pos = f * hop;                   // padded timeline position of acc[0]
            const int from = juce::jmax (pos, N), to = juce::jmin (pos + hop, N + n);
            if (to > from)
                for (int s = 0; s < numStems; ++s)
                {
                    const float* chans[2] = { acc[(size_t) s].getReadPointer (0, from - pos), acc[(size_t) s].getReadPointer (1, from - pos) };
                    writers[(size_t) s]->writeFromFloatArrays (chans, 2, to - from);
                }
            written += juce::jmax (0, to - from);
            for (auto& a : acc)
                for (int c = 0; c < 2; ++c)
                {
                    float* d = a.getWritePointer (c);
                    std::memmove (d, d + hop, sizeof (float) * (size_t) (N - hop));
                    std::fill (d + N - hop, d + N, 0.0f);
                }

            if (progress && f % 64 == 0 && ! progress (0.3f + 0.7f * (float) f / (float) frames))
            {
                for (auto& w : writers) w.reset();
                for (auto& t : temps) t.deleteFile();
                return result;
            }
        }

        for (auto& w : writers) w.reset();
        for (int s = 0; s < numStems; ++s)
        {
            result.files[(size_t) s].deleteFile();
            if (! temps[(size_t) s].moveFileTo (result.files[(size_t) s])) { result.problem = "The stems could not be saved."; return result; }
        }
        juce::ignoreUnused (written);
        if (progress) progress (1.0f);
        result.ok = true;
        return result;
    }

    //==========================================================================
    juce::Array<te::AudioTrack*> findStemTracks (te::WaveAudioClip& clip)
    {
        juce::Array<te::AudioTrack*> out;
        const auto id = clip.itemID.toString();
        for (auto* t : te::getAudioTracks (clip.edit))
            if (t->state[ofId].toString() == id) out.add (t);
        return out;
    }

    juce::Array<te::AudioTrack*> placeStems (te::WaveAudioClip& clip, const Result& r, bool muteOriginal)
    {
        juce::Array<te::AudioTrack*> made;
        auto* lead = dynamic_cast<te::AudioTrack*> (clip.getTrack());
        if (lead == nullptr || ! r.ok) return made;
        auto& edit = clip.edit;
        auto* um = &edit.getUndoManager();
        const auto existing = findStemTracks (clip);
        te::Track* after = existing.isEmpty() ? (te::Track*) lead : (te::Track*) existing.getLast();
        static const juce::uint32 colours[] = { 0xff4caf7d, 0xffe07a4f, 0xff5b8def, 0xffb07ee0 };
        const auto pos = clip.getPosition();

        for (int s = 0; s < numStems; ++s)
        {
            te::AudioTrack* track = nullptr;
            for (auto* t : existing) if ((int) t->state[partId] == s) track = t;
            if (track == nullptr)
            {
                auto t = edit.insertNewAudioTrack (te::TrackInsertPoint (*after, false), nullptr, true);
                if (t == nullptr) continue;
                track = t.get();
                track->state.setProperty (ofId, clip.itemID.toString(), um);
                track->state.setProperty (partId, s, um);
                track->setName (stemName (s) + " (stem)");
                track->setColour (juce::Colour (colours[s]));
                after = track;
            }
            else
            {
                const auto old = track->getClips();
                for (auto* c : old) c->removeFromParent();
            }
            if (auto c = track->insertWaveClip (stemName (s), r.files[(size_t) s], { { pos.getStart(), pos.getEnd() }, pos.getOffset() }, false))
                AudioEdit::prepare (*c);
            made.add (track);
        }
        if (muteOriginal)
            lead->setMute (true);
        return made;
    }

    //==========================================================================
    //  The panel
    //==========================================================================
    namespace
    {
        struct JobState
        {
            std::atomic<float> progress { 0.0f };
            std::atomic<bool> cancel { false }, done { false };
        };

        class Panel final : public juce::Component, private juce::Timer
        {
        public:
            Panel (te::WaveAudioClip& c, std::function<void()> changed)
                : keep (&c), editRef (c.edit), clipID (c.itemID), source (c.getOriginalFile()), onChanged (std::move (changed))
            {
                using namespace StudioColours;
                title.setText ("Split into stems", juce::dontSendNotification);
                title.setFont (studioFont (17.0f, true));
                title.setColour (juce::Label::textColourId, text);
                addAndMakeVisible (title);
                for (auto* l : { &info, &summary })
                {
                    l->setColour (juce::Label::textColourId, textDim);
                    l->setFont (studioFont (12.5f));
                    l->setJustificationType (juce::Justification::topLeft);
                    addAndMakeVisible (*l);
                }
                info.setText ("Makes four tracks from this recording: Voice, Drums, Bass and Other (keys, guitars, choir). "
                              "Together they sound exactly like the original.\n\n"
                              "Best on a stereo song with the lead voice in the middle. It is made on this computer, without internet, "
                              "so it is not as clean as online AI separators: some of each part is heard in the others. "
                              "Good for learning parts, practising (mute the Voice and sing along) and quick remixes.",
                              juce::dontSendNotification);
                muteToggle.setToggleState (true, juce::dontSendNotification);
                addAndMakeVisible (muteToggle);
                goButton.onClick = [this] { go(); };
                addAndMakeVisible (goButton);
                if (std::abs (c.getSpeedRatio() - 1.0) > 0.001 || c.getAutoTempo())
                {
                    goButton.setEnabled (false);
                    summary.setText ("This clip plays faster or slower than it was recorded: split a clip at its own speed.", juce::dontSendNotification);
                }
                else if (! findStemTracks (c).isEmpty())
                    summary.setText ("This clip was split before. Splitting again replaces those stems.", juce::dontSendNotification);
                setSize (520, 290);
            }

            ~Panel() override
            {
                stopTimer();
                if (job != nullptr && ! job->done) job->cancel = true;
            }

            void go()
            {
                if (job != nullptr && ! job->done) { job->cancel = true; return; }
                job = std::make_shared<JobState>();
                const auto state = job;
                const auto src = source;
                const auto safeEdit = editRef;
                const auto id = clipID;
                const bool mute = muteToggle.getToggleState();
                const auto changed = onChanged;
                juce::Component::SafePointer<Panel> safe (this);
                BackgroundWork::run ("Splitting into stems", [=] (const std::atomic<bool>& stopRequested)
                {
                    auto r = std::make_shared<Result> (separate (src, folderFor (src), [state, &stopRequested] (float p)
                    {
                        state->progress = p;
                        if (stopRequested.load()) state->cancel = true;
                        return ! state->cancel.load();
                    }));
                    const bool cancelled = state->cancel.load();
                    BackgroundWork::onMessageThread ([=]
                    {
                        state->done = true;
                        juce::String message;
                        if (cancelled) message = "Stopped. Nothing was changed.";
                        else if (! r->ok) message = r->problem;
                        else if (auto* ed = safeEdit.get())
                        {
                            if (auto* c = dynamic_cast<te::WaveAudioClip*> (te::findClipForID (*ed, id)))
                            {
                                ed->getUndoManager().beginNewTransaction ("Split into stems");
                                placeStems (*c, *r, mute);
                                message = "Done: Voice, Drums, Bass and Other are on their own tracks under the original"
                                          + juce::String (mute ? " (now muted)." : ".") + " Use Solo and Mute to hear each part.";
                                if (changed) changed();
                            }
                            else message = "The clip was deleted.";
                        }
                        if (auto* p = safe.getComponent())
                        {
                            p->summary.setText (message, juce::dontSendNotification);
                            p->stopTimer();
                            p->goButton.setButtonText ("Split into stems");
                        }
                    });
                });
                goButton.setButtonText ("Stop");
                startTimer (250);
            }

            void timerCallback() override
            {
                if (job != nullptr && ! job->done)
                    summary.setText ("Splitting... " + juce::String (juce::roundToInt (job->progress * 100.0f)) + " %", juce::dontSendNotification);
            }

            void paint (juce::Graphics& g) override   { g.fillAll (StudioColours::card); }

            void resized() override
            {
                auto r = getLocalBounds().reduced (14, 10);
                title.setBounds (r.removeFromTop (26));
                info.setBounds (r.removeFromTop (110));
                muteToggle.setBounds (r.removeFromTop (24));
                auto buttons = r.removeFromBottom (30);
                goButton.setBounds (buttons.removeFromRight (170));
                r.removeFromBottom (6);
                summary.setBounds (r);
            }

        private:
            te::Clip::Ptr keep;
            te::SafeSelectable<te::Edit> editRef;
            te::EditItemID clipID;
            juce::File source;
            std::function<void()> onChanged;
            std::shared_ptr<JobState> job;
            juce::Label title, info, summary;
            juce::ToggleButton muteToggle { "Mute the original track (the stems play instead)" };
            juce::TextButton goButton { "Split into stems" };
        };
    }

    std::unique_ptr<juce::Component> createPanel (te::WaveAudioClip& clip, std::function<void()> onChanged)
    {
        return std::make_unique<Panel> (clip, std::move (onChanged));
    }
}
