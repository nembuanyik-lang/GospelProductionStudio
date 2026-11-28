#include "LatencyCheck.h"
#include "StudioEngine.h"
#include "StudioLookAndFeel.h"
#include "BackgroundWork.h"

namespace LatencyCheck
{
    //==========================================================================
    const std::vector<float>& burst()
    {
        static const std::vector<float> b = []
        {
            std::vector<float> v ((size_t) burstLength);
            juce::Random r (0x6a5c0de);                   // always the same noise
            for (int i = 0; i < burstLength; ++i)
            {
                // Short fades so the burst does not click
                const auto fade = juce::jmin (1.0f, (float) juce::jmin (i, burstLength - 1 - i) / 64.0f);
                v[(size_t) i] = (r.nextFloat() * 2.0f - 1.0f) * 0.35f * fade;
            }
            return v;
        }();
        return b;
    }

    std::vector<int> burstStarts (double sampleRate)
    {
        std::vector<int> s;
        for (int k = 0; k < numBursts; ++k)
            s.push_back ((int) (sampleRate * (0.25 + 0.6 * k)));
        return s;
    }

    int totalLength (double sampleRate)
    {
        return (int) (sampleRate * (0.25 + 0.6 * numBursts + 0.6));
    }

    //==========================================================================
    Result analyse (const std::vector<std::vector<float>>& recorded, double sampleRate, double reportedMs)
    {
        Result result;
        result.reportedMs = reportedMs;
        const auto& b = burst();
        double burstEnergy = 0.0;
        for (auto v : b) burstEnergy += (double) v * v;

        const auto starts = burstStarts (sampleRate);
        const int maxLag = (int) (sampleRate * 0.5);          // more than 500 ms would be a broken setup

        int bestChannel = -1;
        float bestStrength = 0.0f;
        std::vector<int> bestLags;
        bool clipped = false;

        for (int ch = 0; ch < (int) recorded.size(); ++ch)
        {
            const auto& x = recorded[(size_t) ch];
            if (x.empty())
                continue;
            for (auto v : x)
                if (std::abs (v) >= 0.99f) { clipped = true; break; }

            std::vector<int> lags;
            float strengthSum = 0.0f;
            for (auto start : starts)
            {
                double best = 0.0;
                int bestLag = -1;
                for (int lag = 0; lag <= maxLag; ++lag)
                {
                    const int from = start + lag;
                    if (from + burstLength > (int) x.size())
                        break;
                    double c = 0.0, e = 1.0e-12;
                    const float* p = x.data() + from;
                    for (int i = 0; i < burstLength; ++i)
                    {
                        c += (double) p[i] * b[(size_t) i];
                        e += (double) p[i] * p[i];
                    }
                    const auto norm = c / std::sqrt (e * burstEnergy);     // 1 = the burst exactly (any loudness)
                    if (norm > best) { best = norm; bestLag = lag; }
                }
                lags.push_back (bestLag);
                strengthSum += (float) best;
            }
            const auto strength = strengthSum / (float) starts.size();
            if (strength > bestStrength)
            {
                bestStrength = strength;
                bestChannel = ch;
                bestLags = lags;
            }
        }

        result.strength = bestStrength;
        result.inputChannel = bestChannel;
        result.clipped = clipped;

        if (bestChannel < 0 || bestStrength < 0.3f)
        {
            result.problem = "The test sound did not come back. Connect a cable from an output of your audio interface "
                             "to an input, turn that input's gain up to the middle, and try again.";
            return result;
        }

        const auto [lo, hi] = std::minmax_element (bestLags.begin(), bestLags.end());
        if (*lo < 0 || *hi - *lo > 2)
        {
            result.problem = "The four measurements did not agree (" + juce::String (*lo) + " to " + juce::String (*hi)
                           + " samples). Close other programs, plug in the charger, and try again.";
            return result;
        }

        std::vector<int> sorted (bestLags);
        std::sort (sorted.begin(), sorted.end());
        result.roundTripSamples = sorted[sorted.size() / 2];
        result.roundTripMs = 1000.0 * result.roundTripSamples / sampleRate;
        result.correctionMs = juce::jlimit (-200.0, 200.0, result.roundTripMs - reportedMs);
        result.ok = true;
        return result;
    }

    juce::String describe (const Result& r)
    {
        if (! r.ok)
            return r.problem;
        juce::String s;
        s << "Measured round trip: " << juce::String (r.roundTripMs, 1) << " ms (input " << (r.inputChannel + 1) << "). "
          << "Your driver reports " << juce::String (r.reportedMs, 1) << " ms. ";
        if (std::abs (r.correctionMs) < 0.5)
            s << "That is right: recordings already land on the beat.";
        else
            s << "So recordings land " << juce::String (std::abs (r.correctionMs), 1) << " ms "
              << (r.correctionMs > 0 ? "late" : "early") << " without a correction.";
        if (r.clipped)
            s << " (The input was overloaded: turn its gain down a little and measure again for the best result.)";
        return s;
    }

    //==========================================================================
    void Probe::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        if (device != nullptr)
            prepare (device->getCurrentSampleRate(), device->getActiveInputChannels().countNumberOfSetBits());
    }

    void Probe::prepare (double rate, int numInputs)
    {
        sampleRate = rate > 0.0 ? rate : 44100.0;
        starts = burstStarts (sampleRate);
        total = totalLength (sampleRate);
        recording.assign ((size_t) juce::jlimit (0, 16, numInputs), std::vector<float> ((size_t) total, 0.0f));
        position = 0;
        finished = false;
    }

    void Probe::audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                                  float* const* outputs, int numOutputs, int numSamples,
                                                  const juce::AudioIODeviceCallbackContext&)
    {
        const auto& b = burst();
        for (int i = 0; i < numSamples; ++i)
        {
            const int pos = position + i;
            float s = 0.0f;
            if (pos < total)
                for (auto start : starts)
                    if (pos >= start && pos < start + burstLength)
                        s = b[(size_t) (pos - start)];

            for (int ch = 0; ch < numOutputs; ++ch)
                if (outputs[ch] != nullptr)
                    outputs[ch][i] = s;

            if (pos < total)
                for (int ch = 0; ch < juce::jmin (numInputs, (int) recording.size()); ++ch)
                    if (inputs[ch] != nullptr)
                        recording[(size_t) ch][(size_t) pos] = inputs[ch][i];
        }
        position += numSamples;
        if (position >= total)
            finished = true;
    }

    //==========================================================================
    //  The panel
    //==========================================================================
    class Panel final : public juce::Component, private juce::Timer
    {
    public:
        explicit Panel (StudioEngine& e) : engine (e)
        {
            using namespace StudioColours;
            for (auto* l : { &howTo, &status, &current })
            {
                l->setColour (juce::Label::textColourId, text);
                l->setJustificationType (juce::Justification::topLeft);
                l->setMinimumHorizontalScale (1.0f);
                addAndMakeVisible (*l);
            }
            howTo.setFont (studioFont (14.0f));
            howTo.setText ("Recordings can land a few milliseconds after the beat, because every audio interface delays "
                           "the sound a little. This check measures it once and corrects every recording.\n\n"
                           "1. Turn your speakers and headphones down.\n"
                           "2. Connect a cable from Output 1 of your audio interface to Input 1 (a guitar or mic cable).\n"
                           "3. Turn Input 1's gain to about the middle.\n"
                           "4. Press Start. You will see four short bursts of noise go out and come back (3 seconds).",
                           juce::dontSendNotification);
            status.setFont (studioFont (14.0f, true));
            status.setColour (juce::Label::textColourId, gold);
            current.setFont (studioFont (13.0f));
            current.setColour (juce::Label::textColourId, textDim);

            for (auto* b : { &startButton, &useButton, &resetButton })
                addAndMakeVisible (*b);
            startButton.onClick = [this] { start(); };
            useButton.onClick   = [this] { setCorrection (lastResult.correctionMs, true); };
            resetButton.onClick = [this] { setCorrection (0.0, false); };
            useButton.setEnabled (false);

            byHand.setRange (-50.0, 50.0, 0.1);
            byHand.setTextValueSuffix (" ms");
            byHand.setValue (engine.getRecordingCorrectionMs(), juce::dontSendNotification);
            byHand.onValueChange = [this] { engine.setRecordingCorrectionMs (byHand.getValue()); showCurrent(); };
            addAndMakeVisible (byHand);
            byHandLabel.setText ("Or set it by hand (positive moves recordings earlier):", juce::dontSendNotification);
            byHandLabel.setFont (studioFont (13.0f));
            addAndMakeVisible (byHandLabel);
            showCurrent();
        }

        ~Panel() override
        {
            stopTimer();
            stopProbe();
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (24, 16);
            howTo.setBounds (r.removeFromTop (150));
            r.removeFromTop (6);
            auto row = r.removeFromTop (36);
            startButton.setBounds (row.removeFromLeft (200));
            row.removeFromLeft (10);
            useButton.setBounds (row.removeFromLeft (220));
            row.removeFromLeft (10);
            resetButton.setBounds (row.removeFromLeft (140));
            r.removeFromTop (10);
            status.setBounds (r.removeFromTop (70));
            current.setBounds (r.removeFromTop (24));
            r.removeFromTop (10);
            byHandLabel.setBounds (r.removeFromTop (22));
            byHand.setBounds (r.removeFromTop (30).withWidth (juce::jmin (r.getWidth(), 460)));
        }

    private:
        void start()
        {
            auto& dm = engine.getAudioDeviceManager();
            auto* device = dm.getCurrentAudioDevice();
            if (device == nullptr)
            {
                status.setText ("No audio device: open Audio Setup first.", juce::dontSendNotification);
                return;
            }
            if (device->getActiveInputChannels().isZero())
            {
                status.setText ("No inputs are switched on: open Audio Setup and switch on Input 1.", juce::dontSendNotification);
                return;
            }
            // Silence everything else (no feedback through the cable), then measure
            engine.prepareForLatencyCheck (true);
            reportedMs = engine.getReportedLatencyMs();
            probe = std::make_unique<Probe>();
            dm.addAudioCallback (probe.get());
            startButton.setEnabled (false);
            useButton.setEnabled (false);
            status.setText ("Measuring...", juce::dontSendNotification);
            startTimer (100);
        }

        void stopProbe()
        {
            if (probe != nullptr)
            {
                engine.getAudioDeviceManager().removeAudioCallback (probe.get());
                engine.prepareForLatencyCheck (false);
            }
        }

        void timerCallback() override
        {
            if (probe == nullptr || ! probe->isFinished())
                return;
            stopTimer();
            stopProbe();
            auto recorded = std::make_shared<std::vector<std::vector<float>>> (probe->getRecording());
            const auto rate = probe->getSampleRate();
            probe.reset();
            status.setText ("Working it out...", juce::dontSendNotification);

            juce::Component::SafePointer<Panel> safe (this);
            const auto reported = reportedMs;
            BackgroundWork::run ("Measuring the recording delay", [safe, recorded, rate, reported] (const std::atomic<bool>&)
            {
                auto r = analyse (*recorded, rate, reported);
                BackgroundWork::onMessageThread ([safe, r]
                {
                    if (auto* p = safe.getComponent())
                        p->show (r);
                });
            }, false);
        }

        void show (const Result& r)
        {
            lastResult = r;
            status.setText (describe (r), juce::dontSendNotification);
            startButton.setEnabled (true);
            startButton.setButtonText ("Measure again");
            useButton.setEnabled (r.ok);
            useButton.setButtonText (r.ok ? "Use " + juce::String (r.correctionMs, 1) + " ms correction" : "Use this correction");
        }

        void setCorrection (double ms, bool measured)
        {
            engine.setRecordingCorrectionMs (ms);
            byHand.setValue (ms, juce::dontSendNotification);
            if (measured)
                status.setText ("Done: every recording is now moved " + juce::String (std::abs (ms), 1) + " ms "
                                + (ms >= 0 ? "earlier" : "later") + ", so it lands on the beat. Remove the cable.",
                                juce::dontSendNotification);
            showCurrent();
        }

        void showCurrent()
        {
            const auto ms = engine.getRecordingCorrectionMs();
            current.setText ("Correction in use: " + (std::abs (ms) < 0.05 ? juce::String ("none") : juce::String (ms, 1) + " ms"),
                             juce::dontSendNotification);
        }

        StudioEngine& engine;
        juce::Label howTo, status, current, byHandLabel;
        juce::TextButton startButton { "Start the check" }, useButton { "Use this correction" }, resetButton { "No correction" };
        juce::Slider byHand { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<Probe> probe;
        Result lastResult;
        double reportedMs = 0.0;
    };

    std::unique_ptr<juce::Component> createPanel (StudioEngine& e)
    {
        return std::make_unique<Panel> (e);
    }
}
