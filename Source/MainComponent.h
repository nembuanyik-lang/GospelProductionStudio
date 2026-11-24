#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "StudioLookAndFeel.h"
#include "LevelMeter.h"
#include "StudioEngine.h"
#include "SongLibrary.h"
#include "LibraryPage.h"
#include "StudioPage.h"

//==============================================================================
//  Main window content.
//
//  Pages:  Song Library  |  Studio  |  Engine & Devices
//  The 88-key keyboard along the bottom plays the Keys track on every page.
//==============================================================================
class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Page { library, studio, engine };

    void showPage (Page);
    void timerCallback() override;
    void showAudioSettings();
    void allNotesOff();
    void paintEnginePage (juce::Graphics&);

    void drawCard (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title) const;
    void drawRow (juce::Graphics&, juce::Rectangle<int>& area, const juce::String& label,
                  const juce::String& value, juce::Colour valueColour) const;

    //==========================================================================
    // Order matters: the engine is created first and destroyed last.
    StudioEngine studio;
    SongLibrary  songLibrary;
    LibraryPage  libraryPage { songLibrary, studio.getMemo() };
    StudioPage   studioPage  { studio, songLibrary };
    Page         page = Page::library;

    juce::MidiKeyboardComponent keyboard { studio.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard };

    juce::TextButton libraryTab  { "Song Library" };
    juce::TextButton studioTab   { "Studio" };
    juce::TextButton engineTab   { "Engine & Devices" };
    juce::TextButton setupButton { "Audio & MIDI Setup" };

    // Engine & Devices page
    juce::TextButton toneButton  { "Test Tone  (A 440 Hz)" };
    juce::TextButton panicButton { "All Notes Off" };
    LevelMeter inputMeter, outputMeter;
    float shownInput = 0.0f, shownOutput = 0.0f;

    juce::TooltipWindow tooltips { this, 600 };

    juce::Rectangle<int> headerArea, footerArea, audioCard, midiCard, levelCard;
    juce::Rectangle<int> inputLabelArea, outputLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
