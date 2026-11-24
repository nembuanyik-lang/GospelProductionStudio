#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <array>

namespace te = tracktion;

//==============================================================================
//  Gospel Organ: a tonewheel organ (B3 style) built from synthesis, no samples.
//  9 drawbars, percussion, key click, scanner vibrato/chorus, overdrive and a
//  two-rotor Leslie (horn and drum) with slow/fast speed ramps.
//  The mod wheel switches the Leslie between slow and fast.
//==============================================================================
class OrganPlugin final : public te::Plugin
{
public:
    explicit OrganPlugin (te::PluginCreationInfo);
    ~OrganPlugin() override;

    static const char* getPluginName()                           { return "Gospel Organ"; }
    static const char* xmlTypeName;

    juce::String getName() const override                        { return getPluginName(); }
    juce::String getPluginType() override                        { return xmlTypeName; }
    juce::String getShortName (int) override                     { return "Organ"; }
    juce::String getSelectableDescription() override             { return getName(); }

    bool isSynth() override                                      { return true; }
    bool takesMidiInput() override                               { return true; }
    bool takesAudioInput() override                              { return false; }
    bool producesAudioWhenNoAudioInput() override                { return true; }
    int  getNumOutputChannelsGivenInputs (int) override          { return 2; }
    BusLayout getBusses() const override                         { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override                        { return 1.0; }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const te::PluginRenderContext&) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    // ---- Settings (stored in the song) --------------------------------------
    std::array<juce::CachedValue<int>, 9> drawbars;        // 16', 5 1/3', 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1'
    juce::CachedValue<bool> percOn, percThird, percSoft, percFast;
    juce::CachedValue<int> vibrato;                         // 0 off, 1-3 V1-V3, 4-6 C1-C3
    juce::CachedValue<int> leslie;                          // 0 off, 1 slow, 2 fast
    juce::CachedValue<float> drive, click, volumeDb;
    juce::CachedValue<juce::String> presetName;

    static const char* drawbarNames[9];

    struct Preset
    {
        const char* name;
        const char* registration;       // "88 8000 000"
        bool perc, third, soft, fast;
        int vibrato, leslie;
        float drive;
    };
    static const std::vector<Preset>& presets();
    void applyPreset (const Preset&);
    juce::String getRegistration() const;

    bool isLeslieFast() const                                    { return currentFast.load(); }
    void setLeslie (int mode);

private:
    void refreshSettings();
    void renderBlock (float* outL, float* outR, int num);

    struct Key
    {
        bool active = false, down = false;
        int note = 0;
        float level = 0.0f;             // contact ramp
        float percEnv = 0.0f;
        float clickEnv = 0.0f;
        int wheels[9] {};
    };

    double sampleRate = 44100.0;
    std::array<Key, 16> keys;
    std::array<double, 128> phase {};
    std::array<double, 128> phaseInc {};
    std::vector<float> sine;
    int keysDown = 0;
    juce::Random noise;
    float clickFilter = 0.0f;

    // Vibrato scanner delay line
    std::vector<float> vibBuffer;
    int vibWrite = 0;
    double vibPhase = 0.0;

    // Leslie
    struct Biquad { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                    float process (float x) { auto y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    Biquad lowSplit, highSplit, lowSplit2, highSplit2;
    std::vector<float> hornBuffer, drumBuffer;
    int rotorWrite = 0;
    double hornAngle = 0.0, drumAngle = 0.0, hornSpeed = 0.8, drumSpeed = 0.67;

    // Settings for the audio thread
    std::array<std::atomic<float>, 9> barGain;
    std::atomic<bool> aPercOn { false }, aPercThird { true }, aPercSoft { false }, aPercFast { true };
    std::atomic<int> aVibrato { 6 }, aLeslie { 1 };
    std::atomic<float> aDrive { 0.2f }, aClick { 0.5f }, aGain { 1.0f };
    std::atomic<bool> currentFast { false };
    std::atomic<int> modWheelFast { -1 };     // -1 = not used, else 0/1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrganPlugin)
};
