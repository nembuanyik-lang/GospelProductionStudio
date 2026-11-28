#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Sfz.h"
#include "Instruments.h"
#include "StudioKeys.h"
#include "Percussion.h"
#include "GuitarPlugin.h"

namespace te = tracktion;

//==============================================================================
//  Gospel Sampler: plays the piano, bass and drum sound packs (SFZ).
//  Velocity layers, round robin, release samples, sustain pedal, hi-hat choke.
//  Until a pack is installed it plays the simple Studio Keys sound, or for
//  the guitars the built-in plucked-string guitar (Step 30).
//==============================================================================
class SamplerPlugin final : public te::Plugin,
                            private juce::ChangeListener
{
public:
    explicit SamplerPlugin (te::PluginCreationInfo);
    ~SamplerPlugin() override;

    static const char* getPluginName()                           { return "Gospel Sampler"; }
    static const char* xmlTypeName;

    juce::String getName() const override;
    juce::String getPluginType() override                        { return xmlTypeName; }
    juce::String getShortName (int) override                     { return "Sampler"; }
    juce::String getSelectableDescription() override             { return getName(); }

    bool isSynth() override                                      { return true; }
    bool takesMidiInput() override                               { return true; }
    bool takesAudioInput() override                              { return false; }
    bool producesAudioWhenNoAudioInput() override                { return true; }
    int  getNumOutputChannelsGivenInputs (int) override          { return 2; }
    BusLayout getBusses() const override                         { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override                        { return 3.0; }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const te::PluginRenderContext&) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    // Settings (stored in the song)
    juce::CachedValue<juce::String> instrumentId;
    juce::CachedValue<float> volumeDb, tone, releaseScale;
    juce::CachedValue<int> touch;                  // 0 light, 1 normal, 2 firm

    enum class Status { loading, ready, notInstalled, failed };
    Status getStatus() const                                     { return status; }
    const Instruments::Choice* getChoice() const                 { return Instruments::find (instrumentId.get()); }
    juce::String getStatusText() const;
    juce::int64 getMemoryBytes() const;

    void reloadInstrument();
    bool isMySounds() const                                      { return instrumentId.get() == "mysounds"; }
    bool isGuitar() const                                        { return guitarSound.load() >= 0; }
    bool isPercussion() const                                    { return percussionMode.load() >= 0; }
    bool hasBuiltInSound() const                                 { return isGuitar() || isPercussion(); }

private:
    struct Voice
    {
        bool active = false;
        const Sfz::Region* region = nullptr;
        const Sfz::Sample* sample = nullptr;
        int note = 0, velocity = 0;
        double pos = 0.0, inc = 1.0;
        float gainL = 1.0f, gainR = 1.0f;
        enum class Stage { attack, hold, decay, sustain, release, kill } stage = Stage::attack;
        float env = 0.0f, envStep = 0.0f, sustainLevel = 1.0f, releaseCoef = 0.999f;
        int stageSamples = 0;
        bool keyDown = true, sustained = false;
        juce::uint64 startTime = 0;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshSettings();
    void noteOn (int note, int velocity);
    void noteOff (int note);
    void releaseVoice (Voice&);
    void startRegion (const Sfz::Region&, int note, int velocity, float extraGainDb);
    void renderVoices (juce::AudioBuffer<float>&, int start, int num);
    Voice* findFreeVoice();

    // Instrument shared with the audio thread
    juce::SpinLock instrumentLock;
    Sfz::InstrumentPtr instrument;
    std::atomic<bool> instrumentChanged { false };
    std::atomic<bool> userKit { false };           // My Sounds: silent (not Studio Keys) when empty
    bool reloadPending = false;
    Status status = Status::loading;
    int loadGeneration = 0;

    std::vector<Voice> voices;
    std::array<int, 128> roundRobin {};
    std::array<juce::uint64, 128> noteOnTime {};
    std::array<int, 128> noteVelocity {};
    juce::uint64 sampleClock = 0;
    bool pedalDown = false;
    float pitchBend = 0.0f;
    juce::Random random;

    // Settings copied for the audio thread
    std::atomic<float> gain { 1.0f }, toneAmount { 1.0f }, releaseMul { 1.0f };
    std::atomic<int> touchMode { 1 };
    float lpL = 0.0f, lpR = 0.0f;

    double sampleRate = 44100.0;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // The Drum Kit also plays African percussion (congas, shakers, bells...) for notes it has no sample for.
    // African Percussion and Balafon (Step 31): recorded where the pack has the sound, built-in otherwise.
    std::atomic<bool> drumKit { false };
    std::atomic<int> percussionMode { -1 };      // 0 African Percussion, 1 Balafon, -1 neither
    Percussion::Synth percussion;
    struct PercHit { int offset, note; float velocity; };
    std::vector<PercHit> percHits;
    bool kitHasNote (int note) const;

    // Fallback until a pack is installed
    std::atomic<int> guitarSound { -1 };          // guitars: 0 clean, 1 nylon; -1 not a guitar
    GuitarSynth guitarFallback;
    void renderGuitarFallback (juce::AudioBuffer<float>&, int start, int num);
    juce::Synthesiser fallback;
    juce::MidiBuffer midi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerPlugin)
};
