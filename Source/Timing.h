#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Timing correction (plan Step 13): line a recording up with the beat.
//
//  We find where each word or note starts (its "note start"), work out the
//  nearest grid line (1/8, 1/16, triplets...) and move it part of the way there.
//  The audio between note starts is gently stretched or squeezed, so nothing
//  is cut and the voice keeps its own sound. The original recording is kept:
//  "Remove timing correction" brings it back.
//==============================================================================
namespace Timing
{
    struct Settings
    {
        double gridBeats = 0.25;       // 1/16 note
        float strength = 0.5f;         // 0..1 (1 = exactly on the grid)
        double toleranceMs = 10.0;     // leave note starts this close alone
        int sensitivity = 1;           // 0 fewer note starts, 1 normal, 2 more
    };

    struct NoteStart
    {
        double sourceSec = 0.0;        // in the original recording
        double newSourceSec = 0.0;     // where it will be after correction
        double offMs = 0.0;            // how far from the grid now (+ late, - early)
        bool moves = false;
        bool inClip = true;            // inside the part of the recording the clip plays
    };

    struct Analysis
    {
        bool ok = false;
        juce::String problem;          // why it can't be used on this clip
        juce::File source;             // the original recording
        double lengthSec = 0.0;
        std::vector<NoteStart> notes;
        int numMoving = 0;
        double averageMoveMs = 0.0, biggestMoveMs = 0.0;
    };

    // Note starts in a recording (seconds), cached per file and sensitivity.
    std::vector<double> findNoteStarts (const juce::File&, int sensitivity);
    std::vector<double> findNoteStarts (const juce::AudioBuffer<float>&, double sampleRate, int sensitivity);

    Analysis analyse (te::WaveAudioClip&, const Settings&);
    bool apply (te::WaveAudioClip&, const Settings&, juce::String& problem);
    void remove (te::WaveAudioClip&);
    bool isCorrected (const te::WaveAudioClip&);
    Settings lastSettings (const te::WaveAudioClip&);

    // Writes 'src' with each input time moved to its output time (pairs in seconds, increasing).
    bool renderWarped (const juce::File& src, const std::vector<std::pair<double, double>>& inToOut, const juce::File& dest);

    // The panel (opens next to the clip)
    std::unique_ptr<juce::Component> createPanel (te::WaveAudioClip&, std::function<void()> onChanged);
}
