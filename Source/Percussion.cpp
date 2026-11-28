#include "Percussion.h"

namespace Percussion
{
    const std::vector<Sound>& sounds()
    {
        static const std::vector<Sound> s {
            { djembeBass, "Djembe bass" }, { djembeTone, "Djembe tone" }, { djembeSlap, "Djembe slap" },
            { congaLow, "Conga low" }, { congaOpen, "Conga open" }, { congaMute, "Conga mute" },
            { talkHigh, "Talking drum high" }, { talkLow, "Talking drum low" }, { talkRise, "Talking drum rise" }, { talkFall, "Talking drum fall" },
            { bellHigh, "Bell high" }, { bellLow, "Bell low (ogene)" }, { cowbell, "Cowbell" },
            { shaker, "Shaker" }, { cabasa, "Shaker soft" }, { tambourine, "Tambourine" },
            { claves, "Claves (bottle)" }, { woodblock, "Woodblock" }, { woodblockLow, "Woodblock low" }, { clap, "Clap" },
        };
        return s;
    }

    bool handles (int note)
    {
        for (auto& s : sounds()) if (s.note == note) return true;
        return false;
    }

    juce::String nameOf (int note)
    {
        for (auto& s : sounds()) if (s.note == note) return s.name;
        return {};
    }

    //==========================================================================
    void Synth::prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        reset();
    }

    void Synth::reset()
    {
        for (auto& v : voices) v = Voice();
    }

    Synth::Voice& Synth::allocate (int note)
    {
        // A drum hit again cuts its previous ring (like a hand on the skin); otherwise a free voice or the oldest
        Voice* v = nullptr;
        if (! balafon)
            for (auto& x : voices) if (x.active && x.note == note) v = &x;
        if (v == nullptr) for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& x : voices) if (x.age < v->age) v = &x;
        }
        *v = Voice();
        v->active = true;
        v->note = note;
        v->age = ++counter;
        return *v;
    }

    void Synth::setNoise (Voice& v, float centre, float q, float amp, float decay, float attack, bool highPass)
    {
        const float w = juce::MathConstants<float>::twoPi * juce::jmin (centre, (float) sampleRate * 0.45f) / (float) sampleRate;
        const float r = juce::jlimit (0.5f, 0.995f, 1.0f - w / (2.0f * juce::jmax (0.3f, q)));
        v.bpCoeffR = r;
        v.bpCoeffA = 2.0f * r * std::cos (w);
        v.bpGain = (1.0f - r * r) * 0.9f;
        v.noiseAmp = amp;
        v.noiseDecay = decay;
        v.noiseAttack = attack;
        v.highPass = highPass;
    }

    void Synth::noteOn (int note, float vel)
    {
        vel = juce::jlimit (0.05f, 1.0f, vel);
        const float bright = 0.5f + 0.5f * vel;

        auto mode = [] (Voice& v, float f, float amp, float decay, float startF = 0.0f, float glide = 0.0f)
        {
            if (v.numModes >= (int) v.modes.size()) return;
            auto& m = v.modes[(size_t) v.numModes++];
            m.freq = f; m.startFreq = startF > 0.0f ? startF : f; m.glide = glide; m.amp = amp; m.decay = decay; m.phase = 0.0f;
        };

        if (balafon)
        {
            auto& v = allocate (note);
            const float f = 440.0f * std::pow (2.0f, (note - 69) / 12.0f);
            const float len = juce::jlimit (0.15f, 0.9f, 0.9f - (note - 48) * 0.015f);
            mode (v, f, 0.7f, len);
            mode (v, f * 3.93f, 0.22f * bright, len * 0.22f);
            mode (v, f * 9.2f, 0.07f * bright, len * 0.07f);
            setNoise (v, 2200.0f, 0.8f, 0.25f * bright, 0.004f);
            v.buzz = 0.12f;
            v.pan = juce::jlimit (-0.6f, 0.6f, (note - 66) / 40.0f);
            for (int i = 0; i < v.numModes; ++i) v.modes[(size_t) i].amp *= 0.25f + 0.75f * vel;
            v.noiseAmp *= 0.25f + 0.75f * vel;
            return;
        }

        if (! handles (note))
            return;
        auto& v = allocate (note);

        switch (note)
        {
            case congaOpen:  mode (v, 330, 0.8f, 0.25f, 352, 0.01f); mode (v, 525, 0.25f * bright, 0.12f); mode (v, 706, 0.15f * bright, 0.08f);
                             setNoise (v, 3000, 1.0f, 0.15f * bright, 0.008f); v.pan = 0.25f; break;
            case congaMute:  mode (v, 330, 0.7f, 0.05f, 345, 0.008f); mode (v, 525, 0.2f, 0.03f);
                             setNoise (v, 3200, 1.0f, 0.25f * bright, 0.006f); v.pan = 0.25f; break;
            case congaLow:   mode (v, 220, 0.9f, 0.3f, 236, 0.012f); mode (v, 350, 0.25f * bright, 0.15f); mode (v, 471, 0.1f * bright, 0.1f);
                             setNoise (v, 2600, 1.0f, 0.12f * bright, 0.008f); v.pan = 0.3f; break;
            case djembeBass: mode (v, 70, 1.0f, 0.25f, 110, 0.02f); mode (v, 105, 0.2f, 0.1f);
                             setNoise (v, 400, 1.0f, 0.1f, 0.02f); v.pan = -0.2f; break;
            case djembeTone: mode (v, 250, 0.8f, 0.18f, 270, 0.01f); mode (v, 398, 0.35f * bright, 0.1f); mode (v, 575, 0.2f * bright, 0.06f);
                             setNoise (v, 2500, 1.0f, 0.15f * bright, 0.006f); v.pan = -0.2f; break;
            case djembeSlap: mode (v, 380, 0.4f, 0.06f); mode (v, 604, 0.3f, 0.04f); mode (v, 1007, 0.25f * bright, 0.03f);
                             setNoise (v, 3500, 0.8f, 0.4f * bright, 0.025f); v.pan = -0.2f; break;
            case talkHigh:   mode (v, 260, 0.9f, 0.3f, 230, 0.04f); mode (v, 520, 0.25f * bright, 0.15f); mode (v, 780, 0.1f * bright, 0.1f);
                             setNoise (v, 2000, 1.0f, 0.15f * bright, 0.01f); v.pan = -0.3f; break;
            case talkLow:    mode (v, 180, 0.9f, 0.32f, 165, 0.04f); mode (v, 360, 0.25f * bright, 0.15f); mode (v, 540, 0.1f * bright, 0.1f);
                             setNoise (v, 1800, 1.0f, 0.15f * bright, 0.01f); v.pan = -0.3f; break;
            case talkFall:   mode (v, 190, 0.9f, 0.45f, 300, 0.12f); mode (v, 380, 0.25f * bright, 0.2f, 600, 0.12f);
                             setNoise (v, 2000, 1.0f, 0.15f * bright, 0.01f); v.pan = -0.3f; break;
            case talkRise:   mode (v, 290, 0.9f, 0.45f, 180, 0.1f); mode (v, 580, 0.25f * bright, 0.2f, 360, 0.1f);
                             setNoise (v, 2000, 1.0f, 0.15f * bright, 0.01f); v.pan = -0.3f; break;
            case shaker:     setNoise (v, 6500, 0.7f, 0.7f, 0.04f, 0.012f, true); v.pan = 0.35f; break;
            case cabasa:     setNoise (v, 5000, 0.7f, 0.45f, 0.06f, 0.018f, true); v.pan = 0.35f; break;
            case tambourine: mode (v, 5200, 0.15f, 0.12f); mode (v, 7300, 0.12f, 0.1f); mode (v, 9100, 0.1f, 0.08f);
                             setNoise (v, 9000, 0.6f, 0.5f, 0.12f, 0.002f, true); v.pan = 0.3f; break;
            case bellHigh:   mode (v, 1180, 0.6f, 0.45f); mode (v, 2466, 0.2f * bright, 0.3f); mode (v, 3269, 0.15f * bright, 0.2f); mode (v, 4838, 0.08f * bright, 0.12f);
                             setNoise (v, 4000, 1.0f, 0.2f, 0.003f); v.pan = -0.15f; break;
            case bellLow:    mode (v, 780, 0.65f, 0.6f); mode (v, 1630, 0.2f * bright, 0.35f); mode (v, 2160, 0.15f * bright, 0.25f); mode (v, 3198, 0.08f * bright, 0.15f);
                             setNoise (v, 3500, 1.0f, 0.2f, 0.003f); v.pan = -0.15f; break;
            case cowbell:    mode (v, 560, 0.5f, 0.3f); mode (v, 845, 0.4f, 0.25f); mode (v, 1680, 0.1f * bright, 0.1f); mode (v, 2535, 0.08f * bright, 0.08f);
                             v.pan = -0.1f; break;
            case claves:     mode (v, 2500, 0.7f, 0.035f); setNoise (v, 4000, 1.0f, 0.15f, 0.002f); v.pan = 0.2f; break;
            case woodblock:  mode (v, 1000, 0.6f, 0.05f); mode (v, 2700, 0.25f * bright, 0.02f); setNoise (v, 3000, 1.0f, 0.12f, 0.002f); v.pan = 0.15f; break;
            case woodblockLow: mode (v, 700, 0.6f, 0.06f); mode (v, 1890, 0.25f * bright, 0.025f); setNoise (v, 2500, 1.0f, 0.12f, 0.002f); v.pan = 0.15f; break;
            case clap:       setNoise (v, 1200, 0.9f, 0.35f, 0.1f); v.repeats = 3; v.repeatGap = 0.009f; v.pan = 0.0f; break;
            default: break;
        }
        for (int i = 0; i < v.numModes; ++i) v.modes[(size_t) i].amp *= 0.2f + 0.8f * vel;
        v.noiseAmp *= 0.2f + 0.8f * vel;
    }

    void Synth::noteOff (int) {}

    void Synth::render (float* left, float* right, int num)
    {
        const float dt = 1.0f / (float) sampleRate;
        const float twoPi = juce::MathConstants<float>::twoPi;

        for (auto& v : voices)
        {
            if (! v.active) continue;
            float longest = v.noiseDecay;
            for (int m = 0; m < v.numModes; ++m) longest = juce::jmax (longest, v.modes[(size_t) m].decay);

            const float gl = std::cos ((v.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * juce::MathConstants<float>::sqrt2 * gain * 0.5f;
            const float gr = std::sin ((v.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * juce::MathConstants<float>::sqrt2 * gain * 0.5f;

            for (int i = 0; i < num; ++i)
            {
                float out = 0.0f;
                for (int m = 0; m < v.numModes; ++m)
                {
                    auto& md = v.modes[(size_t) m];
                    const float f = md.glide > 0.0f ? md.freq + (md.startFreq - md.freq) * std::exp (-v.t / md.glide) : md.freq;
                    md.phase += twoPi * f * dt;
                    if (md.phase > twoPi) md.phase -= twoPi;
                    out += std::sin (md.phase) * md.amp * std::exp (-v.t / md.decay);
                }

                if (v.noiseAmp > 0.0f)
                {
                    float env;
                    if (v.repeats > 0)
                    {
                        // Clap: quick bursts, then a short room tail
                        const int k = juce::jmin (v.repeats, (int) (v.t / v.repeatGap));
                        const float tk = v.t - k * v.repeatGap;
                        env = k < v.repeats ? std::exp (-tk / 0.004f) : std::exp (-tk / v.noiseDecay) * 0.7f;
                    }
                    else
                    {
                        env = v.noiseAttack > 0.0f && v.t < v.noiseAttack ? v.t / v.noiseAttack
                                                                         : std::exp (-(v.t - v.noiseAttack) / v.noiseDecay);
                    }
                    float n = random.nextFloat() * 2.0f - 1.0f;
                    if (v.highPass)
                    {
                        const float hp = 0.85f * (v.hpOut + n - v.hpPrev);
                        v.hpPrev = n; v.hpOut = hp; n = hp;
                    }
                    const float y = v.bpGain * n + v.bpCoeffA * v.bp1 - v.bpCoeffR * v.bpCoeffR * v.bp2;
                    v.bp2 = v.bp1; v.bp1 = y;
                    out += y * v.noiseAmp * env * 3.0f;
                }

                if (v.buzz > 0.0f && v.numModes > 0)
                {
                    // The gourd's membrane buzzes when the bar is loud
                    const float s = std::sin (v.modes[0].phase);
                    out += (s > 0.6f ? (random.nextFloat() - 0.5f) : 0.0f) * v.buzz * std::exp (-v.t / (v.modes[0].decay * 0.5f));
                }

                left[i] += out * gl;
                right[i] += out * gr;
                v.t += dt;
            }
            if (v.t > longest * 7.0f + 0.05f + v.repeats * v.repeatGap)
                v.active = false;
        }
    }
}

//==============================================================================
const char* PercussionPlugin::xmlTypeName = "gpsPercussion";

PercussionPlugin::PercussionPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto* um = getUndoManager();
    mode.referTo (state, "mode", um, 0);
    volumeDb.referTo (state, "volumeDb", um, 0.0f);
}

PercussionPlugin::~PercussionPlugin()
{
    notifyListenersOfDeletion();
}

void PercussionPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    synth.prepare (sampleRate);
}

void PercussionPlugin::reset()
{
    synth.reset();
}

void PercussionPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;
    auto& buffer = *fc.destBuffer;
    buffer.clear (fc.bufferStartSample, fc.bufferNumSamples);
    auto* outL = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;
    std::vector<float> scratch;
    if (outR == nullptr) { scratch.assign ((size_t) fc.bufferNumSamples, 0.0f); outR = scratch.data(); }

    synth.balafon = mode.get() == 1;
    synth.gain = juce::Decibels::decibelsToGain (volumeDb.get());

    const int num = fc.bufferNumSamples;
    int pos = 0;
    auto renderTo = [&] (int until)
    {
        if (until > pos) synth.render (outL + pos, outR + pos, until - pos);
        pos = juce::jmax (pos, until);
    };
    if (fc.bufferForMidiMessages != nullptr)
        for (auto& m : *fc.bufferForMidiMessages)
        {
            renderTo (juce::jlimit (0, juce::jmax (0, num - 1), juce::roundToInt (m.getTimeStamp() * sampleRate)));
            if (m.isNoteOn()) synth.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        }
    renderTo (num);
}
