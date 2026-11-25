#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Audio clip editing (Step 10): split, fades, gain, normalise, pitch, speed.
//==============================================================================
namespace AudioEdit
{
    constexpr double microFadeSeconds = 0.005;     // removes clicks at cuts

    void prepare (te::AudioClipBase&);             // auto-crossfade and the good stretcher
    te::Clip* splitAt (te::Clip&, te::TimePosition);

    float measurePeakDb (te::AudioClipBase&);      // of the part of the file the clip plays
    void normalise (te::AudioClipBase&, float targetDb = -1.0f);

    void setPitch (te::AudioClipBase&, float semitones);
    void setSpeed (te::AudioClipBase&, double ratio, bool keepPitch);
    double getSpeed (te::AudioClipBase&);
}

//==============================================================================
//  Comping: choose which take plays in each part of a clip, then render one
//  smooth "comp" recording that is added as a new take.
//==============================================================================
namespace Comp
{
    struct Segment { double start, end; int take; };     // seconds in the take files

    std::vector<int> recordedTakes (te::WaveAudioClip&);  // take indexes that are not comps
    bool isCompTake (te::WaveAudioClip&, int takeIndex);

    std::vector<Segment> load (te::WaveAudioClip&, double fileStart, double fileEnd, int defaultTake);
    void save (te::WaveAudioClip&, const std::vector<Segment>&);
    void assign (std::vector<Segment>&, double start, double end, int take);

    // Renders the comp into a WAV next to the takes. Safe to call on a background thread.
    juce::Result render (const std::vector<juce::File>& takeFiles, const std::vector<Segment>&,
                         double crossfadeSeconds, const juce::File& output);

    void addCompTake (te::WaveAudioClip&, const juce::File&);
}
