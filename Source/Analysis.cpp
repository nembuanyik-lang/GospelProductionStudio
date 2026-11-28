#include "Analysis.h"
#include "Loudness.h"

namespace Analysis
{
    const std::vector<float>& bandCentres()
    {
        static const std::vector<float> list = []
        {
            std::vector<float> v;
            for (double f = 20.0; f <= 20000.0 * 1.001; f *= std::pow (2.0, 1.0 / 6.0))
                v.push_back ((float) f);
            return v;
        }();
        return list;
    }

    int numBands()      { return (int) bandCentres().size(); }

    const std::vector<ToneBand>& toneBands()
    {
        static const std::vector<ToneBand> list {
            { "Bass",     "kick, bass guitar, organ pedals", 20.0f,   120.0f },
            { "Low mids", "warmth, or mud if too much",      120.0f,  500.0f },
            { "Mids",     "body of voices, keys, guitars",   500.0f,  2000.0f },
            { "Presence", "words, clarity, bite",            2000.0f, 6000.0f },
            { "Air",      "shine, cymbals, breath",          6000.0f, 20000.0f },
        };
        return list;
    }

    std::vector<float> toneBalance (const Spectrum& s)
    {
        std::vector<float> out;
        const auto& centres = bandCentres();
        for (auto& t : toneBands())
        {
            double sum = 0.0;
            int n = 0;
            for (size_t i = 0; i < centres.size() && i < s.db.size(); ++i)
                if (centres[i] >= t.from && centres[i] < t.to)
                {
                    sum += std::pow (10.0, s.db[i] / 10.0);
                    ++n;
                }
            out.push_back (n > 0 ? (float) (10.0 * std::log10 (sum + 1.0e-20)) : -120.0f);
        }
        return out;
    }

    //==========================================================================
    Averager::Averager (double sr) : sampleRate (sr)
    {
        window.resize ((size_t) size);
        for (int i = 0; i < size; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (size - 1));
        ring.assign ((size_t) size, 0.0f);
        work.assign ((size_t) size * 2, 0.0f);
        power.assign ((size_t) size / 2 + 1, 0.0);
    }

    void Averager::reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        std::fill (power.begin(), power.end(), 0.0);
        ringPos = sinceLast = 0;
        frames = seconds = 0.0;
        sumLR = sumLL = sumRR = sumMid = sumSide = 0.0;
    }

    void Averager::add (const float* left, const float* right, int n)
    {
        const double keep = memory > 0.0 ? std::exp (-(double) n / (memory * sampleRate)) : 1.0;
        sumLR *= keep; sumLL *= keep; sumRR *= keep; sumMid *= keep; sumSide *= keep;

        for (int i = 0; i < n; ++i)
        {
            const float l = left[i], r = right != nullptr ? right[i] : l;
            sumLR += l * r; sumLL += l * l; sumRR += r * r;
            const auto m = 0.5f * (l + r), s = 0.5f * (l - r);
            sumMid += m * m; sumSide += s * s;

            ring[(size_t) ringPos] = m;
            ringPos = (ringPos + 1) % size;
            if (++sinceLast >= size / 2)          // half-overlapping frames
            {
                sinceLast = 0;
                runFrame();
            }
        }
        seconds += n / sampleRate;
    }

    void Averager::runFrame()
    {
        std::fill (work.begin(), work.end(), 0.0f);
        for (int i = 0; i < size; ++i)
            work[(size_t) i] = ring[(size_t) ((ringPos + i) % size)] * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (work.data(), true);

        // Remember a few seconds only (live), or everything (a whole file)
        const double keep = memory > 0.0 ? std::exp (-(size / 2.0) / (memory * sampleRate)) : 1.0;
        const double scale = 4.0 / ((double) size * size);           // Hann window, to dBFS-like values
        for (size_t k = 0; k < power.size(); ++k)
            power[k] = power[k] * keep + (double) work[k] * work[k] * scale;
        frames = frames * keep + 1.0;
    }

    Spectrum Averager::getSpectrum() const
    {
        Spectrum s;
        if (frames < 0.5)
            return s;
        const auto& centres = bandCentres();
        const double binHz = sampleRate / size;
        const double half = std::pow (2.0, 1.0 / 12.0);
        for (auto c : centres)
        {
            const int k0 = juce::jlimit (1, (int) power.size() - 1, (int) std::floor (c / half / binHz));
            const int k1 = juce::jlimit (k0, (int) power.size() - 1, (int) std::ceil (c * half / binHz));
            double sum = 0.0;
            for (int k = k0; k <= k1; ++k) sum += power[(size_t) k];
            s.db.push_back ((float) (10.0 * std::log10 (sum / frames + 1.0e-20)));
        }
        return s;
    }

    float Averager::getCorrelation() const
    {
        const auto d = std::sqrt (sumLL * sumRR);
        return d > 1.0e-12 ? (float) (sumLR / d) : 1.0f;
    }

    float Averager::getWidth() const
    {
        return sumMid > 1.0e-12 ? (float) std::sqrt (sumSide / sumMid) : 0.0f;
    }

    //==========================================================================
    FileAnalysis analyseFile (const juce::File& file, std::atomic<bool>* cancel)
    {
        FileAnalysis a;
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
        if (r == nullptr || r->sampleRate <= 0.0)
            return a;

        Averager avg (r->sampleRate);
        LoudnessMeter meter;
        meter.prepare (r->sampleRate, (double) r->lengthInSamples / r->sampleRate + 2.0);
        juce::AudioBuffer<float> buf (2, 16384);
        float peak = 0.0f;
        for (juce::int64 pos = 0; pos < r->lengthInSamples; pos += 16384)
        {
            if (cancel != nullptr && cancel->load())
                return a;
            const auto n = (int) juce::jmin ((juce::int64) 16384, r->lengthInSamples - pos);
            r->read (&buf, 0, n, pos, true, true);
            const auto* l = buf.getReadPointer (0);
            const auto* rr = r->numChannels > 1 ? buf.getReadPointer (1) : nullptr;
            avg.add (l, rr, n);
            meter.process (l, rr, n);
            peak = juce::jmax (peak, buf.getMagnitude (0, 0, n), r->numChannels > 1 ? buf.getMagnitude (1, 0, n) : 0.0f);
        }
        a.spectrum = avg.getSpectrum();
        a.lufs = meter.getIntegrated();
        a.truePeakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
        a.correlation = avg.getCorrelation();
        a.width = avg.getWidth();
        a.seconds = avg.getSecondsAnalysed();
        a.ok = a.spectrum.valid();
        return a;
    }

    juce::StringArray describeDifferences (const FileAnalysis& mix, const FileAnalysis& ref)
    {
        juce::StringArray out;
        if (! mix.ok || ! ref.ok)
            return out;

        // Compare the shapes at the same loudness
        const auto shift = ref.lufs - mix.lufs;
        const auto m = toneBalance (mix.spectrum), r = toneBalance (ref.spectrum);
        const auto& bands = toneBands();
        for (size_t i = 0; i < bands.size(); ++i)
        {
            const auto d = (m[i] + shift) - r[i];
            if (std::abs (d) < 1.5f)
                continue;
            juce::String line;
            line << bands[i].name << ": " << juce::String (std::abs (d), 1) << " dB " << (d > 0 ? "more" : "less")
                 << " than the reference (" << bands[i].hint << ")";
            out.add (line);
        }
        if (out.isEmpty())
            out.add ("Tone balance: close to the reference in every band.");

        const auto loud = mix.lufs - ref.lufs;
        if (std::abs (loud) >= 1.0f)
            out.add ("Loudness: your mix is " + juce::String (std::abs (loud), 1) + " dB " + (loud > 0 ? "louder" : "quieter")
                     + " (" + juce::String (mix.lufs, 1) + " against " + juce::String (ref.lufs, 1) + " LUFS)."
                     + (loud < 0 ? " Mastering can bring it up." : ""));
        else
            out.add ("Loudness: about the same (" + juce::String (mix.lufs, 1) + " LUFS).");

        const auto w = mix.width - ref.width;
        if (std::abs (w) > 0.08f)
            out.add (juce::String ("Stereo: your mix is ") + (w > 0 ? "wider" : "narrower") + " than the reference.");
        if (mix.truePeakDb > 0.0f)
            out.add ("Peaks: your mix goes over 0 dB (" + juce::String (mix.truePeakDb, 1) + " dB) and will clip. Turn the Master down or use Mastering.");
        if (mix.correlation < 0.0f)
            out.add ("Stereo: part of your mix is out of phase (it may disappear on a phone speaker).");
        return out;
    }
}
