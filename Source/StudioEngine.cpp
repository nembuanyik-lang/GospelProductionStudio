#include "StudioEngine.h"
#include "StudioKeysPlugin.h"
#include "ClickSounds.h"

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
    displayKeyboard.removeListener (this);
    engine.getDeviceManager().removeChangeListener (this);

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

    if (track->pluginList.getPluginsOfType<StudioKeysPlugin>().isEmpty())
        if (auto plugin = edit->getPluginCache().createNewPlugin (StudioKeysPlugin::xmlTypeName, {}))
            track->pluginList.insertPlugin (plugin, 0, nullptr);
}

void StudioEngine::assignInputs()
{
    auto* track = getKeysTrack();
    if (track == nullptr)
        return;

    for (auto& d : engine.getDeviceManager().getMidiInDevices())
    {
        if (! isOursToUse (*d, onScreenDevice.get()))
            continue;

        d->setEnabled (true);
        d->setMonitorMode (te::InputDevice::MonitorMode::on);
    }

    edit->getTransport().ensureContextAllocated();

    for (auto* instance : edit->getAllInputDevices())
    {
        if (! isOursToUse (instance->getInputDevice(), onScreenDevice.get()))
            continue;

        if (! instance->getTargets().contains (track->itemID))
            [[maybe_unused]] auto result = instance->setTarget (track->itemID, false, nullptr, 0);

        instance->setRecordingEnabled (track->itemID, true);
    }

    edit->restartPlayback();
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
    if (auto* track = getKeysTrack())
        return track->getClips().size();
    return 0;
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
