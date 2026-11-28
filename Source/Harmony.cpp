#include "Harmony.h"
#include "WavFiles.h"
#include "ChordTrack.h"
#include "Mixing.h"
#include "Takes.h"
#include "Loops.h"
#include "AudioEdit.h"
#include "StudioLookAndFeel.h"
#include "BackgroundWork.h"
#include <tracktion_engine/3rd_party/signalsmith-stretch/signalsmith-stretch.h>

namespace Harmony
{
    namespace
    {
        const juce::Identifier ofId ("gpsHarmonyOf"), partId ("gpsHarmonyPart");

        juce::AudioFormatManager& formats()
        {
            static juce::AudioFormatManager fm;
            static bool done = false;
            if (! done) { fm.registerBasicFormats(); done = true; }
            return fm;
        }

        bool contains (const std::vector<int>& pcs, int note)
        {
            const auto pc = ((note % 12) + 12) % 12;
            return std::find (pcs.begin(), pcs.end(), pc) != pcs.end();
        }

        int chordToneBelow (int note, const std::vector<int>& pcs, int minGap)
        {
            for (int k = minGap; k <= 15; ++k)
                if (contains (pcs, note - k)) return note - k;
            return note - minGap;
        }

        int chordToneAbove (int note, const std::vector<int>& pcs, int minGap)
        {
            for (int k = minGap; k <= 15; ++k)
                if (contains (pcs, note + k)) return note + k;
            return note + minGap;
        }

        const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int minorScale[] = { 0, 2, 3, 5, 7, 8, 10 };

        // Moves a note 'steps' notes up or down the key's scale (a chromatic note keeps its colour)
        int diatonic (int note, int steps, const Theory::Key& key)
        {
            const int* scale = key.minor ? minorScale : majorScale;
            const int pc = (((note - key.tonic) % 12) + 12) % 12;
            int i = 0;
            for (int k = 0; k < 7; ++k)
                if (scale[k] <= pc) i = k;
            const int chromatic = pc - scale[i];
            const int tonicBelow = note - pc;
            int idx = i + steps;
            int octave = 0;
            while (idx < 0)  { idx += 7; --octave; }
            while (idx >= 7) { idx -= 7; ++octave; }
            return tonicBelow + 12 * octave + scale[idx] + chromatic;
        }

        // Without a Chord Track: the simple chord of the key that holds the melody note
        int guessBassPc (int melody, const Theory::Key& key)
        {
            const int* scale = key.minor ? minorScale : majorScale;
            const int pc = (((melody - key.tonic) % 12) + 12) % 12;
            for (int degree : { 0, 4, 3, 5, 1 })           // 1, 5, 4, 6, 2
            {
                for (int k = 0; k < 3; ++k)
                    if (scale[(degree + 2 * k) % 7] == pc)
                        return (key.tonic + scale[degree]) % 12;
            }
            return key.tonic % 12;
        }
    }

    //==========================================================================
    const std::vector<PartInfo>& parts()
    {
        static const std::vector<PartInfo> p {
            { "alto",    "Alto",           "The next chord note below the melody (close harmony)",             0xffe08fb0, 0.97f, -3.0f },
            { "tenor",   "Tenor",          "The chord note below the alto",                                    0xff8fb0e0, 0.92f, -3.0f },
            { "bass",    "Bass",           "The chord's bass note (follows slash chords), an octave or more below", 0xff7e8f60, 0.86f, -4.0f },
            { "soprano", "Soprano (above)", "The next chord note above the melody: use it when the lead sings low", 0xfff0c070, 1.03f, -4.0f },
            { "third_up",   "Third above", "Gospel thirds: a third above, moving with the melody in the key",  0xffd9b26f, 1.0f,  -4.0f },
            { "third_down", "Third below", "A third below, moving with the melody in the key",                0xffb07ee0, 0.97f, -3.0f },
            { "sixth_down", "Sixth below", "A sixth below (the same notes as the third above, an octave down)", 0xff5fb8b0, 0.93f, -3.0f },
            { "octave_down", "Octave below", "The melody an octave lower, like the men singing along",        0xff6f8fa0, 0.88f, -5.0f },
            { "double",  "Lead double",    "The melody itself, sung again: makes the lead a group",            0xffc0c0c0, 1.0f,  -6.0f },
        };
        return p;
    }

    int findPart (const juce::String& id)
    {
        const auto& p = parts();
        for (size_t i = 0; i < p.size(); ++i)
            if (id == p[i].id) return (int) i;
        return -1;
    }

    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Full choir: alto, tenor and bass",           { 0, 1, 2 },    2 },
            { "Big choir: alto, tenor, bass and the lead doubled", { 0, 1, 2, 8 }, 3 },
            { "Gospel trio: third above and third below",   { 4, 5 },       1 },
            { "Close harmony: alto and tenor",              { 0, 1 },       1 },
            { "Duet: third above",                          { 4 },          1 },
            { "Thirds and sixths",                          { 4, 6 },       1 },
            { "Lead sings low: soprano and alto above it",  { 3, 4 },       1 },
            { "Unison choir: the lead with octave below",   { 7, 8 },       3 },
        };
        return p;
    }

    //==========================================================================
    //  Finding the melody: the pitch of each short frame (YIN), then notes
    //==========================================================================
    Melody findMelody (const juce::AudioBuffer<float>& buffer, double sr)
    {
        Melody m;
        const int n = buffer.getNumSamples();
        if (n < 2048 || sr <= 0.0)
            return m;

        // Mono, at about 11-12 kHz (plenty for voices)
        const int d = juce::jmax (1, (int) std::round (sr / 11025.0));
        const double dsRate = sr / d;
        const int len = n / d;
        std::vector<float> y ((size_t) len, 0.0f);
        const float scale = 1.0f / (float) (d * buffer.getNumChannels());
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            const float* x = buffer.getReadPointer (c);
            for (int i = 0; i < len; ++i)
            {
                float s = 0.0f;
                for (int j = 0; j < d; ++j) s += x[i * d + j];
                y[(size_t) i] += s * scale;
            }
        }

        constexpr int window = 512, hop = 64;
        const int tauMin = juce::jmax (2, (int) (dsRate / 1100.0));
        const int tauMax = juce::jmin (window / 2 - 2, (int) (dsRate / 65.0) + 1);
        const int w = window - tauMax;
        const int frames = len > window ? (len - window) / hop + 1 : 0;
        m.frameSec = hop / dsRate;
        m.firstFrameSec = (window / 2) / dsRate;
        m.pitch.assign ((size_t) frames, 0.0f);

        std::vector<float> energy ((size_t) frames, 0.0f);
        float loudest = 0.0f;
        for (int f = 0; f < frames; ++f)
        {
            double e = 0.0;
            const float* a = y.data() + f * hop;
            for (int j = 0; j < window; ++j) e += a[j] * a[j];
            energy[(size_t) f] = (float) (e / window);
            loudest = juce::jmax (loudest, energy[(size_t) f]);
        }
        if (loudest <= 1.0e-10f)
            return m;
        const float gate = juce::jmax (1.0e-8f, loudest * 1.0e-4f);      // 40 dB below the loudest

        std::vector<float> diff ((size_t) tauMax + 2, 1.0f);
        for (int f = 0; f < frames; ++f)
        {
            if (energy[(size_t) f] < gate)
                continue;
            const float* a = y.data() + f * hop;
            double running = 0.0;
            int best = -1;
            diff[0] = 1.0f;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                double dd = 0.0;
                const float* b = a + tau;
                for (int j = 0; j < w; ++j)
                {
                    const auto v = a[j] - b[j];
                    dd += v * v;
                }
                running += dd;
                diff[(size_t) tau] = running > 0.0 ? (float) (dd * tau / running) : 1.0f;

                if (best < 0)
                {
                    if (tau > tauMin && diff[(size_t) tau] < 0.2f)
                        best = tau;
                }
                else if (diff[(size_t) tau] < diff[(size_t) best])
                    best = tau;
                else
                    break;                                  // the bottom of the first dip
            }
            if (best < 2 || best >= tauMax)
                continue;
            const auto y0 = diff[(size_t) best - 1], y1 = diff[(size_t) best], y2 = diff[(size_t) best + 1];
            const auto denom = y0 - 2.0f * y1 + y2;
            const auto offset = std::abs (denom) > 1.0e-9f ? 0.5f * (y0 - y2) / denom : 0.0f;
            const auto freq = dsRate / ((double) best + juce::jlimit (-0.5f, 0.5f, offset));
            m.pitch[(size_t) f] = (float) (69.0 + 12.0 * std::log2 (freq / 440.0));
        }

        // Smooth out single wrong frames (median of 5)
        {
            auto p = m.pitch;
            std::vector<float> v;
            for (int f = 0; f < frames; ++f)
            {
                if (p[(size_t) f] <= 0.0f) continue;
                v.clear();
                for (int j = juce::jmax (0, f - 2); j <= juce::jmin (frames - 1, f + 2); ++j)
                    if (p[(size_t) j] > 0.0f) v.push_back (p[(size_t) j]);
                std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
                m.pitch[(size_t) f] = v[v.size() / 2];
            }
        }

        // Notes: a steady pitch (vibrato allowed); a new note when the pitch moves for ~20 ms
        auto timeOf = [&] (int f) { return m.firstFrameSec + f * m.frameSec; };
        auto close = [&] (int from, int to)
        {
            std::vector<float> v;
            for (int f = from; f <= to; ++f)
                if (m.pitch[(size_t) f] > 0.0f) v.push_back (m.pitch[(size_t) f]);
            if (v.empty()) return;
            std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
            Note note;
            note.start = timeOf (from) - m.frameSec * 0.5;
            note.end = timeOf (to) + m.frameSec * 0.5;
            note.sung = v[v.size() / 2];
            note.note = juce::roundToInt (note.sung);
            m.notes.push_back (note);
        };

        int start = -1, lastVoiced = -1, candidate = -1;
        double sum = 0.0;
        int count = 0;
        for (int f = 0; f < frames; ++f)
        {
            const auto p = m.pitch[(size_t) f];
            if (p <= 0.0f)
            {
                if (start >= 0 && f - lastVoiced > 3)
                {
                    close (start, lastVoiced);
                    start = candidate = -1;
                }
                continue;
            }
            if (start < 0)
            {
                start = f; sum = p; count = 1; candidate = -1; lastVoiced = f;
                continue;
            }
            const auto centre = sum / count;
            if (std::abs (p - centre) > 0.65)
            {
                if (candidate < 0) candidate = f;
                if (f - candidate >= 3)
                {
                    close (start, candidate - 1);
                    start = candidate;
                    sum = 0.0; count = 0;
                    for (int j = candidate; j <= f; ++j)
                        if (m.pitch[(size_t) j] > 0.0f) { sum += m.pitch[(size_t) j]; ++count; }
                    candidate = -1;
                }
            }
            else
            {
                candidate = -1;
                sum += p; ++count;
            }
            lastVoiced = f;
        }
        if (start >= 0)
            close (start, lastVoiced);

        // Very short pieces (slides between notes) join the neighbour they touch with the nearest pitch
        for (bool changed = true; changed;)
        {
            changed = false;
            for (size_t i = 0; i < m.notes.size(); ++i)
            {
                auto& nt = m.notes[i];
                if (nt.end - nt.start >= 0.06)
                    continue;
                const bool prevTouch = i > 0 && nt.start - m.notes[i - 1].end < 0.03;
                const bool nextTouch = i + 1 < m.notes.size() && m.notes[i + 1].start - nt.end < 0.03;
                if (! prevTouch && ! nextTouch)
                {
                    if (nt.end - nt.start < 0.04) { m.notes.erase (m.notes.begin() + (long) i); changed = true; break; }
                    continue;
                }
                size_t into = prevTouch ? i - 1 : i + 1;
                if (prevTouch && nextTouch
                    && std::abs (m.notes[i + 1].sung - nt.sung) < std::abs (m.notes[i - 1].sung - nt.sung))
                    into = i + 1;
                m.notes[into].start = juce::jmin (m.notes[into].start, nt.start);
                m.notes[into].end = juce::jmax (m.notes[into].end, nt.end);
                m.notes.erase (m.notes.begin() + (long) i);
                changed = true;
                break;
            }
        }
        return m;
    }

    Melody findMelody (const juce::File& file)
    {
        static juce::CriticalSection lock;
        static std::map<juce::String, Melody> cache;
        const auto key = file.getFullPathName() + "|" + juce::String (file.getLastModificationTime().toMilliseconds())
                         + "|" + juce::String (file.getSize());
        {
            const juce::ScopedLock sl (lock);
            if (auto it = cache.find (key); it != cache.end())
                return it->second;
        }

        Melody m;
        std::unique_ptr<juce::AudioFormatReader> r (formats().createReaderFor (file));
        if (r != nullptr && r->lengthInSamples > 0 && r->lengthInSamples < (juce::int64) r->sampleRate * 60 * 30)
        {
            const int ch = (int) juce::jlimit (1u, 2u, r->numChannels);
            juce::AudioBuffer<float> b (ch, (int) r->lengthInSamples);
            r->read (&b, 0, b.getNumSamples(), 0, true, ch > 1);
            m = findMelody (b, r->sampleRate);
        }
        const juce::ScopedLock sl (lock);
        if (cache.size() > 20) cache.clear();
        cache[key] = m;
        return m;
    }

    //==========================================================================
    //  Choosing the harmony notes
    //==========================================================================
    int harmonyNote (int part, int m, const ChordSpan* chord, const Theory::Key& key, int previous)
    {
        const bool chordTone = chord != nullptr && contains (chord->upper, m);
        switch (part)
        {
            case 0:     // alto
                return chordTone ? chordToneBelow (m, chord->upper, 3) : diatonic (m, -2, key);

            case 1:     // tenor: below the alto
            {
                const auto alto = harmonyNote (0, m, chord, key, 0);
                return chordTone ? chordToneBelow (alto, chord->upper, 3) : diatonic (m, -4, key);
            }

            case 2:     // bass
            {
                const int pc = chord != nullptr ? chord->bass : guessBassPc (m, key);
                const auto tenor = harmonyNote (1, m, chord, key, 0);
                std::vector<int> options;
                for (int nn = m - 19; nn <= m - 8; ++nn)
                    if (((nn % 12) + 12) % 12 == pc) options.push_back (nn);
                std::vector<int> belowTenor;
                for (auto o : options) if (o < tenor) belowTenor.push_back (o);
                if (! belowTenor.empty()) options = belowTenor;
                int best = options.empty() ? m - 12 : options.back();          // the highest
                if (previous > 0)
                    for (auto o : options)
                        if (std::abs (o - previous) < std::abs (best - previous)) best = o;
                return best;
            }

            case 3:     // soprano: above
                return chordTone ? chordToneAbove (m, chord->upper, 3) : diatonic (m, 2, key);

            case 4:     // third above (in the key; a chord note when the key's third clashes)
            {
                const auto t = diatonic (m, 2, key);
                if (chord != nullptr && ! contains (chord->upper, t))
                {
                    const auto c = chordToneAbove (m, chord->upper, 3);
                    if (c - m <= 5) return c;
                }
                return t;
            }

            case 5:     // third below
            {
                const auto t = diatonic (m, -2, key);
                if (chord != nullptr && ! contains (chord->upper, t))
                {
                    const auto c = chordToneBelow (m, chord->upper, 3);
                    if (m - c <= 5) return c;
                }
                return t;
            }

            case 6:     // sixth below: the third above, an octave down
                return harmonyNote (4, m, chord, key, 0) - 12;

            case 7:     return m - 12;
            default:    return m;
        }
    }

    const ChordSpan* chordFor (const Context& ctx, const Note& n)
    {
        const ChordSpan* best = nullptr;
        double most = 0.0;
        for (auto& c : ctx.chords)
        {
            const auto overlap = juce::jmin (c.end, n.end) - juce::jmax (c.start, n.start);
            if (overlap > most) { most = overlap; best = &c; }
        }
        return best;
    }

    std::vector<int> planPart (int part, const Melody& melody, const Context& ctx)
    {
        return planPart (part, melody, ctx, Edits());
    }

    std::vector<int> planPart (int part, const Melody& melody, const Context& ctx, const Edits& edits)
    {
        std::vector<int> out;
        int previous = 0;
        for (auto& n : melody.notes)
        {
            auto t = harmonyNote (part, n.note, chordFor (ctx, n), ctx.key, previous);
            if (const auto moved = edits.find (part, n))
                t = *moved;                       // a note moved by hand; the next notes follow on from it
            out.push_back (t);
            previous = t;
        }
        return out;
    }

    //==========================================================================
    //  Notes moved by hand (Step 33)
    //==========================================================================
    static int keyOf (const Note& n)    { return juce::roundToInt (n.start * 1000.0); }

    bool Edits::empty() const           { return count() == 0; }

    int Edits::count() const
    {
        int c = 0;
        for (auto& [p, notes] : moved) c += (int) notes.size();
        return c;
    }

    std::optional<int> Edits::find (int part, const Note& n) const
    {
        const auto p = moved.find (part);
        if (p == moved.end()) return std::nullopt;
        // Allow a few ms either way: the melody is found again each time the panel opens
        const auto k = keyOf (n);
        for (auto it = p->second.lower_bound (k - 5); it != p->second.end() && it->first <= k + 5; ++it)
            return it->second;
        return std::nullopt;
    }

    void Edits::set (int part, const Note& n, int midiNote)
    {
        clear (part, n);
        moved[part][keyOf (n)] = juce::jlimit (24, 96, midiNote);
    }

    void Edits::clear (int part, const Note& n)
    {
        const auto p = moved.find (part);
        if (p == moved.end()) return;
        const auto k = keyOf (n);
        for (auto it = p->second.lower_bound (k - 5); it != p->second.end() && it->first <= k + 5;)
            it = p->second.erase (it);
        if (p->second.empty()) moved.erase (p);
    }

    juce::String Edits::toString() const
    {
        juce::StringArray items;
        for (auto& [part, notes] : moved)
            if (part >= 0 && part < (int) parts().size())
                for (auto& [ms, note] : notes)
                    items.add (juce::String (parts()[(size_t) part].id) + "@" + juce::String (ms) + "=" + juce::String (note));
        return items.joinIntoString (" ");
    }

    Edits Edits::fromString (const juce::String& text)
    {
        Edits e;
        for (auto& item : juce::StringArray::fromTokens (text, " ", ""))
        {
            const auto part = findPart (item.upToFirstOccurrenceOf ("@", false, false));
            const auto ms = item.fromFirstOccurrenceOf ("@", false, false).upToFirstOccurrenceOf ("=", false, false);
            const auto note = item.fromFirstOccurrenceOf ("=", false, false);
            if (part >= 0 && ms.isNotEmpty() && note.containsOnly ("0123456789") && note.isNotEmpty())
                e.moved[part][ms.getIntValue()] = juce::jlimit (24, 96, note.getIntValue());
        }
        return e;
    }

    Edits loadEdits (const te::WaveAudioClip& clip)
    {
        return Edits::fromString (clip.state.getProperty ("gpsHarmonyEdits").toString());
    }

    void saveEdits (te::WaveAudioClip& clip, const Edits& e)
    {
        if (e.empty()) clip.state.removeProperty ("gpsHarmonyEdits", nullptr);
        else           clip.state.setProperty ("gpsHarmonyEdits", e.toString(), nullptr);
    }

    int chordToneStep (int midiNote, const ChordSpan* chord, const Theory::Key& key, int direction)
    {
        direction = direction >= 0 ? 1 : -1;
        auto fits = [&] (int pc)
        {
            if (chord != nullptr)
                return pc == chord->bass || std::find (chord->upper.begin(), chord->upper.end(), pc) != chord->upper.end();
            return Theory::isInKey (pc, key);
        };
        for (int n = midiNote + direction; n >= 24 && n <= 96; n += direction)
            if (fits (((n % 12) + 12) % 12))
                return n;
        return midiNote;
    }

    //==========================================================================
    //  Reading the clip and the song
    //==========================================================================
    ClipInfo prepare (te::WaveAudioClip& clip, const Theory::Key& key)
    {
        ClipInfo info;
        info.context.key = key;
        if (std::abs (clip.getSpeedRatio() - 1.0) > 0.001 || clip.getAutoTempo() || Loops::isFollowing (clip))
        {
            info.problem = "Harmonies are made from a recording played at its own speed. This clip has its speed changed or follows the tempo as a loop.";
            return info;
        }
        if (Takes::count (clip) > 1)
        {
            info.problem = "This clip has several takes. First choose the best one: right-click > \"Keep this take, delete the others...\".";
            return info;
        }
        info.source = clip.getOriginalFile();
        if (! info.source.existsAsFile())
        {
            info.problem = "The recording of this clip could not be found.";
            return info;
        }

        auto& ts = clip.edit.tempoSequence;
        const auto pos = clip.getPosition();
        const auto clipStart = pos.getStart().inSeconds(), offset = pos.getOffset().inSeconds();
        auto toSource = [&] (double beat)
        {
            return offset + (ts.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, beat))).inSeconds() - clipStart);
        };
        for (auto& e : ChordTrack::getEvents (clip.edit))
        {
            ChordSpan span;
            span.start = toSource (e.start);
            span.end = toSource (e.start + e.length);
            auto upper = e.chord;
            upper.bass = -1;
            span.upper = Theory::chordPitchClasses (upper, key);
            const auto rel = e.chord.bass >= 0 ? e.chord.bass : e.chord.root;
            span.bass = ((key.tonic + rel) % 12 + 12) % 12;
            info.context.chords.push_back (span);
        }
        info.ok = true;
        return info;
    }

    //==========================================================================
    //  Singing the new part: the recording moved note by note (Signalsmith),
    //  keeping the singer's vowels; each singer of a stack a little different
    //==========================================================================
    bool renderPart (const juce::File& source, const Melody& melody, const Context& ctx, int part,
                     int singers, float looseness, const juce::File& dest, const std::function<bool (float)>& progress,
                     const Edits& edits)
    {
        if (melody.notes.empty() || part < 0 || part >= (int) parts().size())
            return false;
        std::unique_ptr<juce::AudioFormatReader> r (formats().createReaderFor (source));
        if (r == nullptr)
            return false;
        const auto sr = r->sampleRate;
        const int n = (int) r->lengthInSamples;
        const int inCh = (int) juce::jlimit (1u, 2u, r->numChannels);
        juce::AudioBuffer<float> in (inCh, n);
        r->read (&in, 0, n, 0, true, inCh > 1);
        std::vector<float> mono ((size_t) n, 0.0f);
        for (int c = 0; c < inCh; ++c)
            juce::FloatVectorOperations::addWithMultiply (mono.data(), in.getReadPointer (c), 1.0f / (float) inCh, n);

        singers = juce::jlimit (1, 4, singers);
        looseness = juce::jlimit (0.0f, 1.0f, looseness);
        const auto& info = parts()[(size_t) part];
        const auto targets = planPart (part, melody, ctx, edits);

        // The move for each note (in tune: from the middle of what was sung to the new note),
        // and the moment it changes: in the gap between notes, just before the next word
        const auto& notes = melody.notes;
        std::vector<float> shift;
        std::vector<double> boundary;      // boundary[i] = where note i's move ends and note i+1's begins
        for (size_t i = 0; i < notes.size(); ++i)
        {
            shift.push_back ((float) targets[i] - notes[i].sung);
            if (i + 1 < notes.size())
            {
                const auto gapMid = 0.5 * (notes[i].end + notes[i + 1].start);
                boundary.push_back (juce::jmax (gapMid, notes[i + 1].start - 0.15));
            }
        }
        auto shiftAt = [&] (double t)
        {
            const auto it = std::upper_bound (boundary.begin(), boundary.end(), t);
            const auto i = (size_t) (it - boundary.begin());
            float s = shift[i];
            constexpr double ramp = 0.012;          // a quick glide, like a singer moving note
            if (i < boundary.size() && boundary[i] - t < ramp)
                s += (shift[i + 1] - s) * (float) (0.5 * (1.0 - (boundary[i] - t) / ramp));
            if (i > 0 && t - boundary[i - 1] < ramp)
                s += (shift[i - 1] - s) * (float) (0.5 * (1.0 - (t - boundary[i - 1]) / ramp));
            return s;
        };
        // Breaths and "s" sounds between notes are a little softer in the harmonies
        // (many voices on the same consonant sound messy)
        auto voicedGain = [&] (double t)
        {
            constexpr double ramp = 0.02;
            double nearest = 1.0e9;
            // the notes around 't' (notes are sorted)
            auto first = std::lower_bound (notes.begin(), notes.end(), t, [] (const Note& a, double v) { return a.end < v; });
            for (auto it = first == notes.begin() ? first : first - 1; it != notes.end() && it->start - t < 1.0; ++it)
            {
                if (t >= it->start && t <= it->end) return 1.0f;
                nearest = juce::jmin (nearest, t < it->start ? it->start - t : t - it->end);
            }
            const auto f = (float) juce::jlimit (0.0, 1.0, nearest / ramp);
            return 1.0f - 0.5f * f;        // -6 dB away from the notes
        };

        const bool stereo = singers > 1;
        juce::AudioBuffer<float> out (stereo ? 2 : 1, n);
        out.clear();

        signalsmith::stretch::SignalsmithStretch<float> stretch (1234L + part);
        stretch.configure (1, (int) std::lround (sr * 0.06), (int) std::lround (sr * 0.015));
        const int inLat = stretch.inputLatency();
        const int lat = inLat + stretch.outputLatency();
        const int total = n + lat;
        std::vector<float> padded ((size_t) total, 0.0f), voice ((size_t) total, 0.0f);
        std::copy (mono.begin(), mono.end(), padded.begin());

        for (int s = 0; s < singers; ++s)
        {
            juce::Random rng (part * 7919 + s * 104729 + 17);
            auto r1 = [&] { return rng.nextFloat() * 2.0f - 1.0f; };     // -1..1
            const double delaySec = s == 0 ? r1() * looseness * 0.008 : r1() * looseness * 0.028;
            const float detune = s == 0 ? r1() * looseness * 3.0f : (s % 2 == 1 ? 1.0f : -1.0f) * (4.0f + 6.0f * rng.nextFloat()) * (0.3f + looseness);
            const float driftCents = (1.0f + 4.0f * looseness) * (s == 0 ? 0.5f : 1.0f);
            const double driftHz = 0.12 + 0.2 * rng.nextFloat(), driftPhase = rng.nextFloat() * 6.2832;
            const float formant = info.formant * (s == 0 ? 1.0f : 1.0f + r1() * 0.035f);
            const float gain = s == 0 ? 1.0f : juce::Decibels::decibelsToGain (-1.0f - 2.0f * rng.nextFloat());
            const float pan = s == 0 ? 0.0f : (s % 2 == 1 ? -1.0f : 1.0f) * (0.3f + 0.35f * rng.nextFloat()) * (0.6f + 0.4f * looseness);

            stretch.reset();
            stretch.setFormantFactor (formant, true);
            std::fill (voice.begin(), voice.end(), 0.0f);
            constexpr int chunk = 128;
            for (int pos = 0; pos < total; pos += chunk)
            {
                const int len = juce::jmin (chunk, total - pos);
                const double t = (pos + len / 2 - inLat) / sr;
                const auto semis = shiftAt (t) + (detune + driftCents * (float) std::sin (6.2832 * driftHz * t + driftPhase)) / 100.0f;
                stretch.setTransposeSemitones (semis);
                // The singer's own pitch, so the vowel colour is found at once (not after each pause)
                const auto ni = (size_t) (std::upper_bound (boundary.begin(), boundary.end(), t) - boundary.begin());
                stretch.setFormantBase ((float) (440.0 * std::pow (2.0, (notes[ni].sung - 69.0) / 12.0) / sr));
                float* ip[1] = { padded.data() + pos };
                float* op[1] = { voice.data() + pos };
                stretch.process (ip, len, op, len);

                if (progress && (pos / chunk) % 512 == 0
                    && ! progress (((float) s + (float) pos / (float) total) / (float) singers))
                    return false;
            }

            const int delay = (int) std::lround (delaySec * sr);
            const float norm = gain / std::sqrt ((float) singers);
            const float gl = stereo ? std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * juce::MathConstants<float>::sqrt2 : 1.0f;
            const float gr = stereo ? std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * juce::MathConstants<float>::sqrt2 : 1.0f;
            float* L = out.getWritePointer (0);
            float* R = stereo ? out.getWritePointer (1) : nullptr;
            double gt = -1.0;
            float g = 1.0f;
            for (int i = 0; i < n; ++i)
            {
                const int o = i + delay;
                if (o < 0 || o >= n) continue;
                const double t = i / sr;
                if (t - gt > 0.002) { g = voicedGain (t); gt = t; }       // every 2 ms is enough
                const auto v = voice[(size_t) (i + lat)] * norm * g;
                L[o] += v * gl;
                if (R != nullptr) R[o] += v * gr;
            }
        }

        if (progress) progress (1.0f);
        dest.getParentDirectory().createDirectory();
        auto tmp = dest.getSiblingFile (dest.getFileNameWithoutExtension() + ".part");
        tmp.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (tmp, sr, (int) (out.getNumChannels()), 24).release());
            if (w == nullptr || ! w->writeFromAudioSampleBuffer (out, 0, n))
                return false;
        }
        dest.deleteFile();
        return tmp.moveFileTo (dest);
    }

    //==========================================================================
    //  The tracks
    //==========================================================================
    juce::File folderFor (const juce::File& source)
    {
        // Next to the song's recordings (not inside "Timed" or "Following")
        auto dir = source.getParentDirectory();
        if (dir.getFileName() == "Timed" || dir.getFileName() == "Following" || dir.getFileName() == "Harmony")
            dir = dir.getParentDirectory();
        return dir.getChildFile ("Harmony");
    }

    int tidyUp (te::Edit& edit)
    {
        juce::Array<juce::File> used, folders;
        for (auto* t : te::getAudioTracks (edit))
            for (auto* c : t->getClips())
                if (auto* w = dynamic_cast<te::WaveAudioClip*> (c))
                {
                    used.add (w->getOriginalFile());
                    if (w->getOriginalFile().getParentDirectory().getFileName() == "Harmony")
                        folders.addIfNotAlreadyThere (w->getOriginalFile().getParentDirectory());
                }
        // The song's own Audio/Harmony folder, even when no harmony is used any more
        const auto songAudio = edit.editFileRetriever ? edit.editFileRetriever().getParentDirectory().getChildFile ("Audio") : juce::File();
        if (songAudio.isDirectory())
            folders.addIfNotAlreadyThere (songAudio.getChildFile ("Harmony"));

        int removed = 0;
        for (auto& folder : folders)
            if (folder.isDirectory() && folder.getFileName() == "Harmony")
                for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav"))
                    if (! used.contains (f) && f.deleteFile())
                        ++removed;
        return removed;
    }

    bool isHarmonyTrack (const te::AudioTrack& t)      { return t.state.hasProperty (ofId); }

    juce::Array<te::AudioTrack*> findHarmonyTracks (te::WaveAudioClip& clip)
    {
        juce::Array<te::AudioTrack*> result;
        const auto id = clip.itemID.toString();
        for (auto* t : te::getAudioTracks (clip.edit))
            if (t->state[ofId].toString() == id)
                result.add (t);
        return result;
    }

    void removeHarmonies (te::WaveAudioClip& clip)
    {
        for (auto* t : findHarmonyTracks (clip))
            clip.edit.deleteTrack (t);
    }

    juce::Array<te::AudioTrack*> placeParts (te::WaveAudioClip& clip, const std::vector<std::pair<int, juce::File>>& rendered, bool toChoirBus)
    {
        juce::Array<te::AudioTrack*> made;
        auto& edit = clip.edit;
        auto* um = &edit.getUndoManager();
        auto* lead = dynamic_cast<te::AudioTrack*> (clip.getTrack());
        if (lead == nullptr)
            return made;

        const auto existing = findHarmonyTracks (clip);
        te::Track* after = existing.isEmpty() ? (te::Track*) lead : (te::Track*) existing.getLast();

        te::AudioTrack* bus = nullptr;
        if (toChoirBus)
        {
            bus = Mixing::getBusOf (*lead);
            if (bus == nullptr)
                for (auto* b : Mixing::getGroupBuses (edit))
                    if (bus == nullptr && b->getName().startsWithIgnoreCase (Mixing::busKinds()[0].name))
                        bus = b;
            if (bus == nullptr)
                bus = Mixing::createGroupBus (edit, 0);
        }

        const auto pos = clip.getPosition();
        for (auto& [part, file] : rendered)
        {
            const auto& info = parts()[(size_t) part];
            te::AudioTrack* track = nullptr;
            for (auto* t : existing)
                if (t->state[partId].toString() == info.id)
                    track = t;

            if (track == nullptr)
            {
                auto t = edit.insertNewAudioTrack (te::TrackInsertPoint (*after, false), nullptr, true);
                if (t == nullptr) continue;
                track = t.get();
                track->state.setProperty (ofId, clip.itemID.toString(), um);
                track->state.setProperty (partId, info.id, um);
                track->setName (info.name);
                track->setColour (juce::Colour (info.colour));
                if (auto* v = track->getVolumePlugin())
                    v->setVolumeDb (info.levelDb);
                after = track;
            }
            else
            {
                const auto old = track->getClips();
                for (auto* c : old)
                    c->removeFromParent();
            }

            if (auto c = track->insertWaveClip (info.name, file, { { pos.getStart(), pos.getEnd() }, pos.getOffset() }, false))
                AudioEdit::prepare (*c);

            if (bus != nullptr && Mixing::getBusOf (*track) != bus)
                Mixing::routeToBus (*track, bus);
            made.add (track);
        }
        return made;
    }

    juce::String generate (te::WaveAudioClip& clip, const Theory::Key& key, const Settings& s)
    {
        auto info = prepare (clip, key);
        if (! info.ok)
            return info.problem;
        const auto melody = findMelody (info.source);
        if (melody.notes.empty())
            return "No sung notes were found in this clip.";

        std::vector<std::pair<int, juce::File>> rendered;
        auto folder = folderFor (info.source);
        for (auto part : s.parts)
        {
            const auto dest = folder.getNonexistentChildFile (info.source.getFileNameWithoutExtension() + " - " + parts()[(size_t) part].name, ".wav", false);
            if (! renderPart (info.source, melody, info.context, part, s.singers, s.looseness, dest, {}, s.edits))
                return "A part could not be written.";
            rendered.push_back ({ part, dest });
        }
        placeParts (clip, rendered, s.toChoirBus);
        return {};
    }

    //==========================================================================
    //  The panel
    //==========================================================================
    namespace
    {
        struct JobState
        {
            std::atomic<float> progress { 0.0f };
            std::atomic<bool> cancel { false }, done { false };
        };

        class Panel final : public juce::Component, private juce::Timer
        {
        public:
            Panel (te::WaveAudioClip& c, std::function<Theory::Key()> gk, std::function<void()> changed)
                : clip (&c), keep (&c), editRef (c.edit), clipID (c.itemID), getKey (std::move (gk)), onChanged (std::move (changed))
            {
                using namespace StudioColours;
                title.setText ("Harmony and choir", juce::dontSendNotification);
                title.setFont (studioFont (17.0f, true));
                title.setColour (juce::Label::textColourId, text);
                addAndMakeVisible (title);

                for (auto* l : { &info, &summary })
                {
                    l->setColour (juce::Label::textColourId, textDim);
                    l->setFont (studioFont (12.5f));
                    l->setJustificationType (juce::Justification::topLeft);
                    addAndMakeVisible (*l);
                }

                presetBox.setTextWhenNothingSelected ("Choose a ready-made choir...");
                for (size_t i = 0; i < presets().size(); ++i)
                    presetBox.addItem (presets()[i].name, (int) i + 1);
                presetBox.onChange = [this]
                {
                    const auto i = presetBox.getSelectedId() - 1;
                    if (i < 0) return;
                    const auto& p = presets()[(size_t) i];
                    for (size_t k = 0; k < toggles.size(); ++k)
                        toggles[k]->setToggleState (std::find (p.parts.begin(), p.parts.end(), (int) k) != p.parts.end(), juce::dontSendNotification);
                    singers.setValue (p.singers, juce::dontSendNotification);
                    changedSettings();
                };
                addAndMakeVisible (presetBox);

                const auto saved = juce::StringArray::fromTokens (c.state.getProperty ("gpsHarmonyParts", "alto tenor bass").toString(), " ", "");
                for (size_t k = 0; k < parts().size(); ++k)
                {
                    auto t = std::make_unique<juce::ToggleButton> (parts()[k].name);
                    t->setTooltip (parts()[k].hint);
                    t->setToggleState (saved.contains (parts()[k].id), juce::dontSendNotification);
                    t->setColour (juce::ToggleButton::tickColourId, juce::Colour (parts()[k].colour));
                    t->onClick = [this] { presetBox.setSelectedId (0, juce::dontSendNotification); changedSettings(); };
                    addAndMakeVisible (*t);
                    toggles.push_back (std::move (t));
                }

                auto setup = [this] (juce::Slider& sl, double lo, double hi, double v, const juce::String& tip)
                {
                    sl.setSliderStyle (juce::Slider::LinearHorizontal);
                    sl.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 22);
                    sl.setRange (lo, hi, 1.0);
                    sl.setValue (v, juce::dontSendNotification);
                    sl.setTooltip (tip);
                    sl.onValueChange = [this] { changedSettings(); };
                    addAndMakeVisible (sl);
                };
                setup (singers, 1, 4, (int) c.state.getProperty ("gpsHarmonySingers", 2),
                       "How many singers in each part. More singers make a fuller choir (and take longer to make).");
                setup (looseness, 0, 100, (int) c.state.getProperty ("gpsHarmonyLoose", 50),
                       "0 = tight, like one studio singer. 100 = loose, like a big live church choir (more timing and tuning differences).");
                looseness.setTextValueSuffix (" %");

                busToggle.setToggleState ((bool) c.state.getProperty ("gpsHarmonyBus", true), juce::dontSendNotification);
                busToggle.setTooltip ("All the harmony tracks play through one Choir bus: one fader for the whole choir");
                addAndMakeVisible (busToggle);

                makeButton.onClick = [this] { make(); };
                addAndMakeVisible (makeButton);
                removeButton.onClick = [this]
                {
                    if (clip == nullptr || editRef.get() == nullptr) return;
                    clip->edit.getUndoManager().beginNewTransaction ("Remove harmonies");
                    removeHarmonies (*clip);
                    summary.setText ("The harmony tracks were removed (Ctrl+Z brings them back).", juce::dontSendNotification);
                    updateButtons();
                    if (onChanged) onChanged();
                };
                addAndMakeVisible (removeButton);

                // Moving notes by hand (Step 33)
                edits = loadEdits (c);
                selLabel.setColour (juce::Label::textColourId, text);
                selLabel.setFont (studioFont (12.5f));
                addAndMakeVisible (selLabel);
                upButton.onClick        = [this] { moveSelected (1, false); };
                downButton.onClick      = [this] { moveSelected (-1, false); };
                chordUpButton.onClick   = [this] { moveSelected (1, true); };
                chordDownButton.onClick = [this] { moveSelected (-1, true); };
                autoButton.onClick      = [this] { if (hasSelection()) { edits.clear (selPart, melody.notes[(size_t) selNote]); editsChanged(); } };
                resetEditsButton.onClick = [this] { edits = {}; editsChanged(); };
                upButton.setTooltip ("Move the chosen note up a semitone (or press the Up arrow)");
                downButton.setTooltip ("Move the chosen note down a semitone (or press the Down arrow)");
                chordUpButton.setTooltip ("Move the chosen note up to the next note of the chord (Shift + Up)");
                chordDownButton.setTooltip ("Move the chosen note down to the next note of the chord (Shift + Down)");
                autoButton.setTooltip ("Put the chosen note back to the note the app chose");
                for (auto* b : { &upButton, &downButton, &chordUpButton, &chordDownButton, &autoButton, &resetEditsButton })
                    addAndMakeVisible (*b);
                setWantsKeyboardFocus (true);

                setSize (640, 640);
                summary.setText ("Listening to the melody...", juce::dontSendNotification);
                makeButton.setEnabled (false);

                clipInfo = prepare (c, getKey ? getKey() : Theory::Key());
                if (! clipInfo.ok)
                    summary.setText (clipInfo.problem, juce::dontSendNotification);
                else
                {
                    juce::Component::SafePointer<Panel> safe (this);
                    const auto src = clipInfo.source;
                    BackgroundWork::run ("Listening to the melody", [safe, src] (const std::atomic<bool>&)
                    {
                        auto m = std::make_shared<Melody> (findMelody (src));
                        BackgroundWork::onMessageThread ([safe, m]
                        {
                            if (auto* p = safe.getComponent())
                                p->melodyReady (*m);
                        });
                    }, false);
                }
                updateInfo();
                updateButtons();
            }

            ~Panel() override
            {
                stopTimer();
                if (job != nullptr && ! job->done)
                    job->cancel = true;
            }

            void melodyReady (const Melody& m)
            {
                melody = m;
                haveMelody = true;
                updateInfo();
                summary.setText (melody.notes.empty() ? "No sung notes were found in this clip."
                                                      : "Choose the parts, then press \"Make the harmonies\".", juce::dontSendNotification);
                updateButtons();
                repaint();
            }

            std::vector<int> chosenParts() const
            {
                std::vector<int> p;
                for (size_t k = 0; k < toggles.size(); ++k)
                    if (toggles[k]->getToggleState()) p.push_back ((int) k);
                return p;
            }

            void changedSettings()
            {
                if (clip != nullptr && editRef.get() != nullptr)
                {
                    juce::StringArray ids;
                    for (auto p : chosenParts()) ids.add (parts()[(size_t) p].id);
                    clip->state.setProperty ("gpsHarmonyParts", ids.joinIntoString (" "), nullptr);
                    clip->state.setProperty ("gpsHarmonySingers", (int) singers.getValue(), nullptr);
                    clip->state.setProperty ("gpsHarmonyLoose", (int) looseness.getValue(), nullptr);
                    clip->state.setProperty ("gpsHarmonyBus", busToggle.getToggleState(), nullptr);
                }
                updateButtons();
                repaint();
            }

            void updateInfo()
            {
                juce::String t;
                const auto key = clipInfo.context.key;
                t << "Key: " << Theory::noteName (key.tonic, key.flats) << (key.minor ? " minor" : " major") << ".  ";
                if (! clipInfo.context.chords.empty())
                    t << "The harmonies follow the chords of your Chord Track (" << (int) clipInfo.context.chords.size() << " chords).";
                else
                    t << "The Chord Track is empty, so the harmonies follow the key. Add chords for better harmonies.";
                if (haveMelody && ! melody.notes.empty())
                {
                    int lo = 127, hi = 0;
                    for (auto& n : melody.notes) { lo = juce::jmin (lo, n.note); hi = juce::jmax (hi, n.note); }
                    t << "\nMelody: " << (int) melody.notes.size() << " notes, from " << Theory::midiNoteName (lo, key)
                      << " to " << Theory::midiNoteName (hi, key) << ".";
                }
                info.setText (t, juce::dontSendNotification);
            }

            void updateButtons()
            {
                const bool busy = job != nullptr && ! job->done;
                makeButton.setEnabled (! busy && clipInfo.ok && haveMelody && ! melody.notes.empty() && ! chosenParts().empty());
                makeButton.setButtonText (busy ? "Cancel" : "Make the harmonies");
                if (busy) makeButton.setEnabled (true);
                removeButton.setEnabled (! busy && clip != nullptr && editRef.get() != nullptr && ! findHarmonyTracks (*clip).isEmpty());
                chosenPartsCache = chosenParts();
                for (auto* b : { &upButton, &downButton, &chordUpButton, &chordDownButton })
                    b->setEnabled (! busy && hasSelection());
                autoButton.setEnabled (! busy && hasSelection() && edits.find (selPart, melody.notes[(size_t) selNote]).has_value());
                resetEditsButton.setEnabled (! busy && ! edits.empty());
                resetEditsButton.setButtonText (edits.empty() ? "Reset all edits" : "Reset all edits (" + juce::String (edits.count()) + ")");
                updateSelectionText();
            }

            void make()
            {
                if (job != nullptr && ! job->done)
                {
                    job->cancel = true;
                    return;
                }
                if (clip == nullptr || editRef.get() == nullptr)
                    return;

                clipInfo = prepare (*clip, getKey ? getKey() : Theory::Key());      // the chords may have changed
                updateInfo();
                if (! clipInfo.ok) { summary.setText (clipInfo.problem, juce::dontSendNotification); return; }

                job = std::make_shared<JobState>();
                const auto state = job;
                const auto chosen = chosenParts();
                const int nSingers = (int) singers.getValue();
                const float loose = (float) looseness.getValue() / 100.0f;
                const bool toBus = busToggle.getToggleState();
                const auto src = clipInfo.source;
                const auto ctx = clipInfo.context;
                const auto mel = melody;
                const auto moved = edits;
                const auto safeEdit = editRef;
                const auto id = clipID;
                const auto changed = onChanged;
                juce::Component::SafePointer<Panel> safe (this);

                BackgroundWork::run ("Making the harmonies", [=] (const std::atomic<bool>& stopRequested)
                {
                    std::vector<std::pair<int, juce::File>> rendered;
                    auto folder = folderFor (src);
                    bool ok = true;
                    for (size_t k = 0; k < chosen.size() && ok; ++k)
                    {
                        const auto part = chosen[k];
                        const auto dest = folder.getNonexistentChildFile (src.getFileNameWithoutExtension() + " - " + parts()[(size_t) part].name, ".wav", false);
                        ok = renderPart (src, mel, ctx, part, nSingers, loose, dest, [&] (float f)
                        {
                            state->progress = ((float) k + f) / (float) chosen.size();
                            if (stopRequested.load()) state->cancel = true;
                            return ! state->cancel.load();
                        }, moved);
                        if (ok) rendered.push_back ({ part, dest });
                        else dest.deleteFile();
                    }
                    const bool cancelled = state->cancel.load();
                    BackgroundWork::onMessageThread ([=]
                    {
                        state->done = true;
                        juce::String message;
                        if (cancelled)
                        {
                            for (auto& r : rendered) r.second.deleteFile();
                            message = "Stopped. Nothing was changed.";
                        }
                        else if (! ok)
                            message = "A part could not be written (is the disk full?).";
                        else if (auto* ed = safeEdit.get())
                        {
                            if (auto* c = dynamic_cast<te::WaveAudioClip*> (te::findClipForID (*ed, id)))
                            {
                                ed->getUndoManager().beginNewTransaction ("Harmonies");
                                const auto tracks = placeParts (*c, rendered, toBus);
                                message = "Done: " + juce::String (tracks.size()) + (tracks.size() == 1 ? " harmony track" : " harmony tracks")
                                        + " under the lead. Press Play. Change the chords and press the button again to follow them.";
                                if (changed) changed();
                            }
                            else
                                message = "The clip was deleted.";
                        }
                        if (auto* p = safe.getComponent())
                        {
                            p->summary.setText (message, juce::dontSendNotification);
                            p->stopTimer();
                            p->updateButtons();
                        }
                    });
                });
                summary.setText ("Singing the parts... 0 %", juce::dontSendNotification);
                startTimer (200);
                updateButtons();
            }

            void timerCallback() override
            {
                if (job != nullptr && ! job->done)
                    summary.setText ("Singing the parts... " + juce::String (juce::roundToInt (job->progress * 100.0f)) + " %"
                                     + (job->cancel ? "  (stopping)" : ""), juce::dontSendNotification);
            }

            void paint (juce::Graphics& g) override
            {
                using namespace StudioColours;
                g.fillAll (card);
                g.setColour (textDim);
                g.setFont (studioFont (12.0f));
                for (auto& [r, label] : labels)
                    g.drawText (label, r, juce::Justification::centredLeft);

                // Preview: the melody (gold) and the chosen parts, in the part of the recording the clip plays
                auto r = preview.toFloat();
                g.setColour (background);
                g.fillRoundedRectangle (r, 4.0f);
                if (clip == nullptr || editRef.get() == nullptr || ! haveMelody || melody.notes.empty())
                    return;

                const auto geo = geometry();
                const auto& chosen = geo.chosen;
                const auto from = geo.from, len = geo.len;
                const auto rowH = geo.rowH;
                auto yOf = [&] (int note) { return geo.yOf (note); };
                auto xOf = [&] (double t) { return geo.xOf (t); };

                // Chord changes
                g.setColour (cardBorder);
                for (auto& c : clipInfo.context.chords)
                    if (c.start > from && c.start < from + len)
                        g.drawVerticalLine ((int) xOf (c.start), r.getY() + 2, r.getBottom() - 2);

                g.saveState();
                g.reduceClipRegion (preview);
                for (size_t k = 0; k < chosen.size(); ++k)
                {
                    const auto colour = juce::Colour (parts()[(size_t) chosen[k]].colour);
                    for (size_t i = 0; i < melody.notes.size(); ++i)
                    {
                        const auto rect = geo.noteRect (k, i);
                        g.setColour (colour.withAlpha (0.85f));
                        g.fillRect (rect);
                        if (edits.find (chosen[k], melody.notes[i]))
                        {
                            g.setColour (juce::Colours::white);                 // moved by hand
                            g.drawRect (rect.expanded (1.0f), 1.0f);
                        }
                        if (chosen[k] == selPart && (int) i == selNote)
                        {
                            g.setColour (gold);
                            g.drawRect (rect.expanded (3.0f), 2.0f);
                        }
                    }
                }
                g.setColour (gold);
                for (auto& n : melody.notes)
                    g.fillRect (juce::Rectangle<float> (xOf (n.start), yOf (n.note), juce::jmax (1.5f, xOf (n.end) - xOf (n.start)), juce::jmax (2.0f, rowH - 1.0f)));
                g.restoreState();

                g.setColour (textDim);
                g.setFont (studioFont (10.5f));
                g.drawText ("gold: the melody   colours: the parts   white edge: moved by hand   click a part's note to move it",
                            preview.withTrimmedBottom (-16).removeFromBottom (14), juce::Justification::centredRight);
            }

            void resized() override
            {
                labels.clear();
                auto r = getLocalBounds().reduced (14, 10);
                title.setBounds (r.removeFromTop (26));
                info.setBounds (r.removeFromTop (36));
                r.removeFromTop (4);
                presetBox.setBounds (r.removeFromTop (26));
                r.removeFromTop (6);
                {
                    auto grid = r.removeFromTop (3 * 24);
                    const int colW = grid.getWidth() / 3;
                    for (size_t k = 0; k < toggles.size(); ++k)
                        toggles[k]->setBounds (grid.getX() + (int) (k % 3) * colW, grid.getY() + (int) (k / 3) * 24, colW, 24);
                }
                r.removeFromTop (6);
                auto row = [&] (const juce::String& label, juce::Component& c)
                {
                    auto line = r.removeFromTop (26);
                    labels.push_back ({ line.removeFromLeft (130), label });
                    c.setBounds (line);
                    r.removeFromTop (4);
                };
                row ("Singers in each part", singers);
                row ("Tight - loose", looseness);
                busToggle.setBounds (r.removeFromTop (24));
                r.removeFromTop (6);
                auto buttons = r.removeFromBottom (30);
                makeButton.setBounds (buttons.removeFromRight (170));
                buttons.removeFromRight (8);
                removeButton.setBounds (buttons.removeFromRight (190));
                r.removeFromBottom (6);
                summary.setBounds (r.removeFromBottom (36));
                r.removeFromBottom (6);
                {
                    auto editRow = r.removeFromBottom (28);
                    const int w = editRow.getWidth();
                    upButton.setBounds (editRow.removeFromLeft (34));        editRow.removeFromLeft (4);
                    downButton.setBounds (editRow.removeFromLeft (34));      editRow.removeFromLeft (8);
                    chordUpButton.setBounds (editRow.removeFromLeft (110));  editRow.removeFromLeft (4);
                    chordDownButton.setBounds (editRow.removeFromLeft (110)); editRow.removeFromLeft (8);
                    autoButton.setBounds (editRow.removeFromLeft (90));
                    resetEditsButton.setBounds (editRow.removeFromRight (juce::jmin (170, w / 3)));
                }
                r.removeFromBottom (4);
                selLabel.setBounds (r.removeFromBottom (20));
                r.removeFromBottom (18);
                preview = r;
            }

            //------------------------------------------------------------------
            //  Moving notes by hand (Step 33)
            //------------------------------------------------------------------
            struct Geometry
            {
                juce::Rectangle<float> area;
                double from = 0.0, len = 1.0;
                int lo = 0, hi = 127;
                float rowH = 1.0f;
                std::vector<int> chosen;
                std::vector<std::vector<int>> plans;
                const Melody* melody = nullptr;

                float yOf (int note) const    { return area.getBottom() - 4.0f - (float) (note - lo + 1) * rowH; }
                float xOf (double s) const    { return area.getX() + (float) ((s - from) / len) * area.getWidth(); }
                juce::Rectangle<float> noteRect (size_t k, size_t i) const
                {
                    const auto& n = melody->notes[i];
                    return { xOf (n.start), yOf (plans[k][i]), juce::jmax (1.5f, xOf (n.end) - xOf (n.start)), juce::jmax (2.0f, rowH - 1.0f) };
                }
            };

            Geometry geometry() const
            {
                Geometry geo;
                geo.area = preview.toFloat();
                geo.melody = &melody;
                if (clip == nullptr || editRef.get() == nullptr || melody.notes.empty())
                    return geo;
                const auto pos = clip->getPosition();
                geo.from = pos.getOffset().inSeconds();
                geo.len = juce::jmax (0.1, pos.getLength().inSeconds());
                geo.chosen = chosenParts();
                int lo = 127, hi = 0;
                for (auto& n : melody.notes) { lo = juce::jmin (lo, n.note); hi = juce::jmax (hi, n.note); }
                for (auto p : geo.chosen)
                {
                    geo.plans.push_back (planPart (p, melody, clipInfo.context, edits));
                    for (auto v : geo.plans.back()) { lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
                }
                // A steady scale while dragging, so the note follows the mouse
                if (dragging) { lo = juce::jmin (lo, dragLo); hi = juce::jmax (hi, dragHi); }
                geo.lo = lo - 1;
                geo.hi = hi + 1;
                geo.rowH = juce::jmax (1.0f, (geo.area.getHeight() - 8.0f) / (float) (geo.hi - geo.lo + 1));
                return geo;
            }

            bool hasSelection() const
            {
                return haveMelody && selNote >= 0 && selNote < (int) melody.notes.size() && selPart >= 0
                    && std::find (chosenPartsCache.begin(), chosenPartsCache.end(), selPart) != chosenPartsCache.end();
            }

            int currentNote (int part, int index) const
            {
                const auto plan = planPart (part, melody, clipInfo.context, edits);
                return index >= 0 && index < (int) plan.size() ? plan[(size_t) index] : 60;
            }

            void moveSelected (int direction, bool toChordNote)
            {
                if (! hasSelection())
                    return;
                const auto& n = melody.notes[(size_t) selNote];
                const auto now = currentNote (selPart, selNote);
                const auto next = toChordNote ? chordToneStep (now, chordFor (clipInfo.context, n), clipInfo.context.key, direction)
                                              : now + direction;
                edits.set (selPart, n, next);
                editsChanged();
            }

            void editsChanged()
            {
                if (clip != nullptr && editRef.get() != nullptr)
                    saveEdits (*clip, edits);
                if (clip != nullptr && editRef.get() != nullptr && ! findHarmonyTracks (*clip).isEmpty())
                    summary.setText ("Notes changed. Press \"Make the harmonies\" to sing them again with your changes.", juce::dontSendNotification);
                updateButtons();
                repaint();
            }

            void updateSelectionText()
            {
                chosenPartsCache = chosenParts();
                if (! haveMelody || melody.notes.empty())
                {
                    selLabel.setText ({}, juce::dontSendNotification);
                    return;
                }
                if (! hasSelection())
                {
                    selLabel.setText ("Click a coloured note in the picture to change it.", juce::dontSendNotification);
                    return;
                }
                const auto& key = clipInfo.context.key;
                const auto& n = melody.notes[(size_t) selNote];
                const auto now = currentNote (selPart, selNote);
                juce::String t;
                t << parts()[(size_t) selPart].name << ", note " << (selNote + 1) << " of " << (int) melody.notes.size()
                  << " (melody " << Theory::midiNoteName (n.note, key) << "): sings " << Theory::midiNoteName (now, key);
                if (edits.find (selPart, n))
                {
                    Edits without = edits;
                    without.clear (selPart, n);
                    const auto plan = planPart (selPart, melody, clipInfo.context, without);
                    t << ", moved by hand (the app chose " << Theory::midiNoteName (plan[(size_t) selNote], key) << ")";
                }
                selLabel.setText (t, juce::dontSendNotification);
            }

            void mouseDown (const juce::MouseEvent& e) override
            {
                if (! preview.contains (e.getPosition()) || ! haveMelody)
                    return;
                grabKeyboardFocus();
                const auto geo = geometry();
                const auto p = e.position;
                float best = 1.0e9f;
                selPart = -1; selNote = -1;
                for (size_t k = 0; k < geo.chosen.size(); ++k)
                    for (size_t i = 0; i < melody.notes.size(); ++i)
                    {
                        const auto r = geo.noteRect (k, i).expanded (3.0f, 3.0f);
                        if (! r.contains (p)) continue;
                        const auto d = std::abs (r.getCentreY() - p.y);
                        if (d < best) { best = d; selPart = geo.chosen[k]; selNote = (int) i; }
                    }
                if (selNote >= 0)
                {
                    dragStartY = p.y;
                    dragStartNote = currentNote (selPart, selNote);
                    dragRowH = geo.rowH;
                    dragLo = geo.lo + 1; dragHi = geo.hi - 1;
                    dragging = true;
                }
                updateButtons();
                repaint();
            }

            void mouseDrag (const juce::MouseEvent& e) override
            {
                if (! dragging || ! hasSelection())
                    return;
                const auto steps = juce::roundToInt ((dragStartY - e.position.y) / juce::jmax (1.0f, dragRowH));
                const auto& n = melody.notes[(size_t) selNote];
                const auto target = juce::jlimit (24, 96, dragStartNote + steps);
                if (target != currentNote (selPart, selNote))
                {
                    edits.set (selPart, n, target);
                    dragLo = juce::jmin (dragLo, target); dragHi = juce::jmax (dragHi, target);
                    updateSelectionText();
                    repaint();
                }
            }

            void mouseUp (const juce::MouseEvent&) override
            {
                if (dragging)
                {
                    dragging = false;
                    if (hasSelection() && currentNote (selPart, selNote) != dragStartNote)
                        editsChanged();
                }
            }

            bool keyPressed (const juce::KeyPress& k) override
            {
                if (! hasSelection())
                    return false;
                const bool shift = k.getModifiers().isShiftDown();
                if (k.getKeyCode() == juce::KeyPress::upKey)   { moveSelected (1, shift);  return true; }
                if (k.getKeyCode() == juce::KeyPress::downKey) { moveSelected (-1, shift); return true; }
                if (k.getKeyCode() == juce::KeyPress::leftKey || k.getKeyCode() == juce::KeyPress::rightKey)
                {
                    selNote = juce::jlimit (0, (int) melody.notes.size() - 1, selNote + (k.getKeyCode() == juce::KeyPress::rightKey ? 1 : -1));
                    updateButtons();
                    repaint();
                    return true;
                }
                return false;
            }

        private:
            te::WaveAudioClip* clip;
            te::Clip::Ptr keep;
            te::SafeSelectable<te::Edit> editRef;
            te::EditItemID clipID;
            std::function<Theory::Key()> getKey;
            std::function<void()> onChanged;
            ClipInfo clipInfo;
            Melody melody;
            bool haveMelody = false;
            std::shared_ptr<JobState> job;

            juce::Label title, info, summary;
            juce::ComboBox presetBox;
            std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
            juce::Slider singers, looseness;
            juce::ToggleButton busToggle { "Send them to a Choir bus" };
            juce::TextButton makeButton { "Make the harmonies" }, removeButton { "Remove the harmony tracks" };
            juce::Rectangle<int> preview;
            std::vector<std::pair<juce::Rectangle<int>, juce::String>> labels;

            // Moving notes by hand (Step 33)
            Edits edits;
            int selPart = -1, selNote = -1;
            std::vector<int> chosenPartsCache;
            bool dragging = false;
            float dragStartY = 0.0f, dragRowH = 1.0f;
            int dragStartNote = 0, dragLo = 127, dragHi = 0;
            juce::Label selLabel;
            juce::TextButton upButton { juce::String::fromUTF8 ("\xe2\x96\xb2") }, downButton { juce::String::fromUTF8 ("\xe2\x96\xbc") },
                             chordUpButton { juce::String::fromUTF8 ("Chord note \xe2\x96\xb2") }, chordDownButton { juce::String::fromUTF8 ("Chord note \xe2\x96\xbc") },
                             autoButton { "Automatic" }, resetEditsButton { "Reset all edits" };
        };
    }

    std::unique_ptr<juce::Component> createPanel (te::WaveAudioClip& clip, std::function<Theory::Key()> getKey, std::function<void()> onChanged)
    {
        return std::make_unique<Panel> (clip, std::move (getKey), std::move (onChanged));
    }
}
