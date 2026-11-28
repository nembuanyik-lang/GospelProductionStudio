#include "MySounds.h"
#include "Instruments.h"

namespace MySounds
{
    namespace
    {
        const juce::Identifier slotsId ("SOUNDS"), slotId ("SOUND"),
                               fileId ("file"), nameId ("name"), keyId ("key"), modeId ("mode"),
                               volumeId ("volumeDb"), rootId ("root"), rangeId ("range");

        Slot fromTree (const juce::ValueTree& v)
        {
            Slot s;
            s.file = juce::File (v[fileId].toString());
            s.name = v.getProperty (nameId, s.file.getFileNameWithoutExtension()).toString();
            s.key = juce::jlimit (0, 127, (int) v.getProperty (keyId, 48));
            s.mode = (int) v.getProperty (modeId, 0) == 1 ? Mode::melodic : Mode::hit;
            s.volumeDb = (float) v.getProperty (volumeId, 0.0f);
            s.root = (int) v.getProperty (rootId, -1);
            s.range = juce::jlimit (1, 60, (int) v.getProperty (rangeId, 24));
            return s;
        }

        bool keyIsUsed (const std::vector<Slot>& slots, int key)
        {
            for (const auto& s : slots)
            {
                if (s.mode == Mode::hit && s.key == key) return true;
                if (s.mode == Mode::melodic && key >= s.key && key < s.key + s.range) return true;
            }
            return false;
        }

        bool isBlackKey (int note)
        {
            const auto n = note % 12;
            return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
        }
    }

    juce::AudioFormatManager& formats()
    {
        static juce::AudioFormatManager fm;
        static bool done = false;
        if (! done)
        {
            fm.registerBasicFormats();
            done = true;
        }
        return fm;
    }

    bool isSlotTree (const juce::ValueTree& v)  { return v.hasType (slotId) || v.hasType (slotsId); }

    juce::ValueTree slotsTree (juce::ValueTree pluginState)
    {
        auto t = pluginState.getChildWithName (slotsId);
        if (! t.isValid())
        {
            t = juce::ValueTree (slotsId);
            pluginState.appendChild (t, nullptr);
        }
        return t;
    }

    std::vector<Slot> read (const juce::ValueTree& pluginState)
    {
        std::vector<Slot> out;
        for (const auto& v : pluginState.getChildWithName (slotsId))
            if (v.hasType (slotId))
                out.push_back (fromTree (v));
        return out;
    }

    int nextFreeKey (const juce::ValueTree& pluginState, Mode mode)
    {
        const auto slots = read (pluginState);
        if (mode == Mode::melodic)
        {
            // Two octaves that are not used yet, starting on a C
            for (int start = 36; start <= 84; start += 12)
            {
                bool free = true;
                for (int k = start; k < start + 24 && free; ++k)
                    free = ! keyIsUsed (slots, k);
                if (free) return start;
            }
            return 72;
        }

        // Hits: white keys from C3 (48) up, then the black keys
        for (int k = 48; k < 108; ++k)
            if (! isBlackKey (k) && ! keyIsUsed (slots, k))
                return k;
        for (int k = 36; k < 108; ++k)
            if (! keyIsUsed (slots, k))
                return k;
        return 60;
    }

    juce::ValueTree add (juce::ValueTree pluginState, const juce::File& sample, juce::UndoManager* um)
    {
        // A short, clearly pitched sound is probably a note to play tunes with... but a stab
        // ("hey!", horn hit) is pitched too. Start everything as a hit: one click to change it.
        juce::ValueTree v (slotId);
        v.setProperty (fileId, sample.getFullPathName(), nullptr);
        v.setProperty (nameId, sample.getFileNameWithoutExtension(), nullptr);
        v.setProperty (keyId, nextFreeKey (pluginState, Mode::hit), nullptr);
        v.setProperty (modeId, 0, nullptr);
        v.setProperty (volumeId, 0.0f, nullptr);
        v.setProperty (rootId, detectRootNote (sample), nullptr);
        slotsTree (pluginState).appendChild (v, um);
        return v;
    }

    void setMode (juce::ValueTree slot, Mode mode, juce::UndoManager* um)
    {
        if (! slot.isValid())
            return;
        auto plugin = slot.getParent().getParent();
        slot.setProperty (modeId, (int) mode, um);

        if (mode == Mode::melodic)
        {
            // Move it to a free two-octave range, so it does not cover the hits
            slot.setProperty (keyId, -1000, nullptr);
            auto start = nextFreeKey (plugin, Mode::melodic);

            // Best: the range that has the recorded note in it, so that key plays it as recorded
            const int root = slot.getProperty (rootId, -1);
            if (root >= 0)
            {
                const auto slots = read (plugin);
                for (int s0 : { (root / 12) * 12 - 12, (root / 12) * 12 })
                {
                    bool free = s0 >= 0 && s0 + 24 <= 128;
                    for (int k = s0; k < s0 + 24 && free; ++k)
                        free = ! keyIsUsed (slots, k);
                    if (free) { start = s0; break; }
                }
            }
            slot.setProperty (keyId, start, um);
        }
        else
        {
            slot.setProperty (keyId, -1000, nullptr);
            slot.setProperty (keyId, nextFreeKey (plugin, Mode::hit), um);
        }
    }

    juce::File writeSfz (const juce::ValueTree& pluginState)
    {
        const auto slots = read (pluginState);
        juce::String sfz;
        sfz << "// My Sounds, written by Gospel Production Studio\n";

        int regions = 0;
        for (const auto& s : slots)
        {
            if (! s.file.existsAsFile())
                continue;
            sfz << "<region> sample=" << s.file.getFullPathName().replaceCharacter ('\\', '/')
                << "\n volume=" << juce::String (s.volumeDb, 2);
            if (s.mode == Mode::hit)
            {
                sfz << " lokey=" << s.key << " hikey=" << s.key << " pitch_keycenter=" << s.key
                    << " loop_mode=one_shot amp_veltrack=50 ampeg_release=0.05\n";
            }
            else
            {
                const auto hi = juce::jmin (127, s.key + s.range - 1);
                const auto centre = s.root >= 0 ? s.root : s.key;
                sfz << " lokey=" << s.key << " hikey=" << hi << " pitch_keycenter=" << centre
                    << " amp_veltrack=70 ampeg_attack=0.003 ampeg_release=0.3\n";
            }
            ++regions;
        }

        if (regions == 0)
            return {};

        auto folder = SoundPacks::getRootFolder().getParentDirectory().getChildFile ("My Sounds cache");
        folder.createDirectory();
        auto file = folder.getChildFile (juce::String::toHexString (sfz.hashCode64()) + ".sfz");
        if (! file.existsAsFile())
            file.replaceWithText (sfz);
        return file;
    }

    //==========================================================================
    bool isAudioFile (const juce::File& f)
    {
        return f.existsAsFile() && formats().findFormatForFileExtension (f.getFileExtension()) != nullptr;
    }

    juce::String wildcard()
    {
        return formats().getWildcardForAllFormats();
    }

    double lengthSeconds (const juce::File& f)
    {
        std::unique_ptr<juce::AudioFormatReader> r (formats().createReaderFor (f));
        return r != nullptr && r->sampleRate > 0 ? (double) r->lengthInSamples / r->sampleRate : 0.0;
    }

    juce::String keyName (int note)
    {
        return juce::MidiMessage::getMidiNoteName (note, true, true, 4);
    }

    int detectRootNote (const juce::File& f)
    {
        std::unique_ptr<juce::AudioFormatReader> r (formats().createReaderFor (f));
        if (r == nullptr || r->sampleRate < 8000.0 || r->lengthInSamples < 4096)
            return -1;

        const auto sr = r->sampleRate;
        const auto total = (int) juce::jmin ((juce::int64) (sr * 2.5), r->lengthInSamples);
        juce::AudioBuffer<float> buf ((int) juce::jmax (1u, r->numChannels), total);
        r->read (&buf, 0, total, 0, true, true);
        std::vector<float> x ((size_t) total, 0.0f);
        for (int c = 0; c < buf.getNumChannels(); ++c)
            for (int i = 0; i < total; ++i)
                x[(size_t) i] += buf.getSample (c, i) / (float) buf.getNumChannels();

        // Start a little after the attack
        float peak = 0.0f;
        for (auto s : x) peak = juce::jmax (peak, std::abs (s));
        if (peak < 1.0e-3f)
            return -1;
        int start = 0;
        while (start < total && std::abs (x[(size_t) start]) < 0.1f * peak) ++start;
        start += (int) (0.04 * sr);

        const int W = sr > 60000.0 ? 4096 : 2048;
        const int half = W / 2;
        const int hop = (int) (0.08 * sr);
        std::vector<double> notes;
        std::vector<float> d ((size_t) half);

        for (int k = 0; k < 7; ++k)
        {
            const int s0 = start + k * hop;
            if (s0 + W >= total)
                break;
            const float* w = x.data() + s0;

            // Skip quiet windows
            double energy = 0.0;
            for (int j = 0; j < W; ++j) energy += w[j] * w[j];
            if (energy / W < 1.0e-6)
                continue;

            // YIN difference function with cumulative mean normalisation
            d[0] = 1.0f;
            double running = 0.0;
            for (int tau = 1; tau < half; ++tau)
            {
                double sum = 0.0;
                for (int j = 0; j < half; ++j)
                {
                    const auto diff = w[j] - w[j + tau];
                    sum += diff * diff;
                }
                running += sum;
                d[(size_t) tau] = running > 0.0 ? (float) (sum * tau / running) : 1.0f;
            }

            const int minTau = (int) (sr / 1500.0);
            int best = -1;
            for (int tau = juce::jmax (2, minTau); tau < half - 1; ++tau)
                if (d[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 < half && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            if (best <= 0 || best >= half - 1)
                continue;

            // Parabolic interpolation
            const auto a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
            const auto denom = a - 2.0f * b + c;
            const double t = best + (std::abs (denom) > 1.0e-9f ? 0.5 * (a - c) / denom : 0.0);
            const auto freq = sr / t;
            if (freq < 40.0 || freq > 1500.0)
                continue;
            notes.push_back (69.0 + 12.0 * std::log2 (freq / 440.0));
        }

        if (notes.size() < 3)
            return -1;
        std::sort (notes.begin(), notes.end());
        const auto median = notes[notes.size() / 2];
        int agree = 0;
        for (auto n : notes)
            if (std::abs (n - median) < 0.6) ++agree;
        if (agree * 3 < (int) notes.size() * 2)
            return -1;      // the pitch moves around: not one clear note
        return juce::jlimit (0, 127, (int) std::lround (median));
    }
}
