#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <array>

namespace te = tracktion;

//==============================================================================
//  African percussion (plan Step 21), made by synthesis so nothing needs
//  downloading: congas, djembe, talking drum, shakers, tambourine, bells
//  (gankogui / ogene style), claves, woodblock and clap, plus a balafon.
//
//  The notes follow General MIDI where it has the instrument, so grooves and
//  MIDI files from elsewhere play the right sounds:
//    39 clap         54 tambourine   56 cowbell      60 djembe slap   61 djembe tone
//    62 conga mute   63 conga open   64 conga low    65 talking drum high
//    66 talking drum low   67 bell high   68 bell low   69 cabasa (soft shaker)
//    70 shaker       75 claves       76 woodblock    77 woodblock low
//    78 talking drum falls   79 talking drum rises   87 djembe bass
//
//  The Drum Kit plays these too: notes the kit has no sample for are played
//  by this synth, so one Drums track can play a whole African groove.
//==============================================================================
namespace Percussion
{
    struct Sound { int note; const char* name; };
    const std::vector<Sound>& sounds();
    bool handles (int note);
    juce::String nameOf (int note);

    // GM note numbers
    enum : int
    {
        clap = 39, tambourine = 54, cowbell = 56, djembeSlap = 60, djembeTone = 61, congaMute = 62, congaOpen = 63,
        congaLow = 64, talkHigh = 65, talkLow = 66, bellHigh = 67, bellLow = 68, cabasa = 69, shaker = 70,
        claves = 75, woodblock = 76, woodblockLow = 77, talkFall = 78, talkRise = 79, djembeBass = 87
    };

    //==========================================================================
    class Synth
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void noteOn (int note, float velocity);
        void noteOff (int note);
        void render (float* left, float* right, int num);       // adds into the buffers

        bool balafon = false;      // pitched balafon instead of the percussion kit
        float gain = 1.0f;

    private:
        struct Mode { float freq = 0, startFreq = 0, glide = 0, amp = 0, decay = 0, phase = 0; };
        struct Voice
        {
            bool active = false;
            int note = 0;
            juce::uint32 age = 0;
            std::array<Mode, 6> modes;
            int numModes = 0;
            float t = 0.0f;                       // seconds since the hit
            float noiseAmp = 0, noiseDecay = 0.01f, noiseAttack = 0.0f;
            float bp1 = 0, bp2 = 0, bpCoeffA = 0, bpCoeffR = 0, bpGain = 1;   // band-pass for the noise
            float hpPrev = 0, hpOut = 0;
            bool highPass = false;
            int repeats = 0; float repeatGap = 0;     // clap: several quick bursts
            float pan = 0.0f;
            float buzz = 0.0f;                     // balafon gourd buzz
        };

        Voice& allocate (int note);
        void setNoise (Voice&, float centreHz, float q, float amp, float decay, float attack = 0.0f, bool highPass = false);

        double sampleRate = 44100.0;
        std::array<Voice, 24> voices;
        juce::uint32 counter = 0;
        juce::Random random;
    };
}

//==============================================================================
//  The instrument: "African Percussion" or "Balafon" on its own track
//==============================================================================
class PercussionPlugin final : public te::Plugin
{
public:
    explicit PercussionPlugin (te::PluginCreationInfo);
    ~PercussionPlugin() override;

    static const char* getPluginName()                           { return "African Percussion"; }
    static const char* xmlTypeName;

    juce::String getName() const override                        { return mode.get() == 1 ? "Balafon" : getPluginName(); }
    juce::String getPluginType() override                        { return xmlTypeName; }
    juce::String getShortName (int) override                     { return mode.get() == 1 ? "Balafon" : "Perc"; }
    juce::String getSelectableDescription() override             { return getName(); }

    bool isSynth() override                                      { return true; }
    bool takesMidiInput() override                               { return true; }
    bool takesAudioInput() override                              { return false; }
    bool producesAudioWhenNoAudioInput() override                { return true; }
    int  getNumOutputChannelsGivenInputs (int) override          { return 2; }
    BusLayout getBusses() const override                         { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override                        { return 1.5; }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const te::PluginRenderContext&) override;

    juce::CachedValue<int> mode;          // 0 percussion, 1 balafon
    juce::CachedValue<float> volumeDb;

private:
    Percussion::Synth synth;
    double sampleRate = 44100.0;
};
