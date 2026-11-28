#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Loudness.h"

namespace te = tracktion;

//==============================================================================
//  Mastering (Step 15): the last polish on the whole song, on the Master.
//
//      tone (low cut, Warmth, Clarity, Air)  ->  Glue (gentle compression)
//      ->  Width  ->  Push (how loud)  ->  limiter (peaks never pass -1 dB)
//
//  "Compare" plays the song without mastering at the same loudness, so you
//  hear the real difference and not just "louder".
//==============================================================================
class MasterPlugin final : public te::Plugin
{
public:
    explicit MasterPlugin (te::PluginCreationInfo);
    ~MasterPlugin() override;

    static const char* getPluginName()                           { return "Mastering"; }
    static const char* xmlTypeName;

    juce::String getName() const override                        { return getPluginName(); }
    juce::String getPluginType() override                        { return xmlTypeName; }
    juce::String getShortName (int) override                     { return "Master"; }
    juce::String getSelectableDescription() override             { return getName(); }
    bool takesMidiInput() override                               { return false; }
    bool takesAudioInput() override                              { return true; }
    bool producesAudioWhenNoAudioInput() override                { return false; }
    int  getNumOutputChannelsGivenInputs (int) override          { return 2; }
    BusLayout getBusses() const override                         { return BusLayout::singleStereoInOut(); }
    double getLatencySeconds() override                          { return lookahead / sampleRate; }
    double getTailLength() const override                        { return lookaheadSeconds + 0.3; }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const te::PluginRenderContext&) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    // Settings (saved with the song)
    juce::CachedValue<float> warmth, clarity, air;     // dB, -6..+6
    juce::CachedValue<bool>  lowCut;                   // removes rumble under 30 Hz
    juce::CachedValue<float> glue;                     // 0..1
    juce::CachedValue<float> width;                    // 0.5..1.5 (1 = as mixed)
    juce::CachedValue<float> push;                     // dB into the limiter, -12..+18
    juce::CachedValue<float> targetLufs;               // the loudness "Set loudness" aims for
    juce::CachedValue<juce::String> presetName;

    struct Preset { const char* name; float warmth, clarity, air, glue, width, target; const char* hint; };
    static const std::vector<Preset>& presets();
    void applyPreset (const Preset&);

    // Compare: hear the song without mastering, at the same loudness
    std::atomic<bool> compare { false };

    // Meters for the screen
    struct Meters
    {
        std::atomic<float> shortTermOut { -100.0f }, integratedOut { -100.0f }, shortTermIn { -100.0f };
        std::atomic<float> glueReductionDb { 0.0f }, limiterReductionDb { 0.0f }, peakOutDb { -100.0f };
    };
    Meters meters;
    void resetLoudness()          { resetRequested = true; }

    // Find (or add) the mastering on a song's Master
    static MasterPlugin* findIn (te::Edit&);
    static MasterPlugin* addTo (te::Edit&);
    static bool isActive (te::Edit&);        // present and switched on

    static inline std::function<void()> onOpenRequest;   // opens the Mastering page

    static constexpr double lookaheadSeconds = 0.005;
    static constexpr float ceilingDb = -1.0f;

private:
    void updateFilters();

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
        float process (int ch, float x) noexcept
        {
            const auto y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }
        void clear() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    };

    double sampleRate = 44100.0;
    Biquad lowCutFilter, lowShelf, presence, highShelf;
    std::atomic<bool> filtersDirty { true };
    std::atomic<float> aWarmth { 0 }, aClarity { 0 }, aAir { 0 }, aGlue { 0 }, aWidth { 1 }, aPush { 0 };
    std::atomic<bool> aLowCut { true };

    // Glue compressor
    float glueEnvDb = -100.0f, glueGainDb = 0.0f;

    // Lookahead limiter
    int lookahead = 220;
    std::vector<float> delayL, delayR, dryL, dryR, avgWindow;
    int delayPos = 0;
    float releaseGain = 1.0f, avgSum = 0.0f;
    std::vector<std::pair<int, float>> minQueue;      // monotonic queue (index, gain) as a ring
    int mqHead = 0, mqTail = 0, sampleIndex = 0;

    // Compare: input delayed by the same amount, matched in loudness
    float matchGain = 1.0f;

    LoudnessMeter loudIn, loudOut;
    std::atomic<bool> resetRequested { false };
    float peakHold = 0.0f;
    int meterCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterPlugin)
};
