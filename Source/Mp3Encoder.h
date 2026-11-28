#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
//  MP3 files (Step 14), using the LAME encoder when the build includes it.
//==============================================================================
namespace Mp3Encoder
{
    bool isAvailable();

    struct Tags
    {
        juce::String title, artist, album, year;
        // Release details (plan Step 18)
        juce::String genre, composer, copyright, isrc, publisher, comment, track;
        juce::MemoryBlock coverArt;           // JPEG or PNG
    };

    // Converts a WAV (any bit depth) into an MP3 at the given bit rate (kbps).
    // progress: 0..1, called now and then; return false from shouldStop to cancel.
    juce::Result encode (const juce::File& wavIn, const juce::File& mp3Out, int kbps, const Tags&,
                         std::function<void (float)> progress = {},
                         std::function<bool()> shouldStop = {});
}
