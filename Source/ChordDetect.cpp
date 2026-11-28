#include "ChordDetect.h"
#include "ChordTrack.h"
#include "Loops.h"
#include "StudioLookAndFeel.h"
#include "BackgroundWork.h"

namespace ChordDetect
{
    namespace
    {
        const char* qualityIds[] = { "", "m", "7", "maj7", "m7", "sus4", "dim" };
        const float qualityPenalty[] = { 0.0f, 0.0f, 0.03f, 0.045f, 0.045f, 0.05f, 0.05f };
        constexpr int numQualities = 7;
        constexpr int noChord = numQualities * 12;

        const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int minorScale[] = { 0, 2, 3, 5, 7, 8, 10 };

        int pcOf (int n)   { return ((n % 12) + 12) % 12; }

        std::vector<int> intervalsOf (int q)
        {
            std::vector<int> out;
            for (auto i : Theory::findQuality (qualityIds[q]).intervals) out.push_back (pcOf (i));
            return out;
        }

        // Template: each chord note with its first harmonics (a real instrument's C also sounds G and E)
        std::array<float, 12> makeTemplate (int root, int q)
        {
            std::array<float, 12> t {};
            const auto iv = intervalsOf (q);
            for (size_t k = 0; k < iv.size(); ++k)
            {
                const float noteWeight = k == 0 ? 1.0f : (k == 1 ? 0.95f : (k == 2 ? 0.8f : 0.75f));
                for (int h = 1; h <= 5; ++h)
                {
                    const int offset = juce::roundToInt (12.0 * std::log2 ((double) h));
                    t[(size_t) pcOf (root + iv[k] + offset)] += noteWeight * std::pow (0.6f, (float) (h - 1));
                }
            }
            float norm = 0.0f;
            for (auto v : t) norm += v * v;
            norm = std::sqrt (juce::jmax (1.0e-9f, norm));
            for (auto& v : t) v /= norm;
            return t;
        }

        juce::AudioFormatManager& formats()
        {
            static juce::AudioFormatManager fm;
            static bool done = false;
            if (! done) { fm.registerBasicFormats(); done = true; }
            return fm;
        }
    }

    //==========================================================================
    //  Measuring the notes on each beat
    //==========================================================================
    std::vector<BeatChroma> chromaFromAudio (const juce::AudioBuffer<float>& buffer, double sr, const std::vector<double>& edges)
    {
        std::vector<BeatChroma> beats (edges.size() > 1 ? edges.size() - 1 : 0);
        const int n = buffer.getNumSamples();
        if (beats.empty() || n < 8192 || sr <= 0.0)
            return beats;

        std::vector<float> x ((size_t) n, 0.0f);
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            juce::FloatVectorOperations::addWithMultiply (x.data(), buffer.getReadPointer (c), 1.0f / (float) buffer.getNumChannels(), n);

        constexpr int order = 13, size = 1 << order;
        const int hop = size / 4;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> buf ((size_t) size * 2);

        // Each bin: which note, and whether it counts for the bass or the rest
        std::vector<int> binPc ((size_t) size / 2, -1);
        std::vector<char> binBass ((size_t) size / 2, 0), binTreble ((size_t) size / 2, 0);
        for (int k = 1; k < size / 2; ++k)
        {
            const double f = k * sr / size;
            if (f < 32.0 || f > 4200.0) continue;
            binPc[(size_t) k] = pcOf (juce::roundToInt (69.0 + 12.0 * std::log2 (f / 440.0)));
            binBass[(size_t) k] = f < 260.0;
            binTreble[(size_t) k] = f > 100.0;
        }

        const int frames = n > size ? (n - size) / hop + 1 : 0;
        std::vector<std::array<float, 12>> fTreble ((size_t) frames), fBass ((size_t) frames);
        std::vector<float> fEnergy ((size_t) frames, 0.0f);
        for (int f = 0; f < frames; ++f)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            std::copy (x.begin() + f * hop, x.begin() + f * hop + size, buf.begin());
            window.multiplyWithWindowingTable (buf.data(), (size_t) size);
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);
            fTreble[(size_t) f].fill (0.0f);
            fBass[(size_t) f].fill (0.0f);
            float e = 0.0f;
            for (int k = 2; k < size / 2 - 1; ++k)
            {
                const auto pc = binPc[(size_t) k];
                if (pc < 0) continue;
                const float m = buf[(size_t) k];
                e += m * m;
                // Only the peaks of the spectrum (the notes), not the noise between them
                if (m < buf[(size_t) k - 1] || m < buf[(size_t) k + 1]) continue;
                const float v = std::log1p (m * 20.0f);
                if (binTreble[(size_t) k]) fTreble[(size_t) f][(size_t) pc] += v;
                if (binBass[(size_t) k])   fBass[(size_t) f][(size_t) pc] += v;
            }
            fEnergy[(size_t) f] = e;
        }

        for (size_t b = 0; b < beats.size(); ++b)
        {
            int count = 0;
            for (int f = 0; f < frames; ++f)
            {
                const double t = (f * hop + size / 2) / sr;
                if (t < edges[b] || t >= edges[b + 1]) continue;
                for (int i = 0; i < 12; ++i)
                {
                    beats[b].treble[(size_t) i] += fTreble[(size_t) f][(size_t) i];
                    beats[b].bass[(size_t) i] += fBass[(size_t) f][(size_t) i];
                }
                beats[b].energy += fEnergy[(size_t) f];
                ++count;
            }
            if (count == 0 && frames > 0)
            {
                const int f = juce::jlimit (0, frames - 1, (int) (((edges[b] + edges[b + 1]) * 0.5 * sr - size / 2) / hop));
                beats[b].treble = fTreble[(size_t) f];
                beats[b].bass = fBass[(size_t) f];
                beats[b].energy = fEnergy[(size_t) f];
                count = 1;
            }
            if (count > 1)
            {
                for (auto& v : beats[b].treble) v /= (float) count;
                for (auto& v : beats[b].bass) v /= (float) count;
                beats[b].energy /= (float) count;
            }
        }
        return beats;
    }

    std::vector<BeatChroma> chromaFromNotes (const std::vector<std::tuple<int, double, double, int>>& notes, double firstBeat, int numBeats)
    {
        std::vector<BeatChroma> beats ((size_t) juce::jmax (0, numBeats));
        for (int b = 0; b < numBeats; ++b)
        {
            const double b0 = firstBeat + b, b1 = b0 + 1.0;
            int lowest = 200;
            float lowestWeight = 0.0f;
            for (auto& [pitch, start, length, vel] : notes)
            {
                const double overlap = juce::jmin (b1, start + length) - juce::jmax (b0, start);
                if (overlap <= 0.0) continue;
                const float w = (float) overlap * (0.3f + vel / 127.0f);
                if (pitch < 48)
                {
                    // Low notes are the bass line (like the audio, where the bass is measured on its own)
                    beats[(size_t) b].energy += w;
                    if (pitch < lowest) { lowest = pitch; lowestWeight = w; }
                    continue;
                }
                beats[(size_t) b].treble[(size_t) pcOf (pitch)] += w;
                // Harmonics, like a real instrument, so the same templates fit
                beats[(size_t) b].treble[(size_t) pcOf (pitch + 7)] += w * 0.36f;
                beats[(size_t) b].treble[(size_t) pcOf (pitch + 4)] += w * 0.13f;
                beats[(size_t) b].energy += w;
                if (pitch < lowest) { lowest = pitch; lowestWeight = w; }
            }
            if (lowest < 200)
                beats[(size_t) b].bass[(size_t) pcOf (lowest)] += lowestWeight + 1.0f;
        }
        return beats;
    }

    Theory::Key findKey (const std::vector<BeatChroma>& beats, float& strength)
    {
        static const float major[] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float minor[] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        std::array<double, 12> total {};
        for (auto& b : beats)
        {
            double s = 1.0e-9;
            for (int i = 0; i < 12; ++i) s += b.treble[(size_t) i] + b.bass[(size_t) i];
            for (int i = 0; i < 12; ++i) total[(size_t) i] += (b.treble[(size_t) i] + b.bass[(size_t) i]) / s;
        }
        auto corr = [&] (const float* profile, int tonic)
        {
            double mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += total[(size_t) i]; my += profile[i]; }
            mx /= 12; my /= 12;
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double a = total[(size_t) pcOf (i + tonic)] - mx, b = profile[i] - my;
                sxy += a * b; sxx += a * a; syy += b * b;
            }
            return sxy / std::sqrt (sxx * syy + 1.0e-12);
        };
        Theory::Key best;
        double bestR = -2.0, second = -2.0;
        for (int t = 0; t < 12; ++t)
            for (int m = 0; m < 2; ++m)
            {
                const auto r = corr (m == 0 ? major : minor, t);
                if (r > bestR) { second = bestR; bestR = r; best.tonic = t; best.minor = m == 1; }
                else if (r > second) second = r;
            }
        best = Theory::keyFromSong (best.tonic, best.minor, {});
        strength = (float) juce::jlimit (0.0, 1.0, bestR);
        return best;
    }

    //==========================================================================
    //  Choosing the chords (best path through the song)
    //==========================================================================
    std::vector<Detected> decode (const std::vector<BeatChroma>& beats, double firstBeat, double bpb, const Theory::Key& key)
    {
        std::vector<Detected> out;
        const int T = (int) beats.size();
        if (T == 0)
            return out;
        constexpr int S = noChord + 1;

        std::array<std::array<float, 12>, noChord> templates;
        std::array<bool, noChord> diatonic {};
        const int* scale = key.minor ? minorScale : majorScale;
        for (int q = 0; q < numQualities; ++q)
            for (int r = 0; r < 12; ++r)
            {
                templates[(size_t) (q * 12 + r)] = makeTemplate (r, q);
                bool in = true;
                for (auto iv : intervalsOf (q))
                {
                    const int rel = pcOf (r + iv - key.tonic);
                    in = in && std::find (scale, scale + 7, rel) != scale + 7;
                }
                diatonic[(size_t) (q * 12 + r)] = in;
            }

        std::vector<float> energies;
        for (auto& b : beats) energies.push_back (b.energy);
        auto sorted = energies;
        std::sort (sorted.begin(), sorted.end());
        const float quiet = sorted[sorted.size() / 2] * 0.03f;

        // Scores for every beat and chord
        std::vector<std::array<float, S>> emit ((size_t) T);
        for (int t = 0; t < T; ++t)
        {
            auto& e = emit[(size_t) t];
            const auto& c = beats[(size_t) t];
            float norm = 0.0f;
            for (auto v : c.treble) norm += v * v;
            norm = std::sqrt (norm);
            if (c.energy <= quiet || norm < 1.0e-6f)
            {
                e.fill (0.0f);
                e[noChord] = 1.0f;
                continue;
            }
            for (int s = 0; s < noChord; ++s)
            {
                float dot = 0.0f;
                for (int i = 0; i < 12; ++i) dot += c.treble[(size_t) i] / norm * templates[(size_t) s][(size_t) i];
                // The bass usually plays the root: a little help from it
                float bassTotal = 1.0e-6f;
                for (auto v : c.bass) bassTotal += v;
                const float bassRoot = c.bass[(size_t) (s % 12)] / bassTotal;
                e[(size_t) s] = dot + (diatonic[(size_t) s] ? 0.03f : 0.0f) - qualityPenalty[s / 12] + 0.08f * bassRoot;
            }
            e[noChord] = 0.25f;
        }

        // Viterbi: changing chord costs something, least on the first beat of a bar
        auto changeCost = [&] (int t)
        {
            const double inBar = std::fmod (firstBeat + t, bpb);
            if (inBar < 1.0e-6 || bpb - inBar < 1.0e-6) return 0.06f;
            if (std::abs (inBar - bpb / 2.0) < 1.0e-6) return 0.1f;
            return 0.18f;
        };
        std::vector<std::array<float, S>> score ((size_t) T);
        std::vector<std::array<short, S>> back ((size_t) T);
        score[0] = emit[0];
        for (int t = 1; t < T; ++t)
        {
            int bestPrev = 0;
            for (int s = 1; s < S; ++s) if (score[(size_t) t - 1][(size_t) s] > score[(size_t) t - 1][(size_t) bestPrev]) bestPrev = s;
            const float cost = changeCost (t);
            for (int s = 0; s < S; ++s)
            {
                const float stay = score[(size_t) t - 1][(size_t) s];
                const float move = score[(size_t) t - 1][(size_t) bestPrev] - cost;
                if (stay >= move) { score[(size_t) t][(size_t) s] = stay + emit[(size_t) t][(size_t) s]; back[(size_t) t][(size_t) s] = (short) s; }
                else              { score[(size_t) t][(size_t) s] = move + emit[(size_t) t][(size_t) s]; back[(size_t) t][(size_t) s] = (short) bestPrev; }
            }
        }
        std::vector<int> path ((size_t) T);
        int s = 0;
        for (int k = 1; k < S; ++k) if (score[(size_t) T - 1][(size_t) k] > score[(size_t) T - 1][(size_t) s]) s = k;
        for (int t = T - 1; t >= 0; --t) { path[(size_t) t] = s; s = back[(size_t) t][(size_t) s]; }

        // Join the beats into chords; slash chords from the bass
        for (int t = 0; t < T;)
        {
            int u = t;
            while (u < T && path[(size_t) u] == path[(size_t) t]) ++u;
            const int st = path[(size_t) t];
            if (st != noChord)
            {
                Detected d;
                d.start = firstBeat + t;
                d.length = u - t;
                d.rootPc = st % 12;
                d.quality = qualityIds[st / 12];
                std::array<float, 12> bass {};
                float conf = 0.0f;
                for (int k = t; k < u; ++k)
                {
                    // The bass note at the start of the chord says the most (later it often walks to the fifth)
                    const float w = k == t ? 2.0f : 1.0f;
                    for (int i = 0; i < 12; ++i) bass[(size_t) i] += w * beats[(size_t) k].bass[(size_t) i];
                    float second = 0.0f;
                    for (int x = 0; x < noChord; ++x) if (x != st) second = juce::jmax (second, emit[(size_t) k][(size_t) x]);
                    conf += juce::jlimit (0.0f, 1.0f, (emit[(size_t) k][(size_t) st] - second) * 8.0f + 0.5f);
                }
                d.confidence = conf / (float) (u - t);
                float total = 1.0e-6f;
                int bpc = 0;
                for (int i = 0; i < 12; ++i) { total += bass[(size_t) i]; if (bass[(size_t) i] > bass[(size_t) bpc]) bpc = i; }
                if (bpc != d.rootPc && bass[(size_t) bpc] > 1.4f * bass[(size_t) d.rootPc] && bass[(size_t) bpc] > 0.3f * total)
                    d.bassPc = bpc;
                out.push_back (d);
            }
            t = u;
        }
        return out;
    }

    double guessTempo (const juce::AudioBuffer<float>& buffer, double sr)
    {
        const int n = buffer.getNumSamples();
        if (n < (int) (sr * 4.0)) return 0.0;
        constexpr int order = 10, size = 1 << order, hop = 512;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> buf ((size_t) size * 2), prev ((size_t) size / 2, 0.0f), flux;
        for (int pos = 0; pos + size < n; pos += hop)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                for (int i = 0; i < size; ++i) buf[(size_t) i] += buffer.getSample (c, pos + i);
            window.multiplyWithWindowingTable (buf.data(), (size_t) size);
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);
            float f = 0.0f;
            for (int k = 1; k < size / 2; ++k)
            {
                const auto m = std::log1p (buf[(size_t) k] * 10.0f);
                f += juce::jmax (0.0f, m - prev[(size_t) k]);
                prev[(size_t) k] = m;
            }
            flux.push_back (f);
        }
        float mean = 0.0f;
        for (auto v : flux) mean += v;
        mean /= (float) juce::jmax<size_t> (1, flux.size());
        for (auto& v : flux) v = juce::jmax (0.0f, v - mean);

        const double frameRate = sr / hop;
        double best = 0.0, bestBpm = 0.0;
        std::vector<double> ac;
        const int minLag = (int) std::floor (frameRate * 60.0 / 190.0), maxLag = (int) std::ceil (frameRate * 60.0 / 55.0);
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double s = 0.0;
            for (size_t i = (size_t) lag; i < flux.size(); ++i) s += flux[i] * flux[i - (size_t) lag];
            ac.push_back (s / (double) (flux.size() - (size_t) lag));
        }
        for (size_t i = 1; i + 1 < ac.size(); ++i)
        {
            if (ac[i] < ac[i - 1] || ac[i] < ac[i + 1]) continue;
            const double lag = minLag + (double) i + 0.5 * (ac[i - 1] - ac[i + 1]) / (ac[i - 1] - 2.0 * ac[i] + ac[i + 1] + 1.0e-12);
            const double bpm = 60.0 * frameRate / lag;
            // Most church songs sit between 70 and 140 BPM
            const double prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 100.0) / 0.6, 2.0));
            if (ac[i] * prior > best) { best = ac[i] * prior; bestBpm = bpm; }
        }
        return std::round (bestBpm * 2.0) / 2.0;
    }

    juce::String chordName (const Detected& d, const Theory::Key& key)
    {
        return Theory::nameLabel (toChord (d, key), key);
    }

    Theory::Chord toChord (const Detected& d, const Theory::Key& key)
    {
        Theory::Chord c;
        c.root = pcOf (d.rootPc - key.tonic);
        c.quality = d.quality;
        c.bass = d.bassPc >= 0 ? pcOf (d.bassPc - key.tonic) : -1;
        return c;
    }

    //==========================================================================
    //  On a clip
    //==========================================================================
    namespace
    {
        struct Job
        {
            bool ok = false;
            juce::String problem;
            bool midi = false;
            std::vector<std::tuple<int, double, double, int>> notes;
            juce::File source;
            std::vector<double> edges;          // seconds in the recording, one more than the beats
            double firstBeat = 0.0, bpb = 4.0;
            int numBeats = 0;
        };

        Job prepareJob (te::Clip& clip, double bpb)
        {
            Job job;
            job.bpb = bpb;
            auto& ts = clip.edit.tempoSequence;
            const auto pos = clip.getPosition();
            const double startBeat = ts.toBeats (pos.getStart()).inBeats(), endBeat = ts.toBeats (pos.getEnd()).inBeats();
            job.firstBeat = std::ceil (startBeat - 1.0e-6);
            job.numBeats = (int) std::floor (endBeat + 1.0e-6 - job.firstBeat);
            if (job.numBeats < 1) { job.problem = "This clip is shorter than one beat."; return job; }

            if (auto* mc = dynamic_cast<te::MidiClip*> (&clip))
            {
                job.midi = true;
                const auto contentStart = mc->getContentBeatAtTime (pos.getStart()).inBeats();
                for (auto* n : mc->getSequence().getNotes())
                    job.notes.push_back ({ n->getNoteNumber(), startBeat + (n->getStartBeat().inBeats() - contentStart), n->getLengthBeats().inBeats(), n->getVelocity() });
                if (job.notes.empty()) { job.problem = "This clip has no notes."; return job; }
                job.ok = true;
                return job;
            }
            auto* wc = dynamic_cast<te::WaveAudioClip*> (&clip);
            if (wc == nullptr) { job.problem = "Chords can be found in recordings and MIDI clips."; return job; }
            if (std::abs (wc->getSpeedRatio() - 1.0) > 0.001 || wc->getAutoTempo() || Loops::isFollowing (*wc))
            {
                job.problem = "This clip plays faster or slower than it was recorded. Find the chords on a clip at its own speed.";
                return job;
            }
            job.source = wc->getOriginalFile();
            const auto clipStart = pos.getStart().inSeconds(), offset = pos.getOffset().inSeconds();
            for (int b = 0; b <= job.numBeats; ++b)
                job.edges.push_back (offset + ts.toTime (te::BeatPosition::fromBeats (job.firstBeat + b)).inSeconds() - clipStart);
            job.ok = true;
            return job;
        }

        Result runJob (const Job& job)
        {
            Result r;
            if (! job.ok) { r.problem = job.problem; return r; }
            std::vector<BeatChroma> beats;
            if (job.midi)
                beats = chromaFromNotes (job.notes, job.firstBeat, job.numBeats);
            else
            {
                std::unique_ptr<juce::AudioFormatReader> rd (formats().createReaderFor (job.source));
                if (rd == nullptr) { r.problem = "The recording could not be read."; return r; }
                const auto maxLen = (juce::int64) (rd->sampleRate * 60.0 * 15.0);
                const int len = (int) juce::jmin (rd->lengthInSamples, maxLen);
                const int ch = (int) juce::jlimit (1u, 2u, rd->numChannels);
                juce::AudioBuffer<float> b (ch, len);
                rd->read (&b, 0, len, 0, true, ch > 1);
                beats = chromaFromAudio (b, rd->sampleRate, job.edges);
                r.tempoGuess = guessTempo (b, rd->sampleRate);
            }
            r.key = findKey (beats, r.keyStrength);
            r.chords = decode (beats, job.firstBeat, job.bpb, r.key);
            r.firstBeat = job.firstBeat;
            r.lastBeat = job.firstBeat + job.numBeats;
            r.ok = ! r.chords.empty();
            if (! r.ok) r.problem = "No chords were heard in this clip.";
            return r;
        }
    }

    Result detect (te::Clip& clip, double bpb)
    {
        return runJob (prepareJob (clip, bpb));
    }

    int writeToChordTrack (te::Edit& edit, const Result& r, const Theory::Key& songKey)
    {
        if (! r.ok) return 0;
        ChordTrack::clearRange (edit, r.firstBeat, r.lastBeat);
        for (auto& d : r.chords)
            ChordTrack::add (edit, toChord (d, songKey), d.start, d.length);
        return (int) r.chords.size();
    }

    //==========================================================================
    //  The panel
    //==========================================================================
    namespace
    {
        class Panel final : public juce::Component
        {
        public:
            Panel (te::Clip& c, std::function<Theory::Key()> gk, std::function<double()> gb, std::function<void()> changed)
                : keep (&c), editRef (c.edit), getKey (std::move (gk)), getBpb (std::move (gb)), onChanged (std::move (changed))
            {
                using namespace StudioColours;
                title.setText ("Find the chords", juce::dontSendNotification);
                title.setFont (studioFont (17.0f, true));
                title.setColour (juce::Label::textColourId, text);
                addAndMakeVisible (title);
                summary.setColour (juce::Label::textColourId, textDim);
                summary.setFont (studioFont (12.5f));
                summary.setJustificationType (juce::Justification::topLeft);
                addAndMakeVisible (summary);
                list.setMultiLine (true);
                list.setReadOnly (true);
                list.setScrollbarsShown (true);
                list.setFont (studioFont (13.0f));
                addAndMakeVisible (list);
                writeButton.onClick = [this] { write(); };
                writeButton.setEnabled (false);
                addAndMakeVisible (writeButton);
                setSize (560, 420);

                summary.setText ("Listening to the clip...", juce::dontSendNotification);
                auto job = std::make_shared<Job> (prepareJob (c, getBpb ? getBpb() : 4.0));
                juce::Component::SafePointer<Panel> safe (this);
                BackgroundWork::run ("Finding the chords", [safe, job] (const std::atomic<bool>&)
                {
                    auto found = std::make_shared<Result> (runJob (*job));
                    BackgroundWork::onMessageThread ([safe, found] { if (auto* p = safe.getComponent()) p->show (*found); });
                }, false);
            }

            void show (const Result& r)
            {
                result = r;
                if (! r.ok) { summary.setText (r.problem, juce::dontSendNotification); return; }
                const auto songKey = getKey ? getKey() : Theory::Key();
                const auto bpb = getBpb ? getBpb() : 4.0;
                juce::String t;
                t << "Found " << (int) r.chords.size() << (r.chords.size() == 1 ? " chord" : " chords") << ". The music seems to be in "
                  << Theory::noteName (r.key.tonic, r.key.flats) << (r.key.minor ? " minor" : " major");
                if (r.key.tonic != songKey.tonic || r.key.minor != songKey.minor)
                    t << " (your song is set to " << Theory::noteName (songKey.tonic, songKey.flats) << (songKey.minor ? " minor" : " major")
                      << ": change the Key at the top if you want the chord numbers to match)";
                t << ".";
                if (r.tempoGuess > 0.0 && editRef.get() != nullptr)
                {
                    const auto songBpm = editRef->tempoSequence.getTempoAt (te::TimePosition()).getBpm();
                    t << "\nThe recording's tempo is about " << juce::String (r.tempoGuess, 1) << " BPM";
                    if (std::abs (r.tempoGuess - songBpm) > 2.0 && std::abs (r.tempoGuess / 2.0 - songBpm) > 1.0 && std::abs (r.tempoGuess * 2.0 - songBpm) > 2.0)
                        t << " but the song is at " << juce::roundToInt (songBpm) << " BPM: set the song's Tempo to match and line the clip up with bar 1, then find the chords again";
                    t << ".";
                }
                summary.setText (t, juce::dontSendNotification);

                juce::String lines;
                for (auto& d : r.chords)
                {
                    const int bar = (int) std::floor (d.start / bpb) + 1;
                    const double beat = std::fmod (d.start, bpb) + 1.0;
                    lines << "Bar " << bar << (beat > 1.001 ? "." + juce::String ((int) beat) : juce::String()) << "   "
                          << chordName (d, songKey) << "   " << Theory::numberLabel (toChord (d, songKey))
                          << (d.confidence < 0.45f ? "   (not sure)" : "") << "\n";
                }
                list.setText (lines, false);
                writeButton.setEnabled (true);
            }

            void write()
            {
                if (editRef.get() == nullptr || ! result.ok) return;
                editRef->getUndoManager().beginNewTransaction ("Chords from a recording");
                const auto n = writeToChordTrack (*editRef, result, getKey ? getKey() : Theory::Key());
                summary.setText (juce::String (n) + " chords are on the Chord Track (Ctrl+Z takes them away). Check the ones marked \"not sure\" by ear.",
                                 juce::dontSendNotification);
                if (onChanged) onChanged();
            }

            void paint (juce::Graphics& g) override   { g.fillAll (StudioColours::card); }

            void resized() override
            {
                auto r = getLocalBounds().reduced (14, 10);
                title.setBounds (r.removeFromTop (26));
                summary.setBounds (r.removeFromTop (78));
                auto buttons = r.removeFromBottom (30);
                writeButton.setBounds (buttons.removeFromRight (220));
                r.removeFromBottom (8);
                list.setBounds (r);
            }

        private:
            te::Clip::Ptr keep;
            te::SafeSelectable<te::Edit> editRef;
            std::function<Theory::Key()> getKey;
            std::function<double()> getBpb;
            std::function<void()> onChanged;
            Result result;
            juce::Label title, summary;
            juce::TextEditor list;
            juce::TextButton writeButton { "Put them on the Chord Track" };
        };
    }

    std::unique_ptr<juce::Component> createPanel (te::Clip& clip, std::function<Theory::Key()> getKey, std::function<double()> getBpb,
                                                  std::function<void()> onChanged)
    {
        return std::make_unique<Panel> (clip, std::move (getKey), std::move (getBpb), std::move (onChanged));
    }
}
