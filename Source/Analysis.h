#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>

//==============================================================================
//  Analysers (plan Step 16): what a mix sounds like, in numbers.
//
//  Spectrum: how much sound there is from deep bass to the top "air", in
//  1/6-octave bands. Tone balance: the same in five plain bands. Stereo:
//  how wide the mix is. Loudness: LUFS. A reference song is measured the
//  same way, so the two can be compared fairly.
//==============================================================================
namespace Analysis
{
    // 1/6-octave bands from 20 Hz to 20 kHz
    const std::vector<float>& bandCentres();
    int numBands();

    struct Spectrum
    {
        std::vector<float> db;                 // one value per band (average power, dB)
        bool valid() const                     { return ! db.empty(); }
    };

    // The five bands people talk about
    struct ToneBand { const char* name; const char* hint; float from, to; };
    const std::vector<ToneBand>& toneBands();
    std::vector<float> toneBalance (const Spectrum&);     // dB per tone band

    // Collects a long-term average spectrum and stereo figures
    class Averager
    {
    public:
        explicit Averager (double sampleRate = 44100.0);
        void reset();
        void add (const float* left, const float* right, int numSamples);
        Spectrum getSpectrum() const;
        float getCorrelation() const;          // +1 mono, 0 wide, below 0 out of phase
        float getWidth() const;                // side level against mid level, 0..1+
        double getSecondsAnalysed() const      { return seconds; }

        // Only the last few seconds (for the live view)
        void setDecay (double secondsToRemember)  { memory = secondsToRemember; }

    private:
        void runFrame();

        double sampleRate;
        static constexpr int order = 13, size = 1 << order;
        juce::dsp::FFT fft { order };
        std::vector<float> window, ring, work;
        int ringPos = 0, sinceLast = 0;
        std::vector<double> power;            // per FFT bin, averaged
        double frames = 0.0, seconds = 0.0, memory = 0.0;
        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0, sumMid = 0.0, sumSide = 0.0;
    };

    struct FileAnalysis
    {
        bool ok = false;
        Spectrum spectrum;
        float lufs = -100.0f, truePeakDb = -100.0f, correlation = 1.0f, width = 0.0f;
        double seconds = 0.0;
    };

    FileAnalysis analyseFile (const juce::File&, std::atomic<bool>* cancel = nullptr);

    // Plain words about the difference between a mix and a reference (loudness-matched)
    juce::StringArray describeDifferences (const FileAnalysis& mix, const FileAnalysis& reference);
}
