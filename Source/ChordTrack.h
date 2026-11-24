#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Theory.h"

namespace te = tracktion;

//==============================================================================
//  The Chord Track: chords stored inside the song's project file, in beats,
//  relative to the key. All changes go through the project's undo history.
//==============================================================================
namespace ChordTrack
{
    struct Event
    {
        Theory::Chord chord;
        double start = 0.0, length = 4.0;      // beats
        juce::ValueTree node;
    };

    std::vector<Event> getEvents (te::Edit&);                       // sorted by start
    juce::ValueTree add (te::Edit&, const Theory::Chord&, double start, double length);
    void setChord (te::Edit&, juce::ValueTree node, const Theory::Chord&);
    void setTiming (te::Edit&, juce::ValueTree node, double start, double length);
    void remove (te::Edit&, juce::ValueTree node);
    void clearRange (te::Edit&, double start, double end);         // removes chords starting in [start, end)

    Event fromNode (const juce::ValueTree&);
    int indexAt (const std::vector<Event>&, double beat);          // chord sounding at beat, or -1

    /** Places a progression at 'start' (overwriting chords there); returns the end beat. */
    double insert (te::Edit&, const std::vector<Theory::TimedChord>&, double start);

    std::vector<Theory::TimedChord> toTimed (const std::vector<Event>&);

    struct NotesOptions
    {
        bool keys = true, bass = true, anticipate = false;
        Theory::Voicing voicing = Theory::Voicing::gospel;
        Theory::LeftHand leftHand = Theory::LeftHand::root;
    };

    /** Writes the chords as MIDI clips on the Keys and Bass tracks. Returns a short report. */
    juce::String chordsToNotes (te::Edit&, const Theory::Key&, const NotesOptions&);
}
