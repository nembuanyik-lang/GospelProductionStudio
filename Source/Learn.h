#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "StudioEngine.h"
#include "SongLibrary.h"

//==============================================================================
//  Learn: a step-by-step course inside the app.
//
//  The "Learn" button opens the course page (modules and lessons). Starting a
//  lesson closes the page and shows a small coach card over the real screens:
//  one step at a time, a "Show me" button that makes the right button glow,
//  and "Your turn" tasks that tick themselves when you have done them.
//  Lessons use a "Practice Song", so real songs are never touched.
//==============================================================================
namespace Learn
{
    // What the course needs from the main window
    struct Host
    {
        virtual ~Host() = default;
        virtual StudioEngine& getStudio() = 0;
        virtual SongLibrary& getLibrary() = 0;
        virtual juce::String currentPage() const = 0;          // "library", "studio", "engine"
        virtual juce::String currentPanel() const = 0;         // "" or the title of the page shown inside the window ("Help")
        virtual void showPage (const juce::String& page) = 0;
        virtual void selectSongInLibrary (const juce::String& id) = 0;
        virtual void openSongInStudio (const juce::String& id) = 0;
        virtual void closePanel() = 0;
        virtual void showCoursePage() = 0;
        virtual void startLesson (const juce::String& lessonId) = 0;
        virtual juce::Rectangle<int> coachArea() const = 0;    // where the coach card may sit (window coordinates)
    };

    // The state of the song and app when a step starts, so tasks can ask "has it changed?"
    struct Snapshot
    {
        int songs = 0, chords = 0, tracks = 0, lyricSections = 0, timelineSections = 0, midiMessages = 0, tempo = 0;
        int clips = 0, recordings = 0, notes = 0;
        juce::String key;
    };

    struct Context
    {
        Host& host;
        const Snapshot& start;

        StudioEngine& studio() const      { return host.getStudio(); }
        const Song* practiceSong() const;
        bool practiceOpen() const;        // the practice song is open in the Studio
        te::Edit* edit() const;           // the open song (only when it is the practice song)
        int chordCount() const;
        int trackCount() const;
        int timelineSectionCount() const;
        bool hasTrackNamed (const juce::String& part) const;
        bool hasBand() const;
        bool anyTrackMuted() const;
        bool isOnScreen (const juce::String& target) const;

        // Part 2: recording, harmony, editing, mixing, release, versions
        int clipCount() const;                 // every clip on every track
        int recordingCount() const;            // recorded or imported audio clips
        int noteCount() const;                 // notes in every MIDI clip (band parts included)
        bool hasAudioTrack() const;            // a track for a microphone or guitar
        bool audioTrackHasInput() const;
        bool audioTrackArmed() const;
        bool hasHarmony() const;
        bool hasVocalTuning() const;
        bool hasTimingCorrection() const;
        bool hasVocalChain() const;
        int busCount() const;
        bool masteringOn() const;
        bool hasNamedVersion() const;
    };

    using Check = std::function<bool (const Context&)>;

    struct Step
    {
        juce::String text;            // what to read
        juce::String target;          // what "Show me" makes glow: "id:...", "tip:...", or the button's text
        juce::String where;           // the page the target is on ("library", "studio", "engine"), opened by "Show me"
        juce::String task;            // "Your turn: ..." (empty: just read and press Next)
        Check check;                  // true once the task is done
        juce::String opener;          // clicked by "Show me" when the target is inside a window that is not open yet ("Band...");
                                      // several in turn are separated by | ("Mixer|Mastering")
    };

    struct Lesson
    {
        juce::String id, title, why;
        int minutes = 3;
        juce::String startWith;        // "", "practice:library" or "practice:studio"
        std::vector<Step> steps;
    };

    struct Module
    {
        juce::String title, intro;
        std::vector<Lesson> lessons;
    };

    const std::vector<Module>& course();
    const Lesson* findLesson (const juce::String& id);
    int lessonCount();

    // Progress (kept in the app settings)
    bool isDone (const juce::String& lessonId);
    void setDone (const juce::String& lessonId, bool done);
    int doneCount();
    void resetProgress();

    // The practice song: found, or made (G major, 80 BPM, two verses and two choruses)
    static constexpr const char* practiceTitle = "Practice Song";
    juce::String findPracticeSong (const SongLibrary&);
    juce::String ensurePracticeSong (SongLibrary&);

    // Finding things on the screen. "id:x" = component ID, "tip:x" = tooltip starts with x,
    // otherwise the text of a button or label. Only components that are showing count.
    juce::Component* findOnScreen (const juce::String& target);
    juce::Component* findIn (juce::Component& root, const juce::String& target);

    // Opens what a step needs (its page, then its openers in turn) and finds its target; calls back with it (or nullptr)
    void reveal (Host&, const Step&, std::function<void (juce::Component*)> found);

    // A pulsing gold ring around a component; it removes itself after a while or once clicked
    void spotlight (juce::Component& target, int milliseconds = 9000);
    bool isSpotlightShowing();

    // The course page (inside the window) and the coach card (over the screens)
    std::unique_ptr<juce::Component> createCoursePage (Host&, std::function<void (const juce::String& lessonId)> onStart);

    // The coach card. onFinished: the lesson was closed or finished (remove the card then).
    std::unique_ptr<juce::Component> createCoach (Host&, const juce::String& lessonId, std::function<void()> onFinished);
    void placeCoach (juce::Component& coach);          // after the window changed size, or to bring it to the front

    // Driving the coach without the mouse (tests and the screen audit)
    struct CoachState { juce::String lessonId; int step = 0; bool taskDone = false; juce::String message; };
    CoachState coachState (juce::Component& coach);
    void coachGoTo (juce::Component& coach, int step);
    void coachNext (juce::Component& coach);
    juce::Component* coachShowMe (juce::Component& coach);

    // The handbook: every lesson as one page to print or save as PDF (opens in the web browser)
    juce::String handbookHtml (const juce::String& version);
    juce::File writeHandbook (const juce::File& file, const juce::String& version);

    // Tests
    juce::StringArray checkCourse();      // problems in the lessons themselves (empty when all is well)
}
