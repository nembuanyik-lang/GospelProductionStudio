#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

//==============================================================================
//  Loudness in LUFS (ITU-R BS.1770 / EBU R128), as streaming services measure it.
//  Short-term: the last 3 seconds. Integrated: the whole song, with the standard gates.
//  Safe on the audio thread after prepare() (no allocation while processing).
//==============================================================================
class LoudnessMeter
{
public:
    void prepare (double sampleRate, double maxSeconds = 3600.0)
    {
        sr = sampleRate;
        // K-weighting: a high shelf (+4 dB above ~1.7 kHz) and a high-pass (~38 Hz), for any sample rate
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                      2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K;
            highPass = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        subBlockLength = juce::jmax (1, (int) std::round (sr * 0.1));
        blocks.assign ((size_t) (maxSeconds * 10.0) + 8, 0.0);
        reset();
    }

    void reset()
    {
        for (auto& s : state) s = {};
        subSum = 0.0;
        subCount = 0;
        for (auto& s : recent) s = 0.0;
        recentCount = 0;
        numBlocks = 0;
        shortTerm = momentary = integrated = -100.0f;
    }

    void process (const float* left, const float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto l = filter (0, left[i]);
            const auto r = filter (1, right != nullptr ? right[i] : left[i]);
            subSum += l * l + r * r;

            if (++subCount >= subBlockLength)
                finishSubBlock();
        }
    }

    float getShortTerm() const     { return shortTerm; }
    float getMomentary() const     { return momentary; }
    float getIntegrated() const    { return integrated; }

    static float toLufs (double meanSquare)   { return meanSquare > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (meanSquare)) : -100.0f; }

private:
    struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State  { double z1a = 0, z2a = 0, z1b = 0, z2b = 0; };

    double filter (int ch, float x)
    {
        auto& s = state[ch];
        // Transposed direct form II, two stages
        const double y1 = shelf.b0 * x + s.z1a;
        s.z1a = shelf.b1 * x - shelf.a1 * y1 + s.z2a;
        s.z2a = shelf.b2 * x - shelf.a2 * y1;
        const double y2 = highPass.b0 * y1 + s.z1b;
        s.z1b = highPass.b1 * y1 - highPass.a1 * y2 + s.z2b;
        s.z2b = highPass.b2 * y1 - highPass.a2 * y2;
        return y2;
    }

    void finishSubBlock()
    {
        const auto ms = subSum / (double) subCount;
        subSum = 0.0;
        subCount = 0;

        // Keep the last 30 sub-blocks (3 s)
        for (int i = 29; i > 0; --i) recent[i] = recent[i - 1];
        recent[0] = ms;
        recentCount = juce::jmin (30, recentCount + 1);

        double m = 0.0;
        for (int i = 0; i < juce::jmin (4, recentCount); ++i) m += recent[i];
        const auto mom = m / juce::jmax (1, juce::jmin (4, recentCount));
        momentary = toLufs (mom);

        double st = 0.0;
        for (int i = 0; i < recentCount; ++i) st += recent[i];
        shortTerm = toLufs (st / juce::jmax (1, recentCount));

        // 400 ms gating blocks, 75 % overlap (one every 100 ms)
        if (recentCount >= 4 && numBlocks < blocks.size())
        {
            blocks[numBlocks++] = mom;
            updateIntegrated();
        }
    }

    void updateIntegrated()
    {
        const double absGate = std::pow (10.0, (-70.0 + 0.691) / 10.0);
        double sum = 0.0; int n = 0;
        for (size_t i = 0; i < numBlocks; ++i)
            if (blocks[i] > absGate) { sum += blocks[i]; ++n; }
        if (n == 0) { integrated = -100.0f; return; }

        const double relGate = (sum / n) * std::pow (10.0, -10.0 / 10.0);
        double sum2 = 0.0; int n2 = 0;
        for (size_t i = 0; i < numBlocks; ++i)
            if (blocks[i] > absGate && blocks[i] > relGate) { sum2 += blocks[i]; ++n2; }
        integrated = n2 > 0 ? toLufs (sum2 / n2) : -100.0f;
    }

    double sr = 44100.0;
    Coeffs shelf, highPass;
    State state[2];
    int subBlockLength = 4410, subCount = 0, recentCount = 0;
    double subSum = 0.0, recent[30] {};
    std::vector<double> blocks;
    size_t numBlocks = 0;
    float shortTerm = -100.0f, momentary = -100.0f, integrated = -100.0f;
};
