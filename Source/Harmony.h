#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Theory.h"
#include <map>
#include <optional>

namespace te = tracktion;

//==============================================================================
//  Harmony generator and choir stacks (plan Step 19).
//
//  We listen to one sung melody (a recorded clip), find its notes, and for each
//  note choose the harmony notes from the chord sounding at that moment in the
//  Chord Track (or from the key when there are no chords). Each harmony part is
//  the same recording moved to its new notes, keeping the words, the vibrato and
//  the feel. Every part gets its own tone (a deeper colour for low voices) and,
//  for choir stacks, several "singers" who each start a little early or late and
//  sit a few cents high or low, so it sounds like a real choir and not one voice
//  copied. Each part goes on its own track, so it can be mixed or muted.
//==============================================================================
namespace Harmony
{
    struct PartInfo
    {
        const char* id;          // stored on the track
        const char* name;        // shown to the user, also the track name
        const char* hint;
        juce::uint32 colour;
        float formant;           // tone: below 1 = deeper, darker voice
        float levelDb;           // starting fader level
    };

    // 0 Alto, 1 Tenor, 2 Bass, 3 Soprano (above), 4 Third above, 5 Third below,
    // 6 Sixth below, 7 Octave below, 8 Double (same notes)
    const std::vector<PartInfo>& parts();
    int findPart (const juce::String& id);

    struct Preset { const char* name; std::vector<int> parts; int singers; };
    const std::vector<Preset>& presets();

    //==========================================================================
    // The sung melody
    struct Note
    {
        double start = 0.0, end = 0.0;   // seconds in the recording
        float sung = 0.0f;               // MIDI note, with cents (the middle of the vibrato)
        int note = 0;                    // nearest MIDI note
    };

    // Pitch of each short frame (MIDI note, 0 = no clear pitch), and the notes made from it.
    struct Melody
    {
        double frameSec = 0.0;           // time between frames
        double firstFrameSec = 0.0;      // time of frame 0
        std::vector<float> pitch;
        std::vector<Note> notes;
    };

    Melody findMelody (const juce::AudioBuffer<float>&, double sampleRate);
    Melody findMelody (const juce::File&);                       // cached per file

    //==========================================================================
    // The chords, in recording seconds
    struct ChordSpan
    {
        double start = 0.0, end = 0.0;
        std::vector<int> upper;          // pitch classes for the upper voices
        int bass = 0;                    // pitch class of the bass note
    };

    struct Context
    {
        Theory::Key key;
        std::vector<ChordSpan> chords;   // empty: harmonies follow the key
    };

    // The harmony note for one melody note (pure music theory, used by the tests too).
    // 'previous' is this part's previous note (0 = none), for smooth bass lines.
    int harmonyNote (int part, int melody, const ChordSpan* chord, const Theory::Key&, int previous);

    // The chord for a note: the one that covers most of it (nullptr = none)
    const ChordSpan* chordFor (const Context&, const Note&);

    //==========================================================================
    // Notes moved by hand (Step 33). Each moved note is found by the part and
    // the time the melody note starts (ms), so the edits stay with the clip.
    struct Edits
    {
        std::map<int, std::map<int, int>> moved;          // part -> (note start in ms -> MIDI note)

        bool empty() const;
        int count() const;
        std::optional<int> find (int part, const Note&) const;
        void set (int part, const Note&, int midiNote);
        void clear (int part, const Note&);
        juce::String toString() const;                    // "alto@1234=65 tenor@5678=60"
        static Edits fromString (const juce::String&);
    };

    Edits loadEdits (const te::WaveAudioClip&);
    void saveEdits (te::WaveAudioClip&, const Edits&);

    // Target MIDI note for every melody note, for one part (with any notes moved by hand)
    std::vector<int> planPart (int part, const Melody&, const Context&);
    std::vector<int> planPart (int part, const Melody&, const Context&, const Edits&);

    // The next note of the chord (or of the key, without chords) above or below
    int chordToneStep (int midiNote, const ChordSpan*, const Theory::Key&, int direction);

    //==========================================================================
    struct Settings
    {
        std::vector<int> parts { 0, 1, 2 };
        int singers = 1;                 // singers in each part, 1..4
        float looseness = 0.5f;          // 0 tight (studio) .. 1 loose (big live choir)
        bool toChoirBus = true;
        Edits edits;
    };

    struct ClipInfo
    {
        bool ok = false;
        juce::String problem;
        juce::File source;
        Context context;
    };

    // What is needed from the clip and the song (call on the message thread)
    ClipInfo prepare (te::WaveAudioClip&, const Theory::Key&);

    // Writes one part (all its singers) to 'dest'. Safe on a background thread.
    bool renderPart (const juce::File& source, const Melody&, const Context&, int part,
                     int singers, float looseness, const juce::File& dest,
                     const std::function<bool (float)>& progress = {}, const Edits& edits = {});

    // Puts the rendered parts on their own tracks next to the clip's track
    // (re-using the tracks of an earlier run). Message thread.
    juce::Array<te::AudioTrack*> placeParts (te::WaveAudioClip&, const std::vector<std::pair<int, juce::File>>& rendered, bool toChoirBus);

    // Everything in one go (used by the tests)
    juce::String generate (te::WaveAudioClip&, const Theory::Key&, const Settings&);

    juce::File folderFor (const juce::File& source);         // where the parts are written
    juce::Array<te::AudioTrack*> findHarmonyTracks (te::WaveAudioClip&);
    void removeHarmonies (te::WaveAudioClip&);
    bool isHarmonyTrack (const te::AudioTrack&);

    // When a song is opened: deletes harmony recordings that no clip uses (from earlier runs)
    int tidyUp (te::Edit&);

    // The panel (opens from the clip's right-click menu)
    std::unique_ptr<juce::Component> createPanel (te::WaveAudioClip&, std::function<Theory::Key()> getKey, std::function<void()> onChanged);
}
