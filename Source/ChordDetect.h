#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Theory.h"
#include <array>

namespace te = tracktion;

//==============================================================================
//  Chord detection (plan Step 22): listen to a recording (or read a MIDI clip),
//  find the chord on each beat and put the chords on the Chord Track.
//
//  How: for every beat of the song we measure how strong each of the 12 notes
//  is (a "chroma"), separately for the bass. Each chord (major, minor, 7, maj7,
//  m7, sus4, dim) is compared with it; chords in the key get a small bonus; and
//  the best path through the song is chosen so chords change mostly on strong
//  beats and do not flicker. A bass note that is not the root gives a slash chord.
//==============================================================================
namespace ChordDetect
{
    struct BeatChroma
    {
        std::array<float, 12> treble {}, bass {};
        float energy = 0.0f;
    };

    struct Detected
    {
        double start = 0.0, length = 1.0;      // song beats
        int rootPc = 0, bassPc = -1;            // absolute pitch classes (C = 0)
        juce::String quality;                   // Theory quality id
        float confidence = 0.0f;                // 0..1
    };

    struct Result
    {
        bool ok = false;
        juce::String problem;
        std::vector<Detected> chords;
        Theory::Key key;                        // the key the music seems to be in
        float keyStrength = 0.0f;
        double tempoGuess = 0.0;                // BPM, for recordings (0 = not known)
        double firstBeat = 0.0, lastBeat = 0.0;
    };

    // ---- The pieces (pure, used by the tests too) -------------------------------
    std::vector<BeatChroma> chromaFromAudio (const juce::AudioBuffer<float>&, double sampleRate, const std::vector<double>& beatEdgesSec);
    std::vector<BeatChroma> chromaFromNotes (const std::vector<std::tuple<int, double, double, int>>& notes,     // pitch, start, length (beats), velocity
                                             double firstBeat, int numBeats);
    Theory::Key findKey (const std::vector<BeatChroma>&, float& strength);
    std::vector<Detected> decode (const std::vector<BeatChroma>&, double firstBeat, double beatsPerBar, const Theory::Key&);
    double guessTempo (const juce::AudioBuffer<float>&, double sampleRate);

    juce::String chordName (const Detected&, const Theory::Key&);
    Theory::Chord toChord (const Detected&, const Theory::Key& songKey);      // relative to the song's key

    // ---- On a clip in the song ----------------------------------------------------
    Result detect (te::Clip&, double beatsPerBar);
    int writeToChordTrack (te::Edit&, const Result&, const Theory::Key& songKey);   // replaces the chords in that range

    std::unique_ptr<juce::Component> createPanel (te::Clip&, std::function<Theory::Key()> getKey, std::function<double()> getBeatsPerBar,
                                                  std::function<void()> onChanged);
}
