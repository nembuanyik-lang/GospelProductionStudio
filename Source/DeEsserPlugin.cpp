#include "DeEsserPlugin.h"

const char* DeEsserPlugin::xmlTypeName = "gpsDeEsser";

DeEsserPlugin::DeEsserPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    frequency   = { *this, "split freq", "deessFreq", "Frequency", 6000.0f, { 3000.0f, 11000.0f },
                    [] (float v) { return juce::String (v / 1000.0f, 1) + " kHz"; },
                    [] (const juce::String& s) { return s.getFloatValue() * (s.containsIgnoreCase ("k") ? 1000.0f : 1.0f); } };
    thresholdDb = { *this, "sibilance level", "deessThreshold", "Threshold", -30.0f, { -60.0f, 0.0f },
                    [] (float v) { return juce::String (v, 1) + " dB"; },
                    [] (const juce::String& s) { return s.getFloatValue(); } };
    rangeDb     = { *this, "range", "deessRange", "Range", 8.0f, { 0.0f, 20.0f },
                    [] (float v) { return juce::String (v, 1) + " dB"; },
                    [] (const juce::String& s) { return s.getFloatValue(); } };

    low.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    high.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
}

DeEsserPlugin::~DeEsserPlugin()
{
    notifyListenersOfDeletion();
    frequency.reset();
    thresholdDb.reset();
    rangeDb.reset();
}

void DeEsserPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    frequency.setFromValueTree (v);
    thresholdDb.setFromValueTree (v);
    rangeDb.setFromValueTree (v);
}

void DeEsserPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, info.blockSizeSamples), 2 };
    low.prepare (spec);
    high.prepare (spec);
    lastFreq = -1.0f;
    updateFilters();
    reset();
}

void DeEsserPlugin::reset()
{
    low.reset();
    high.reset();
    env = 0.0f;
    gainSmoothed = 1.0f;
    reduction = 0.0f;
}

void DeEsserPlugin::updateFilters()
{
    const auto f = juce::jlimit (1000.0f, (float) (sampleRate * 0.45), frequency.getCurrentValue());
    if (std::abs (f - lastFreq) > 1.0f)
    {
        low.setCutoffFrequency (f);
        high.setCutoffFrequency (f);
        lastFreq = f;
    }
}

void DeEsserPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int start = fc.bufferStartSample, num = fc.bufferNumSamples;
    const int channels = juce::jmin (2, buffer.getNumChannels());
    if (channels == 0 || num <= 0)
        return;

    updateFilters();
    const auto thr = thresholdDb.getCurrentValue();
    const auto range = rangeDb.getCurrentValue();
    constexpr float ratio = 6.0f;
    const auto attack  = std::exp (-1.0f / (0.0005f * (float) sampleRate));
    const auto release = std::exp (-1.0f / (0.040f * (float) sampleRate));
    const auto smooth  = std::exp (-1.0f / (0.002f * (float) sampleRate));

    float* data[2] = { buffer.getWritePointer (0, start), buffer.getWritePointer (channels > 1 ? 1 : 0, start) };
    float most = 0.0f;

    for (int i = 0; i < num; ++i)
    {
        float lo[2], hi[2], peak = 0.0f;
        for (int c = 0; c < channels; ++c)
        {
            const auto x = data[c][i];
            lo[c] = low.processSample (c, x);
            hi[c] = high.processSample (c, x);
            peak = juce::jmax (peak, std::abs (hi[c]));
        }

        // How loud is the hiss? (fast up, slower down)
        env = peak > env ? attack * env + (1.0f - attack) * peak
                         : release * env + (1.0f - release) * peak;

        const auto levelDb = juce::Decibels::gainToDecibels (env, -120.0f);
        const auto over = levelDb - thr;
        const auto cutDb = over > 0.0f ? juce::jmin (range, over * (1.0f - 1.0f / ratio)) : 0.0f;
        const auto target = juce::Decibels::decibelsToGain (-cutDb);
        gainSmoothed = smooth * gainSmoothed + (1.0f - smooth) * target;
        most = juce::jmax (most, -juce::Decibels::gainToDecibels (gainSmoothed, -60.0f));

        for (int c = 0; c < channels; ++c)
            data[c][i] = lo[c] + hi[c] * gainSmoothed;
    }

    reduction = most;
    if (most > maxReduction.load())
        maxReduction = most;
}
