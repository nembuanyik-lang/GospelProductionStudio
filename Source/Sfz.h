#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

//==============================================================================
//  SFZ instruments: parsing, sample loading and a shared cache.
//  Sound packs are folders of SFZ files with WAV, FLAC or OGG samples.
//==============================================================================
namespace Sfz
{
    enum class LoopMode { none, oneShot, continuous, sustain };
    enum class Trigger  { attack, release };

    struct Region
    {
        int sampleIndex = -1;
        int lokey = 0, hikey = 127, keycenter = 60;
        int lovel = 1, hivel = 127;
        float lorand = 0.0f, hirand = 1.0f;
        int seqLength = 1, seqPosition = 1;
        float tune = 0.0f;              // cents
        float transpose = 0.0f;         // semitones
        float keytrack = 100.0f;        // cents per key
        float volume = 0.0f;            // dB
        float pan = 0.0f;               // -100..100
        float ampVeltrack = 100.0f;     // %
        float attack = 0.0f, hold = 0.0f, decay = 0.0f, sustain = 100.0f, release = 0.001f;
        float rtDecay = 0.0f;           // dB per second held (release samples)
        juce::int64 offset = 0, end = -1;
        LoopMode loopMode = LoopMode::none;
        juce::int64 loopStart = 0, loopEnd = -1;
        Trigger trigger = Trigger::attack;
        int group = 0, offBy = 0;
        bool offFast = true;
    };

    struct Sample
    {
        std::vector<std::vector<juce::int16>> channels;    // one or two channels
        double sampleRate = 44100.0;
        juce::int64 length = 0;
        juce::int64 loopStart = -1, loopEnd = -1;          // from the file's own loop points
    };

    struct Instrument
    {
        juce::String name;
        juce::File file;
        std::vector<Region> regions;
        std::vector<Sample> samples;
        juce::int64 memoryBytes = 0;
        juce::StringArray warnings;
    };

    using InstrumentPtr = std::shared_ptr<const Instrument>;

    // Parses an SFZ file into regions with sample paths (samples are not loaded).
    struct ParsedRegion { Region region; juce::File sample; };
    std::vector<ParsedRegion> parse (const juce::File& sfzFile, juce::StringArray& warnings);

    // Parses and loads all samples. Slow: call on a background thread.
    InstrumentPtr load (const juce::File& sfzFile, std::atomic<bool>* cancel = nullptr);

    int noteNumberFromText (const juce::String&);   // "c4" = 60, "f#2", "61"

    //==========================================================================
    //  Shared cache: tracks using the same instrument share one copy in memory.
    //==========================================================================
    class Library final : private juce::DeletedAtShutdown
    {
    public:
        static Library& getInstance();

        // Calls back on the message thread with the loaded instrument (or nullptr).
        void request (const juce::File& sfzFile, std::function<void (InstrumentPtr)> onLoaded);
        void forget (const juce::File& sfzFile);     // after a pack is removed

        juce::int64 getMemoryInUse();

    private:
        Library();
        ~Library() override;

        juce::CriticalSection lock;
        std::map<juce::String, std::weak_ptr<const Instrument>> cache;
        std::map<juce::String, std::vector<std::function<void (InstrumentPtr)>>> waiting;
        juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads (2).withThreadName ("Sample loader") };

        static Library* instance;
        JUCE_DECLARE_NON_COPYABLE (Library)
    };
}
