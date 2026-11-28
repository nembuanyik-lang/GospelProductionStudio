#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Vocal tuning (Step 16): gentle pitch correction for a sung part.
//
//  It listens to the voice (one note at a time), finds the nearest note of the
//  song's key and moves the voice towards it: "Strength" says how far, "Speed"
//  how quickly (slow keeps natural slides and vibrato; fast is the robotic effect).
//  The voice keeps its own character (formants are kept), so small corrections
//  stay invisible.
//==============================================================================
class VocalTunePlugin final : public te::Plugin,
                              private juce::Timer
{
public:
    explicit VocalTunePlugin (te::PluginCreationInfo);
    ~VocalTunePlugin() override;

    static const char* getPluginName()                           { return "Vocal tuning"; }
    static const char* xmlTypeName;

    juce::String getName() const override                        { return getPluginName(); }
    juce::String getPluginType() override                        { return xmlTypeName; }
    juce::String getShortName (int) override                     { return "Tune"; }
    juce::String getSelectableDescription() override             { return getName(); }
    bool takesMidiInput() override                               { return false; }
    bool takesAudioInput() override                              { return true; }
    bool producesAudioWhenNoAudioInput() override                { return false; }
    int  getNumOutputChannelsGivenInputs (int) override          { return 2; }
    BusLayout getBusses() const override                         { return BusLayout::singleStereoInOut(); }
    double getLatencySeconds() override;
    double getTailLength() const override                        { return 0.2; }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const te::PluginRenderContext&) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    std::unique_ptr<EditorComponent> createEditor() override;

    // Settings (saved with the song)
    juce::CachedValue<float> strength;          // 0..1
    juce::CachedValue<float> speedMs;           // 5 (fast, robotic) .. 300 (very natural)
    juce::CachedValue<int>   scaleMode;         // 0 the song's key, 1 key + blue notes, 2 all notes
    juce::CachedValue<int>   keyOverride;       // -1 follow the song, else 0..11 (C..B) with...
    juce::CachedValue<bool>  minorOverride;     // ...this mode
    juce::CachedValue<int>   range;             // 0 low voice (men), 1 high voice (women, children)

    // Notes the voice may land on (12 flags, C..B), from the song's key or the override
    std::array<bool, 12> allowedNotes() const;
    juce::String describeKey() const;           // "Ab major"

    // The last few seconds of what was sung and what is heard, for the screen
    struct History
    {
        static constexpr int size = 600;
        std::array<std::atomic<float>, size> sung {}, heard {};   // MIDI note numbers, 0 = no pitch
        std::atomic<int> write { 0 };
    };
    History history;
    std::atomic<float> currentSung { 0 }, currentHeard { 0 };

    static constexpr int hopsPerSecond (double sr)   { return (int) (sr / 512.0); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    double sampleRate = 44100.0;
    std::atomic<float> aStrength { 0.8f }, aSpeed { 60.0f };
    std::atomic<int> aRange { 1 };
    std::atomic<std::uint16_t> aAllowed { 0xfff };
    void timerCallback() override       { refreshSettings(); }    // follows the song's key

public:
    void refreshSettings();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalTunePlugin)
};
