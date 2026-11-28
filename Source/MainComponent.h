#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "StudioLookAndFeel.h"
#include "LevelMeter.h"
#include "StudioEngine.h"
#include "SongLibrary.h"
#include "SongSafety.h"
#include "LibraryPage.h"
#include "StudioPage.h"
#include "Updater.h"
#include "SoundLibrary.h"
#include "PanelPage.h"
#include "Learn.h"

//==============================================================================
//  Main window content.
//
//  Pages:  Song Library  |  Studio  |  Engine & Devices
//  The 88-key keyboard along the bottom plays the Keys track on every page.
//==============================================================================
class MainComponent final : public juce::Component,
                            public Learn::Host,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // Before the app closes (Step 24): a recording is stopped and kept, and the song is saved
    void prepareToClose();

    // Learn::Host: what the course needs from the window
    StudioEngine& getStudio() override                    { return studio; }
    SongLibrary& getLibrary() override                    { return songLibrary; }
    juce::String currentPage() const override;
    juce::String currentPanel() const override            { return panel != nullptr ? panel->getTitle() : juce::String(); }
    void showPage (const juce::String&) override;
    void selectSongInLibrary (const juce::String& id) override;
    void openSongInStudio (const juce::String& id) override;
    void closePanel() override;
    void showCoursePage() override;
    juce::Rectangle<int> coachArea() const override       { return pageArea; }
    void startLesson (const juce::String& lessonId) override;
    juce::Component* getCoach() const                     { return coach.get(); }
    void runLearnAudit();
    void buildShowcase (const juce::File& vocal);           // GPS_TEST_SHOWCASE: a finished-looking song for the handbook pictures                                   // GPS_TEST_LEARN_AUDIT: can every "Show me" find its button?

private:
    std::unique_ptr<juce::Drawable> logo;          // the level-meter cross
    void offerRecovery (const SongSafety::Session::Previous&);
    enum class Page { library, studio, engine };

    void showPage (Page);
    void timerCallback() override;
    void showAudioSettings();

    // Sound Library, Plugins and Audio Setup open as pages inside the window, with "Back"
    std::unique_ptr<PanelPage> panel;
    void openPanel (const juce::String& title, std::unique_ptr<juce::Component> content);
    std::unique_ptr<juce::Component> coach;                 // the Learn coach card, while a lesson runs
    void allNotesOff();
    void paintEnginePage (juce::Graphics&);

    void checkForUpdates (bool userAsked);
    void startUpdate();
    void setBanner (bool visible);

    void drawCard (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title) const;
    void drawRow (juce::Graphics&, juce::Rectangle<int>& area, const juce::String& label,
                  const juce::String& value, juce::Colour valueColour) const;

    //==========================================================================
    // Order matters: the engine is created first and destroyed last.
    SoundLibrary soundLibrary;      // before the engine: instruments ask it for packs
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
    juce::TextButton updatesButton { "Updates" };
    juce::TextButton soundsButton { "Sound Library" };
    juce::TextButton pluginsButton { "Plugins" };
    juce::TextButton helpButton { "Help" };
    juce::TextButton learnButton { "Learn" };
    juce::TextButton keyboardButton { "Hide keyboard" };
    bool compactLayout = false, keyboardShown = true;

    // Updates
    Updater updater;
    Updater::Release pendingRelease;
    bool bannerVisible = false;
    juce::String bannerText;
    float downloadProgress = -1.0f;
    juce::TextButton updateNowButton { "Update Now" }, laterButton { "Later" };
    juce::Rectangle<int> bannerArea;

    // Engine & Devices page
    juce::TextButton toneButton  { "Test Tone  (A 440 Hz)" };
    juce::TextButton delayButton { "Check recording delay..." };       // Step 28
    juce::TextButton panicButton { "All Notes Off" };
    LevelMeter inputMeter, outputMeter;
    float shownInput = 0.0f, shownOutput = 0.0f;

    juce::TooltipWindow tooltips { this, 600 };

    juce::Rectangle<int> headerArea, footerArea, audioCard, midiCard, levelCard, pageArea;
    juce::Rectangle<int> inputLabelArea, outputLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
