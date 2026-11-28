#pragma once

#include "AnalyserPlugin.h"

//==============================================================================
//  The Analyser page (plan Step 16): spectrum, tone balance, stereo and
//  loudness of your mix, and comparison with a reference song (A/B at the
//  same loudness).
//==============================================================================
class AnalyserPage final : public juce::Component,
                           private juce::Timer
{
public:
    explicit AnalyserPage (te::Edit&);
    ~AnalyserPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void chooseReference();
    void measureWholeMix();
    void updateGain();
    void refreshTexts();
    AnalyserPlugin* plugin() const     { return AnalyserPlugin::findIn (edit); }

    void drawSpectrum (juce::Graphics&, juce::Rectangle<float>);
    void drawBalance (juce::Graphics&, juce::Rectangle<float>);
    void drawStereo (juce::Graphics&, juce::Rectangle<float>);

    te::Edit& edit;
    te::Plugin::Ptr keep;

    juce::TextButton playButton { "Play" }, mixButton { "A: Your mix" }, refButton { "B: Reference" },
                     chooseButton { "Choose a reference song..." }, clearButton { "Remove" },
                     measureButton { "Measure my whole mix" };
    juce::Slider offset;
    juce::Label refName, gainNote, offsetLabel, measureStatus, differences, help;

    Analysis::Averager live { 44100.0 };
    Analysis::Spectrum liveSpectrum;
    Analysis::FileAnalysis wholeMix;
    std::vector<float> scratchL, scratchR;
    double liveRate = 0.0;
    int seenVersion = -1;

    juce::Rectangle<int> spectrumArea, balanceArea, stereoArea, loudArea, topArea;

    // Measuring the whole song
    std::shared_ptr<te::EditRenderer::Handle> renderHandle;
    juce::File tempFile;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyserPage)
};
