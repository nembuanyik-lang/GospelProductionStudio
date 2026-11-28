#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <array>

namespace te = tracktion;

//==============================================================================
//  The plucked-string sound itself, shared by the Gospel Guitar plugin and by
//  the sampler (Step 30), which plays it until the guitar sound pack is downloaded.
//==============================================================================
class GuitarSynth
{
public:
    int sound = 0;                         // 0 clean electric, 1 nylon acoustic
    float volumeDb = 0.0f;

    void prepare (double sampleRate);
    void reset();
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();
    void render (float* outL, float* outR, int num);       // adds to the buffers
    double getSampleRate() const                           { return sampleRate; }

private:
    struct Voice
    {
        bool active = false, held = false;
        int note = 0;
        juce::uint32 age = 0;
        std::vector<float> line;           // the string
        int length = 0, writePos = 0;
        float frac = 0.0f, apState = 0.0f, apCoeff = 0.0f;
        float lp = 0.0f, loopGain = 0.999f, damp = 0.5f;
        float level = 0.0f, muteGain = 1.0f;
        int silentFor = 0;
    };

    double sampleRate = 44100.0;
    std::array<Voice, 12> voices;
    juce::uint32 counter = 0;
    juce::Random random;

    // Body and width
    float bodyZ[2][2] {}, body2Z[2][2] {};
    float toneLp = 0.0f, dcX = 0.0f, dcY = 0.0f;
    std::vector<float> haas;
    int haasPos = 0;
};

//==============================================================================
//  Gospel Guitar (plan Step 20): a plucked-string guitar built from synthesis
//  (no download). Each string is a tuned delay line excited by a pick; the
//  pick position, the string damping and a small body give the tone.
//  Two sounds: a clean electric (praise, highlife and makossa picking) and a
//  nylon acoustic (worship ballads). Letting go of a key mutes the string, so
//  short notes give the "chick" of rhythm guitar.
//==============================================================================
class GuitarPlugin final : public te::Plugin
{
public:
    explicit GuitarPlugin (te::PluginCreationInfo);
    ~GuitarPlugin() override;

    static const char* getPluginName()                           { return "Gospel Guitar"; }
    static const char* xmlTypeName;

    juce::String getName() const override                        { return getPluginName(); }
    juce::String getPluginType() override                        { return xmlTypeName; }
    juce::String getShortName (int) override                     { return "Guitar"; }
    juce::String getSelectableDescription() override             { return getName(); }

    bool isSynth() override                                      { return true; }
    bool takesMidiInput() override                               { return true; }
    bool takesAudioInput() override                              { return false; }
    bool producesAudioWhenNoAudioInput() override                { return true; }
    int  getNumOutputChannelsGivenInputs (int) override          { return 2; }
    BusLayout getBusses() const override                         { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override                        { return 2.0; }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const te::PluginRenderContext&) override;

    // ---- Settings (stored in the song) --------------------------------------
    juce::CachedValue<int> sound;          // 0 clean electric, 1 nylon acoustic
    juce::CachedValue<float> volumeDb;

private:
    GuitarSynth synth;
};
