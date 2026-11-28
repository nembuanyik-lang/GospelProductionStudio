#pragma once

#include "StudioEngine.h"

//==============================================================================
//  The mixer (Step 12): one channel strip per track, the shared reverb and
//  delay, and the master. Each strip: effects, sends, pan, fader with meter,
//  mute and solo. Opened from the Studio with the "Mixer" button (or X).
//==============================================================================
class MixerView final : public juce::Component,
                        private juce::Timer
{
public:
    explicit MixerView (StudioEngine&);
    ~MixerView() override;

    void releaseEdit();                      // the song is closing: let go of everything
    void rebuild();

    void paint (juce::Graphics&) override;
    void resized() override;

    class Strip;

private:
    void timerCallback() override;
    void visibilityChanged() override;
    juce::String signature() const;

    StudioEngine& engine;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<Strip> strips;
    std::unique_ptr<Strip> master;
    juce::String lastSignature;
    juce::Label hint;
    juce::TextButton addBusButton { "+ Bus" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerView)
};
