#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
//  Studio Keys: a simple electric-piano-style test instrument.
//  Used in Stage 0 only, to prove the MIDI keyboard and audio output work.
//  It is replaced by the sampled piano and organ in Stage 1, Step 8.
//==============================================================================
struct StudioKeysSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class StudioKeysVoice final : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<StudioKeysSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        const auto sr = (float) getSampleRate();
        const auto hz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

        phase       = 0.0;
        phaseDelta  = juce::MathConstants<double>::twoPi * hz / getSampleRate();
        amplitude   = 0.16f * (0.25f + 0.75f * velocity);
        envelope    = 0.0f;
        attackStep  = 1.0f / (0.004f * sr);                       // 4 ms attack
        decayMul    = std::pow (0.001f, 1.0f / (3.5f * sr));      // -60 dB in 3.5 s
        releaseMul  = std::pow (0.001f, 1.0f / (0.30f * sr));     // -60 dB in 0.3 s
        attacking   = true;
        releasing   = false;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            releasing = true;
        }
        else
        {
            clearCurrentNote();
            phaseDelta = 0.0;
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (juce::exactlyEqual (phaseDelta, 0.0))
            return;

        const auto numChannels = output.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            if (attacking)
            {
                envelope += attackStep;
                if (envelope >= 1.0f) { envelope = 1.0f; attacking = false; }
            }
            else
            {
                envelope *= releasing ? releaseMul : decayMul;
            }

            const auto sample = (float) (amplitude * envelope
                                 * (std::sin (phase)
                                    + 0.30 * std::sin (2.0 * phase)
                                    + 0.08 * std::sin (3.0 * phase)));

            phase += phaseDelta;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            for (int ch = 0; ch < numChannels; ++ch)
                output.addSample (ch, startSample + i, sample);

            if (! attacking && envelope < 0.0005f)
            {
                clearCurrentNote();
                phaseDelta = 0.0;
                break;
            }
        }
    }

private:
    double phase = 0.0, phaseDelta = 0.0;
    float amplitude = 0.0f, envelope = 0.0f;
    float attackStep = 0.0f, decayMul = 1.0f, releaseMul = 1.0f;
    bool attacking = false, releasing = false;
};
