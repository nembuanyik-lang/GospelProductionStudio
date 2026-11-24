#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Takes on an audio clip (loop recording makes one take per pass).
//  The engine's own take switching only works with its project system, so
//  this chooses takes by pointing the clip at the take's recording.
//==============================================================================
namespace Takes
{
    int count (te::WaveAudioClip&);
    juce::File fileFor (te::WaveAudioClip&, int index);
    int current (te::WaveAudioClip&);                     // -1 if the clip plays something else
    void select (te::WaveAudioClip&, int index);
    void keepOnly (te::WaveAudioClip&, int index, bool deleteOtherFiles);
}
