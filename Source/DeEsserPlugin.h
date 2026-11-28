#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  De-esser (Step 12): softens harsh "s", "sh" and "t" sounds in a voice.
//
//  The voice is split at a frequency (about 6 kHz). Only when the top part gets
//  loud (a hissing "s") is that top part turned down, by at most "Range".
//  Vowels and the body of the voice are not touched.
//==============================================================================
class DeEsserPlugin final : public te::Plugin
{
public:
    explicit DeEsserPlugin (te::PluginCreationInfo);
    ~DeEsserPlugin() override;

    static const char* getPluginName()                           { return "De-esser"; }
    static const char* xmlTypeName;

    juce::String getName() const override                        { return getPluginName(); }
    juce::String getPluginType() override                        { return xmlTypeName; }
    juce::String getShortName (int) override                     { return "De-ess"; }
    juce::String getSelectableDescription() override             { return getName(); }
    bool takesMidiInput() override                               { return false; }
    bool takesAudioInput() override                              { return true; }
    bool producesAudioWhenNoAudioInput() override                { return false; }
    int  getNumOutputChannelsGivenInputs (int) override          { return 2; }
    BusLayout getBusses() const override                         { return BusLayout::singleStereoInOut(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    te::ParameterWithStateValue frequency, thresholdDb, rangeDb;

    // How much it is turning the "s" down right now (dB, for meters and tests)
    float getReductionDb() const                                 { return reduction.load(); }
    float getMaxReductionDb() const                              { return maxReduction.load(); }
    void  resetMaxReduction()                                    { maxReduction = 0.0f; }

private:
    void updateFilters();

    double sampleRate = 44100.0;
    float lastFreq = -1.0f;
    juce::dsp::LinkwitzRileyFilter<float> low, high;
    float env = 0.0f, gainSmoothed = 1.0f;
    std::atomic<float> reduction { 0.0f }, maxReduction { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEsserPlugin)
};
