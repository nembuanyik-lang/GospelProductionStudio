#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Analysis.h"
#include "Loudness.h"

namespace te = tracktion;

//==============================================================================
//  The analyser and reference player (plan Step 16), last on the Master.
//
//  While the song plays it passes the sound to the Analyser page (spectrum,
//  stereo, loudness). It can also play a reference song you like instead of
//  your mix (A/B), turned down or up so both are equally loud: louder always
//  sounds "better", so a fair comparison must be at the same loudness.
//  It does nothing when exporting: the reference is never in your files.
//==============================================================================
class AnalyserPlugin final : public te::Plugin
{
public:
    explicit AnalyserPlugin (te::PluginCreationInfo);
    ~AnalyserPlugin() override;

    static const char* getPluginName()                           { return "Analyser"; }
    static const char* xmlTypeName;

    juce::String getName() const override                        { return getPluginName(); }
    juce::String getPluginType() override                        { return xmlTypeName; }
    juce::String getShortName (int) override                     { return "Analyser"; }
    juce::String getSelectableDescription() override             { return getName(); }
    bool takesMidiInput() override                               { return false; }
    bool takesAudioInput() override                              { return true; }
    bool producesAudioWhenNoAudioInput() override                { return false; }
    int  getNumOutputChannelsGivenInputs (int) override          { return 2; }
    BusLayout getBusses() const override                         { return BusLayout::singleStereoInOut(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;

    // Saved with the song
    juce::CachedValue<juce::String> referencePath;
    juce::CachedValue<float> referenceOffset;          // seconds into the reference at the song's start

    // Reference: load it (decoded in the background) and measure it
    void loadReference (const juce::File&);
    void clearReference();
    juce::File getReferenceFile() const                          { return juce::File (referencePath.get()); }
    bool isReferenceReady() const                                { return refReady.load(); }
    bool isReferenceLoading() const                              { return refLoading.load(); }
    double getReferenceLength() const                            { return refLengthSeconds.load(); }
    const Analysis::FileAnalysis& getReferenceAnalysis() const   { return refAnalysis; }   // message thread

    // A/B
    std::atomic<bool> listenToReference { false };
    std::atomic<float> referenceGainDb { 0.0f };       // set by the page to match loudness

    // Live sound for the screen (what you hear), read on the message thread
    int readLive (float* left, float* right, int maxSamples);
    float getLiveIntegratedLufs() const                          { return mixIntegrated.load(); }
    float getLiveShortTermLufs() const                           { return mixShortTerm.load(); }
    double getSampleRate() const                                 { return sampleRate; }
    int getVersion() const                                       { return version.load(); }   // changes when the reference does

    static AnalyserPlugin* findIn (te::Edit&);
    static AnalyserPlugin* addTo (te::Edit&);
    static inline std::function<void()> onOpenRequest;          // opens the Analyser page

private:
    struct RefAudio
    {
        std::vector<juce::int16> left, right;
        double sampleRate = 44100.0;
        juce::int64 length = 0;
    };

    double sampleRate = 44100.0;
    juce::SpinLock refLock;
    std::shared_ptr<const RefAudio> ref;
    std::atomic<bool> refReady { false }, refLoading { false };
    std::atomic<double> refLengthSeconds { 0.0 };
    Analysis::FileAnalysis refAnalysis;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    int loadGeneration = 0;
    std::atomic<int> version { 0 };

    LoudnessMeter meter;
    std::atomic<float> mixIntegrated { -100.0f }, mixShortTerm { -100.0f };
    bool wasPlaying = false;
    float smoothGain = 1.0f;

    juce::AbstractFifo fifo { 1 << 16 };
    std::vector<float> fifoL, fifoR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyserPlugin)
};
