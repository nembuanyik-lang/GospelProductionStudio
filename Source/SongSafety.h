#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <optional>

namespace te = tracktion;

//==============================================================================
//  Never lose a song (plan Steps 43-44)
//
//  Versions: copies of the song file (a few kilobytes each) in the song's
//  "Versions" folder: when it opens, every 10 minutes of work, when it closes,
//  and whenever you save one with a name. You can go back to any of them.
//  The recordings themselves stay where they are; the clean-up on opening keeps
//  every file an old version still uses.
//
//  Recovery: a small marker file says the app is running (and recording). If
//  the app did not close properly (power cut, crash), the next start says so,
//  repairs the song file if it was being saved at that moment, and rescues a
//  recording that was cut off onto a new track.
//==============================================================================
namespace SongSafety
{
    //==========================================================================
    //  Versions
    struct Version
    {
        juce::File file;
        juce::Time time;
        juce::String kind;          // opened, auto, closed, named, restore
        juce::String name;          // for named versions
        juce::String describe() const;
    };

    juce::File versionsFolder (const juce::File& editFile);
    // Copies the saved song file. Skipped when nothing changed since the newest version (unless it has a name).
    bool snapshot (const juce::File& editFile, const juce::String& kind, const juce::String& name = {});
    std::vector<Version> list (const juce::File& editFile);                  // newest first
    // Keeps every named version, the 20 newest automatic ones and one a day for 30 days
    int prune (const juce::File& editFile, juce::Time now = juce::Time::getCurrentTime());
    // Goes back to a version: the song as it is now is kept first as a version ("before going back")
    bool restore (const juce::File& editFile, const Version&);
    // The text of every version: the clean-up keeps the files they mention
    juce::String allVersionsText (const juce::File& songFolder);

    //==========================================================================
    //  Repairing a song file the app was saving when it stopped
    bool isReadableSong (const juce::File&);
    struct Repair { bool changed = false; juce::String message; };
    Repair prepareSongFile (const juce::File& editFile);

    // A recording cut off by a power cut has a WAV header that still says "empty"
    bool repairWavHeader (const juce::File&);

    //==========================================================================
    //  Did the app close properly last time?
    namespace Session
    {
        struct Previous
        {
            juce::String songId;
            juce::File editFile;
            juce::Time started, lastSeen;
            bool wasRecording = false;
            double recordStartSeconds = 0.0;
            juce::Time recordStartedAt;
            juce::StringArray filesBeforeRecording;      // audio files in the song folder when recording started
        };

        // Call once when the app starts: remembers whether the last session ended unexpectedly
        void appStarted (const juce::File& markerFile = {});
        void songOpened (const juce::String& songId, const juce::File& editFile);
        void songClosed();
        void recordingStarted (const juce::File& editFile, double startSeconds);
        void recordingStopped();
        void appClosedNormally();                         // removes the marker

        std::optional<Previous> previousEndedUnexpectedly();
        void forgetPrevious();

        // Recordings the app was making when it stopped: audio files that appeared after recording started
        juce::Array<juce::File> findCutOffRecordings (const Previous&, te::Edit* songNowOpen);
        // Repairs them and puts each on a new track at the place recording started. Returns how many.
        int rescueRecordings (te::Edit&, const Previous&);

        juce::File defaultMarkerFile();
    }

    //==========================================================================
    //  The Versions page
    std::unique_ptr<juce::Component> createVersionsPage (const juce::File& editFile,
                                                         std::function<void()> saveNow,
                                                         std::function<void (const Version&)> goBackTo);
}
