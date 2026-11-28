#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
//  WAV files written the current JUCE way (Step 26 replaced the outdated
//  createWriterFor calls). Returns nullptr when the file cannot be opened.
//==============================================================================
namespace WavFiles
{
    inline std::unique_ptr<juce::AudioFormatWriter> writer (const juce::File& file, double sampleRate, int numChannels, int bits = 24)
    {
        auto fileStream = std::make_unique<juce::FileOutputStream> (file);
        if (! fileStream->openedOk())
            return {};
        std::unique_ptr<juce::OutputStream> stream (fileStream.release());
        juce::WavAudioFormat wav;
        return wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                                          .withNumChannels (numChannels)
                                                                          .withBitsPerSample (bits));
    }
}
