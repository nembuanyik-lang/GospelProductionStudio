#include "Backing.h"
#include "Harmony.h"
#include "Grooves.h"
#include "Mixing.h"
#include "StudioEngine.h"
#include "SamplerPlugin.h"
#include "OrganPlugin.h"
#include "GuitarPlugin.h"
#include "StudioLookAndFeel.h"

namespace Backing
{
    namespace
    {
        const juce::Identifier bandId ("gpsBand");

        const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int minorScale[] = { 0, 2, 3, 5, 7, 8, 10 };

        int pcOf (int note)   { return ((note % 12) + 12) % 12; }

        // The note with pitch class 'pc' nearest to 'near', inside [lo, hi]
        int nearestWithPc (int pc, int nearNote, int lo, int hi)
        {
            int best = -1;
            for (int n = lo; n <= hi; ++n)
                if (pcOf (n) == pc && (best < 0 || std::abs (n - nearNote) < std::abs (best - nearNote)))
                    best = n;
            return best < 0 ? lo + pcOf (pc - lo) : best;
        }

        // 'steps' scale notes below 'note' (diatonic walk), keeping a chromatic note's colour
        int scaleStep (int note, int steps, const Theory::Key& key)
        {
            const int* scale = key.minor ? minorScale : majorScale;
            const int pc = pcOf (note - key.tonic);
            int i = 0;
            for (int k = 0; k < 7; ++k) if (scale[k] <= pc) i = k;
            const int chromatic = pc - scale[i];
            int idx = i + steps, octave = 0;
            while (idx < 0)  { idx += 7; --octave; }
            while (idx >= 7) { idx -= 7; ++octave; }
            return note - pc + 12 * octave + scale[idx] + chromatic;
        }

        std::vector<int> upperPcs (const Theory::Chord& c, const Theory::Key& key)
        {
            auto u = c;
            u.bass = -1;
            return Theory::chordPitchClasses (u, key);
        }

        // Guitar shape: from the root in E3..D#4, chord notes stacked upwards (4-5 strings)
        std::vector<int> guitarShape (const Theory::Chord& c, const Theory::Key& key)
        {
            const auto pcs = upperPcs (c, key);
            const int rootPc = (key.tonic + c.root) % 12;
            int n = 52 + pcOf (rootPc - 52);
            std::vector<int> shape { n };
            // Third, fifth, then the colour notes, each at least 3 semitones above the last
            for (int guard = 0; guard < 24 && shape.size() < 5; ++guard)
            {
                ++n;
                if (n > 79) break;
                if (std::find (pcs.begin(), pcs.end(), pcOf (n)) != pcs.end() && n - shape.back() >= 3)
                    shape.push_back (n);
            }
            return shape;
        }

        // A few drum notes
        constexpr int kick = 36, snare = 38, crash = 49, hiTom = 50, midTom = 47, lowTom = 45, floorTom = 43;
    }

    juce::String playerName (int p)
    {
        static const char* names[] = { "Bass", "Piano", "Organ", "Guitar", "Drums" };
        return names[juce::jlimit (0, numPlayers - 1, p)];
    }

    //==========================================================================
    //  The styles
    //==========================================================================
    const std::vector<Style>& styles()
    {
        static const std::vector<Style> s {
            { "Praise (mid tempo)", "Bright 16ths: pushing bass, piano stabs, organ bed, guitar chicks. 95-120 BPM",
              4, 4, 4, "Praise (mid tempo)", 0, {
                { "R=======R===5===", "R=-rR-5-R=-r5=A-" },
                { "C===========c===", "C==-T-c-C==-T-c=" },
                { "P===============", "P===============" },
                { "D===============", "m-U-m-U-m-U-mUU-" } } },
            { "Worship ballad", "Slow and open: long bass notes, flowing piano, warm organ, picked guitar. 60-80 BPM",
              4, 4, 4, "Gospel ballad", 1, {
                { "R===============", "R=======5===R===" },
                { "C=======c=======", "C=====c=c===c===" },
                { "P===============", "P===============" },
                { "D===============", "a-a-a-a-a-a-a-a-" } } },
            { "Gospel shout (fast)", "Fast church shout: driving octave bass, off-beat piano, guitar chicks. 130-160 BPM",
              4, 4, 4, "Gospel shout (fast)", 0, {
                { "R===R===R===5===", "R-RrO-R-R-5rO-A-" },
                { "C===c===C===c===", "C-T-c-T-C-T-c-T-" },
                { "P=======P=======", "P=======P=======" },
                { "--m---m---m---m-", "-m-m-m-m-m-m-m-m" } } },
            { "Praise shuffle", "Swinging triplet feel, like a Sunday morning praise break. 90-120 BPM",
              4, 4, 3, "Praise shuffle", 0, {
                { "R=====5=====", "R=rO=rR=r5=A" },
                { "C=====c=====", "C=-c=TC=-c=T" },
                { "P===========", "P===========" },
                { "D===========", "---D-U---D-U" } } },
            { "Makossa praise", "Cameroonian makossa: busy syncopated bass, picked guitar lines, light piano. 105-125 BPM",
              4, 4, 4, "Makossa (classic)", 0, {
                { "R==-R==-5==-R==-", "R-rR-5O-r-R-5-O-" },
                { "C===============", "--c---c---c---c-" },
                { "P===============", "P===============" },
                { "a-a-a-a-a-a-a-a-", "a-aaa-aaa-aaa-aa" } } },
            { "Highlife praise", "West African highlife: melodic bass, ringing guitar patterns, off-beat piano. 100-125 BPM",
              4, 4, 4, "Highlife (Ghana classic)", 0, {
                { "R===5===R===5===", "R-5-O-5-R-3-5-A-" },
                { "C=======c=======", "-c-c-c-c-c-c-c-c" },
                { "P===============", "P===============" },
                { "a-a-a-a-a-a-a-a-", "a-aa-aa-a-aa-aa-" } } },
            { "Afro-worship", "Modern African worship: steady pulse, warm keys, guitar picking. 70-100 BPM",
              4, 4, 4, "Afro-worship", 0, {
                { "R=======R===5===", "R==-R-5-R==-5-A-" },
                { "C=======c=======", "C===c=-cC===c=-c" },
                { "P===============", "P===============" },
                { "a---a---a---a---", "a-a-a-a-a-a-a-a-" } } },
            { "Hymn (traditional)", "Church hymn: block chords on the beat, organ, walking bass. Any tempo",
              4, 4, 4, "Hymn (soft)", 1, {
                { "R=======5=======", "R===R===5===R===" },
                { "C=======C=======", "C===C===C===C===" },
                { "P===============", "P===============" },
                { "D===============", "D===U===D===U===" } } },
            { "6/8 worship", "Rolling 6/8: two big pulses per bar, piano eighths, guitar strums. 50-75 BPM (dotted quarter)",
              6, 8, 2, "6/8 worship (driving)", 1, {
                { "R=====5=====", "R==rR=5==r5=" },
                { "C=====C=====", "C=c=c=C=c=c=" },
                { "P===========", "P===========" },
                { "D=====D=====", "D=U=U=D=U=U=" } } },
            { "12/8 slow gospel", "Old-school slow 12/8: triplet piano, deep bass, organ. 50-70 BPM",
              12, 8, 1, "12/8 slow gospel", 1, {
                { "R=====5=====", "R==R=5R==5=A" },
                { "C=====C=====", "C=cc=cC=cc=c" },
                { "P===========", "P===========" },
                { "D=====D=====", "a=aa=aa=aa=a" } } },
            // ---- The African and Cameroonian pack (Step 21) ---------------------------
            { "Bikutsi praise", "Beti bikutsi from the Centre region: fast 6/8, hammering bass, rapid guitar picking, bottle and shaker. 130-160 BPM (eighths)",
              6, 8, 2, "Bikutsi (6/8, fast)", 0, {
                { "R=====R=====", "R-RR-5R-RR-A" },
                { "C=====C=====", "C-c-c-C-c-c-" },
                { "P===========", "P===========" },
                { "a-a-a-a-a-a-", "a-aa-aa-aa-a" } } },
            { "Assiko praise", "Bassa assiko: quick and bright, bass on every beat, guitar picking, the bottle on 16ths. 120-140 BPM",
              4, 4, 4, "Assiko (Bassa, bottle)", 0, {
                { "R===R===5===R===", "R-R-5-R-R-R-5-A-" },
                { "C=======C=======", "-c-c-c-c-c-c-c-c" },
                { "P===============", "P===============" },
                { "a-a-a-a-a-a-a-a-", "a-aaa-aaa-aaa-aa" } } },
            { "Bend skin praise", "Bamileke bend skin: mid-fast, bouncy bass, guitar chicks on the off-beat, tambourine. 110-130 BPM",
              4, 4, 4, "Bend skin", 0, {
                { "R=======R===5===", "R--rR-5-R--rO-A-" },
                { "C=======c=======", "--c---c---c---c-" },
                { "P===============", "P===============" },
                { "D=======D=======", "-m-m-m-m-m-m-m-m" } } },
            { "Mangambeu praise", "Bangangte mangambeu: rolling shakers and bell, syncopated bass, ringing guitar. 100-120 BPM",
              4, 4, 4, "Mangambeu (Bamileke)", 0, {
                { "R=======R===5===", "R-rR-5--R-rO-5A-" },
                { "C=======c=======", "-c--c-c--c--c-c-" },
                { "P===============", "P===============" },
                { "a-a-a-a-a-a-a-a-", "a-aa-a-aa-aa-a-a" } } },
            { "Njang worship (12/8)", "North-West Cameroon njang: the 12/8 bell, djembe and shakers, deep bass, rolling piano. 50-70 BPM",
              12, 8, 1, "Njang (North-West, 12/8)", 1, {
                { "R=====5=====", "R==R=5R==5=A" },
                { "C=====C=====", "C=cc=cC=cc=c" },
                { "P===========", "P===========" },
                { "D=====D=====", "a=aa=aa=aa=a" } } },
            { "Nigerian praise (Igbo)", "Igbo praise: the ogene bell, congas, steady bass and highlife guitar. 120-140 BPM",
              4, 4, 4, "Nigerian praise (Igbo, ogene)", 0, {
                { "R===5===R===5===", "R-5-R-5-R-5-O-A-" },
                { "C=======c=======", "-c-c-c-c-c-c-c-c" },
                { "P===============", "P===============" },
                { "a-a-a-a-a-a-a-a-", "a-aa-aa-a-aa-aa-" } } },
            { "Yoruba praise (talking drum)", "Yoruba praise with talking drum and shekere, warm bass, organ. 100-120 BPM",
              4, 4, 4, "Yoruba praise (talking drum)", 0, {
                { "R=======R===5===", "R=-rR-5-R=-r5=A-" },
                { "C=======c=======", "C==-c-c-C==-c-c-" },
                { "P===============", "P===============" },
                { "a---a---a---a---", "a-a-a-a-a-a-a-a-" } } },
            { "Afrobeat praise", "Afrobeat groove: tight bass riff, chicken-scratch guitar, cowbell and shaker. 100-115 BPM",
              4, 4, 4, "Afrobeat", 0, {
                { "R=====5=R=====5=", "R--R--5-R-O--5A-" },
                { "C=======c=======", "-T--T--T-T--T--T" },
                { "P===============", "P===============" },
                { "-m-m-m-m-m-m-m-m", "m-mm-mm-m-mm-mm-" } } },
            { "Soukous praise", "Congolese soukous / ndombolo: fast, busy sebene guitar, driving bass. 125-145 BPM",
              4, 4, 4, "Soukous / Ndombolo", 0, {
                { "R===R===5===R===", "R-R-O-R-R-5-O-A-" },
                { "C=======c=======", "C-c-c-c-C-c-c-c-" },
                { "P===============", "P===============" },
                { "a-a-a-a-a-a-a-a-", "aaaaaaaaaaaaaaaa" } } },
            { "Congolese rumba worship", "Slow rumba: clave, congas, sweet picked guitar, gentle bass. 85-105 BPM",
              4, 4, 4, "Rumba congolaise", 0, {
                { "R=====5=R=======", "R==5==R=-R=5=-A-" },
                { "C=====c=C=====c=", "C==c==C=-c=c=-c-" },
                { "P===============", "P===============" },
                { "a---a---a---a---", "a-a-aa-aa-a-aa-a" } } },
            { "African 12/8 worship", "Slow African worship in 12/8 with the bell and djembe, deep bass, soft guitar. 50-70 BPM",
              12, 8, 1, "African 12/8 worship (bell)", 1, {
                { "R=====5=====", "R==R=5R==5=A" },
                { "C=====C=====", "C=cc=cC=cc=c" },
                { "P===========", "P===========" },
                { "D=====D=====", "D=====a=aa=a" } } },
        };
        return s;
    }

    bool fits (const Style& s, int numerator, int denominator)
    {
        return s.numerator == numerator && s.denominator == denominator;
    }

    bool isFullSection (const juce::String& name)
    {
        const auto n = name.toLowerCase();
        for (auto* light : { "intro", "verse", "outro", "interlude", "tag" })
            if (n.contains (light)) return false;
        return true;           // chorus, pre-chorus, bridge, vamp, and anything else
    }

    //==========================================================================
    //  Arranging
    //==========================================================================
    std::vector<Part> arrange (const std::vector<ChordTrack::Event>& events, const std::vector<SectionSpan>& sections,
                               const Theory::Key& key, double bpb, const Settings& settings)
    {
        std::vector<Part> parts;
        if (events.empty() || bpb <= 0.0)
            return parts;
        const auto& style = styles()[(size_t) juce::jlimit (0, (int) styles().size() - 1, settings.style)];
        const int spb = style.stepsPerBeat;
        const double stepLen = 1.0 / spb;
        const int stepsPerBar = juce::roundToInt (bpb * spb);
        const double eighth = style.denominator == 8 ? 1.0 : 0.5;       // push window: one eighth note
        const double first = events.front().start;
        const double end = events.back().start + events.back().length;
        const bool hasEnding = settings.ending && events.size() > 1;
        const double patternEnd = hasEnding ? events.back().start : end;
        constexpr double eps = 1.0e-6;

        auto chordIndexAt = [&] (double beat)
        {
            int idx = 0;
            for (int i = 0; i < (int) events.size(); ++i)
                if (events[(size_t) i].start <= beat + eps) idx = i;
            return idx;
        };
        auto nextChange = [&] (double beat)
        {
            for (auto& e : events)
                if (e.start > beat + eps) return e.start;
            return end;
        };
        auto isFull = [&] (double beat)
        {
            if (! settings.followSections || sections.empty()) return true;
            for (auto& s : sections)
                if (beat >= s.start - eps && beat < s.end - eps) return isFullSection (s.name);
            return true;
        };

        // Fill windows: the end of the bar before each new section
        const double fillLen = style.denominator == 8 ? bpb / 2.0 : 2.0;
        std::vector<std::pair<double, double>> fills;      // [start, end)
        if (settings.fills)
            for (auto& s : sections)
                if (s.start > first + fillLen - eps && s.start <= patternEnd + eps)
                    fills.push_back ({ s.start - fillLen, s.start });
        auto inFill = [&] (double beat)
        {
            for (auto& f : fills) if (beat >= f.first - eps && beat < f.second - eps) return true;
            return false;
        };

        // The step grid, aligned to bars
        const double gridStart = std::floor (first / bpb + eps) * bpb;
        const int totalSteps = (int) std::ceil ((patternEnd - gridStart) / stepLen - eps);

        for (int p = 0; p < numPlayers; ++p)
        {
            if (! settings.players[p]) continue;
            Part part;
            part.player = p;
            juce::Random rng (1000 + p * 77 + settings.style);
            auto human = [&] (Note n, bool timing)
            {
                const auto h = juce::jlimit (0.0f, 1.0f, settings.humanise);
                if (timing && h > 0.0f)
                    n.start = juce::jmax (first, n.start + (rng.nextFloat() * 2.0f - 1.0f) * 0.02 * h);
                n.velocity = juce::jlimit (20, 127, n.velocity + juce::roundToInt ((rng.nextFloat() * 2.0f - 1.0f) * 10.0f * h));
                return n;
            };

            if (p == drums)
            {
                const Grooves::Groove* groove = nullptr;
                for (auto& g : Grooves::all())
                    if (juce::String (g.name) == style.groove) groove = &g;
                if (groove != nullptr && patternEnd - gridStart > eps)
                {
                    te::MidiList list;
                    Grooves::writeTo (list, *groove, 0.0, patternEnd - gridStart, bpb, nullptr);
                    for (auto* mn : list.getNotes())
                    {
                        Note n;
                        n.pitch = mn->getNoteNumber();
                        n.start = gridStart + mn->getStartBeat().inBeats();
                        n.length = juce::jmax (0.05, mn->getLengthBeats().inBeats());
                        n.velocity = mn->getVelocity();
                        if (n.start < first - eps || n.start >= patternEnd - eps || inFill (n.start)) continue;
                        if (! isFull (n.start))
                        {
                            if (n.pitch == crash || n.pitch == 46 || (n.pitch >= 41 && n.pitch <= 50 && n.pitch != 42 && n.pitch != 44)) continue;
                            n.velocity = juce::roundToInt (n.velocity * 0.78f);
                        }
                        part.notes.push_back (human (n, false));
                    }
                    // Fills: a run down the toms (African grooves: djembe slaps and a talking drum call),
                    // louder towards the end, then a crash on the new section
                    const bool african = Grooves::usesPercussion (*groove);
                    for (auto& f : fills)
                    {
                        if (african)
                        {
                            const double st = style.denominator == 8 ? 0.5 : 0.25;
                            const int count = (int) std::round ((f.second - f.first) / st);
                            for (int i = 0; i < count; ++i)
                            {
                                const bool late = i >= count - 2;
                                Note n { late ? (i == count - 1 ? 79 : 78) : (i % 3 == 2 ? 61 : 60), f.first + i * st, st, juce::jmin (124, 80 + i * 40 / juce::jmax (1, count)) };
                                part.notes.push_back (n);
                            }
                            part.notes.push_back ({ 87, f.first, 0.5, 110 });
                            if (f.second < patternEnd - eps)
                            {
                                part.notes.push_back ({ crash, f.second, 1.0, 112 });
                                part.notes.push_back ({ kick, f.second, 0.25, 110 });
                            }
                            continue;
                        }
                        const double step = style.denominator == 8 ? 0.5 : 0.25;
                        const int count = (int) std::round ((f.second - f.first) / step);
                        const int toms[] = { snare, snare, hiTom, hiTom, midTom, midTom, lowTom, floorTom };
                        for (int i = 0; i < count; ++i)
                        {
                            Note n;
                            n.pitch = toms[juce::jlimit (0, 7, i * 8 / juce::jmax (1, count))];
                            n.start = f.first + i * step;
                            n.length = step;
                            n.velocity = juce::jmin (124, 75 + i * 45 / juce::jmax (1, count));
                            part.notes.push_back (n);
                        }
                        part.notes.push_back ({ kick, f.first, 0.25, 100 });
                        if (f.second < patternEnd - eps)
                        {
                            part.notes.push_back ({ crash, f.second, 1.0, 112 });
                            part.notes.push_back ({ kick, f.second, 0.25, 110 });
                        }
                    }
                }
                if (hasEnding)
                {
                    part.notes.push_back ({ crash, patternEnd, 2.0, 118 });
                    part.notes.push_back ({ kick, patternEnd, 0.5, 118 });
                }
                parts.push_back (std::move (part));
                continue;
            }

            // ---- Pitched players --------------------------------------------
            int prevBass = 0, keysCentre = 0, organCentre = 64, arpIndex = 0, lastChord = -1;

            auto bassRoot = [&] (const Theory::Chord& c)
            {
                const int pc = (key.tonic + (c.bass >= 0 ? c.bass : c.root)) % 12;
                return nearestWithPc (pc, prevBass > 0 ? prevBass : 36, 28, 47);
            };

            auto notesFor = [&] (char sym, int chordIdx, double at, double& length, int& velocity) -> std::vector<int>
            {
                const auto& c = events[(size_t) chordIdx].chord;
                const bool accent = std::abs (std::fmod (at - gridStart, 1.0)) < eps;
                velocity = accent ? 96 : 86;
                switch (p)
                {
                    case bass:
                    {
                        const int root = bassRoot (c);
                        int n = root;
                        if (sym == '5') n = root + 7 <= 50 ? root + 7 : root - 5;
                        else if (sym == 'O') n = root + 12;
                        else if (sym == '3')
                        {
                            const auto pcs = upperPcs (c, key);
                            const int rp = pcOf (root);
                            n = std::find (pcs.begin(), pcs.end(), (rp + 4) % 12) != pcs.end() ? root + 4
                              : std::find (pcs.begin(), pcs.end(), (rp + 3) % 12) != pcs.end() ? root + 3 : root + 7;
                        }
                        else if (sym == 'A')
                        {
                            const auto nc = nextChange (at);
                            if (nc - at <= 1.0 + eps && nc < end - eps)
                            {
                                const auto& next = events[(size_t) chordIndexAt (nc)].chord;
                                const int pc = (key.tonic + (next.bass >= 0 ? next.bass : next.root)) % 12;
                                const int target = nearestWithPc (pc, root, 28, 47);
                                n = target == root ? root + 7 : (target > root ? target - 1 : target + 1);
                            }
                            else n = root + 7 <= 50 ? root + 7 : root - 5;
                        }
                        if (sym == 'r') velocity = 68;
                        if (sym != 'O' && sym != 'A' && sym != '5' && sym != '3') prevBass = n;
                        return { n };
                    }
                    case keys:
                    {
                        auto v = Theory::voice (c, key, Theory::Voicing::gospel, Theory::LeftHand::root, keysCentre);
                        if (chordIdx != lastChord)
                        {
                            // Follow the voice leading, but never let the hands drift out of the piano's middle
                            keysCentre = Theory::rightHandCentre (std::vector<int> (v.begin() + 1, v.end()));
                            while (keysCentre > 74) keysCentre -= 12;
                            while (keysCentre > 0 && keysCentre < 58) keysCentre += 12;
                        }
                        if (sym == 'c') velocity = 70;
                        if (sym == 'T') return std::vector<int> (v.begin() + 1, v.end());
                        if (sym == 'B') return { v.front() };
                        return v;
                    }
                    case organ:
                    {
                        auto v = Theory::voice (c, key, Theory::Voicing::close, Theory::LeftHand::root, organCentre);
                        std::vector<int> rh (v.begin() + 1, v.end());
                        if (chordIdx != lastChord)
                        {
                            organCentre = Theory::rightHandCentre (rh);
                            while (organCentre > 74) organCentre -= 12;
                            while (organCentre < 56) organCentre += 12;
                        }
                        velocity = 90;
                        if (sym == 's') length = juce::jmin (length, stepLen);
                        return rh;
                    }
                    default:      // guitar
                    {
                        auto shape = guitarShape (c, key);
                        if (sym == 'U') { velocity = 74; return std::vector<int> (shape.end() - std::min<long> (3, (long) shape.size()), shape.end()); }
                        if (sym == 'm')
                        {
                            velocity = 62;
                            length = juce::jmin (length, 0.08);
                            return std::vector<int> (shape.end() - std::min<long> (3, (long) shape.size()), shape.end());
                        }
                        if (sym == 'a')
                        {
                            std::vector<int> top (shape.end() - std::min<long> (4, (long) shape.size()), shape.end());
                            int sum = 0;
                            for (auto t : top) sum += t;
                            if (! top.empty() && sum / (int) top.size() < 66) for (auto& t : top) t += 12;
                            static const int order[] = { 0, 2, 1, 3, 2, 1 };
                            const int pick = order[arpIndex++ % 6] % (int) std::max<size_t> (1, top.size());
                            velocity = accent ? 88 : 76;
                            return { top[(size_t) pick] };
                        }
                        return shape;
                    }
                }
            };

            for (int s = 0; s < totalSteps; ++s)
            {
                const double at = gridStart + s * stepLen;
                if (at < first - eps || at >= patternEnd - eps) continue;
                const int inBar = s % stepsPerBar;
                const char* pattern = style.patterns[p][isFull (at) ? 1 : 0];
                const int plen = (int) std::strlen (pattern);
                const char sym = pattern[inBar % juce::jmax (1, plen)];
                if (sym == '-' || sym == '=') continue;
                if (inFill (at) && (p == bass || p == keys || p == guitar)) continue;

                // How long: over the following '=' steps (within the bar)
                int hold = 1;
                while (inBar + hold < plen && pattern[inBar + hold] == '=') ++hold;
                double length = hold * stepLen;

                // Which chord: a note struck just before a chord change and held into it takes the new chord (a push)
                int ci = chordIndexAt (at);
                const auto change = nextChange (at);
                double stop = change;
                if (change - at <= eighth + eps && at + length > change + eps && change < patternEnd - eps)
                {
                    ci = chordIndexAt (change);
                    stop = nextChange (change);
                }
                length = juce::jmin (length, stop - at, patternEnd - at);
                if (length <= eps) continue;

                int velocity = 90;
                const auto pitches = notesFor (sym, ci, at, length, velocity);
                if (! isFull (at)) velocity = juce::roundToInt (velocity * 0.85f);
                lastChord = ci;

                for (size_t k = 0; k < pitches.size(); ++k)
                {
                    Note n;
                    n.pitch = pitches[k];
                    // Strums: one string after the other (down: low to high, up: high to low)
                    const double strum = p == guitar && pitches.size() > 1 ? 0.012 * (double) (sym == 'U' ? pitches.size() - 1 - k : k) : 0.0;
                    n.start = at + strum;
                    n.length = juce::jmax (0.05, length - 0.04 - strum);
                    n.velocity = velocity;
                    part.notes.push_back (human (n, k == 0));
                    if (k > 0) part.notes.back().start = part.notes[part.notes.size() - 1 - k].start + strum;
                }
            }

            // Bass and organ always play a new chord, even when their rhythm rests there
            if (p == bass || p == organ)
                for (size_t i = 1; i < events.size(); ++i)
                {
                    const double c = events[i].start;
                    if (c >= patternEnd - eps || inFill (c - 0.01)) continue;
                    bool struck = false;
                    for (auto& n : part.notes)
                        if (n.start > c - eighth - 0.03 && n.start < c + 0.03) { struck = true; break; }
                    if (struck) continue;
                    double nextStart = juce::jmin (nextChange (c), patternEnd);
                    for (auto& n : part.notes) if (n.start > c + eps) nextStart = juce::jmin (nextStart, n.start);
                    for (auto& n : part.notes)          // what was still sounding stops here
                        if (n.start < c && n.start + n.length > c) n.length = c - n.start - 0.02;
                    double len = (p == bass ? juce::jmin (1.0, nextStart - c) : nextStart - c) - 0.04;
                    int vel = 90;
                    const auto pitches = notesFor (p == bass ? 'R' : 'P', (int) i, c, len, vel);
                    for (auto pitch : pitches)
                        part.notes.push_back ({ pitch, c, juce::jmax (0.1, len), vel });
                }

            // Fills: bass walks up to the new chord; piano plays a gospel run up to it
            for (auto& f : fills)
            {
                const auto& next = events[(size_t) chordIndexAt (f.second)].chord;
                if (p == bass)
                {
                    const int pc = (key.tonic + (next.bass >= 0 ? next.bass : next.root)) % 12;
                    const int target = nearestWithPc (pc, prevBass > 0 ? prevBass : 36, 31, 47);
                    const int count = (int) std::round ((f.second - f.first) / eighth);
                    for (int i = 0; i < count; ++i)
                    {
                        const auto stepLenF = (f.second - f.first) / count;
                        Note n { scaleStep (target, -(count - i), key), f.first + i * stepLenF, stepLenF - 0.04, 84 + i * 6 };
                        part.notes.push_back (human (n, true));
                    }
                }
                else if (p == keys)
                {
                    // Pentatonic run (major or minor) rising to the top note of the next chord
                    const auto v = Theory::voice (next, key, Theory::Voicing::gospel, Theory::LeftHand::root, keysCentre);
                    const int top = v.back();
                    const int penta[] = { 0, 2, 4, 7, 9 }, mpenta[] = { 0, 3, 5, 7, 10 };
                    std::vector<int> scale;
                    for (int n = top - 14; n < top; ++n)
                    {
                        const int rel = pcOf (n - key.tonic);
                        for (int k = 0; k < 5; ++k)
                            if ((key.minor ? mpenta[k] : penta[k]) == rel) scale.push_back (n);
                    }
                    const double step = style.denominator == 8 ? 0.5 : 0.25;
                    const int count = (int) std::round ((f.second - f.first) / step);
                    for (int i = 0; i < count && ! scale.empty(); ++i)
                    {
                        const int idx = juce::jmax (0, (int) scale.size() - count + i);
                        Note n { scale[(size_t) juce::jmin (idx, (int) scale.size() - 1)], f.first + i * step, step * 0.9, 70 + i * 30 / juce::jmax (1, count) };
                        part.notes.push_back (human (n, false));
                    }
                }
            }

            // The ending: everyone on the last chord, held
            if (hasEnding)
            {
                const auto& last = events.back();
                double len = last.length - 0.05;
                int vel = 100;
                const char sym = p == bass ? 'R' : p == keys ? 'C' : p == organ ? 'P' : 'D';
                const auto pitches = notesFor (sym, (int) events.size() - 1, last.start, len, vel);
                for (size_t k = 0; k < pitches.size(); ++k)
                    part.notes.push_back ({ pitches[k], last.start + (p == guitar ? 0.015 * (double) k : 0.0), len, 105 });
            }

            std::sort (part.notes.begin(), part.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            parts.push_back (std::move (part));
        }

        //======================================================================
        //  A livelier band (Step 34)
        //======================================================================
        auto partFor = [&] (int player) -> Part*
        {
            for (auto& pt : parts) if (pt.player == player) return &pt;
            return nullptr;
        };
        auto sortAll = [&]
        {
            for (auto& pt : parts)
                std::sort (pt.notes.begin(), pt.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        };
        juce::Random liveRng (4321 + settings.style);
        auto human = [&] (Note n, bool timing)
        {
            const auto h = juce::jlimit (0.0f, 1.0f, settings.humanise);
            if (timing && h > 0.0f)
                n.start = juce::jmax (first, n.start + (liveRng.nextFloat() * 2.0f - 1.0f) * 0.02 * h);
            n.velocity = juce::jlimit (20, 127, n.velocity + juce::roundToInt ((liveRng.nextFloat() * 2.0f - 1.0f) * 10.0f * h));
            return n;
        };
        const double phrase = 4.0 * bpb;
        const bool africanGroove = [&]
        {
            for (auto& g : Grooves::all())
                if (juce::String (g.name) == style.groove) return Grooves::usesPercussion (g);
            return false;
        }();

        // 1. The song grows: a sparer first verse, a lifted last chorus, a build into each new section
        if (settings.growth && ! sections.empty())
        {
            const SectionSpan* firstVerse = nullptr;
            const SectionSpan* lastChorus = nullptr;
            int verses = 0, choruses = 0;
            for (auto& sct : sections)
            {
                const auto n = sct.name.toLowerCase();
                if (n.contains ("verse")) { ++verses; if (firstVerse == nullptr || sct.start < firstVerse->start) firstVerse = &sct; }
                if (n.contains ("chorus") && ! n.contains ("pre")) { ++choruses; if (lastChorus == nullptr || sct.start > lastChorus->start) lastChorus = &sct; }
            }
            auto inside = [&] (const SectionSpan* sct, double beat) { return sct != nullptr && beat >= sct->start - eps && beat < sct->end - eps; };

            if (verses >= 2)
            {
                // The first verse: just the rhythm section, a little softer (the guitar and organ come in later)
                for (auto& pt : parts)
                {
                    if (pt.player == guitar || pt.player == organ)
                        pt.notes.erase (std::remove_if (pt.notes.begin(), pt.notes.end(), [&] (const Note& n) { return inside (firstVerse, n.start) && ! inFill (n.start); }),
                                        pt.notes.end());
                    else
                        for (auto& n : pt.notes)
                            if (inside (firstVerse, n.start) && ! inFill (n.start)) n.velocity = juce::roundToInt (n.velocity * 0.9f);
                }
            }
            if (choruses >= 2)
            {
                // The last chorus lifts: louder, the piano's top note doubled an octave up, a crash every 4 bars
                for (auto& pt : parts)
                {
                    std::vector<Note> extra;
                    for (auto& n : pt.notes)
                    {
                        if (! inside (lastChorus, n.start)) continue;
                        n.velocity = juce::jmin (127, n.velocity + 8);
                        if (pt.player == keys && n.pitch >= 60)
                        {
                            bool top = true;
                            for (auto& o : pt.notes)
                                if (&o != &n && std::abs (o.start - n.start) < 0.01 && o.pitch > n.pitch) { top = false; break; }
                            int stacked = 0;
                            for (auto& o : pt.notes) if (std::abs (o.start - n.start) < 0.01) ++stacked;
                            if (top && stacked >= 3 && n.pitch + 12 <= 96)
                                extra.push_back ({ n.pitch + 12, n.start, n.length, juce::jmax (40, n.velocity - 14) });
                        }
                    }
                    pt.notes.insert (pt.notes.end(), extra.begin(), extra.end());
                    if (pt.player == drums && lastChorus != nullptr)
                        for (double b = lastChorus->start + phrase; b < juce::jmin (lastChorus->end, patternEnd) - eps; b += phrase)
                        {
                            bool has = false;
                            for (auto& n : pt.notes) if (n.pitch == crash && std::abs (n.start - b) < 0.05) has = true;
                            if (! has) { pt.notes.push_back ({ crash, b, 1.0, 104 }); pt.notes.push_back ({ kick, b, 0.25, 104 }); }
                        }
                }
            }
            // A build in the last bar before each new section: everyone a little louder towards it
            for (auto& sct : sections)
                if (sct.start > first + bpb - eps && sct.start <= patternEnd + eps)
                    for (auto& pt : parts)
                        for (auto& n : pt.notes)
                            if (n.start >= sct.start - bpb - eps && n.start < sct.start - eps)
                                n.velocity = juce::jmin (127, n.velocity + juce::roundToInt (8.0 * (n.start - (sct.start - bpb)) / bpb));
        }

        // 2. Drum variation: a crash on each new section, an open hi-hat at the end of each phrase, ghost notes
        if (settings.drumVariation)
            if (auto* dr = partFor (drums))
            {
                auto hasNear = [&] (int pitch, double at, double within)
                {
                    for (auto& n : dr->notes) if (n.pitch == pitch && std::abs (n.start - at) < within) return true;
                    return false;
                };
                for (auto& sct : sections)
                    if (sct.start > first + eps && sct.start < patternEnd - eps && ! hasNear (crash, sct.start, 0.05))
                    {
                        dr->notes.push_back ({ crash, sct.start, 1.0, 106 });
                        if (! hasNear (kick, sct.start, 0.05)) dr->notes.push_back ({ kick, sct.start, 0.25, 104 });
                    }
                if (! africanGroove)
                {
                    // Phrase ends: the last off-beat of every 4th bar opens the hi-hat
                    std::vector<double> phraseStarts { gridStart };
                    for (auto& sct : sections) phraseStarts.push_back (sct.start);
                    for (auto ps : phraseStarts)
                        for (double b = ps + phrase; b < patternEnd - eps; b += phrase)
                        {
                            const double at = b - eighth;
                            if (inFill (at) || at < first) continue;
                            bool opened = false;
                            for (auto& n : dr->notes)
                                if (n.pitch == 42 && std::abs (n.start - at) < 0.03) { n.pitch = 46; n.velocity = juce::jmax (n.velocity, 84); opened = true; }
                            if (! opened && ! hasNear (46, at, 0.03))
                                dr->notes.push_back ({ 46, at, eighth, 84 });
                        }
                    // Ghost notes on the snare in full sections (straight 4/4 styles only)
                    if (style.denominator == 4 && spb >= 4)
                        for (double bar = gridStart; bar < patternEnd - eps; bar += bpb)
                            for (double off : { 1.75, 2.25, 3.75 })
                            {
                                const double at = bar + off;
                                if (at >= patternEnd - eps || at < first || inFill (at) || ! isFull (at) || off >= bpb) continue;
                                if (liveRng.nextFloat() < 0.45f && ! hasNear (snare, at, 0.1))
                                    dr->notes.push_back ({ snare, at, 0.1, 30 + liveRng.nextInt (8) });
                            }
                }
            }

        // 3. Answering the singer: the keys, guitar or bass fill the singer's rests, taking turns
        if (settings.answer)
        {
            const auto windows = answerWindows (settings.singing, sections, first, patternEnd, bpb);
            std::vector<int> answerers;
            for (int who : { keys, guitar, keys, bass })
                if (settings.players[who] && partFor (who) != nullptr) answerers.push_back (who);
            int turn = 0;
            const int penta[] = { 0, 2, 4, 7, 9 }, mpenta[] = { 0, 3, 5, 7, 10 };
            auto pentaNotes = [&] (int lo, int hi)
            {
                std::vector<int> out;
                for (int n = lo; n <= hi; ++n)
                {
                    const int rel = pcOf (n - key.tonic);
                    for (int k = 0; k < 5; ++k) if ((key.minor ? mpenta[k] : penta[k]) == rel) out.push_back (n);
                }
                return out;
            };
            for (auto [ws, we] : windows)
            {
                if (answerers.empty()) break;
                if (ws < first || we > patternEnd + eps || inFill (ws) || inFill (we - 0.01)) continue;
                const int who = answerers[(size_t) (turn++ % (int) answerers.size())];
                auto* pt = partFor (who);
                if (pt == nullptr) continue;
                // The player makes room: what it was going to play here gives way to the answer
                pt->notes.erase (std::remove_if (pt->notes.begin(), pt->notes.end(), [&] (const Note& n) { return n.start >= ws - eps && n.start < we - eps; }),
                                 pt->notes.end());
                for (auto& n : pt->notes)
                    if (n.start < ws && n.start + n.length > ws) n.length = juce::jmax (0.05, ws - n.start - 0.02);

                const auto& here = events[(size_t) chordIndexAt (ws)].chord;
                const auto& landing = events[(size_t) chordIndexAt (juce::jmin (we, patternEnd - eps))].chord;
                const auto tones = upperPcs (landing, key);
                auto isTone = [&] (int n) { return std::find (tones.begin(), tones.end(), pcOf (n)) != tones.end(); };
                const bool upwards = (turn % 2) == 0;
                const double len = we - ws;

                if (who == keys || who == guitar)
                {
                    const double step = who == keys ? (style.denominator == 8 ? eighth / 2.0 : 0.25) : eighth;
                    const int count = juce::jlimit (2, who == keys ? 7 : 4, (int) std::floor (len / step + eps));
                    auto scale = who == keys ? pentaNotes (70, 89) : pentaNotes (62, 79);
                    if (scale.size() < 3) continue;
                    // End on a note of the chord, near the middle of the range
                    int endIdx = (int) scale.size() / 2;
                    for (int i = 0; i < (int) scale.size(); ++i)
                        if (isTone (scale[(size_t) i]) && std::abs (i - (int) scale.size() / 2) < std::abs (endIdx - (int) scale.size() / 2)) endIdx = i;
                    if (! isTone (scale[(size_t) endIdx]))
                        for (int i = 0; i < (int) scale.size(); ++i) if (isTone (scale[(size_t) i])) { endIdx = i; break; }
                    for (int i = 0; i < count; ++i)
                    {
                        const int idx = juce::jlimit (0, (int) scale.size() - 1, upwards ? endIdx - (count - 1 - i) : endIdx + (count - 1 - i));
                        const bool last = i == count - 1;
                        Note n { scale[(size_t) idx], ws + i * step, last ? juce::jmin (0.9, we - (ws + i * step) - 0.02) : step * 0.85,
                                 (who == keys ? 70 : 74) + i * 16 / juce::jmax (1, count) };
                        if (n.length > 0.04) pt->notes.push_back (human (n, false));
                    }
                }
                else if (who == bass)
                {
                    // A walk from this chord's root towards the next chord
                    const int from = nearestWithPc ((key.tonic + (here.bass >= 0 ? here.bass : here.root)) % 12, 38, 31, 47);
                    const int to = nearestWithPc ((key.tonic + (landing.bass >= 0 ? landing.bass : landing.root)) % 12, from, 31, 50);
                    const double step = len >= 2.0 - eps ? 1.0 : eighth;
                    const int count = juce::jlimit (2, 4, (int) std::floor (len / step + eps));
                    const int dir = to >= from ? 1 : -1;
                    for (int i = 0; i < count; ++i)
                    {
                        Note n { scaleStep (from, dir * i, key), ws + i * step, step * 0.9, 84 + i * 4 };
                        pt->notes.push_back (human (n, true));
                    }
                }
            }
        }
        sortAll();
        return parts;
    }

    //==========================================================================
    //  Step 34: where the band answers the singer
    //==========================================================================
    std::vector<std::pair<double, double>> answerWindows (const std::vector<std::pair<double, double>>& singing,
                                                          const std::vector<SectionSpan>& sections, double first, double end, double bpb)
    {
        std::vector<std::pair<double, double>> out;
        constexpr double eps = 1.0e-6;
        if (! singing.empty())
        {
            // The singer's rests of a beat or more (an answer lasts at most 2 beats, right after the phrase)
            auto spans = singing;
            std::sort (spans.begin(), spans.end());
            double cursor = juce::jmax (first, spans.front().first);       // nothing before the singer starts
            for (size_t i = 0; i < spans.size(); ++i)
            {
                const auto [s, e] = spans[i];
                if (s - cursor >= 1.0 - eps && cursor >= first - eps)
                {
                    const double ws = std::ceil (cursor * 4.0 - eps) / 4.0;              // on the next 16th
                    const double we = juce::jmin (s - 0.25, ws + 2.0);
                    if (we - ws >= 0.75 - eps && we <= end + eps) out.push_back ({ ws, we });
                }
                cursor = juce::jmax (cursor, e);
            }
            return out;
        }
        // No melody: the end of every 4-bar phrase inside a section (not the section's last bar: that is the fill)
        const double len = juce::jmin (1.5, bpb / 2.0);
        auto addPhrases = [&] (double from, double to)
        {
            for (double b = from + 4.0 * bpb; b < to - eps; b += 4.0 * bpb)
                if (b - len >= first - eps && b <= end + eps) out.push_back ({ b - len, b - 0.1 });
        };
        if (sections.empty())
            addPhrases (std::floor (first / bpb + eps) * bpb, end);
        else
            for (auto& s : sections) addPhrases (s.start, juce::jmin (s.end, end));
        std::sort (out.begin(), out.end());
        return out;
    }

    std::vector<std::pair<double, double>> findSinging (te::Edit& edit)
    {
        std::vector<std::pair<double, double>> out;
        auto& ts = edit.tempoSequence;
        auto isLead = [] (const juce::String& name)
        {
            const auto n = name.toLowerCase();
            for (auto* w : { "lead", "vocal", "voice", "melody", "sing", "solo", "vox" })
                if (n.contains (w)) return true;
            return false;
        };
        for (auto* t : te::getAudioTracks (edit))
        {
            if (! isLead (t->getName()) || t->state.hasProperty ("gpsHarmonyOf") || t->state.hasProperty ("gpsStemOf"))
                continue;
            for (auto* c : t->getClips())
            {
                if (isBandClip (*c)) continue;
                if (auto* mc = dynamic_cast<te::MidiClip*> (c))
                {
                    for (auto* n : mc->getSequence().getNotes())
                    {
                        const auto start = mc->getStartBeat().inBeats() + n->getStartBeat().inBeats() - mc->getOffsetInBeats().inBeats();
                        out.push_back ({ start, start + n->getLengthBeats().inBeats() });
                    }
                }
                else if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c))
                {
                    const auto file = wc->getOriginalFile();
                    if (! file.existsAsFile() || wc->getPosition().getLength().inSeconds() > 900.0) continue;
                    const auto melody = Harmony::findMelody (file);
                    const auto clipStart = wc->getPosition().getStart().inSeconds();
                    const auto offset = wc->getPosition().getOffset().inSeconds();
                    const auto clipEnd = wc->getPosition().getEnd().inSeconds();
                    for (auto& n : melody.notes)
                    {
                        const auto s = clipStart + (n.start - offset), e = clipStart + (n.end - offset);
                        if (e <= clipStart || s >= clipEnd) continue;
                        out.push_back ({ ts.toBeats (te::TimePosition::fromSeconds (juce::jmax (clipStart, s))).inBeats(),
                                         ts.toBeats (te::TimePosition::fromSeconds (juce::jmin (clipEnd, e))).inBeats() });
                    }
                }
            }
        }
        std::sort (out.begin(), out.end());
        return out;
    }

    //==========================================================================
    //  The song
    //==========================================================================
    std::vector<SectionSpan> getSections (te::Edit& edit)
    {
        std::vector<SectionSpan> out;
        if (auto* arranger = edit.getArrangerTrack())
            for (auto* c : arranger->getClips())
            {
                const auto pos = c->getPosition();
                out.push_back ({ edit.tempoSequence.toBeats (pos.getStart()).inBeats(), edit.tempoSequence.toBeats (pos.getEnd()).inBeats(), c->getName() });
            }
        std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.start < b.start; });
        return out;
    }

    te::AudioTrack* findTrackFor (te::Edit& edit, int player)
    {
        te::AudioTrack* byName = nullptr;
        for (auto* t : Mixing::getSongTracks (edit))
        {
            if (! StudioEngine::isInstrumentTrack (*t)) continue;
            const auto id = StudioEngine::getInstrumentId (*t);
            const auto name = t->getName().toLowerCase();
            bool exact = false, named = false;
            switch (player)
            {
                case bass:   exact = id.startsWith ("bass");   named = name.contains ("bass");  break;
                case keys:   exact = name == "keys" || name.contains ("piano");
                             named = id == "grand" || id == "grandFull" || id == "upright" || id == "studioKeys"; break;
                case organ:  exact = id == "organ";            named = name.contains ("organ"); break;
                case guitar: exact = id.startsWith ("guitar"); named = name.contains ("guitar"); break;
                case drums:  exact = id == "drums";            named = name.contains ("drum");  break;
                default: break;
            }
            if (exact) return t;
            if (named && byName == nullptr) byName = t;
        }
        return byName;
    }

    juce::String instrumentFor (int player, const Style& style)
    {
        switch (player)
        {
            case bass:   return "bassFinger";
            case keys:   return "grand";
            case organ:  return "organ";
            case guitar: return style.guitarSound == 1 ? "guitarNylon" : "guitar";
            default:     return "drums";
        }
    }

    bool isBandClip (const te::Clip& c)      { return c.state.hasProperty (bandId); }

    int remove (te::Edit& edit)
    {
        int n = 0;
        for (auto* t : te::getAudioTracks (edit))
        {
            const auto clips = t->getClips();
            for (auto* c : clips)
                if (isBandClip (*c)) { c->removeFromParent(); ++n; }
        }
        return n;
    }

    namespace
    {
        te::AudioTrack* makeOwnTrack (te::Edit& edit, const juce::String& name, juce::Colour colour, const juce::String& instrumentId)
        {
            auto after = Mixing::getSongTracks (edit).getLast();
            auto t = edit.insertNewAudioTrack (after != nullptr ? te::TrackInsertPoint (*after, false) : te::TrackInsertPoint::getEndOfTracks (edit), nullptr, true);
            if (t == nullptr) return nullptr;
            t->setName (name);
            t->setColour (colour);
            const char* type = instrumentId == "organ" ? OrganPlugin::xmlTypeName : SamplerPlugin::xmlTypeName;
            if (auto p = edit.getPluginCache().createNewPlugin (type, {}))
            {
                if (auto* s = dynamic_cast<SamplerPlugin*> (p.get())) s->instrumentId = instrumentId;
                t->pluginList.insertPlugin (p, 0, nullptr);
            }
            return t.get();
        }
    }

    juce::String apply (te::Edit& edit, const Theory::Key& key, double bpb, const Settings& settings, TrackMaker makeTrack)
    {
        const auto events = ChordTrack::getEvents (edit);
        if (events.size() < 1)
            return "The Chord Track is empty. Add chords first (or a progression from the palette).";
        const auto& style = styles()[(size_t) juce::jlimit (0, (int) styles().size() - 1, settings.style)];

        auto withSinger = settings;
        if (settings.answer && withSinger.singing.empty())
            withSinger.singing = findSinging (edit);              // the band answers where the singer rests
        const auto parts = arrange (events, getSections (edit), key, bpb, withSinger);
        auto& ts = edit.tempoSequence;
        auto* um = &edit.getUndoManager();
        const double first = std::floor (events.front().start / bpb + 1.0e-6) * bpb;
        const double last = events.back().start + events.back().length;

        static const juce::uint32 colours[] = { 0xff5b8def, 0xffd4a843, 0xffc9774a, 0xff8fc96b, 0xffe07a4f };
        juce::StringArray made, created;
        int ownClips = 0;
        for (auto& part : parts)
        {
            auto* track = findTrackFor (edit, part.player);
            if (track == nullptr)
            {
                const auto name = part.player == keys ? juce::String ("Piano") : playerName (part.player);
                const auto inst = instrumentFor (part.player, style);
                track = makeTrack ? makeTrack (part.player, name, juce::Colour (colours[part.player]), inst)
                                  : makeOwnTrack (edit, name, juce::Colour (colours[part.player]), inst);
                if (track == nullptr) continue;
                created.add (track->getName());
            }

            // Replace the band (and "Chords to notes") clips here; count the user's own clips in the way
            const auto clips = track->getClips();
            for (auto* c : clips)
            {
                if (isBandClip (*c) || c->getName() == "Chords (from Chord Track)")
                    c->removeFromParent();
                else
                {
                    const auto b0 = ts.toBeats (c->getPosition().getStart()).inBeats(), b1 = ts.toBeats (c->getPosition().getEnd()).inBeats();
                    if (b1 > first && b0 < last) ++ownClips;
                }
            }

            auto clip = track->insertMIDIClip ("Band: " + juce::String (style.name),
                                               te::TimeRange (ts.toTime (te::BeatPosition::fromBeats (first)), ts.toTime (te::BeatPosition::fromBeats (last + 0.01))), nullptr);
            if (clip == nullptr) continue;
            clip->state.setProperty (bandId, style.name, um);
            auto& seq = clip->getSequence();
            for (auto& n : part.notes)
                seq.addNote (n.pitch, te::BeatPosition::fromBeats (juce::jmax (0.0, n.start - first)),
                             te::BeatDuration::fromBeats (juce::jmax (0.03, n.length)), n.velocity, 0, um);
            made.add (playerName (part.player) + " on " + track->getName());
        }

        if (made.isEmpty())
            return "Choose at least one player.";
        juce::String report = "The band plays " + juce::String (style.name) + ": " + made.joinIntoString (", ") + ".";
        if (! created.isEmpty())
            report << " New tracks: " << created.joinIntoString (", ") << ".";
        if (ownClips > 0)
            report << " " << ownClips << " of your own clips are on those tracks at the same time: mute or delete them if they clash.";
        return report;
    }

    //==========================================================================
    //  The panel
    //==========================================================================
    namespace
    {
        class Panel final : public juce::Component
        {
        public:
            Panel (te::Edit& e, std::function<Theory::Key()> gk, std::function<double()> gb, TrackMaker tm, std::function<void()> changed)
                : edit (&e), editRef (e), getKey (std::move (gk)), getBpb (std::move (gb)), makeTrack (std::move (tm)), onChanged (std::move (changed))
            {
                using namespace StudioColours;
                title.setText ("Backing band", juce::dontSendNotification);
                title.setFont (studioFont (17.0f, true));
                title.setColour (juce::Label::textColourId, text);
                addAndMakeVisible (title);
                for (auto* l : { &info, &feel, &summary })
                {
                    l->setColour (juce::Label::textColourId, textDim);
                    l->setFont (studioFont (12.5f));
                    l->setJustificationType (juce::Justification::topLeft);
                    addAndMakeVisible (*l);
                }

                int num = 4, den = 4;
                if (auto* sig = e.tempoSequence.getTimeSig (0)) { num = sig->numerator; den = sig->denominator; }
                const auto saved = e.state.getProperty ("gpsBandStyle", "").toString();
                int firstFit = 0;
                for (size_t i = 0; i < styles().size(); ++i)
                {
                    const bool ok = fits (styles()[i], num, den);
                    styleBox.addItem (juce::String (styles()[i].name) + (ok ? "" : "   (for " + juce::String (styles()[i].numerator) + "/" + juce::String (styles()[i].denominator) + ")"), (int) i + 1);
                    styleBox.setItemEnabled ((int) i + 1, ok);
                    if (ok && firstFit == 0) firstFit = (int) i + 1;
                    if (ok && saved == styles()[i].name) styleBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
                }
                if (styleBox.getSelectedId() == 0) styleBox.setSelectedId (firstFit, juce::dontSendNotification);
                styleBox.setComponentID ("band.style");
                styleBox.onChange = [this] { update(); };
                addAndMakeVisible (styleBox);

                const auto savedPlayers = e.state.getProperty ("gpsBandPlayers", "11111").toString();
                for (int p = 0; p < numPlayers; ++p)
                {
                    auto t = std::make_unique<juce::ToggleButton> (playerName (p));
                    t->setToggleState (savedPlayers.length() > p ? savedPlayers[p] == '1' : true, juce::dontSendNotification);
                    t->onClick = [this] { update(); };
                    addAndMakeVisible (*t);
                    toggles.push_back (std::move (t));
                }
                for (auto* t : { &sectionsToggle, &fillsToggle, &endingToggle, &answerToggle, &drumToggle, &growthToggle })
                {
                    t->setToggleState (true, juce::dontSendNotification);
                    addAndMakeVisible (*t);
                }
                answerToggle.setTooltip ("Where the singer rests (a track named Lead, Vocal or Melody), the piano, guitar or bass plays a short answer. Without a vocal yet: at the end of each 4-bar phrase");
                drumToggle.setTooltip ("A crash on each new section, an open hi-hat at the end of each phrase, soft ghost notes on the snare in the fuller sections");
                growthToggle.setTooltip ("The first verse is sparer (no guitar or organ yet), the last chorus lifts (louder, piano an octave up, crashes), and the band builds into each new section");
                sectionsToggle.setTooltip ("Verses, intro and outro are lighter; choruses, bridges and vamps are fuller (from the Sections lane)");
                fillsToggle.setTooltip ("In the bar before each new section: bass walk-up, piano run and a drum fill, then a crash");
                endingToggle.setTooltip ("On the last chord everyone plays together and holds it");

                humanise.setSliderStyle (juce::Slider::LinearHorizontal);
                humanise.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 22);
                humanise.setRange (0, 100, 1);
                humanise.setValue (50, juce::dontSendNotification);
                humanise.setTextValueSuffix (" %");
                humanise.setTooltip ("0 = exactly on the grid. Higher = small timing and strength differences, like real players");
                addAndMakeVisible (humanise);

                makeButton.onClick = [this] { make(); };
                removeButton.onClick = [this]
                {
                    if (editRef.get() == nullptr) return;
                    edit->getUndoManager().beginNewTransaction ("Remove the band");
                    const auto n = remove (*edit);
                    summary.setText (n > 0 ? "The band's clips were removed (Ctrl+Z brings them back)." : "There was no band to remove.", juce::dontSendNotification);
                    if (onChanged) onChanged();
                    update();
                };
                addAndMakeVisible (makeButton);
                addAndMakeVisible (removeButton);
                setSize (560, 456);
                update();
            }

            Settings current() const
            {
                Settings s;
                s.style = juce::jmax (0, styleBox.getSelectedId() - 1);
                for (int p = 0; p < numPlayers; ++p) s.players[p] = toggles[(size_t) p]->getToggleState();
                s.followSections = sectionsToggle.getToggleState();
                s.fills = fillsToggle.getToggleState();
                s.ending = endingToggle.getToggleState();
                s.answer = answerToggle.getToggleState();
                s.drumVariation = drumToggle.getToggleState();
                s.growth = growthToggle.getToggleState();
                s.humanise = (float) humanise.getValue() / 100.0f;
                return s;
            }

            void update()
            {
                if (editRef.get() == nullptr) return;
                const auto s = current();
                feel.setText (styles()[(size_t) s.style].feel, juce::dontSendNotification);

                const auto events = ChordTrack::getEvents (*edit);
                const auto sections = getSections (*edit);
                const auto key = getKey ? getKey() : Theory::Key();
                juce::String t;
                if (events.empty())
                    t << "The Chord Track is empty: add chords first, the band plays them.";
                else
                {
                    const auto bpb = getBpb ? getBpb() : 4.0;
                    t << "Plays your " << (int) events.size() << " chords in " << Theory::noteName (key.tonic, key.flats) << (key.minor ? " minor" : " major")
                      << ", bars " << (int) (events.front().start / bpb) + 1 << " to " << (int) std::ceil ((events.back().start + events.back().length) / bpb) << ". ";
                    if (sections.empty()) t << "No sections: add Verse/Chorus in the Sections lane to get lighter verses and fills.";
                    else
                    {
                        juce::StringArray names;
                        for (auto& sec : sections) names.add (sec.name);
                        t << "Sections: " << names.joinIntoString (", ") << ".";
                    }
                }
                info.setText (t, juce::dontSendNotification);

                for (int p = 0; p < numPlayers; ++p)
                {
                    auto* track = findTrackFor (*edit, p);
                    toggles[(size_t) p]->setButtonText (playerName (p) + (track != nullptr ? "  > " + track->getName() : juce::String ("  > new track")));
                }
                makeButton.setEnabled (! events.empty());
                bool any = false;
                for (auto* tr : te::getAudioTracks (*edit))
                    for (auto* c : tr->getClips()) any = any || isBandClip (*c);
                removeButton.setEnabled (any);
            }

            void make()
            {
                if (editRef.get() == nullptr) return;
                const auto s = current();
                juce::String players;
                for (int p = 0; p < numPlayers; ++p) players << (s.players[p] ? "1" : "0");
                edit->state.setProperty ("gpsBandStyle", styles()[(size_t) s.style].name, nullptr);
                edit->state.setProperty ("gpsBandPlayers", players, nullptr);
                edit->getUndoManager().beginNewTransaction ("Backing band");
                const auto report = apply (*edit, getKey ? getKey() : Theory::Key(), getBpb ? getBpb() : 4.0, s, makeTrack);
                summary.setText (report + "  Press Play.", juce::dontSendNotification);
                if (onChanged) onChanged();
                update();
            }

            void paint (juce::Graphics& g) override
            {
                using namespace StudioColours;
                g.fillAll (card);
                g.setColour (textDim);
                g.setFont (studioFont (12.0f));
                for (auto& [r, label] : labels)
                    g.drawText (label, r, juce::Justification::centredLeft);
            }

            void resized() override
            {
                labels.clear();
                auto r = getLocalBounds().reduced (14, 10);
                title.setBounds (r.removeFromTop (26));
                info.setBounds (r.removeFromTop (36));
                r.removeFromTop (4);
                auto row = [&] (const juce::String& label, juce::Component& c, int h = 26)
                {
                    auto line = r.removeFromTop (h);
                    labels.push_back ({ line.removeFromLeft (90), label });
                    c.setBounds (line);
                    r.removeFromTop (4);
                };
                row ("Style", styleBox);
                feel.setBounds (r.removeFromTop (32).withTrimmedLeft (90));
                {
                    auto line = r.removeFromTop (2 * 24);
                    labels.push_back ({ line.removeFromLeft (90).removeFromTop (24), "Players" });
                    const int w = line.getWidth() / 3;
                    for (int p = 0; p < numPlayers; ++p)
                        toggles[(size_t) p]->setBounds (line.getX() + (p % 3) * w, line.getY() + (p / 3) * 24, w, 24);
                }
                r.removeFromTop (4);
                sectionsToggle.setBounds (r.removeFromTop (24));
                fillsToggle.setBounds (r.removeFromTop (24));
                endingToggle.setBounds (r.removeFromTop (24));
                answerToggle.setBounds (r.removeFromTop (24));
                drumToggle.setBounds (r.removeFromTop (24));
                growthToggle.setBounds (r.removeFromTop (24));
                r.removeFromTop (4);
                row ("Humanise", humanise);
                auto buttons = r.removeFromBottom (30);
                makeButton.setBounds (buttons.removeFromRight (150));
                buttons.removeFromRight (8);
                removeButton.setBounds (buttons.removeFromRight (150));
                r.removeFromBottom (6);
                summary.setBounds (r);
            }

        private:
            te::Edit* edit;
            te::SafeSelectable<te::Edit> editRef;
            std::function<Theory::Key()> getKey;
            std::function<double()> getBpb;
            TrackMaker makeTrack;
            std::function<void()> onChanged;
            juce::Label title, info, feel, summary;
            juce::ComboBox styleBox;
            std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
            juce::ToggleButton sectionsToggle { "Follow the sections (lighter verses, fuller choruses)" },
                               fillsToggle { "Fills before each new section" },
                               endingToggle { "Ending: everyone hits the last chord" },
                               answerToggle { "Answer the singer (short runs where the singer rests)" },
                               drumToggle { "Drum variety (crash, open hi-hat, ghost notes)" },
                               growthToggle { "The song grows (sparer first verse, lifted last chorus)" };
            juce::Slider humanise;
            juce::TextButton makeButton { "Make the band" }, removeButton { "Remove the band" };
            std::vector<std::pair<juce::Rectangle<int>, juce::String>> labels;
        };
    }

    std::unique_ptr<juce::Component> createPanel (te::Edit& edit, std::function<Theory::Key()> getKey, std::function<double()> getBpb,
                                                  TrackMaker makeTrack, std::function<void()> onChanged)
    {
        return std::make_unique<Panel> (edit, std::move (getKey), std::move (getBpb), std::move (makeTrack), std::move (onChanged));
    }
}
