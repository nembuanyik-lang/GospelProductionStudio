#include "OrganPlugin.h"

const char* OrganPlugin::xmlTypeName = "GospelOrgan";
const char* OrganPlugin::drawbarNames[9] = { "16'", "5 1/3'", "8'", "4'", "2 2/3'", "2'", "1 3/5'", "1 1/3'", "1'" };

namespace
{
    // Semitones above the played key for each drawbar
    constexpr int drawbarOffset[9] = { -12, 7, 0, 12, 19, 24, 28, 31, 36 };
    constexpr int lowestWheel = 24, highestWheel = 113;    // the tonewheel range; footages fold back inside it
    constexpr int sineSize = 4096;

    void setLowPass (auto& f, double sr, double hz)
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / sr;
        const auto c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * 0.7071);
        const auto a0 = 1.0 + alpha;
        f.b0 = (float) ((1.0 - c) / 2.0 / a0); f.b1 = (float) ((1.0 - c) / a0); f.b2 = f.b0;
        f.a1 = (float) (-2.0 * c / a0); f.a2 = (float) ((1.0 - alpha) / a0);
    }

    void setHighPass (auto& f, double sr, double hz)
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / sr;
        const auto c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * 0.7071);
        const auto a0 = 1.0 + alpha;
        f.b0 = (float) ((1.0 + c) / 2.0 / a0); f.b1 = (float) (-(1.0 + c) / a0); f.b2 = f.b0;
        f.a1 = (float) (-2.0 * c / a0); f.a2 = (float) ((1.0 - alpha) / a0);
    }

    float readDelay (const std::vector<float>& buf, int write, float delaySamples)
    {
        const auto size = (int) buf.size();
        auto pos = (float) write - delaySamples;
        while (pos < 0) pos += (float) size;
        const auto i = (int) pos;
        const auto frac = pos - (float) i;
        const auto a = buf[(size_t) (i % size)], b = buf[(size_t) ((i + 1) % size)];
        return a + (b - a) * frac;
    }
}

//==============================================================================
const std::vector<OrganPlugin::Preset>& OrganPlugin::presets()
{
    static const std::vector<Preset> list {
        { "Gospel Full",          "88 8800 008", false, true,  false, true,  6, 1, 0.35f },
        { "Gospel Classic",       "88 8000 000", true,  true,  false, true,  6, 1, 0.25f },
        { "Praise Break (Shout)", "88 8888 888", false, true,  false, true,  6, 2, 0.55f },
        { "Sweet Ballad",         "00 8800 000", false, true,  true,  false, 4, 1, 0.05f },
        { "Soft Worship Pad",     "00 6600 000", false, true,  true,  false, 1, 1, 0.0f  },
        { "Comping Stabs",        "80 0000 008", true,  true,  false, true,  6, 1, 0.3f  },
        { "Soul Classic",         "88 6000 000", false, true,  false, true,  6, 1, 0.3f  },
        { "Flute Lead",           "00 8000 000", false, true,  false, true,  3, 1, 0.0f  },
        { "Bright Choir Lift",    "00 8888 800", false, true,  false, true,  6, 2, 0.15f },
    };
    return list;
}

OrganPlugin::OrganPlugin (te::PluginCreationInfo info)
    : te::Plugin (info)
{
    auto* um = getUndoManager();
    const int defaults[9] = { 8, 8, 8, 8, 0, 0, 0, 0, 8 };
    for (int i = 0; i < 9; ++i)
        drawbars[(size_t) i].referTo (state, juce::Identifier ("drawbar" + juce::String (i)), um, defaults[i]);

    percOn.referTo (state, "percOn", um, false);
    percThird.referTo (state, "percThird", um, true);
    percSoft.referTo (state, "percSoft", um, false);
    percFast.referTo (state, "percFast", um, true);
    vibrato.referTo (state, "vibrato", um, 6);
    leslie.referTo (state, "leslie", um, 1);
    drive.referTo (state, "drive", um, 0.35f);
    click.referTo (state, "click", um, 0.5f);
    volumeDb.referTo (state, "volumeDb", um, 0.0f);
    presetName.referTo (state, "preset", um, "Gospel Full");

    sine.resize (sineSize + 1);
    for (int i = 0; i <= sineSize; ++i)
        sine[(size_t) i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * i / sineSize);

    for (auto& g : barGain) g = 0.0f;
    refreshSettings();
}

OrganPlugin::~OrganPlugin()
{
    notifyListenersOfDeletion();
}

void OrganPlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v == state)
        refreshSettings();
    te::Plugin::valueTreePropertyChanged (v, id);
}

void OrganPlugin::refreshSettings()
{
    for (size_t i = 0; i < 9; ++i)
    {
        const auto n = juce::jlimit (0, 8, drawbars[i].get());
        // Each drawbar step is about 3 dB; 0 is silent
        barGain[i] = n == 0 ? 0.0f : std::pow (10.0f, -3.0f * (float) (8 - n) / 20.0f);
    }
    aPercOn = percOn.get();
    aPercThird = percThird.get();
    aPercSoft = percSoft.get();
    aPercFast = percFast.get();
    aVibrato = juce::jlimit (0, 6, vibrato.get());
    aLeslie = juce::jlimit (0, 2, leslie.get());
    aDrive = juce::jlimit (0.0f, 1.0f, drive.get());
    aClick = juce::jlimit (0.0f, 1.0f, click.get());
    aGain = juce::Decibels::decibelsToGain (juce::jlimit (-40.0f, 12.0f, volumeDb.get()));
    modWheelFast = -1;      // the panel's choice wins until the mod wheel moves again
}

juce::String OrganPlugin::getRegistration() const
{
    juce::String s;
    for (int i = 0; i < 9; ++i)
    {
        s << juce::String (juce::jlimit (0, 8, drawbars[(size_t) i].get()));
        if (i == 1 || i == 5) s << " ";
    }
    return s;
}

void OrganPlugin::applyPreset (const Preset& p)
{
    const juce::String reg = juce::String (p.registration).removeCharacters (" ");
    for (int i = 0; i < 9 && i < reg.length(); ++i)
        drawbars[(size_t) i] = reg[i] - '0';
    percOn = p.perc;
    percThird = p.third;
    percSoft = p.soft;
    percFast = p.fast;
    vibrato = p.vibrato;
    leslie = p.leslie;
    drive = p.drive;
    presetName = p.name;
}

void OrganPlugin::setLeslie (int mode)
{
    leslie = juce::jlimit (0, 2, mode);
}

//==============================================================================
void OrganPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;

    for (int p = 0; p < 128; ++p)
        phaseInc[(size_t) p] = 440.0 * std::pow (2.0, (p - 69) / 12.0) / sampleRate;

    vibBuffer.assign ((size_t) (sampleRate * 0.01) + 4, 0.0f);
    hornBuffer.assign ((size_t) (sampleRate * 0.01) + 4, 0.0f);
    drumBuffer.assign ((size_t) (sampleRate * 0.01) + 4, 0.0f);
    vibWrite = rotorWrite = 0;

    setLowPass (lowSplit, sampleRate, 800.0);
    setLowPass (lowSplit2, sampleRate, 800.0);
    setHighPass (highSplit, sampleRate, 800.0);
    setHighPass (highSplit2, sampleRate, 800.0);
}

void OrganPlugin::reset()
{
    for (auto& k : keys)
        k = Key();
    keysDown = 0;
}

void OrganPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    buffer.clear (fc.bufferStartSample, fc.bufferNumSamples);
    auto* outL = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;
    std::vector<float> scratch;
    if (outR == nullptr) { scratch.assign ((size_t) fc.bufferNumSamples, 0.0f); outR = scratch.data(); }

    const int num = fc.bufferNumSamples;
    int pos = 0;

    auto renderTo = [&] (int until)
    {
        if (until > pos)
            renderBlock (outL + pos, outR + pos, until - pos);
        pos = juce::jmax (pos, until);
    };

    if (fc.bufferForMidiMessages != nullptr)
    {
        if (fc.bufferForMidiMessages->isAllNotesOff)
        {
            for (auto& k : keys) k.down = false;
            keysDown = 0;
        }

        for (auto& m : *fc.bufferForMidiMessages)
        {
            const auto offset = juce::jlimit (0, juce::jmax (0, num - 1), juce::roundToInt (m.getTimeStamp() * sampleRate));
            renderTo (offset);

            if (m.isNoteOn())
            {
                const auto note = m.getNoteNumber();

                // Percussion is single-trigger: only when no other key is held
                const bool triggerPerc = keysDown == 0;

                Key* slot = nullptr;
                for (auto& k : keys)
                    if (k.active && k.note == note) { slot = &k; break; }
                if (slot == nullptr)
                    for (auto& k : keys)
                        if (! k.active) { slot = &k; break; }
                if (slot == nullptr)
                {
                    slot = &keys[0];
                    for (auto& k : keys)
                        if (! k.down && k.level < slot->level) slot = &k;
                }

                if (! slot->down) ++keysDown;
                slot->active = true;
                slot->down = true;
                slot->note = note;
                slot->clickEnv = 1.0f;
                if (triggerPerc) slot->percEnv = 1.0f;

                for (int d = 0; d < 9; ++d)
                {
                    auto w = note + drawbarOffset[d];
                    while (w > highestWheel) w -= 12;
                    while (w < lowestWheel)  w += 12;
                    slot->wheels[d] = w;
                }
            }
            else if (m.isNoteOff())
            {
                for (auto& k : keys)
                    if (k.active && k.down && k.note == m.getNoteNumber())
                    {
                        k.down = false;
                        k.clickEnv = juce::jmax (k.clickEnv, 0.4f);
                        keysDown = juce::jmax (0, keysDown - 1);
                    }
            }
            else if (m.isControllerOfType (1))
            {
                modWheelFast = m.getControllerValue() >= 64 ? 1 : 0;
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                for (auto& k : keys) k.down = false;
                keysDown = 0;
            }
        }
    }

    renderTo (num);
}

void OrganPlugin::renderBlock (float* outL, float* outR, int num)
{
    const auto sr = (float) sampleRate;
    const auto contactOn = 1.0f / (0.0015f * sr), contactOff = 1.0f / (0.006f * sr);
    const auto clickDecay = std::exp (-1.0f / (0.004f * sr));

    const auto percOnNow = aPercOn.load();
    const auto percDecay = std::exp (std::log (0.001f) / ((aPercFast.load() ? 0.45f : 1.6f) * sr));
    const auto percLevel = aPercSoft.load() ? 0.5f : 1.0f;
    const auto percWheelOffset = aPercThird.load() ? 19 : 12;
    // With percussion on (normal volume) the manual drops about 3 dB, and the 1' drawbar is used by the percussion circuit
    const auto manualGain = percOnNow && ! aPercSoft.load() ? 0.7f : 1.0f;

    float gains[9];
    for (size_t d = 0; d < 9; ++d)
        gains[d] = barGain[d].load() * manualGain;
    if (percOnNow)
        gains[8] = 0.0f;

    const auto clickAmount = aClick.load() * 0.25f;
    const auto vibMode = aVibrato.load();
    const float vibDepthMs[] = { 0.0f, 0.25f, 0.5f, 0.9f, 0.25f, 0.5f, 0.9f };
    const auto vibDepth = vibDepthMs[vibMode] * 0.001f * sr;
    const auto vibChorus = vibMode >= 4;
    const auto vibInc = 6.9 / sampleRate;

    const auto drv = aDrive.load();
    const auto driveGain = 1.0f + drv * 7.0f;
    const auto driveComp = 1.0f / std::tanh (driveGain * 0.5f) * 0.5f;

    // Leslie speeds: the panel setting, or the mod wheel once it has moved
    const auto leslieMode = aLeslie.load();
    const auto wheel = modWheelFast.load();
    const auto fast = leslieMode != 0 && (wheel >= 0 ? wheel == 1 : leslieMode == 2);
    currentFast = fast;
    const auto hornTarget = fast ? 6.8 : 0.8, drumTarget = fast ? 5.9 : 0.67;
    const auto hornRamp = 1.0 - std::exp (-1.0 / (0.7 * sampleRate));   // horn speeds up in about a second
    const auto drumRamp = 1.0 - std::exp (-1.0 / (3.0 * sampleRate));   // the heavy drum takes several seconds
    const auto gainOut = aGain.load() * 0.16f;

    for (int i = 0; i < num; ++i)
    {
        float s = 0.0f;
        float clickSum = 0.0f;

        for (auto& k : keys)
        {
            if (! k.active)
                continue;

            if (k.down) k.level = juce::jmin (1.0f, k.level + contactOn);
            else
            {
                k.level -= contactOff;
                if (k.level <= 0.0f) { k.active = false; k.level = 0.0f; continue; }
            }

            float v = 0.0f;
            for (int d = 0; d < 9; ++d)
            {
                if (gains[d] <= 0.0f) continue;
                const auto ph = phase[(size_t) k.wheels[d]] * sineSize;
                const auto idx = (int) ph;
                const auto frac = (float) (ph - idx);
                v += gains[d] * (sine[(size_t) idx] + (sine[(size_t) idx + 1] - sine[(size_t) idx]) * frac);
            }

            if (percOnNow && k.percEnv > 0.0001f)
            {
                auto w = k.note + percWheelOffset;
                while (w > highestWheel) w -= 12;
                const auto ph = phase[(size_t) w] * sineSize;
                const auto idx = (int) ph;
                v += percLevel * k.percEnv * 1.2f * sine[(size_t) idx];
                k.percEnv *= percDecay;
            }

            s += v * k.level;

            if (k.clickEnv > 0.001f)
            {
                clickSum += k.clickEnv;
                k.clickEnv *= clickDecay;
            }
        }

        // Key click: a short, filtered burst at each key contact
        if (clickSum > 0.0f && clickAmount > 0.0f)
        {
            clickFilter += 0.35f * ((noise.nextFloat() * 2.0f - 1.0f) - clickFilter);
            s += clickFilter * clickSum * clickAmount;
        }

        // Advance every tonewheel (they always turn, so chords stay phase-locked like the real thing)
        for (int w = lowestWheel; w <= highestWheel; ++w)
        {
            auto& p = phase[(size_t) w];
            p += phaseInc[(size_t) w];
            if (p >= 1.0) p -= 1.0;
        }

        // Scanner vibrato / chorus
        if (vibMode > 0)
        {
            vibBuffer[(size_t) vibWrite] = s;
            vibPhase += vibInc;
            if (vibPhase >= 1.0) vibPhase -= 1.0;
            const auto delay = 2.0f + vibDepth * (0.5f + 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * vibPhase));
            const auto wet = readDelay (vibBuffer, vibWrite, delay);
            vibWrite = (vibWrite + 1) % (int) vibBuffer.size();
            s = vibChorus ? 0.5f * (s + wet) * 1.4f : wet;
        }

        // Overdrive (the tube preamp)
        if (drv > 0.001f)
            s = std::tanh (s * driveGain * 0.5f) * driveComp * 2.0f;

        // Leslie
        float l, r;
        if (leslieMode == 0)
        {
            l = r = s;
        }
        else
        {
            hornSpeed += (hornTarget - hornSpeed) * hornRamp;
            drumSpeed += (drumTarget - drumSpeed) * drumRamp;
            hornAngle += hornSpeed / sampleRate;
            drumAngle += drumSpeed / sampleRate;
            if (hornAngle >= 1.0) hornAngle -= 1.0;
            if (drumAngle >= 1.0) drumAngle -= 1.0;

            const auto low = lowSplit2.process (lowSplit.process (s));
            const auto high = highSplit2.process (highSplit.process (s));

            hornBuffer[(size_t) rotorWrite] = high;
            drumBuffer[(size_t) rotorWrite] = low;

            const auto twoPi = juce::MathConstants<double>::twoPi;
            const auto hA = (float) (hornAngle * twoPi), dA = (float) (drumAngle * twoPi);
            const auto hornDepth = 0.0007f * sr, drumDepth = 0.0003f * sr;

            // Two microphones, left and right of the cabinet
            const auto hornL = readDelay (hornBuffer, rotorWrite, 3.0f + hornDepth * (1.0f + std::sin (hA)))
                                 * (0.62f + 0.38f * std::cos (hA));
            const auto hornR = readDelay (hornBuffer, rotorWrite, 3.0f + hornDepth * (1.0f - std::sin (hA)))
                                 * (0.62f - 0.38f * std::cos (hA));
            const auto drumL = readDelay (drumBuffer, rotorWrite, 3.0f + drumDepth * (1.0f + std::sin (dA)))
                                 * (0.78f + 0.22f * std::cos (dA));
            const auto drumR = readDelay (drumBuffer, rotorWrite, 3.0f + drumDepth * (1.0f - std::sin (dA)))
                                 * (0.78f - 0.22f * std::cos (dA));

            rotorWrite = (rotorWrite + 1) % (int) hornBuffer.size();
            l = (hornL + drumL) * 1.15f;
            r = (hornR + drumR) * 1.15f;
        }

        outL[i] += l * gainOut;
        outR[i] += r * gainOut;
    }
}
