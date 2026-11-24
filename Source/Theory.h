#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
//  Gospel harmony: chord vocabulary, labels, voicings, progressions.
//
//  Chords are stored relative to the song's key (like Nashville numbers),
//  so a key change moves every chord with it.
//==============================================================================
namespace Theory
{
    //==========================================================================
    struct Quality
    {
        const char* id;          // stored in the project, e.g. "m7"
        const char* suffix;      // shown after the root, e.g. "m7"
        const char* group;       // palette grouping
        std::vector<int> intervals;
    };

    const std::vector<Quality>& qualities();
    const Quality& findQuality (const juce::String& id);

    //==========================================================================
    struct Chord
    {
        int  root = 0;               // semitones above the key's tonic, 0..11
        bool sharp = false;          // spell as #4 rather than b5, etc.
        juce::String quality;        // Quality id ("" = major triad)
        int  bass = -1;              // slash bass, semitones above tonic; -1 = none

        bool operator== (const Chord& o) const
        {
            return root == o.root && quality == o.quality && bass == o.bass;
        }
    };

    enum class Display { both, numbers, names };

    struct Key
    {
        int  tonic = 0;              // pitch class, C = 0
        bool minor = false;
        bool flats = true;           // spell names with flats
    };

    Key keyFromSong (int pitchClass, bool minor, const juce::String& keyName);

    juce::String degreeLabel (int semitones, bool sharp);           // "1", "b3", "#4"
    juce::String numberLabel (const Chord&);                        // "2m7", "4/5", "#4°7"
    juce::String nameLabel (const Chord&, const Key&);              // "Bbm7", "Db/Eb"
    juce::String label (const Chord&, const Key&, Display);
    juce::String noteName (int pitchClass, bool flats);
    juce::String midiNoteName (int midiNote, const Key&);           // "Ab3", middle C = C4

    // Pitch classes (0..11, C = 0) for checking notes against chords and keys
    std::vector<int> chordPitchClasses (const Chord&, const Key&);  // includes the slash bass
    bool isInKey (int pitchClass, const Key&);

    //==========================================================================
    // Voicings
    enum class Voicing { close, spread, gospel, shell, fourths, upperStructure, cluster };
    enum class LeftHand { root, rootFifth, octave };

    juce::StringArray voicingNames();
    juce::StringArray leftHandNames();

    /** MIDI notes for a chord. 'previousTop' (if > 0) keeps the right hand close
        to the previous chord for smooth voice leading. */
    std::vector<int> voice (const Chord&, const Key&, Voicing, LeftHand, int previousCentre = 0);
    int rightHandCentre (const std::vector<int>& notes);
    int bassNote (const Chord&, const Key&);                        // for the Bass track, C1..B2

    //==========================================================================
    // Palette contents for a key
    struct PaletteChord { Chord chord; juce::String hint; };
    struct PaletteGroup { juce::String name; std::vector<PaletteChord> chords; };

    std::vector<PaletteGroup> paletteFor (const Key&);
    std::vector<Chord> suggestionsAfter (const Chord&, const Key&);

    //==========================================================================
    // Progressions
    struct TimedChord { Chord chord; double beats; };

    enum class Level { simple, rich, advanced };
    juce::StringArray levelNames();

    struct Progression
    {
        juce::String category, name, text, feel;
    };

    const std::vector<Progression>& library();
    juce::StringArray categories();

    /** Parses "1 4/5 2.m7@2 5.7@2", one token per chord; '@beats' (default: one bar). */
    std::vector<TimedChord> parse (const juce::String& text, double beatsPerBar, bool minorKey);
    juce::String toText (const std::vector<TimedChord>&);

    std::vector<TimedChord> applyLevel (const std::vector<TimedChord>&, Level, bool minorKey);
    Chord enrich (const Chord&, bool minorKey);
}
