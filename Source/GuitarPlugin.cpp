#include "GuitarPlugin.h"

const char* GuitarPlugin::xmlTypeName = "gpsGuitar";

GuitarPlugin::GuitarPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto* um = getUndoManager();
    sound.referTo (state, "sound", um, 0);
    volumeDb.referTo (state, "volumeDb", um, 0.0f);
}

GuitarPlugin::~GuitarPlugin()
{
    notifyListenersOfDeletion();
}

void GuitarPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    synth.prepare (info.sampleRate);
}

void GuitarPlugin::reset()
{
    synth.reset();
}

//==============================================================================
void GuitarSynth::prepare (double rate)
{
    sampleRate = rate;
    const auto maxLen = (size_t) (sampleRate / 30.0) + 8;          // below the lowest guitar note
    for (auto& v : voices)
        v.line.assign (maxLen, 0.0f);
    haas.assign ((size_t) (sampleRate * 0.02) + 2, 0.0f);
    reset();
}

void GuitarSynth::reset()
{
    for (auto& v : voices)
    {
        v.active = v.held = false;
        std::fill (v.line.begin(), v.line.end(), 0.0f);
    }
    std::fill (haas.begin(), haas.end(), 0.0f);
    for (auto& z : bodyZ) z[0] = z[1] = 0.0f;
    for (auto& z : body2Z) z[0] = z[1] = 0.0f;
    toneLp = dcX = dcY = 0.0f;
}

void GuitarSynth::allNotesOff()
{
    for (auto& v : voices)
        if (v.active && v.held)
            noteOff (v.note);
}

void GuitarSynth::noteOn (int note, float velocity)
{
    if (voices[0].line.empty())
        return;

    // The same string again (re-plucked), or a free one, or the oldest
    Voice* v = nullptr;
    for (auto& x : voices) if (x.active && x.note == note) v = &x;
    if (v == nullptr) for (auto& x : voices) if (! x.active) { v = &x; break; }
    if (v == nullptr)
    {
        v = &voices[0];
        for (auto& x : voices) if (x.age < v->age) v = &x;
    }

    const bool nylon = sound == 1;
    const double f = 440.0 * std::pow (2.0, (juce::jlimit (28, 100, note) - 69) / 12.0);
    const double period = sampleRate / f;

    // Loop filter: y = s*x + (1-s)*x[n-1] (delay 1-s samples), then a fractional all-pass
    const float s = nylon ? 0.52f : 0.62f + 0.15f * velocity;
    const double total = period - (1.0 - s);
    int n = (int) std::floor (total - 0.5);
    n = juce::jlimit (2, (int) v->line.size() - 2, n);
    const auto delta = (float) (total - n);
    v->apCoeff = (1.0f - delta) / (1.0f + delta);
    v->damp = s;

    const double t60 = juce::jlimit (1.0, 4.5, 4.2 - (note - 40) * 0.05) * (nylon ? 0.7 : 1.0);
    v->loopGain = (float) std::pow (10.0, -3.0 / (t60 * f));

    // The pick: a short burst, darker when played softly, with the pick position's notch
    const bool replucked = v->active;
    std::vector<float> burst ((size_t) n, 0.0f);
    float lp = 0.0f;
    const float bright = juce::jlimit (0.15f, 0.95f, (nylon ? 0.3f : 0.45f) + 0.5f * velocity);
    for (int i = 0; i < n; ++i)
    {
        lp += bright * ((random.nextFloat() * 2.0f - 1.0f) - lp);
        burst[(size_t) i] = lp;
    }
    const int pick = juce::jmax (1, (int) std::round (n * (nylon ? 0.2 : 0.13)));
    float peak = 1.0e-6f;
    for (int i = n - 1; i >= 0; --i)
    {
        burst[(size_t) i] -= i >= pick ? burst[(size_t) (i - pick)] : 0.0f;
        peak = juce::jmax (peak, std::abs (burst[(size_t) i]));
    }
    const float amp = 0.5f * (0.25f + 0.75f * velocity) / peak;

    for (int i = 0; i < (int) v->line.size(); ++i)
    {
        const float old = replucked ? v->line[(size_t) i] * 0.3f : 0.0f;
        v->line[(size_t) i] = old + (i < n ? burst[(size_t) i] * amp : 0.0f);
    }
    v->length = n;
    v->writePos = 0;
    v->apState = 0.0f;
    v->lp = 0.0f;
    v->muteGain = 1.0f;
    v->note = note;
    v->active = v->held = true;
    v->silentFor = 0;
    v->age = ++counter;
}

void GuitarSynth::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.held && v.note == note)
        {
            v.held = false;
            // The hand touches the string: it dies away in about 0.1 s
            const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            v.muteGain = (float) std::pow (10.0, -3.0 / (0.1 * f)) / juce::jmax (0.5f, v.loopGain);
            v.damp = juce::jmin (v.damp, 0.4f);
        }
}

void GuitarSynth::render (float* outL, float* outR, int num)
{
    const bool nylon = sound == 1;
    const float gain = juce::Decibels::decibelsToGain (volumeDb) * 0.55f;

    for (int i = 0; i < num; ++i)
    {
        float x = 0.0f;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const int size = v.length;
            // The sample written 'size' samples ago
            const float y = v.line[(size_t) v.writePos];
            const float ap = v.apCoeff * y + v.apState;
            v.apState = y - v.apCoeff * ap;
            const float filtered = v.damp * ap + (1.0f - v.damp) * v.lp;
            v.lp = ap;
            v.line[(size_t) v.writePos] = filtered * v.loopGain * v.muteGain;
            if (++v.writePos >= size) v.writePos = 0;
            x += ap;

            if (std::abs (ap) < 1.0e-4f) { if (++v.silentFor > 4096) v.active = false; }
            else v.silentFor = 0;
        }

        // DC blocker
        const float dc = x - dcX + 0.995f * dcY;
        dcX = x; dcY = dc;
        x = dc;

        // Body: two wooden resonances (stronger on the nylon guitar), then the tone
        auto resonate = [this] (float in, float z[2], double freq, double q)
        {
            const double w = juce::MathConstants<double>::twoPi * freq / sampleRate;
            const auto r = (float) std::exp (-w / (2.0 * q));
            const auto c = (float) (2.0 * r * std::cos (w));
            const float out = in * (1.0f - r) + c * z[0] - r * r * z[1];
            z[1] = z[0]; z[0] = out;
            return out;
        };
        const float body = resonate (x, bodyZ[0], nylon ? 105.0 : 140.0, 6.0) + resonate (x, body2Z[0], nylon ? 220.0 : 380.0, 5.0);
        x += body * (nylon ? 0.9f : 0.35f);
        const float toneCoeff = nylon ? 0.45f : 0.65f;
        toneLp += toneCoeff * (x - toneLp);
        x = toneLp * gain;

        // A little width (a short echo on the right)
        haas[(size_t) haasPos] = x;
        const int d = (int) haas.size() - 1;
        const float late = haas[(size_t) ((haasPos + 1) % (d + 1))];
        haasPos = (haasPos + 1) % (d + 1);
        outL[i] += x;
        outR[i] += x * 0.85f + late * 0.25f;
    }
}

void GuitarPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    buffer.clear (fc.bufferStartSample, fc.bufferNumSamples);
    auto* outL = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;
    std::vector<float> scratch;
    if (outR == nullptr) { scratch.assign ((size_t) fc.bufferNumSamples, 0.0f); outR = scratch.data(); }

    synth.sound = sound.get();
    synth.volumeDb = volumeDb.get();

    const int num = fc.bufferNumSamples;
    int pos = 0;
    auto renderTo = [&] (int until)
    {
        if (until > pos)
            synth.render (outL + pos, outR + pos, until - pos);
        pos = juce::jmax (pos, until);
    };

    if (fc.bufferForMidiMessages != nullptr)
    {
        if (fc.bufferForMidiMessages->isAllNotesOff)
            synth.allNotesOff();

        for (auto& m : *fc.bufferForMidiMessages)
        {
            renderTo (juce::jlimit (0, juce::jmax (0, num - 1), juce::roundToInt (m.getTimeStamp() * synth.getSampleRate())));
            if (m.isNoteOn())                      synth.noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff())                synth.noteOff (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                synth.allNotesOff();
        }
    }
    renderTo (num);
}
