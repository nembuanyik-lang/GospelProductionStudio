#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Starter drum grooves (General MIDI drum notes), inserted as clips on the
//  Drums track. The full African and Cameroonian groove pack is Step 21.
//
//  Steps: X accent, x normal, o soft, g ghost, - rest.
//==============================================================================
namespace Grooves
{
    struct Lane { int note; const char* steps; };

    struct Groove
    {
        const char* name;
        const char* category;
        int numerator, denominator;     // time signature it is written for
        int stepsPerBeat;               // steps per song beat (a beat is a quarter in 4/4, an eighth in 6/8)
        std::vector<Lane> lanes;
    };

    const std::vector<Groove>& all();
    bool fits (const Groove&, int numerator, int denominator);
    int numSteps (const Groove&);

    // Writes the groove, repeated, from startBeat (clip beats) for lengthBeats.
    void writeTo (te::MidiList&, const Groove&, double startBeat, double lengthBeats, double beatsPerBar,
                  juce::UndoManager* = nullptr);

    // GM drum notes used by the grooves
    namespace Drum
    {
        enum : int { kick = 36, rim = 37, snare = 38, closedHat = 42, pedalHat = 44, openHat = 46,
                     lowTom = 45, midTom = 47, highTom = 50, floorTom = 43, crash = 49, ride = 51, rideBell = 53 };
    }
}
