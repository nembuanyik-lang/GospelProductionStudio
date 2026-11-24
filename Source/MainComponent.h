#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "StudioLookAndFeel.h"
#include "LevelMeter.h"
#include "StudioKeys.h"

//==============================================================================
//  Stage 0 / Step 2 - Studio Shell
//
//  Proves the foundation works on the real computer:
//    - audio device opens (laptop sound now, audio interface later)
//    - test tone plays through speakers / headphones
//    - MIDI keyboard (YPT-240) is detected, lights the on-screen keys and sounds
//    - input meter moves when the microphone is connected
//    - settings are remembered between sessions
//==============================================================================
class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    //==========================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showAudioSettings();
    void enableNewMidiInputs();
    void saveSettings();
    void allNotesOff();

    void drawCard (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title) const;
    void drawRow (juce::Graphics&, juce::Rectangle<int>& area, const juce::String& label,
                  const juce::String& value, juce::Colour valueColour) const;

    //==========================================================================
    std::unique_ptr<juce::PropertiesFile> settings;

    juce::MidiKeyboardState     keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::MidiMessageCollector  midiCollector;
    juce::Synthesiser           synth;

    juce::StringArray            knownMidiInputs;
    juce::MidiDeviceListConnection midiListConnection;

    juce::TextButton setupButton { "Audio & MIDI Setup" };
    juce::TextButton toneButton  { "Test Tone  (A 440 Hz)" };
    juce::TextButton panicButton { "All Notes Off" };

    LevelMeter inputMeter, outputMeter;

    //==========================================================================
    // Shared between the audio thread and the screen
    std::atomic<bool>  toneOn         { false };
    std::atomic<float> inputPeak      { 0.0f };
    std::atomic<float> outputPeak     { 0.0f };
    std::atomic<int>   lastNote       { -1 };
    std::atomic<int>   lastVelocity   { 0 };
    std::atomic<int>   midiEventCount { 0 };

    // Audio thread only
    double tonePhase = 0.0;
    double currentSampleRate = 44100.0;

    // Screen only
    float shownInput = 0.0f, shownOutput = 0.0f;
    juce::Rectangle<int> headerArea, footerArea, audioCard, midiCard, levelCard;
    juce::Rectangle<int> inputLabelArea, outputLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
