#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
//  Metronome sounds, generated once into the app's data folder as WAV files:
//  an accented version for beat 1 and a normal version for the other beats.
//==============================================================================
namespace ClickSounds
{
    juce::StringArray getNames();                       // "Click", "Woodblock", "Rim", "Cowbell"

    /** Makes sure both WAV files for this sound exist; returns { accent, normal }. */
    std::pair<juce::File, juce::File> ensureFiles (int soundIndex);
}
