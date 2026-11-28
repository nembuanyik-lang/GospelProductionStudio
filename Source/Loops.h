#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Loops that follow the song (Step 18).
//
//  A loop (a drum loop, a percussion groove, a guitar riff) was recorded at
//  its own tempo. We work out that tempo (from the file, its name like
//  "Shaker 96bpm", or its length in whole bars), then let the clip follow the
//  song's tempo and repeat. If its key is known ("Guitar riff G"), it can
//  follow the song's key too.
//==============================================================================
namespace Loops
{
    struct Guess
    {
        double bpm = 0.0;         // the loop's own tempo
        double beats = 0.0;       // how many beats long it is
        int root = -1;            // its key note (0-11), -1 when unknown
        juce::String how;         // where the tempo came from (shown to the user)
        bool valid() const        { return bpm > 0.0 && beats > 0.0; }
    };

    // songBpm and beatsPerBar help to choose between equally possible tempos.
    Guess guess (te::Engine&, const juce::File&, double songBpm, int beatsPerBar, bool sureItIsALoop);

    std::optional<double> tempoFromName (const juce::String& name);
    int rootFromName (const juce::String& name);                 // 0-11 or -1

    // Makes the clip follow the song tempo and repeat 'repeats' times.
    bool follow (te::WaveAudioClip&, const Guess&, int repeats);
    void stopFollowing (te::WaveAudioClip&);                     // plays at its own speed again
    bool refresh (te::WaveAudioClip&);                           // after a tempo or key change: true if remade
    int refreshAll (te::Edit&);
    bool isFollowing (const te::WaveAudioClip&);

    // Changes the tempo we think the loop has (e.g. the guess was half or double).
    void setLoopTempo (te::WaveAudioClip&, double bpm);
    double getLoopTempo (te::WaveAudioClip&);

    // Key: follow the song's key (needs the loop's key).
    void setRoot (te::WaveAudioClip&, int root);                 // 0-11, -1 unknown
    int getRoot (te::WaveAudioClip&);
    void setFollowKey (te::WaveAudioClip&, bool);
    bool isFollowingKey (const te::WaveAudioClip&);

    juce::String describe (te::WaveAudioClip&);                  // "Loop 96 BPM, 2 bars, G"
}
