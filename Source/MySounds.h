#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

//==============================================================================
//  My Sounds (Step 18): your own samples on the keyboard.
//
//  Each sound sits on a key. A "hit" (a choir "hey!", a horn stab, a cymbal
//  swell) plays as it is, all the way through. A "melodic" sound (one note of
//  a flute, a pad, a voice "ooh") is spread over two octaves so you can play
//  tunes with it, in tune with the note it was recorded on.
//
//  The sounds are stored in the song (copied into its "Sounds" folder) and
//  played by the Gospel Sampler through a small SFZ file written for them.
//==============================================================================
namespace MySounds
{
    enum class Mode { hit = 0, melodic = 1 };

    struct Slot
    {
        juce::String name;
        juce::File file;
        int key = 48;               // the key it plays on (melodic: the lowest key of its range)
        Mode mode = Mode::hit;
        float volumeDb = 0.0f;
        int root = -1;              // melodic: the note it was recorded on (-1 = unknown, plays at "key")
        int range = 24;             // melodic: how many keys up from "key"
    };

    // The list lives in the plugin's state, so it is saved with the song and undoable.
    std::vector<Slot> read (const juce::ValueTree& pluginState);
    juce::ValueTree slotsTree (juce::ValueTree pluginState);          // creates it if missing
    bool isSlotTree (const juce::ValueTree&);

    // Adds a sound on the next free key. Returns the new slot's tree.
    juce::ValueTree add (juce::ValueTree pluginState, const juce::File& sample, juce::UndoManager*);
    void setMode (juce::ValueTree slot, Mode, juce::UndoManager*);
    int nextFreeKey (const juce::ValueTree& pluginState, Mode);

    // Writes the SFZ for these sounds (in a cache folder) and returns it. Invalid File when empty.
    juce::File writeSfz (const juce::ValueTree& pluginState);

    // Helpers
    bool isAudioFile (const juce::File&);
    juce::String wildcard();                                   // "*.wav;*.aif;..."
    double lengthSeconds (const juce::File&);
    int detectRootNote (const juce::File&);                    // MIDI note, -1 when not clearly pitched
    juce::String keyName (int note);                           // "C3", "F#4"

    juce::AudioFormatManager& formats();
}
