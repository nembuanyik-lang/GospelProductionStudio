#include "SamplerPlugin.h"
#include "MySounds.h"

const char* SamplerPlugin::xmlTypeName = "GospelSampler";

namespace
{
    constexpr int maxVoices = 96;
    constexpr float killSeconds = 0.006f;
}

//==============================================================================
SamplerPlugin::SamplerPlugin (te::PluginCreationInfo info)
    : te::Plugin (info)
{
    auto* um = getUndoManager();
    instrumentId.referTo (state, "instrument", um, "grand");
    volumeDb.referTo (state, "volumeDb", um, 0.0f);
    tone.referTo (state, "tone", um, 1.0f);
    releaseScale.referTo (state, "releaseScale", um, 1.0f);
    touch.referTo (state, "touch", um, 1);

    voices.resize ((size_t) maxVoices);
    for (int i = 0; i < 24; ++i)
        fallback.addVoice (new StudioKeysVoice());
    fallback.addSound (new StudioKeysSound());

    refreshSettings();
    SoundPacks::changes().addChangeListener (this);
    reloadInstrument();
}

SamplerPlugin::~SamplerPlugin()
{
    alive->store (false);
    SoundPacks::changes().removeChangeListener (this);
    notifyListenersOfDeletion();
}

juce::String SamplerPlugin::getName() const
{
    if (auto* c = Instruments::find (instrumentId.get()))
        return c->name;
    return getPluginName();
}

juce::String SamplerPlugin::getStatusText() const
{
    switch (status)
    {
        case Status::loading:      return "loading";
        case Status::notInstalled: return hasBuiltInSound() ? "built-in sound" : "not downloaded";
        case Status::failed:       return "could not load";
        case Status::ready:        break;
    }
    return {};
}

juce::int64 SamplerPlugin::getMemoryBytes() const
{
    return instrument != nullptr ? instrument->memoryBytes : 0;
}

void SamplerPlugin::changeListenerCallback (juce::ChangeBroadcaster*)
{
    reloadInstrument();     // a pack was installed or removed
}

void SamplerPlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v == state)
    {
        if (id == juce::Identifier ("instrument"))
            reloadInstrument();
        refreshSettings();
    }
    else if (MySounds::isSlotTree (v) && isMySounds())
    {
        // Several changes often come together (key and mode): reload once
        if (! reloadPending)
        {
            reloadPending = true;
            auto flag = alive;
            juce::MessageManager::callAsync ([flag, this]
            {
                if (! flag->load()) return;
                reloadPending = false;
                reloadInstrument();
            });
        }
    }
    te::Plugin::valueTreePropertyChanged (v, id);
}

void SamplerPlugin::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (MySounds::isSlotTree (child) && isMySounds())
        reloadInstrument();
    te::Plugin::valueTreeChildAdded (parent, child);
}

void SamplerPlugin::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index)
{
    if (MySounds::isSlotTree (child) && isMySounds())
        reloadInstrument();
    te::Plugin::valueTreeChildRemoved (parent, child, index);
}

void SamplerPlugin::refreshSettings()
{
    gain = juce::Decibels::decibelsToGain (juce::jlimit (-40.0f, 12.0f, volumeDb.get()));
    toneAmount = juce::jlimit (0.0f, 1.0f, tone.get());
    releaseMul = juce::jlimit (0.1f, 4.0f, releaseScale.get());
    touchMode = juce::jlimit (0, 2, touch.get());
    drumKit = instrumentId.get() == "drums";
    const auto id = instrumentId.get();
    guitarSound = id == "guitar" ? 0 : id == "guitarNylon" ? 1 : -1;
    percussionMode = id == "percussion" ? 0 : id == "balafon" ? 1 : -1;
}

bool SamplerPlugin::kitHasNote (int note) const
{
    if (const auto* inst = instrument.get())
        for (const auto& r : inst->regions)
            if (note >= r.lokey && note <= r.hikey) return true;
    return false;
}

void SamplerPlugin::reloadInstrument()
{
    const auto* choice = getChoice();
    const auto generation = ++loadGeneration;

    auto setInstrument = [this] (Sfz::InstrumentPtr inst)
    {
        Sfz::InstrumentPtr old;
        {
            const juce::SpinLock::ScopedLockType sl (instrumentLock);
            old = instrument;
            instrument = inst;
            instrumentChanged = true;
        }
        old.reset();    // freed here, on the message thread
    };

    userKit = isMySounds();
    if (isMySounds())
    {
        const auto sfz = MySounds::writeSfz (state);
        if (! sfz.existsAsFile())
        {
            status = Status::ready;       // nothing added yet: silent
            setInstrument (nullptr);
            changed();
            return;
        }
        status = Status::loading;
        changed();
        auto flag = alive;
        auto* self = this;
        Sfz::Library::getInstance().request (sfz, [flag, self, generation, setInstrument] (Sfz::InstrumentPtr inst)
        {
            if (! flag->load() || generation != self->loadGeneration)
                return;
            setInstrument (inst);
            self->status = inst != nullptr ? Status::ready : Status::failed;
            self->changed();
        });
        return;
    }

    if (choice == nullptr || choice->kind != Instruments::Kind::sampler)
    {
        status = Status::failed;
        setInstrument (nullptr);
        return;
    }

    const auto file = SoundPacks::findInstrument (choice->pack, choice->patch);
    if (! file.existsAsFile())
    {
        status = Status::notInstalled;
        setInstrument (nullptr);
        changed();
        return;
    }

    status = Status::loading;
    changed();

    // The load finishes later: only touch this plugin if it still exists
    // (never keep it alive, its song may have been closed meanwhile).
    auto flag = alive;
    auto* self = this;
    // The Drum Kit plays the recorded African percussion for notes it has no sample for (Step 32)
    const auto extra = instrumentId.get() == "drums" ? SoundPacks::findInstrument ("percussion-african", "Percussion.sfz") : juce::File();

    Sfz::Library::getInstance().request (file, [flag, self, generation, setInstrument] (Sfz::InstrumentPtr inst)
    {
        if (! flag->load() || generation != self->loadGeneration)
            return;

        setInstrument (inst);
        self->status = inst != nullptr ? Status::ready : Status::failed;
        self->changed();
    }, extra);
}

//==============================================================================
void SamplerPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    fallback.setCurrentPlaybackSampleRate (sampleRate);
    guitarFallback.prepare (sampleRate);
    midi.ensureSize (8192);
    percussion.prepare (sampleRate);
    percHits.reserve (256);
}

void SamplerPlugin::reset()
{
    for (auto& v : voices)
        v.active = false;
    fallback.allNotesOff (0, false);
    guitarFallback.reset();
    pedalDown = false;
    percussion.reset();
}

SamplerPlugin::Voice* SamplerPlugin::findFreeVoice()
{
    Voice* oldestReleasing = nullptr;
    Voice* oldest = nullptr;
    for (auto& v : voices)
    {
        if (! v.active)
            return &v;
        if (v.stage == Voice::Stage::release && (oldestReleasing == nullptr || v.startTime < oldestReleasing->startTime))
            oldestReleasing = &v;
        if (oldest == nullptr || v.startTime < oldest->startTime)
            oldest = &v;
    }
    auto* victim = oldestReleasing != nullptr ? oldestReleasing : oldest;
    victim->active = false;
    return victim;
}

void SamplerPlugin::startRegion (const Sfz::Region& r, int note, int velocity, float extraGainDb)
{
    const auto* inst = instrument.get();
    if (inst == nullptr || ! juce::isPositiveAndBelow (r.sampleIndex, (int) inst->samples.size()))
        return;

    // Choke: this region's group silences regions that are "off by" it (open hi-hat by the pedal hi-hat)
    if (r.group != 0)
        for (auto& v : voices)
            if (v.active && v.region != nullptr && v.region->offBy == r.group)
            {
                if (v.region->offFast) { v.stage = Voice::Stage::kill; v.envStep = v.env / juce::jmax (1.0f, killSeconds * (float) sampleRate); }
                else                   releaseVoice (v);
            }

    auto* v = findFreeVoice();
    const auto& s = inst->samples[(size_t) r.sampleIndex];

    v->active = true;
    v->region = &r;
    v->sample = &s;
    v->note = note;
    v->velocity = velocity;
    v->pos = (double) juce::jlimit ((juce::int64) 0, juce::jmax ((juce::int64) 0, s.length - 2), r.offset);

    const auto semis = (note - r.keycenter) * r.keytrack / 100.0f + r.transpose + r.tune / 100.0f + pitchBend;
    v->inc = std::pow (2.0, semis / 12.0) * s.sampleRate / sampleRate;

    // Velocity: amp_veltrack with the chosen touch
    const auto exponent = touchMode.load() == 0 ? 0.6f : touchMode.load() == 2 ? 1.4f : 1.0f;
    const auto vel = std::pow (velocity / 127.0f, exponent);
    const auto vt = r.ampVeltrack / 100.0f;
    const auto velGain = (1.0f - vt) + vt * vel * vel;
    const auto g = velGain * juce::Decibels::decibelsToGain (r.volume + extraGainDb);

    const auto pan = juce::jlimit (-1.0f, 1.0f, r.pan / 100.0f);
    v->gainL = g * juce::jmin (1.0f, 1.0f - pan);
    v->gainR = g * juce::jmin (1.0f, 1.0f + pan);

    // Envelope
    v->sustainLevel = juce::jlimit (0.0f, 1.0f, r.sustain / 100.0f);
    v->env = 0.0f;
    const auto attackSamples = juce::jmax (1, (int) (r.attack * sampleRate));
    v->stage = Voice::Stage::attack;
    v->stageSamples = attackSamples;
    v->envStep = 1.0f / (float) attackSamples;
    if (r.attack <= 0.0f) { v->env = 1.0f; v->stage = Voice::Stage::hold; v->stageSamples = (int) (r.hold * sampleRate); }

    const auto rel = juce::jmax (0.005f, r.release * releaseMul.load());
    v->releaseCoef = (float) std::exp (std::log (0.001) / (rel * sampleRate));
    v->keyDown = true;
    v->sustained = false;
    v->startTime = sampleClock;
}

void SamplerPlugin::noteOn (int note, int velocity)
{
    const auto* inst = instrument.get();
    if (inst == nullptr)
        return;

    noteOnTime[(size_t) note] = sampleClock;
    noteVelocity[(size_t) note] = velocity;

    // Retriggering the same key while it still rings (pedal) fades the old one gently
    for (auto& v : voices)
        if (v.active && v.note == note && v.region != nullptr && v.region->trigger == Sfz::Trigger::attack
             && v.region->loopMode != Sfz::LoopMode::oneShot && ! v.keyDown)
            releaseVoice (v);

    const auto rnd = random.nextFloat();
    const auto rr = roundRobin[(size_t) note];

    for (const auto& r : inst->regions)
    {
        if (r.trigger != Sfz::Trigger::attack) continue;
        if (note < r.lokey || note > r.hikey || velocity < r.lovel || velocity > r.hivel) continue;
        if (rnd < r.lorand || rnd >= r.hirand) continue;
        if (r.seqLength > 1 && (rr % r.seqLength) + 1 != r.seqPosition) continue;
        startRegion (r, note, velocity, 0.0f);
    }
    roundRobin[(size_t) note] = rr + 1;
}

void SamplerPlugin::releaseVoice (Voice& v)
{
    if (v.stage == Voice::Stage::release || v.stage == Voice::Stage::kill)
        return;
    v.stage = Voice::Stage::release;
}

void SamplerPlugin::noteOff (int note)
{
    const auto* inst = instrument.get();
    bool released = false;

    for (auto& v : voices)
    {
        if (! v.active || v.note != note || ! v.keyDown || v.region == nullptr)
            continue;
        v.keyDown = false;
        if (v.region->trigger != Sfz::Trigger::attack)
            continue;
        if (pedalDown) { v.sustained = true; continue; }
        if (v.region->loopMode == Sfz::LoopMode::oneShot) continue;
        releaseVoice (v);
        released = true;
    }

    // Release samples (damper noise) play when the note really stops
    if (inst != nullptr && ! pedalDown)
    {
        const auto heldSeconds = (float) ((sampleClock - noteOnTime[(size_t) note]) / sampleRate);
        const auto velocity = juce::jmax (1, noteVelocity[(size_t) note]);
        for (const auto& r : inst->regions)
            if (r.trigger == Sfz::Trigger::release && note >= r.lokey && note <= r.hikey
                 && velocity >= r.lovel && velocity <= r.hivel)
                startRegion (r, note, velocity, -r.rtDecay * heldSeconds);
    }
    juce::ignoreUnused (released);
}

//==============================================================================
void SamplerPlugin::renderVoices (juce::AudioBuffer<float>& buffer, int start, int num)
{
    auto* outL = buffer.getWritePointer (0, start);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : outL;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        const auto& s = *v.sample;
        const auto& r = *v.region;
        const auto* d0 = s.channels[0].data();
        const auto* d1 = s.channels.size() > 1 ? s.channels[1].data() : d0;
        const auto end = (double) juce::jmin (r.end, s.length) - 1.0;
        const auto looping = (r.loopMode == Sfz::LoopMode::continuous || (r.loopMode == Sfz::LoopMode::sustain && v.keyDown))
                               && r.loopEnd > r.loopStart;
        const auto loopLen = (double) (r.loopEnd - r.loopStart);
        constexpr float scale = 1.0f / 32768.0f;

        for (int i = 0; i < num; ++i)
        {
            // Envelope
            switch (v.stage)
            {
                case Voice::Stage::attack:
                    v.env += v.envStep;
                    if (--v.stageSamples <= 0) { v.env = 1.0f; v.stage = Voice::Stage::hold; v.stageSamples = (int) (r.hold * sampleRate); }
                    break;
                case Voice::Stage::hold:
                    if (--v.stageSamples <= 0)
                    {
                        v.stage = Voice::Stage::decay;
                        v.stageSamples = juce::jmax (1, (int) (r.decay * sampleRate));
                        v.envStep = (1.0f - v.sustainLevel) / (float) v.stageSamples;
                    }
                    break;
                case Voice::Stage::decay:
                    v.env -= v.envStep;
                    if (--v.stageSamples <= 0) { v.env = v.sustainLevel; v.stage = Voice::Stage::sustain; }
                    break;
                case Voice::Stage::sustain:
                    break;
                case Voice::Stage::release:
                    v.env *= v.releaseCoef;
                    if (v.env < 0.0001f) { v.active = false; }
                    break;
                case Voice::Stage::kill:
                    v.env -= v.envStep;
                    if (v.env <= 0.0f) { v.active = false; }
                    break;
            }
            if (! v.active)
                break;

            // Sample with linear interpolation
            const auto ip = (juce::int64) v.pos;
            const auto frac = (float) (v.pos - (double) ip);
            const auto a0 = d0[ip] * scale, b0 = d0[ip + 1] * scale;
            const auto a1 = d1[ip] * scale, b1 = d1[ip + 1] * scale;
            const auto l = a0 + (b0 - a0) * frac;
            const auto rr = a1 + (b1 - a1) * frac;

            outL[i] += l * v.gainL * v.env;
            outR[i] += rr * v.gainR * v.env;

            v.pos += v.inc;
            if (looping && v.pos >= (double) r.loopEnd)
                v.pos -= loopLen;
            if (v.pos >= end)
            {
                v.active = false;
                break;
            }
        }
    }
}

void SamplerPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    buffer.clear (fc.bufferStartSample, fc.bufferNumSamples);

    const juce::SpinLock::ScopedTryLockType sl (instrumentLock);
    if (! sl.isLocked())
        return;

    if (instrumentChanged.exchange (false))
        for (auto& v : voices)
            v.active = false;

    const bool useFallback = instrument == nullptr && ! userKit.load();
    const bool kit = drumKit.load();
    const int percMode = percussionMode.load();
    const bool hybrid = kit || percMode >= 0;
    midi.clear();
    percHits.clear();

    // Split the block at each MIDI event
    int pos = 0;
    const int num = fc.bufferNumSamples;

    auto renderTo = [&] (int until)
    {
        if (until > pos && ! useFallback)
            renderVoices (buffer, fc.bufferStartSample + pos, until - pos);
        sampleClock += (juce::uint64) juce::jmax (0, until - pos);
        pos = juce::jmax (pos, until);
    };

    if (fc.bufferForMidiMessages != nullptr)
    {
        if (fc.bufferForMidiMessages->isAllNotesOff)
        {
            for (auto& v : voices)
                if (v.active) releaseVoice (v);
            pedalDown = false;
            fallback.allNotesOff (0, true);
            guitarFallback.allNotesOff();
        }

        for (auto& m : *fc.bufferForMidiMessages)
        {
            const auto offset = juce::jlimit (0, juce::jmax (0, num - 1), juce::roundToInt (m.getTimeStamp() * sampleRate));

            const bool toSynth = kit ? Percussion::handles (m.getNoteNumber()) && ! kitHasNote (m.getNoteNumber())
                                     : percMode >= 0 && (useFallback || ! kitHasNote (m.getNoteNumber()));
            if (hybrid && (m.isNoteOn() || m.isNoteOff()) && toSynth)
            {
                if (m.isNoteOn() && percHits.size() < 256)
                    percHits.push_back ({ offset, m.getNoteNumber(), m.getFloatVelocity() });
                continue;
            }

            if (useFallback)
            {
                midi.addEvent (m, fc.bufferStartSample + offset);
                continue;
            }

            renderTo (offset);

            if (m.isNoteOn())
                noteOn (m.getNoteNumber(), juce::jmax (1, (int) m.getVelocity()));
            else if (m.isNoteOff())
                noteOff (m.getNoteNumber());
            else if (m.isSustainPedalOn())
                pedalDown = true;
            else if (m.isSustainPedalOff())
            {
                pedalDown = false;
                for (auto& v : voices)
                    if (v.active && v.sustained && ! v.keyDown)
                    {
                        v.sustained = false;
                        releaseVoice (v);
                    }
            }
            else if (m.isPitchWheel())
                pitchBend = (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                for (auto& v : voices)
                    if (v.active) releaseVoice (v);
        }
    }

    // African percussion for the Drum Kit (added after the kit's own tone control)
    auto addPercussion = [&]
    {
        if (! hybrid) return;
        auto* l = buffer.getWritePointer (0, fc.bufferStartSample);
        auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;
        std::array<float, 512> spare {};
        percussion.gain = gain.load() * (kit ? 0.7f : 1.0f);   // under the kit it sits back, like real hand percussion
        percussion.balafon = percMode == 1;
        int p = 0;
        auto renderPercTo = [&] (int until)
        {
            while (p < until)
            {
                const int n = juce::jmin (until - p, (int) spare.size());
                if (r != nullptr) percussion.render (l + p, r + p, n);
                else { std::fill (spare.begin(), spare.begin() + n, 0.0f); percussion.render (l + p, spare.data(), n); }
                p += n;
            }
        };
        for (auto& h : percHits)
        {
            renderPercTo (h.offset);
            percussion.noteOn (h.note, h.velocity);
        }
        renderPercTo (num);
    };

    if (useFallback && guitarSound.load() >= 0)
    {
        renderGuitarFallback (buffer, fc.bufferStartSample, num);
        sampleClock += (juce::uint64) num;
        return;
    }

    if (useFallback && percMode >= 0)
    {
        sampleClock += (juce::uint64) num;
        addPercussion();
        return;
    }

    if (useFallback)
    {
        fallback.renderNextBlock (buffer, midi, fc.bufferStartSample, num);
        sampleClock += (juce::uint64) num;
        addPercussion();
        return;
    }

    renderTo (num);

    // Volume and tone (gentle low-pass that darkens as Tone goes down)
    const auto g = gain.load();
    const auto t = toneAmount.load();
    const auto cutoff = 1200.0f * std::pow (16.0f, t);                // 1.2 kHz .. 19 kHz
    const auto coef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * juce::jmin (cutoff, (float) sampleRate * 0.45f) / (float) sampleRate);

    auto* l = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;
    for (int i = 0; i < num; ++i)
    {
        if (t < 0.999f)
        {
            lpL += coef * (l[i] - lpL);
            l[i] = lpL;
            if (r != nullptr) { lpR += coef * (r[i] - lpR); r[i] = lpR; }
        }
        l[i] *= g;
        if (r != nullptr) r[i] *= g;
    }
    addPercussion();
}

//==============================================================================
//  Guitars before the guitar pack is downloaded: the built-in plucked strings
//==============================================================================
void SamplerPlugin::renderGuitarFallback (juce::AudioBuffer<float>& buffer, int start, int num)
{
    guitarFallback.sound = guitarSound.load();
    guitarFallback.volumeDb = juce::Decibels::gainToDecibels (gain.load());

    auto* l = buffer.getWritePointer (0, start);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : nullptr;
    std::array<float, 256> spare {};
    auto renderTo = [&] (int& p, int until)
    {
        while (p < until)
        {
            const int n = r != nullptr ? until - p : juce::jmin (until - p, (int) spare.size());
            guitarFallback.render (l + p, r != nullptr ? r + p : spare.data(), n);
            p += n;
        }
    };

    int p = 0;
    for (const auto m : midi)
    {
        renderTo (p, juce::jlimit (0, num, m.samplePosition - start));
        const auto msg = m.getMessage();
        if (msg.isNoteOn())                                  guitarFallback.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())                            guitarFallback.noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) guitarFallback.allNotesOff();
    }
    renderTo (p, num);
}
