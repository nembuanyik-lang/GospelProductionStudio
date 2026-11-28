#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Theory.h"
#include "ChordTrack.h"

namespace te = tracktion;

//==============================================================================
//  Backing players that follow your chords (plan Step 20).
//
//  A band (bass, piano, organ, guitar and drums) plays the Chord Track in a
//  gospel style. Each player has a rhythm for the style, a light version for
//  verses and a fuller one for choruses, pushes into the next chord like real
//  gospel players, plays a fill before each new section (bass walk-up, piano
//  run, drum fill) and everyone hits the last chord together. The parts are
//  written as MIDI clips on the right tracks, so every note can be edited.
//==============================================================================
namespace Backing
{
    enum Player { bass = 0, keys, organ, guitar, drums, numPlayers };
    juce::String playerName (int player);

    struct Style
    {
        const char* name;
        const char* feel;               // one line for the user
        int numerator, denominator;     // the time signature it is written for
        int stepsPerBeat;               // pattern steps per song beat
        const char* groove;             // drum groove (Grooves.cpp)
        int guitarSound;                // 0 clean electric, 1 nylon
        // Patterns: [player][0 light, 1 full], one character per step for one bar
        //   bass:   R root, r soft root, 5 fifth, O octave, 3 third, A approach to the next chord
        //   keys:   C chord, c soft chord, T right hand only, B left hand only
        //   organ:  P held chord, s short chord
        //   guitar: D down strum, U up strum, m muted "chick", a picked note (arpeggio)
        //   any:    = keep holding, - rest
        const char* patterns[4][2];
    };

    const std::vector<Style>& styles();
    bool fits (const Style&, int numerator, int denominator);

    struct Settings
    {
        int style = 0;
        bool players[numPlayers] { true, true, true, true, true };
        bool followSections = true;     // lighter verses, fuller choruses
        bool fills = true;              // before each new section
        bool ending = true;             // everyone hits the last chord
        float humanise = 0.5f;          // 0..1

        // A livelier band (Step 34)
        bool answer = true;             // keys, guitar or bass answer when the singer rests
        bool drumVariation = true;      // open hi-hat at phrase ends, ghost notes, crash on each new section
        bool growth = true;             // a sparer first verse, a lifted last chorus, a build into each section
        std::vector<std::pair<double, double>> singing;     // where the melody sings (beats); empty: phrase ends
    };

    struct Note { int pitch = 60; double start = 0.0, length = 1.0; int velocity = 90; };     // song beats
    struct Part { int player = 0; std::vector<Note> notes; };
    struct SectionSpan { double start = 0.0, end = 0.0; juce::String name; };

    // Pure: the parts for these chords and sections (beats). Used by the tests too.
    std::vector<Part> arrange (const std::vector<ChordTrack::Event>&, const std::vector<SectionSpan>&,
                               const Theory::Key&, double beatsPerBar, const Settings&);

    bool isFullSection (const juce::String& sectionName);
    std::vector<SectionSpan> getSections (te::Edit&);

    // Where the lead vocal or melody sings (beats): from a track named Lead / Vocal / Melody... (Step 34)
    std::vector<std::pair<double, double>> findSinging (te::Edit&);
    // The places where the band answers the singer: the singer's rests, or the ends of 4-bar phrases
    std::vector<std::pair<double, double>> answerWindows (const std::vector<std::pair<double, double>>& singing,
                                                          const std::vector<SectionSpan>&, double first, double end, double bpb);

    // The track each player uses (nullptr: none yet)
    te::AudioTrack* findTrackFor (te::Edit&, int player);
    juce::String instrumentFor (int player, const Style&);       // the built-in instrument for a new track

    using TrackMaker = std::function<te::AudioTrack* (int player, const juce::String& name, juce::Colour, const juce::String& instrumentId)>;

    // Writes the band into the song (replacing an earlier band). Returns a short report.
    juce::String apply (te::Edit&, const Theory::Key&, double beatsPerBar, const Settings&, TrackMaker = {});
    int remove (te::Edit&);                                     // removes the band's clips
    bool isBandClip (const te::Clip&);

    std::unique_ptr<juce::Component> createPanel (te::Edit&, std::function<Theory::Key()> getKey, std::function<double()> getBeatsPerBar,
                                                  TrackMaker, std::function<void()> onChanged);
}
