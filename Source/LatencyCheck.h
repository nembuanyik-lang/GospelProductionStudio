#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <vector>

class StudioEngine;

//==============================================================================
//  Recording delay check (plan Step 28)
//
//  Every audio interface delays sound a little on the way out and back in.
//  Tracktion already moves recordings earlier by the delay the driver reports,
//  but drivers are often a few milliseconds wrong, so recordings land late.
//
//  With a cable from an output to an input, the Probe plays a short noise
//  burst four times and records what comes back. analyse() finds each burst
//  in the recording: the true round trip. The correction is the part the
//  driver does not report; it is applied to every input (te::WaveInputDevice
//  record adjustment, saved by Tracktion per device).
//==============================================================================
namespace LatencyCheck
{
    constexpr int burstLength = 4096;
    constexpr int numBursts = 4;

    struct Result
    {
        bool ok = false;
        double roundTripMs = 0.0;       // measured, output to input
        int roundTripSamples = 0;
        double reportedMs = 0.0;        // what the driver says (already corrected by the engine)
        double correctionMs = 0.0;      // what is missing: roundTrip - reported
        float strength = 0.0f;          // 0..1, how clearly the bursts came back
        int inputChannel = -1;
        bool clipped = false;
        juce::String problem;           // when not ok: what to do
    };

    // The test sound: a fixed noise burst (the same every time)
    const std::vector<float>& burst();
    // Where each burst starts, in samples
    std::vector<int> burstStarts (double sampleRate);
    int totalLength (double sampleRate);

    // Finds the bursts in what each input recorded. Slow (about a second): not on the audio thread.
    Result analyse (const std::vector<std::vector<float>>& recorded, double sampleRate, double reportedMs);

    // Plays the bursts on every output and records every input
    class Probe final : public juce::AudioIODeviceCallback
    {
    public:
        bool isFinished() const                 { return finished.load(); }
        double getSampleRate() const            { return sampleRate; }
        const std::vector<std::vector<float>>& getRecording() const   { return recording; }

        // Also used by the tests to stand in for a real device
        void audioDeviceAboutToStart (juce::AudioIODevice*) override;
        void prepare (double sampleRate, int numInputs);
        void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                               float* const* outputs, int numOutputs, int numSamples,
                                               const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceStopped() override      {}

    private:
        double sampleRate = 44100.0;
        std::vector<std::vector<float>> recording;
        std::vector<int> starts;
        int total = 0, position = 0;
        std::atomic<bool> finished { false };
    };

    juce::String describe (const Result&);

    // The settings panel: how to connect the cable, Start, the result, and setting it by hand
    std::unique_ptr<juce::Component> createPanel (StudioEngine&);
}
