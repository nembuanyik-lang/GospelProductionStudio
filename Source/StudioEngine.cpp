#include "StudioEngine.h"
#include "StudioKeysPlugin.h"
#include "SamplerPlugin.h"
#include "OrganPlugin.h"
#include "ClickSounds.h"
#include <map>

const char* StudioKeysPlugin::xmlTypeName = "GospelStudioKeys";

// Declared in the engine's ClickNode header, which is not part of its public include.
namespace tracktion::inline engine::Click
{
    void setClickWaveFile (Engine&, bool big, const juce::String& filename);
}

namespace
{
    const juce::String onScreenDeviceName ("On-screen Keyboard");

    std::unique_ptr<juce::PropertiesFile> openAppSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "GospelProductionStudio";
        o.filenameSuffix      = ".settings";
        o.folderName          = "GospelProductionStudio";
        o.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (o);
    }

    bool isOursToUse (const te::InputDevice& d, const te::MidiInputDevice* onScreen)
    {
        return d.getDeviceType() == te::InputDevice::physicalMidiDevice || &d == onScreen;
    }
}

//==============================================================================
//  Extra audio alongside the engine: test tone, voice memo record and play.
//==============================================================================
struct StudioEngine::AuxCallback final : public juce::AudioIODeviceCallback
{
    explicit AuxCallback (StudioEngine& o) : owner (o) {}

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();
        const auto block = device->getCurrentBufferSizeSamples();
        owner.memo.prepare (sampleRate, block);
        scratch.setSize (2, juce::jmax (block, 4096));
    }

    void audioDeviceStopped() override
    {
        owner.memo.release();
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        // The device manager mixes this callback with the engine's output,
        // so every output channel must be written here.
        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputs[ch], numSamples);

        if (numSamples > scratch.getNumSamples())
            return;

        // Voice memo recording from the first two inputs.
        if (numInputs > 0)
        {
            const auto n = juce::jmin (numInputs, 2);
            for (int ch = 0; ch < n; ++ch)
            {
                if (inputs[ch] != nullptr)
                    scratch.copyFrom (ch, 0, inputs[ch], numSamples);
                else
                    scratch.clear (ch, 0, numSamples);
            }

            juce::AudioBuffer<float> in (scratch.getArrayOfWritePointers(), n, numSamples);
            owner.memo.processInput (in, 0, numSamples);
        }

        // Voice memo playback and the test tone, built in stereo then copied out.
        scratch.clear (0, numSamples);
        owner.memo.renderPlayback (scratch, 0, numSamples);

        if (owner.toneOn.load() && sampleRate > 0.0)
        {
            const auto delta = juce::MathConstants<double>::twoPi * 440.0 / sampleRate;
            const auto gain  = juce::Decibels::decibelsToGain (-18.0f);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto s = gain * (float) std::sin (tonePhase);
                tonePhase += delta;
                if (tonePhase >= juce::MathConstants<double>::twoPi)
                    tonePhase -= juce::MathConstants<double>::twoPi;

                scratch.addSample (0, i, s);
                scratch.addSample (1, i, s);
            }
        }

        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::copy (outputs[ch], scratch.getReadPointer (ch % 2), numSamples);
    }

    StudioEngine& owner;
    juce::AudioBuffer<float> scratch;
    double sampleRate = 0.0, tonePhase = 0.0;
};

//==============================================================================
//  Copies the keys played on a hardware keyboard to the on-screen keyboard.
//==============================================================================
struct StudioEngine::Mirror final : public juce::MidiKeyboardState::Listener
{
    Mirror (StudioEngine& o, std::shared_ptr<te::MidiInputDevice> d)
        : owner (o), device (std::move (d))
    {
        device->keyboardState.addListener (this);
    }

    ~Mirror() override
    {
        device->keyboardState.removeListener (this);
    }

    void handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity) override
    {
        owner.lastNote.store (note);
        owner.lastVelocity.store (juce::roundToInt (velocity * 127.0f));
        owner.midiCount.fetch_add (1);
        owner.displayKeyboard.noteOn (mirrorChannel, note, velocity);
    }

    void handleNoteOff (juce::MidiKeyboardState*, int, int note, float velocity) override
    {
        owner.midiCount.fetch_add (1);
        owner.displayKeyboard.noteOff (mirrorChannel, note, velocity);
    }

    StudioEngine& owner;
    std::shared_ptr<te::MidiInputDevice> device;
};

//==============================================================================
StudioEngine::StudioEngine()
{
    engine.getPluginManager().createBuiltInType<StudioKeysPlugin>();
    engine.getPluginManager().createBuiltInType<SamplerPlugin>();
    engine.getPluginManager().createBuiltInType<OrganPlugin>();

    auto& dm = engine.getDeviceManager();

    bool haveOnScreen = false;
    for (auto& d : dm.getMidiInDevices())
        haveOnScreen = haveOnScreen || d->getName() == onScreenDeviceName;

    if (! haveOnScreen)
        [[maybe_unused]] auto result = dm.createVirtualMidiDevice (onScreenDeviceName);

    attachMirrors();
    dm.addChangeListener (this);
    displayKeyboard.addListener (this);

    aux = std::make_unique<AuxCallback> (*this);
    getAudioDeviceManager().addAudioCallback (aux.get());
    inputLevel  = getAudioDeviceManager().getInputLevelGetter();
    outputLevel = getAudioDeviceManager().getOutputLevelGetter();

    clickSound = openAppSettings()->getIntValue ("clickSound", 1);
    setClickSound (clickSound);

    scratchFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("GospelProductionStudio")
                      .getChildFile ("scratch.tracktionedit");
    scratchFile.getParentDirectory().createDirectory();
    scratchFile.deleteFile();
    loadEdit (scratchFile);
}

StudioEngine::~StudioEngine()
{
    alive->store (false);
    displayKeyboard.removeListener (this);
    engine.getDeviceManager().removeChangeListener (this);
    for (auto& [device, client] : inputMeters)
        device->levelMeasurer.removeClient (*client);
    inputMeters.clear();

    if (edit != nullptr)
        edit->getTransport().stop (false, true);

    save();
    memo.stopRecording();
    memo.stopPlayback();
    getAudioDeviceManager().removeAudioCallback (aux.get());

    mirrors.clear();
    onScreenDevice.reset();
    edit.reset();
    engine.getDeviceManager().saveSettings();
}

//==============================================================================
//  Chord audition and settings
//==============================================================================
void StudioEngine::audition (const std::vector<int>& notes, bool arpeggio, int durationMs)
{
    if (onScreenDevice == nullptr || isRecording() || notes.empty())
        return;

    auto device = onScreenDevice;
    auto flag = alive;
    auto* display = &displayKeyboard;

    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto note = notes[i];
        const auto delay = arpeggio ? (int) i * 110 : 0;
        const auto velocity = i == 0 ? 0.72f : 0.62f;

        juce::Timer::callAfterDelay (juce::jmax (1, delay), [device, flag, display, note, velocity]
        {
            if (! flag->load()) return;
            device->keyboardState.noteOn (1, note, velocity);
            display->noteOn (mirrorChannel, note, velocity);
        });

        juce::Timer::callAfterDelay (delay + durationMs, [device, flag, display, note]
        {
            if (! flag->load()) return;
            device->keyboardState.noteOff (1, note, 0.0f);
            display->noteOff (mirrorChannel, note, 0.0f);
        });
    }
}

int StudioEngine::getSetting (const juce::String& key, int defaultValue)
{
    return openAppSettings()->getIntValue (key, defaultValue);
}

void StudioEngine::setSetting (const juce::String& key, int value)
{
    auto settings = openAppSettings();
    settings->setValue (key, value);
    settings->saveIfNeeded();
}

//==============================================================================
//  Devices
//==============================================================================
void StudioEngine::attachMirrors()
{
    mirrors.clear();
    onScreenDevice.reset();

    for (auto& d : engine.getDeviceManager().getMidiInDevices())
    {
        if (d->getDeviceType() == te::InputDevice::physicalMidiDevice)
            mirrors.push_back (std::make_unique<Mirror> (*this, d));
        else if (d->getName() == onScreenDeviceName)
            onScreenDevice = d;
    }
}

std::vector<StudioEngine::MidiInputInfo> StudioEngine::getPhysicalMidiInputs() const
{
    std::vector<MidiInputInfo> result;
    for (auto& d : engine.getDeviceManager().getMidiInDevices())
        if (d->getDeviceType() == te::InputDevice::physicalMidiDevice)
            result.push_back ({ d->getName(), d->isEnabled() });
    return result;
}

void StudioEngine::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // A keyboard was plugged in or out, or the audio device changed.
    attachMirrors();
    prepareAudioInputs();
    assignInputs();
}

void StudioEngine::handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    // Only keys clicked on screen (channel 1) are sent to the engine;
    // mirrored hardware keys (channel 16) are for display only.
    if (channel != onScreenChannel || onScreenDevice == nullptr)
        return;

    lastNote.store (note);
    lastVelocity.store (juce::roundToInt (velocity * 127.0f));
    midiCount.fetch_add (1);
    onScreenDevice->keyboardState.noteOn (1, note, velocity);
}

void StudioEngine::handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    if (channel != onScreenChannel || onScreenDevice == nullptr)
        return;

    midiCount.fetch_add (1);
    onScreenDevice->keyboardState.noteOff (1, note, velocity);
}

float StudioEngine::getInputLevel()    { return inputLevel  != nullptr ? (float) inputLevel->getCurrentLevel()  : 0.0f; }
float StudioEngine::getOutputLevel()   { return outputLevel != nullptr ? (float) outputLevel->getCurrentLevel() : 0.0f; }
float StudioEngine::getCpuUsage() const { return engine.getDeviceManager().getCpuUsage(); }

//==============================================================================
//  Projects
//==============================================================================
void StudioEngine::loadEdit (const juce::File& file)
{
    if (edit != nullptr)
    {
        edit->getTransport().stop (false, true);
        save();
        edit.reset();
    }

    const auto isNew = ! file.existsAsFile();
    edit = isNew ? te::createEmptyEdit (engine, file)
                 : te::loadEditFromFile (engine, file);

    edit->editFileRetriever = [file] { return file; };
    edit->playInStopEnabled = true;

    if (isNew)
    {
        edit->clickTrackEnabled = true;
        edit->clickTrackEmphasiseBars = true;
        edit->setCountInMode (te::Edit::CountIn::oneBar);
        edit->getTransport().setLoopRange ({ te::TimePosition(), te::TimePosition::fromSeconds (8.0) });
    }

    prepareKeysTrack();

    if (isNew)
        createStarterLayout();

    assignInputs();
}

te::AudioTrack* StudioEngine::getKeysTrack() const
{
    return edit != nullptr ? te::getAudioTracks (*edit)[0] : nullptr;
}

void StudioEngine::prepareKeysTrack()
{
    edit->ensureNumberOfAudioTracks (1);
    auto* track = getKeysTrack();
    if (track == nullptr)
        return;

    if (track->getName() != "Keys")
        track->setName ("Keys");

    addInstrument (*track);
}

void StudioEngine::addInstrument (te::AudioTrack& track, const juce::String& instrumentId)
{
    // Only when the track has no instrument yet (songs from earlier versions keep theirs)
    if (getInstrumentPlugin (track) == nullptr)
        setInstrument (track, instrumentId);
}

te::Plugin* StudioEngine::getInstrumentPlugin (te::AudioTrack& track)
{
    for (auto* p : track.pluginList.getPlugins())
        if (p->isSynth())
            return p;
    return nullptr;
}

juce::String StudioEngine::getInstrumentId (te::AudioTrack& track)
{
    auto* p = getInstrumentPlugin (track);
    if (auto* s = dynamic_cast<SamplerPlugin*> (p)) return s->instrumentId.get();
    if (dynamic_cast<OrganPlugin*> (p) != nullptr)  return "organ";
    if (dynamic_cast<StudioKeysPlugin*> (p) != nullptr) return "studioKeys";
    return {};
}

juce::String StudioEngine::describeInstrument (te::AudioTrack& track)
{
    auto* p = getInstrumentPlugin (track);
    if (p == nullptr)
        return "Audio";
    if (auto* s = dynamic_cast<SamplerPlugin*> (p))
    {
        const auto st = s->getStatusText();
        return s->getName() + (st.isNotEmpty() ? " (" + st + ")" : juce::String());
    }
    if (auto* o = dynamic_cast<OrganPlugin*> (p))
        return "Gospel Organ  " + o->getRegistration();
    return p->getName();
}

void StudioEngine::setInstrument (te::AudioTrack& track, const juce::String& instrumentId)
{
    const auto* choice = Instruments::find (instrumentId);
    if (choice == nullptr || edit == nullptr)
        return;

    // Same kind of instrument already there: just switch the sound
    if (auto* s = dynamic_cast<SamplerPlugin*> (getInstrumentPlugin (track)))
        if (choice->kind == Instruments::Kind::sampler)
        {
            s->instrumentId = instrumentId;
            return;
        }

    for (auto* p : track.pluginList.getPlugins())
        if (p->isSynth())
            p->deleteFromParent();

    const char* type = choice->kind == Instruments::Kind::organ   ? OrganPlugin::xmlTypeName
                     : choice->kind == Instruments::Kind::sampler ? SamplerPlugin::xmlTypeName
                                                                  : StudioKeysPlugin::xmlTypeName;

    if (auto plugin = edit->getPluginCache().createNewPlugin (type, {}))
    {
        if (auto* s = dynamic_cast<SamplerPlugin*> (plugin.get()))
            s->instrumentId = instrumentId;
        track.pluginList.insertPlugin (plugin, 0, nullptr);
    }
}

bool StudioEngine::isInstrumentTrack (te::AudioTrack& track)
{
    for (auto* p : track.pluginList.getPlugins())
        if (p->isSynth())
            return true;
    return false;
}

void StudioEngine::createStarterLayout()
{
    if (auto* keys = getKeysTrack())
        keys->setColour (juce::Colour (0xffd4a843));

    struct Starter { const char* name; bool instrument; juce::uint32 colour; const char* instrumentId; };
    const Starter starters[] = { { "Organ",      true,  0xffc9774a, "organ" },
                                 { "Bass",       true,  0xff5b8def, "bassFinger" },
                                 { "Drums",      true,  0xffe07a4f, "drums" },
                                 { "Lead Vocal", false, 0xff4caf7d, "" },
                                 { "Choir",      false, 0xffb07ee0, "" } };

    te::Track* previous = getKeysTrack();
    for (const auto& s : starters)
        previous = addTrack (s.instrument, s.name, juce::Colour (s.colour), previous, s.instrumentId);
}

te::AudioTrack* StudioEngine::addTrack (bool instrument, const juce::String& name, juce::Colour colour, te::Track* after,
                                        const juce::String& instrumentId)
{
    if (edit == nullptr)
        return nullptr;

    const auto insertPoint = after != nullptr ? te::TrackInsertPoint (*after, false)
                                              : te::TrackInsertPoint::getEndOfTracks (*edit);

    auto track = edit->insertNewAudioTrack (insertPoint, nullptr, true);
    if (track == nullptr)
        return nullptr;

    track->setName (name);
    track->setColour (colour);

    if (instrument)
        addInstrument (*track, instrumentId);

    return track.get();
}

void StudioEngine::deleteTrack (te::AudioTrack& track)
{
    const auto wasArmed = getArmedTrack() == &track;
    edit->deleteTrack (&track);

    if (wasArmed || getArmedTrack() == nullptr)
    {
        te::AudioTrack* firstInstrument = nullptr;
        for (auto* t : te::getAudioTracks (*edit))
            if (firstInstrument == nullptr && isInstrumentTrack (*t))
                firstInstrument = t;
        setArmedTrack (firstInstrument);
    }
}

te::AudioTrack* StudioEngine::getArmedTrack() const
{
    if (edit == nullptr)
        return nullptr;

    for (auto* instance : edit->getAllInputDevices())
    {
        if (! isOursToUse (instance->getInputDevice(), onScreenDevice.get()))
            continue;

        for (auto id : instance->getTargets())
            if (auto* t = te::findAudioTrackForID (*edit, id))
                return t;
    }
    return nullptr;
}

void StudioEngine::setArmedTrack (te::AudioTrack* track)
{
    if (edit == nullptr)
        return;

    edit->getTransport().ensureContextAllocated();

    for (auto* instance : edit->getAllInputDevices())
    {
        if (! isOursToUse (instance->getInputDevice(), onScreenDevice.get()))
            continue;

        for (auto id : instance->getTargets())
            if (track == nullptr || id != track->itemID)
                [[maybe_unused]] auto r = instance->removeTarget (id, nullptr);

        if (track != nullptr)
        {
            if (! instance->getTargets().contains (track->itemID))
                [[maybe_unused]] auto result = instance->setTarget (track->itemID, false, nullptr, 0);

            instance->setRecordingEnabled (track->itemID, true);
        }
    }

    edit->restartPlayback();
}

void StudioEngine::ensureSections (const Song& song)
{
    if (edit == nullptr)
        return;

    auto* arranger = edit->getArrangerTrack();
    if (arranger == nullptr || ! arranger->getClips().isEmpty() || song.sections.empty())
        return;

    auto barsFor = [] (const juce::String& type)
    {
        if (type == "Verse" || type == "Chorus" || type == "Bridge" || type == "Vamp") return 8;
        return 4;
    };

    auto colourFor = [] (const juce::String& type)
    {
        if (type == "Verse")      return juce::Colour (0xff5b8def);
        if (type == "Chorus")     return juce::Colour (0xffd4a843);
        if (type == "Bridge")     return juce::Colour (0xffb07ee0);
        if (type == "Vamp")       return juce::Colour (0xffe07a4f);
        if (type == "Pre-Chorus") return juce::Colour (0xff4fb3c8);
        return juce::Colour (0xff8a93a3);
    };

    std::map<juce::String, int> totals, seen;
    for (const auto& s : song.sections)
        ++totals[s.type];

    int bar = 0;
    for (const auto& s : song.sections)
    {
        const auto n = ++seen[s.type];
        const auto name = totals[s.type] > 1 ? s.type + " " + juce::String (n) : s.type;
        const auto length = barsFor (s.type);

        te::tempo::BarsAndBeats from, to;
        from.bars = bar;
        to.bars = bar + length;

        if (auto* clip = arranger->insertNewClip (te::TrackItem::Type::arranger, name,
                                                  te::TimeRange (edit->tempoSequence.toTime (from),
                                                                 edit->tempoSequence.toTime (to)),
                                                  nullptr))
            clip->setColour (colourFor (s.type));

        bar += length;
    }

    save();
}

void StudioEngine::assignInputs()
{
    if (edit == nullptr)
        return;

    prepareAudioInputs();

    for (auto& d : engine.getDeviceManager().getMidiInDevices())
    {
        if (! isOursToUse (*d, onScreenDevice.get()))
            continue;

        d->setEnabled (true);
        d->setMonitorMode (te::InputDevice::MonitorMode::on);
    }

    edit->getTransport().ensureContextAllocated();

    // Keep the track the keyboard was armed to; otherwise use the Keys track.
    auto* target = getArmedTrack();
    if (target == nullptr)
        target = getKeysTrack();

    setArmedTrack (target);
}

void StudioEngine::openSong (const Song& song, const juce::File& editFile)
{
    if (openSongId == song.id && edit != nullptr)
    {
        applySongSettings (song);
        return;
    }

    editFile.getParentDirectory().createDirectory();
    loadEdit (editFile);
    openSongId = song.id;
    applySongSettings (song);
    save();
}

void StudioEngine::closeSong()
{
    loadEdit (scratchFile);
    openSongId = {};
}

void StudioEngine::save()
{
    if (edit != nullptr && openSongId.isNotEmpty())
        te::EditFileOperations (*edit).save (false, true, false);
}

void StudioEngine::autosaveIfChanged()
{
    if (edit != nullptr && openSongId.isNotEmpty() && ! isRecording() && edit->hasChangedSinceSaved())
        save();
}

void StudioEngine::applySongSettings (const Song& song)
{
    if (edit == nullptr)
        return;

    auto& tempoSeq = edit->tempoSequence;
    bool changed = false;

    if (auto* sig = tempoSeq.getTimeSig (0))
    {
        const auto current = juce::String (sig->numerator.get()) + "/" + juce::String (sig->denominator.get());
        if (current != song.timeSignature)
        {
            sig->setStringTimeSig (song.timeSignature);
            changed = true;
        }
    }

    if (auto* tempo = tempoSeq.getTempo (0))
    {
        const auto bpm = song.getEngineBpm();
        if (std::abs (tempo->getBpm() - bpm) > 0.001)
        {
            tempo->setBpm (bpm);
            changed = true;
        }
    }

    if (edit->pitchSequence.getNumPitches() > 0)
    {
        if (auto* pitch = edit->pitchSequence.getPitch (0))
        {
            const auto midiPitch = 60 + song.getKeyPitchClass();
            if (pitch->getPitch() != midiPitch)
            {
                pitch->setPitch (midiPitch);
                changed = true;
            }

            const auto scale = song.isMinorKey() ? te::Scale::minor : te::Scale::major;
            if (pitch->getScale() != scale)
            {
                pitch->setScaleID (scale);
                changed = true;
            }
        }
    }

    if (changed && song.id == openSongId)
        save();
}

//==============================================================================
//  Transport
//==============================================================================
void StudioEngine::togglePlay()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (t.isPlaying())
        t.stop (false, false);
    else
        t.play (false);
}

void StudioEngine::stop()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (t.isPlaying() || t.isRecording())
    {
        const auto wasRecording = t.isRecording();
        t.stop (false, false);
        if (wasRecording)
            save();
    }
    else
    {
        t.setPosition (te::TimePosition());
    }
}

void StudioEngine::toggleRecord()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (t.isRecording())
    {
        t.stop (false, false);
        save();
    }
    else
    {
        recordStartSeconds = t.getPosition().inSeconds();
        t.record (false);
    }
}

void StudioEngine::returnToStart()
{
    if (edit != nullptr)
        edit->getTransport().setPosition (te::TimePosition());
}

bool StudioEngine::isPlaying() const   { return edit != nullptr && edit->getTransport().isPlaying(); }
bool StudioEngine::isRecording() const { return edit != nullptr && edit->getTransport().isRecording(); }

bool StudioEngine::isInCountIn() const
{
    return edit != nullptr && edit->getTransport().isRecording()
           && edit->getTransport().getPosition() < te::TimePosition();
}

void StudioEngine::setLooping (bool shouldLoop)
{
    if (edit != nullptr)
        edit->getTransport().looping = shouldLoop;
}

bool StudioEngine::isLooping() const
{
    return edit != nullptr && edit->getTransport().looping.get();
}

void StudioEngine::setLoopBars (int firstBar, int numBars)
{
    if (edit == nullptr)
        return;

    te::tempo::BarsAndBeats start, end;
    start.bars = juce::jmax (0, firstBar - 1);
    end.bars   = start.bars + juce::jmax (1, numBars);

    edit->getTransport().setLoopRange ({ edit->tempoSequence.toTime (start),
                                         edit->tempoSequence.toTime (end) });
}

std::pair<int, int> StudioEngine::getLoopBars() const
{
    if (edit == nullptr)
        return { 1, 4 };

    const auto loop  = edit->getTransport().getLoopRange();
    const auto start = edit->tempoSequence.toBarsAndBeats (loop.getStart());
    const auto end   = edit->tempoSequence.toBarsAndBeats (loop.getEnd());
    const auto endBars = end.bars + (end.getWholeBeats() > 0 || end.getFractionalBeats().inBeats() > 0.01 ? 1 : 0);
    return { start.bars + 1, juce::jmax (1, endBars - start.bars) };
}

StudioEngine::Position StudioEngine::getPosition() const
{
    Position p;
    if (edit == nullptr)
        return p;

    const auto pos = edit->getTransport().getPosition();
    const auto bb  = edit->tempoSequence.toBarsAndBeats (pos);

    p.seconds    = pos.inSeconds();
    p.countIn    = p.seconds < 0.0;
    p.bar        = bb.bars + 1;
    p.beat       = bb.getWholeBeats() + 1;
    p.beatsInBar = bb.numerator > 0 ? bb.numerator
                                    : edit->tempoSequence.getTimeSigAt (pos).numerator.get();
    return p;
}

int StudioEngine::getNumTakes() const
{
    int n = 0;
    if (edit != nullptr)
        for (auto* t : te::getAudioTracks (*edit))
            n += t->getClips().size();
    return n;
}

void StudioEngine::setPositionSeconds (double seconds)
{
    if (edit != nullptr)
        edit->getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds)));
}

void StudioEngine::clearTakes()
{
    if (auto* track = getKeysTrack())
    {
        const auto clips = track->getClips();
        for (int i = clips.size(); --i >= 0;)
            clips.getUnchecked (i)->removeFromParent();
        save();
    }
}

//==============================================================================
//  Metronome
//==============================================================================
void StudioEngine::setClickEnabled (bool b)   { if (edit != nullptr) edit->clickTrackEnabled = b; }
bool StudioEngine::isClickEnabled() const     { return edit != nullptr && edit->clickTrackEnabled.get(); }
void StudioEngine::setClickVolume (float v)   { if (edit != nullptr) edit->setClickTrackVolume (juce::jlimit (0.2f, 1.0f, v)); }
float StudioEngine::getClickVolume() const    { return edit != nullptr ? edit->getClickTrackVolume() : 0.6f; }
void StudioEngine::setClickAccent (bool b)    { if (edit != nullptr) edit->clickTrackEmphasiseBars = b; }
bool StudioEngine::isClickAccent() const      { return edit != nullptr && edit->clickTrackEmphasiseBars.get(); }

void StudioEngine::setClickSound (int index)
{
    clickSound = juce::jlimit (0, ClickSounds::getNames().size() - 1, index);
    const auto [accent, normal] = ClickSounds::ensureFiles (clickSound);

    te::Click::setClickWaveFile (engine, true,  accent.getFullPathName());
    te::Click::setClickWaveFile (engine, false, normal.getFullPathName());

    auto settings = openAppSettings();
    settings->setValue ("clickSound", clickSound);
    settings->saveIfNeeded();

    if (edit != nullptr)
        edit->restartPlayback();
}

namespace
{
    const te::Edit::CountIn countInModes[] = { te::Edit::CountIn::none, te::Edit::CountIn::oneBar,
                                               te::Edit::CountIn::twoBar, te::Edit::CountIn::oneBeat,
                                               te::Edit::CountIn::twoBeat };
}

void StudioEngine::setCountIn (int index)
{
    if (edit != nullptr && juce::isPositiveAndBelow (index, (int) std::size (countInModes)))
        edit->setCountInMode (countInModes[index]);
}

int StudioEngine::getCountIn() const
{
    if (edit == nullptr)
        return 1;

    const auto mode = edit->getCountInMode();
    for (int i = 0; i < (int) std::size (countInModes); ++i)
        if (countInModes[i] == mode)
            return i;
    return 0;
}

//==============================================================================
//  Audio recording (Step 9)
//==============================================================================
void StudioEngine::prepareAudioInputs()
{
    auto& dm = engine.getDeviceManager();

    for (auto* d : dm.getWaveInputDevices())
    {
        if (! d->isEnabled())
            dm.setDeviceEnabled (*d, true);
        d->setOutputFormat ("WAV");
        d->setBitDepth (24);

        if (inputMeters.find (d) == inputMeters.end())
        {
            auto client = std::make_unique<te::LevelMeasurer::Client>();
            d->levelMeasurer.addClient (*client);
            inputMeters[d] = std::move (client);
        }
    }

    // Forget meters of inputs that went away
    const auto current = dm.getWaveInputDevices();
    for (auto it = inputMeters.begin(); it != inputMeters.end();)
        if (std::find (current.begin(), current.end(), it->first) == current.end()) it = inputMeters.erase (it);
        else ++it;
}

std::vector<te::WaveInputDevice*> StudioEngine::getAudioInputs() const
{
    std::vector<te::WaveInputDevice*> out;
    for (auto* d : engine.getDeviceManager().getWaveInputDevices())
        if (d->isEnabled() && d->getDeviceType() == te::InputDevice::waveDevice)
            out.push_back (d);
    return out;
}

juce::String StudioEngine::describeInput (te::WaveInputDevice* d)
{
    if (d == nullptr)
        return "No input";
    return d->getName();
}

te::InputDeviceInstance* StudioEngine::findInstance (te::InputDevice& device) const
{
    if (edit == nullptr)
        return nullptr;
    for (auto* instance : edit->getAllInputDevices())
        if (&instance->getInputDevice() == &device)
            return instance;
    return nullptr;
}

te::WaveInputDevice* StudioEngine::getTrackInput (te::AudioTrack& track) const
{
    for (auto* d : getAudioInputs())
        if (auto* inst = findInstance (*d))
            if (inst->getTargets().contains (track.itemID))
                return d;
    return nullptr;
}

void StudioEngine::setTrackInput (te::AudioTrack& track, te::WaveInputDevice* device)
{
    if (edit == nullptr)
        return;

    edit->getTransport().ensureContextAllocated();
    auto* um = &edit->getUndoManager();
    const auto wasArmed = isAudioArmed (track);

    // One input per track: take this track off any other input
    for (auto* d : getAudioInputs())
        if (auto* inst = findInstance (*d))
            if (d != device && inst->getTargets().contains (track.itemID))
                [[maybe_unused]] auto r = inst->removeTarget (track.itemID, um);

    if (device != nullptr)
        if (auto* inst = findInstance (*device))
        {
            // One track per input too: this input now feeds only this track
            [[maybe_unused]] auto r = inst->setTarget (track.itemID, true, um);
            inst->setRecordingEnabled (track.itemID, wasArmed);
        }

    edit->restartPlayback();
}

bool StudioEngine::isAudioArmed (te::AudioTrack& track) const
{
    if (auto* d = getTrackInput (track))
        if (auto* inst = findInstance (*d))
            return inst->isRecordingEnabled (track.itemID);
    return false;
}

void StudioEngine::setAudioArmed (te::AudioTrack& track, bool armed)
{
    if (auto* d = getTrackInput (track))
        if (auto* inst = findInstance (*d))
            inst->setRecordingEnabled (track.itemID, armed);
}

float StudioEngine::getTrackInputLevel (te::AudioTrack& track)
{
    auto* d = getTrackInput (track);
    if (d == nullptr)
        return 0.0f;
    auto it = inputMeters.find (d);
    if (it == inputMeters.end())
        return 0.0f;

    float db = -100.0f;
    for (int ch = 0; ch < 2; ++ch)
        db = juce::jmax (db, it->second->getAndClearAudioLevel (ch).dB);
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

bool StudioEngine::areInputsMono() const
{
    for (auto* d : getAudioInputs())
        if (d->getChannelSet().size() > 1)
            return false;
    return true;
}

void StudioEngine::setInputsMono (bool mono)
{
    engine.getDeviceManager().setAllWaveInputsToNumChannels (mono ? 1u : 2u);
    prepareAudioInputs();
    if (edit != nullptr)
    {
        edit->getTransport().ensureContextAllocated();
        edit->restartPlayback();
    }
}

te::InputDevice::MonitorMode StudioEngine::getMonitorMode (te::AudioTrack& track) const
{
    if (auto* d = getTrackInput (track))
        return d->getMonitorMode();
    return te::InputDevice::MonitorMode::automatic;
}

void StudioEngine::setMonitorMode (te::AudioTrack& track, te::InputDevice::MonitorMode mode)
{
    if (auto* d = getTrackInput (track))
        d->setMonitorMode (mode);
}

bool StudioEngine::isPunchEnabled() const
{
    return edit != nullptr && edit->recordingPunchInOut;
}

void StudioEngine::setPunchEnabled (bool shouldPunch)
{
    if (edit != nullptr)
        edit->recordingPunchInOut = shouldPunch;
}

te::Clip* StudioEngine::importAudio (te::AudioTrack& track, const juce::File& file, double startSeconds)
{
    if (edit == nullptr || ! file.existsAsFile())
        return nullptr;

    // Keep a copy inside the song folder so the song never loses its audio
    auto folder = edit->editFileRetriever ? edit->editFileRetriever().getParentDirectory().getChildFile ("Audio") : juce::File();
    auto source = file;
    if (folder != juce::File() && ! file.isAChildOf (folder.getParentDirectory()))
    {
        folder.createDirectory();
        auto copy = folder.getNonexistentChildFile (file.getFileNameWithoutExtension(), file.getFileExtension(), false);
        if (file.copyFileTo (copy))
            source = copy;
    }

    const te::AudioFile audioFile (engine, source);
    if (! audioFile.isValid())
        return nullptr;

    const auto start = te::TimePosition::fromSeconds (juce::jmax (0.0, startSeconds));
    const te::ClipPosition position { { start, start + te::TimeDuration::fromSeconds (audioFile.getLength()) }, {} };
    auto clip = track.insertWaveClip (file.getFileNameWithoutExtension(), source, position, false);
    return clip.get();
}
