#pragma once

#include "MasterPlugin.h"

//==============================================================================
//  The Mastering page (Step 15): presets, tone, glue, width, loudness,
//  "Compare" (before/after at the same loudness) and meters.
//  "Set loudness for the whole song" plays the song through in the background
//  and sets Push so the song lands on the chosen loudness.
//==============================================================================
class MasteringPage final : public juce::Component,
                            private juce::Timer
{
public:
    explicit MasteringPage (te::Edit&);
    ~MasteringPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshFromPlugin();
    void setOn (bool);
    void startAutoLoudness();
    void autoStep();
    void finishAuto (const juce::String& message, bool good);
    MasterPlugin* plugin() const     { return MasterPlugin::findIn (edit); }

    struct Row
    {
        juce::Label name, value;
        juce::Slider slider;
    };
    void setupRow (Row&, const juce::String& name, double min, double max, double def, double step,
                   std::function<void (float)> onChange);

    te::Edit& edit;

    juce::ToggleButton onButton { "Mastering on" };
    juce::ComboBox presetBox;
    juce::TextButton compareButton { "Compare: hear before" };
    juce::Label presetHint, help;

    Row warmth, clarity, air, glue, width, push;
    juce::ToggleButton lowCutButton { "Remove rumble (below 30 Hz)" };
    juce::ComboBox targetBox;
    juce::TextButton autoButton { "Set loudness for the whole song" }, resetFaderButton { "Set Master fader to 0 dB" };
    juce::Label autoStatus, faderNote;

    juce::Rectangle<int> toneCard, glueCard, loudCard, meterCard;

    // Automatic loudness
    std::shared_ptr<te::EditRenderer::Handle> renderHandle;
    juce::File tempFile;
    int autoPass = 0;
    bool autoRunning = false;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // Meter display
    float shownGlue = 0, shownLimit = 0;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasteringPage)
};
