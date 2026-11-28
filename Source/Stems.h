#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Stem separation (plan Step 22): split a mixed recording into Voice, Drums,
//  Bass and Other, each on its own track.
//
//  How (no internet, no AI model): the sound is cut into short slices of
//  frequencies. Sounds that are steady in pitch (singing, bass, keys) are told
//  apart from short hits (drums) by how they look over time and over frequency
//  ("harmonic / percussive" separation). The steady part is split again: below
//  about 200 Hz is the bass; what sits in the middle of the stereo picture in
//  the voice's range is the voice; the rest is "Other". The four stems always
//  add up exactly to the original, so nothing is lost.
//
//  It works best on a stereo mix with the lead voice in the middle. It is not
//  as clean as the big online AI separators: good for learning parts, practice
//  (mute the voice and sing along) and quick remixes.
//==============================================================================
namespace Stems
{
    enum Stem { voice = 0, drums, bass, other, numStems };
    juce::String stemName (int);

    struct Result
    {
        bool ok = false;
        juce::String problem;
        std::array<juce::File, numStems> files;
    };

    // Writes four stereo WAV files into 'folder'. 'progress' gets 0..1 and returns false to stop.
    Result separate (const juce::File& source, const juce::File& folder, const std::function<bool (float)>& progress = {});

    // Puts the stems on tracks under the clip's track (re-using the tracks of an earlier split)
    juce::Array<te::AudioTrack*> placeStems (te::WaveAudioClip&, const Result&, bool muteOriginal);
    juce::Array<te::AudioTrack*> findStemTracks (te::WaveAudioClip&);
    juce::File folderFor (const juce::File& source);

    std::unique_ptr<juce::Component> createPanel (te::WaveAudioClip&, std::function<void()> onChanged);
}
