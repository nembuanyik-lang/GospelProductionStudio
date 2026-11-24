#include "ClickSounds.h"

namespace
{
    constexpr double rate = 48000.0;

    juce::File folder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("GospelProductionStudio")
                   .getChildFile ("ClickSounds");
    }

    // A decaying sum of sine partials plus an optional noise "tick".
    juce::AudioBuffer<float> synthesise (std::initializer_list<std::pair<double, double>> partials,
                                         double decaySeconds, double noiseAmount, double gain)
    {
        const int length = (int) (rate * juce::jmin (0.25, decaySeconds * 5.0));
        juce::AudioBuffer<float> buffer (1, length);
        auto* d = buffer.getWritePointer (0);

        juce::Random random (1234);
        double lowpassed = 0.0;

        for (int i = 0; i < length; ++i)
        {
            const auto t = (double) i / rate;
            const auto env = std::exp (-t / decaySeconds);
            const auto attack = juce::jmin (1.0, t / 0.0008);

            double s = 0.0;
            for (const auto& [freq, amp] : partials)
                s += amp * std::sin (juce::MathConstants<double>::twoPi * freq * t);

            // Short bright noise burst for the stick attack.
            const auto noise = random.nextDouble() * 2.0 - 1.0;
            lowpassed += 0.35 * (noise - lowpassed);
            s += noiseAmount * (noise - lowpassed) * std::exp (-t / 0.004);

            d[i] = (float) (gain * attack * env * s);
        }

        // Normalise to the requested peak.
        const auto peak = buffer.getMagnitude (0, 0, length);
        if (peak > 0.0f)
            buffer.applyGain ((float) gain / peak);

        return buffer;
    }

    juce::AudioBuffer<float> makeSound (int index, bool accent)
    {
        const auto up = accent ? 1.25 : 1.0;      // accented beat is higher...
        const auto g  = accent ? 0.95 : 0.70;     // ...and louder

        switch (index)
        {
            case 1:  return synthesise ({ { 880.0 * up, 1.0 }, { 1390.0 * up, 0.55 }, { 2650.0 * up, 0.15 } }, 0.035, 0.25, g); // Woodblock
            case 2:  return synthesise ({ { 420.0 * up, 0.6 }, { 1730.0 * up, 0.35 } }, 0.020, 0.90, g);                         // Rim
            case 3:  return synthesise ({ { 545.0 * up, 1.0 }, { 815.0 * up, 0.85 }, { 1630.0 * up, 0.2 } }, 0.110, 0.10, g);   // Cowbell
            default: return synthesise ({ { 2000.0 * up, 1.0 }, { 4000.0 * up, 0.25 } }, 0.012, 0.15, g);                       // Click
        }
    }

    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
            return false;

        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                       .withSampleRate (rate)
                                                       .withNumChannels (1)
                                                       .withBitsPerSample (24));
        if (writer == nullptr)
            return false;

        return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
}

juce::StringArray ClickSounds::getNames()
{
    return { "Click", "Woodblock", "Rim", "Cowbell" };
}

std::pair<juce::File, juce::File> ClickSounds::ensureFiles (int soundIndex)
{
    soundIndex = juce::jlimit (0, getNames().size() - 1, soundIndex);
    const auto dir = folder();
    dir.createDirectory();

    const auto name = getNames()[soundIndex].toLowerCase();
    const auto accentFile = dir.getChildFile (name + "-accent-v1.wav");
    const auto normalFile = dir.getChildFile (name + "-v1.wav");

    if (! accentFile.existsAsFile())
        writeWav (accentFile, makeSound (soundIndex, true));

    if (! normalFile.existsAsFile())
        writeWav (normalFile, makeSound (soundIndex, false));

    return { accentFile, normalFile };
}
