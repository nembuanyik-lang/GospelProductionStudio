#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Recordings and tempo changes (plan Step 23).
//
//  The engine keeps a recording on the same bar when the song's tempo changes,
//  but does not stretch the sound inside it, so a vocal recorded at 80 BPM drifts
//  against a band at 100 BPM. Here each recording remembers its original file and
//  the tempo it belongs to. When the tempo changes, the song chooses:
//    - Stretch: every recording is time-stretched from its original (pitch kept)
//      so it stays exactly with the beat. Back to the old tempo = the untouched
//      original again.
//    - Keep: recordings stay where they are in seconds (right when you set the
//      tempo to match an imported song you want to learn).
//  Loops that follow the tempo already have their own way (Loops.h).
//==============================================================================
namespace TempoFollow
{
    enum class Mode { ask, stretch, keep };
    Mode getMode (te::Edit&);
    void setMode (te::Edit&, Mode);

    // Recordings this applies to: audio clips at their own speed that are not tempo-following loops
    bool applies (te::WaveAudioClip&);
    juce::Array<te::WaveAudioClip*> recordings (te::Edit&);

    // Call just before the tempo changes: remembers each recording's original and its place in beats
    void prepareForChange (te::Edit&);

    // Changes the tempo keeping recordings where they are in seconds (nothing drifts in between)
    void setTempoKeepingRecordings (te::Edit&, double newBpm);

    // A tool put another file back on the clip (e.g. "Remove timing correction"): that file is the
    // original at the tempo it was made for, and the clip will be stretched to the current tempo
    void sourceReplaced (te::WaveAudioClip&);

    // Forget the originals: recordings stay as they are now
    void forget (te::Edit&);

    // Recordings waiting to be stretched to the current tempo
    int numWaiting (te::Edit&);

    //==========================================================================
    // Stretching, in two halves so the slow part can run in the background
    struct Job { te::EditItemID clip; juce::File source, dest; double ratio = 1.0; };
    std::vector<Job> jobsFor (te::Edit&);                                   // message thread
    bool render (const Job&, const std::function<bool()>& keepGoing = {});  // any thread
    bool finish (te::Edit&, const Job&);                                    // message thread

    int refreshNow (te::Edit&);                                             // all at once (tests)

    // When a song is opened: deletes stretched copies no clip uses
    int tidyUp (te::Edit&);
}
