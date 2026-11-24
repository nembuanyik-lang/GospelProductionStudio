#include "Theory.h"

namespace Theory
{
//==============================================================================
//  Vocabulary
//==============================================================================
const std::vector<Quality>& qualities()
{
    static const std::vector<Quality> q {
        { "",        "",         "Triads",       { 0, 4, 7 } },
        { "m",       "m",        "Triads",       { 0, 3, 7 } },
        { "dim",     "\xc2\xb0", "Triads",       { 0, 3, 6 } },
        { "aug",     "+",        "Triads",       { 0, 4, 8 } },
        { "sus2",    "sus2",     "Triads",       { 0, 2, 7 } },
        { "sus4",    "sus4",     "Triads",       { 0, 5, 7 } },

        { "add9",    "add9",     "Added tones",  { 0, 4, 7, 14 } },
        { "madd9",   "m(add9)",  "Added tones",  { 0, 3, 7, 14 } },
        { "6",       "6",        "Added tones",  { 0, 4, 7, 9 } },
        { "m6",      "m6",       "Added tones",  { 0, 3, 7, 9 } },
        { "69",      "6/9",      "Added tones",  { 0, 4, 7, 9, 14 } },
        { "m69",     "m6/9",     "Added tones",  { 0, 3, 7, 9, 14 } },

        { "maj7",    "maj7",     "Major 7ths",   { 0, 4, 7, 11 } },
        { "maj9",    "maj9",     "Major 7ths",   { 0, 4, 7, 11, 14 } },
        { "maj13",   "maj13",    "Major 7ths",   { 0, 4, 11, 14, 21 } },
        { "maj7#11", "maj7#11",  "Major 7ths",   { 0, 4, 11, 14, 18 } },

        { "m7",      "m7",       "Minor 7ths",   { 0, 3, 7, 10 } },
        { "m9",      "m9",       "Minor 7ths",   { 0, 3, 7, 10, 14 } },
        { "m11",     "m11",      "Minor 7ths",   { 0, 3, 10, 14, 17 } },
        { "mmaj7",   "m(maj7)",  "Minor 7ths",   { 0, 3, 7, 11 } },

        { "7",       "7",        "Dominants",    { 0, 4, 7, 10 } },
        { "9",       "9",        "Dominants",    { 0, 4, 10, 14 } },
        { "13",      "13",       "Dominants",    { 0, 4, 10, 14, 21 } },
        { "7sus4",   "7sus4",    "Dominants",    { 0, 5, 7, 10 } },
        { "9sus4",   "9sus4",    "Dominants",    { 0, 5, 10, 14 } },
        { "13sus",   "13sus",    "Dominants",    { 0, 5, 10, 14, 21 } },

        { "7b9",     "7(b9)",    "Tension",      { 0, 4, 10, 13 } },
        { "7#9",     "7(#9)",    "Tension",      { 0, 4, 10, 15 } },
        { "7#5",     "7(#5)",    "Tension",      { 0, 4, 8, 10 } },
        { "13b9",    "13(b9)",   "Tension",      { 0, 4, 10, 13, 21 } },
        { "7alt",    "7alt",     "Tension",      { 0, 4, 10, 13, 15, 20 } },

        { "dim7",    "\xc2\xb0" "7", "Diminished", { 0, 3, 6, 9 } },
        { "m7b5",    "\xc3\xb8" "7", "Diminished", { 0, 3, 6, 10 } },
    };
    return q;
}

const Quality& findQuality (const juce::String& id)
{
    for (const auto& q : qualities())
        if (id == q.id)
            return q;
    return qualities().front();
}

static bool isMinorQuality (const juce::String& q)    { return q == "m" || q.startsWith ("m7") || q == "m9" || q == "m11" || q == "m6" || q == "m69" || q == "madd9" || q == "mmaj7"; }
static bool isDominantQuality (const juce::String& q) { return q == "7" || q == "9" || q == "13" || q.startsWith ("7") || q == "13b9" || q == "13sus" || q == "9sus4"; }

//==============================================================================
//  Labels
//==============================================================================
juce::String noteName (int pc, bool flats)
{
    static const char* flatNames[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    static const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    pc = ((pc % 12) + 12) % 12;
    return flats ? flatNames[pc] : sharpNames[pc];
}

Key keyFromSong (int pitchClass, bool minor, const juce::String& keyName)
{
    Key k;
    k.tonic = pitchClass;
    k.minor = minor;

    const auto root = keyName.upToFirstOccurrenceOf (" ", false, false);
    if (root.containsChar ('#'))      k.flats = false;
    else if (root.containsChar ('b')) k.flats = true;
    else
    {
        // Natural keys: sharp keys are G, D, A, E, B (major) and their relative minors.
        const auto majorPc = minor ? (pitchClass + 3) % 12 : pitchClass;
        k.flats = ! (majorPc == 7 || majorPc == 2 || majorPc == 9 || majorPc == 4 || majorPc == 11);
    }
    return k;
}

juce::String degreeLabel (int semitones, bool sharp)
{
    static const char* flatDegrees[]  = { "1", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7" };
    static const char* sharpDegrees[] = { "1", "#1", "2", "#2", "3", "4", "#4", "5", "#5", "6", "#6", "7" };
    semitones = ((semitones % 12) + 12) % 12;
    return juce::String::fromUTF8 (sharp ? sharpDegrees[semitones] : flatDegrees[semitones]);
}

juce::String numberLabel (const Chord& c)
{
    auto s = degreeLabel (c.root, c.sharp) + juce::String::fromUTF8 (findQuality (c.quality).suffix);
    if (c.bass >= 0)
        s << "/" << degreeLabel (c.bass, false);
    return s;
}

juce::String nameLabel (const Chord& c, const Key& k)
{
    const auto flats = c.sharp ? false : k.flats;
    auto s = noteName (k.tonic + c.root, flats) + juce::String::fromUTF8 (findQuality (c.quality).suffix);
    if (c.bass >= 0)
        s << "/" << noteName (k.tonic + c.bass, k.flats);
    return s;
}

juce::String label (const Chord& c, const Key& k, Display d)
{
    switch (d)
    {
        case Display::numbers: return numberLabel (c);
        case Display::names:   return nameLabel (c, k);
        case Display::both:    break;
    }
    return numberLabel (c) + juce::String::fromUTF8 (" \xc2\xb7 ") + nameLabel (c, k);
}

//==============================================================================
//  Voicings
//==============================================================================
juce::StringArray voicingNames()   { return { "Close", "Spread", "Gospel (rootless)", "Shell", "Fourths", "Upper structure", "Cluster" }; }
juce::StringArray leftHandNames()  { return { "Root", "Root + 5th", "Octave" }; }

static std::vector<int> rightHandTones (const Chord& c, Voicing v)
{
    const auto& q = findQuality (c.quality);
    std::vector<int> iv = q.intervals;

    auto has = [&iv] (int x) { return std::find (iv.begin(), iv.end(), x) != iv.end(); };
    const int third   = has (4) ? 4 : has (3) ? 3 : has (5) ? 5 : has (2) ? 2 : 4;
    const int seventh = has (11) ? 11 : has (10) ? 10 : has (9) && juce::String (q.id) == "dim7" ? 9 : -1;
    const bool dominant = isDominantQuality (q.id);
    const bool altered  = has (13) || has (15) || has (8) || has (20);

    std::vector<int> t;

    switch (v)
    {
        case Voicing::close:
        case Voicing::spread:
            for (auto x : iv)
                if (x != 0 || iv.size() <= 3)
                    t.push_back (x % 12);
            break;

        case Voicing::gospel:
            t.push_back (third);
            if (seventh >= 0) t.push_back (seventh);
            else if (has (9)) t.push_back (9);
            else t.push_back (7);
            if (has (13)) t.push_back (13 - 12 + 12);          // b9
            else if (has (15)) t.push_back (15);               // #9
            else t.push_back (14);                             // 9
            if (dominant && ! altered && third != 5) t.push_back (21);   // 13
            else if (has (21)) t.push_back (21);
            else if (has (17)) t.push_back (17);
            break;

        case Voicing::shell:
            t = { third, seventh >= 0 ? seventh : 7 };
            break;

        case Voicing::fourths:
            t = { third, third + 5, third + 10 };
            break;

        case Voicing::upperStructure:
            if (dominant)                     t = { third, 10, 14, 18, 21 };
            else if (isMinorQuality (q.id))   t = { 3, 10, 14, 17 };
            else if (juce::String (q.id) == "dim7" || juce::String (q.id) == "m7b5") t = { 3, 6, seventh >= 0 ? seventh : 9, 14 };
            else                              t = { 4, 11, 14, 18 };
            break;

        case Voicing::cluster:
            t = { 2, third, 7 };
            if (seventh >= 0) t.push_back (seventh);
            break;
    }

    std::sort (t.begin(), t.end());
    t.erase (std::unique (t.begin(), t.end()), t.end());
    return t;
}

static std::vector<int> stack (int rootPc, const std::vector<int>& tones, int start)
{
    std::vector<int> notes;
    int previous = start - 1;
    for (auto t : tones)
    {
        const auto pc = (rootPc + t) % 12;
        int p = previous + 1;
        while (((p % 12) + 12) % 12 != pc) ++p;
        notes.push_back (p);
        previous = p;
    }
    return notes;
}

int rightHandCentre (const std::vector<int>& notes)
{
    if (notes.size() < 2) return 0;
    int sum = 0, n = 0;
    for (size_t i = 1; i < notes.size(); ++i)       // skip the lowest (bass) note
        if (notes[i] >= 52) { sum += notes[i]; ++n; }
    return n > 0 ? sum / n : 0;
}

std::vector<int> voice (const Chord& c, const Key& k, Voicing v, LeftHand lh, int previousCentre)
{
    const int rootPc = (k.tonic + c.root) % 12;
    const int bassPc = (k.tonic + (c.bass >= 0 ? c.bass : c.root)) % 12;

    // Left hand around E2..D#3
    int bass = 36 + bassPc;
    if (bass < 40) bass += 12;

    std::vector<int> left { bass };
    if (lh == LeftHand::rootFifth && c.bass < 0) left.push_back (bass + 7);
    if (lh == LeftHand::octave) left.push_back (bass + 12);

    // Right hand: try every inversion, keep the one nearest the target centre.
    auto tones = rightHandTones (c, v);
    const int target = previousCentre > 0 ? previousCentre : 65;
    std::vector<int> best;
    int bestDistance = 1 << 20;

    for (size_t r = 0; r < tones.size(); ++r)
    {
        std::vector<int> rotated (tones.begin() + (long) r, tones.end());
        rotated.insert (rotated.end(), tones.begin(), tones.begin() + (long) r);

        for (int start = target - 14; start <= target - 2; start += 12)
        {
            auto notes = stack (rootPc, rotated, start);
            if (v == Voicing::spread && notes.size() >= 3)
            {
                notes[notes.size() - 2] -= 12;         // drop-2 spread
                std::sort (notes.begin(), notes.end());
            }

            int sum = 0;
            for (auto n : notes) sum += n;
            const auto centre = notes.empty() ? target : sum / (int) notes.size();
            const auto d = std::abs (centre - target);
            if (d < bestDistance) { bestDistance = d; best = notes; }
        }
    }

    const auto leftTop = *std::max_element (left.begin(), left.end());
    while (! best.empty() && best.front() <= leftTop + 2)
        for (auto& n : best) n += 12;

    left.insert (left.end(), best.begin(), best.end());
    return left;
}

int bassNote (const Chord& c, const Key& k)
{
    int n = 24 + (k.tonic + (c.bass >= 0 ? c.bass : c.root)) % 12;
    if (n < 28) n += 12;
    return n;
}

//==============================================================================
//  Palette
//==============================================================================
static Chord ch (int root, const char* q, int bass = -1, bool sharp = false)
{
    Chord c;
    c.root = root; c.quality = q; c.bass = bass; c.sharp = sharp;
    return c;
}

std::vector<PaletteGroup> paletteFor (const Key& k)
{
    std::vector<PaletteGroup> g;

    if (! k.minor)
    {
        g.push_back ({ "In the key", { { ch (0, ""), "Home" }, { ch (2, "m"), "" }, { ch (4, "m"), "" }, { ch (5, ""), "" },
                                       { ch (7, ""), "" }, { ch (9, "m"), "" }, { ch (11, "dim"), "" } } });
        g.push_back ({ "Sevenths", { { ch (0, "maj7"), "" }, { ch (2, "m7"), "" }, { ch (4, "m7"), "" }, { ch (5, "maj7"), "" },
                                     { ch (7, "7"), "" }, { ch (9, "m7"), "" }, { ch (11, "m7b5"), "" } } });
        g.push_back ({ "Gospel favourites", { { ch (0, "", 4), "1 over 3" }, { ch (5, "", 7), "the gospel sus" },
                                              { ch (2, "m7", 7), "2 over 5" }, { ch (7, "13sus"), "" }, { ch (0, "add9"), "" },
                                              { ch (5, "maj9"), "" }, { ch (9, "m9"), "" }, { ch (0, "69"), "" } } });
    }
    else
    {
        g.push_back ({ "In the key", { { ch (0, "m"), "Home" }, { ch (2, "dim"), "" }, { ch (3, ""), "" }, { ch (5, "m"), "" },
                                       { ch (7, "m"), "" }, { ch (8, ""), "" }, { ch (10, ""), "" } } });
        g.push_back ({ "Sevenths", { { ch (0, "m7"), "" }, { ch (2, "m7b5"), "" }, { ch (3, "maj7"), "" }, { ch (5, "m7"), "" },
                                     { ch (7, "7"), "harmonic" }, { ch (8, "maj7"), "" }, { ch (10, "7"), "" } } });
        g.push_back ({ "Gospel favourites", { { ch (0, "m9"), "" }, { ch (5, "m9"), "" }, { ch (8, "maj7"), "" },
                                              { ch (7, "7b9"), "" }, { ch (10, "13sus"), "" }, { ch (0, "m", 10), "1m over b7" },
                                              { ch (5, "7"), "dorian 4" }, { ch (0, "mmaj7"), "" } } });
    }

    g.push_back ({ "Passing diminished", { { ch (1, "dim7", -1, true), "1 to 2" }, { ch (3, "dim7", -1, true), "2 to 3" },
                                           { ch (6, "dim7", -1, true), "4 to 5" }, { ch (8, "dim7", -1, true), "5 to 6" },
                                           { ch (3, "dim7"), "3 down to 2" } } });

    g.push_back ({ "Secondary dominants", { { ch (0, "7"), "to 4" }, { ch (2, "7"), "to 5" }, { ch (4, "7"), "to 6m" },
                                            { ch (9, "7"), "to 2m" }, { ch (11, "7"), "to 3m" }, { ch (4, "7b9"), "to 6m" },
                                            { ch (9, "7b9"), "to 2m" } } });

    g.push_back ({ "Tritone substitutions", { { ch (1, "7"), "to 1" }, { ch (3, "7"), "to 2m" }, { ch (8, "7"), "to 5" },
                                              { ch (10, "7"), "to 6m" }, { ch (1, "maj7"), "Neapolitan" } } });

    g.push_back ({ "Borrowed and backdoor", { { ch (5, "m6"), "minor 4" }, { ch (5, "m7"), "" }, { ch (10, "9"), "backdoor" },
                                              { ch (8, "maj7"), "" }, { ch (3, "maj7"), "" }, { ch (10, "") , "" },
                                              { ch (8, ""), "" } } });

    g.push_back ({ "Tension and endings", { { ch (7, "7b9"), "" }, { ch (7, "7#9"), "" }, { ch (7, "7alt"), "" },
                                            { ch (7, "13b9"), "" }, { ch (7, "7#5"), "" }, { ch (0, "maj7#11"), "" },
                                            { ch (0, "maj13"), "" } } });
    return g;
}

std::vector<Chord> suggestionsAfter (const Chord& c, const Key& k)
{
    std::vector<Chord> s;
    auto add = [&s] (Chord x) { if (std::find (s.begin(), s.end(), x) == s.end() && s.size() < 7) s.push_back (x); };

    // Dominant chords want to resolve a fourth up.
    if (isDominantQuality (c.quality) && c.root != 7)
    {
        const auto target = (c.root + 5) % 12;
        const bool minorTarget = ! k.minor ? (target == 2 || target == 4 || target == 9) : (target == 0 || target == 5 || target == 7);
        add (ch (target, minorTarget ? "m7" : ""));
    }

    if (! k.minor)
    {
        switch (c.root)
        {
            case 0:  add (ch (5, "")); add (ch (9, "m7")); add (ch (2, "m7")); add (ch (0, "", 4)); add (ch (0, "7")); add (ch (7, "", 11)); add (ch (4, "m7")); break;
            case 1:  add (ch (2, "m7")); add (ch (0, "")); break;
            case 2:  add (ch (7, "7")); add (ch (7, "13sus")); add (ch (2, "m7", 7)); add (ch (1, "7")); add (ch (4, "m7")); add (ch (3, "dim7", -1, true)); break;
            case 3:  add (ch (4, "m7")); add (ch (2, "m7")); add (ch (5, "")); break;
            case 4:  add (ch (9, "m7")); add (ch (9, "7")); add (ch (5, "")); add (ch (3, "dim7")); add (ch (5, "maj7")); break;
            case 5:  add (ch (7, "")); add (ch (0, "", 7)); add (ch (6, "dim7", -1, true)); add (ch (5, "m6")); add (ch (4, "m7")); add (ch (2, "m7")); add (ch (5, "", 7)); break;
            case 6:  add (ch (0, "", 7)); add (ch (7, "")); break;
            case 7:  add (ch (0, "")); add (ch (9, "m")); add (ch (7, "7b9")); add (ch (5, "", 7)); add (ch (0, "", 4)); add (ch (8, "")); break;
            case 8:  add (ch (9, "m")); add (ch (10, "")); add (ch (7, "")); break;
            case 9:  add (ch (2, "m7")); add (ch (5, "")); add (ch (2, "7")); add (ch (7, "")); add (ch (4, "m7")); break;
            case 10: add (ch (0, "")); add (ch (5, "")); break;
            case 11: add (ch (4, "7")); add (ch (0, "")); add (ch (4, "m7")); break;
            default: break;
        }
    }
    else
    {
        switch (c.root)
        {
            case 0:  add (ch (5, "m7")); add (ch (8, "")); add (ch (10, "")); add (ch (7, "7")); add (ch (0, "m", 10)); break;
            case 2:  add (ch (7, "7b9")); break;
            case 3:  add (ch (8, "")); add (ch (5, "m7")); break;
            case 5:  add (ch (7, "7")); add (ch (10, "")); add (ch (0, "m")); break;
            case 7:  add (ch (0, "m")); add (ch (8, "")); break;
            case 8:  add (ch (10, "")); add (ch (5, "m7")); add (ch (7, "7")); break;
            case 10: add (ch (3, "")); add (ch (0, "m")); add (ch (8, "")); break;
            default: add (ch (0, "m")); break;
        }
    }
    return s;
}

//==============================================================================
//  Progressions
//==============================================================================
juce::StringArray levelNames()   { return { "Simple", "Rich", "Advanced gospel" }; }

const std::vector<Progression>& library()
{
    static const std::vector<Progression> p {
        // Foundations
        { "Foundations", "Classic 1-4-5-1",            "1 4 5 1", "Any" },
        { "Foundations", "1-6-4-5",                    "1 6 4 5", "Any" },
        { "Foundations", "2-5-1",                      "2.m7 5.7 1.maj7@8", "Any" },
        { "Foundations", "6-2-5-1",                    "6.m7 2.m7 5.7 1.maj7", "Any" },
        { "Foundations", "Amen 1-4-1",                 "1 4 1@8", "Any" },
        { "Foundations", "Circle of fifths",           "1.maj7 4.maj7 7.m7b5 3.7 6.m7 2.m7 5.7 1.maj7", "Any" },
        { "Foundations", "Step climb 1-2-3-4",         "1 2.m7 3.m7 4", "Any" },

        // Worship ballads
        { "Worship ballads", "Ballad climb",           "1 1/3 4 5.sus4@2 5@2", "Ballad" },
        { "Worship ballads", "Gospel ballad 4-5/4-3-6","4.maj7 5/4 3.m7 6.m7 2.m7 5.13sus@2 5.7@2 1@8", "Ballad" },
        { "Worship ballads", "Lush 1-3-4-5",           "1.maj9 3.m7 4.maj9 5.13sus", "Ballad" },
        { "Worship ballads", "Descending worship",     "1 5/7 6 6.m/5 4 1/3 2.m7 5", "Ballad" },
        { "Worship ballads", "Sus resolution",         "4/5 5.7 1.add9@8", "Ballad" },
        { "Worship ballads", "Altar call",             "1.add9 4/1 1.add9 4/1", "Ballad" },
        { "Worship ballads", "Minor 4 tag",            "1 4 4.m6 1@8", "Ballad" },
        { "Worship ballads", "Hymn-style ballad",      "1 4/1 1 5/7 6 2.m/4 5.sus4@2 5@2 1", "Ballad" },

        // Praise and shout
        { "Praise and shout", "Shout cycle 3-6-2-5",   "3.7@2 6.7@2 2.7@2 5.7@2 1@8", "Up-tempo" },
        { "Praise and shout", "Praise walk-up",        "1@2 1.7@2 4@2 #4.dim7@2 1/5@2 6.7@2 2.m7@2 5.7@2", "Up-tempo" },
        { "Praise and shout", "Praise break 1-4",      "1@2 4@2 1@2 4@2", "Up-tempo" },
        { "Praise and shout", "Quick 1-4-5-4",         "1@2 4@2 5@2 4@2", "Up-tempo" },
        { "Praise and shout", "Holy dance 1-4-1-5",    "1@2 4@2 1@2 5@2", "Up-tempo" },
        { "Praise and shout", "Shout in relative minor","6@2 2@2 3.7@2 6@2", "Up-tempo" },
        { "Praise and shout", "Jubilee 1-6-2-5",       "1@2 6.7@2 2.m7@2 5.7@2", "Up-tempo" },

        // Walk-ups and walk-downs
        { "Walk-ups and walk-downs", "Gospel walk-up to 4",   "1@2 1/3@2 4@2 #4.dim7@2 1/5 5.7", "Any" },
        { "Walk-ups and walk-downs", "Walk-up to 5",          "1/3@2 4@2 #4.dim7@2 5@2", "Any" },
        { "Walk-ups and walk-downs", "Walk-down 1-7-6",       "1@2 1/7@2 6@2 6.m/5@2 4@2 1/3@2 2.m7@2 5.7@2", "Any" },
        { "Walk-ups and walk-downs", "Bass climb 1-2-3-4",    "1@2 2.m7@2 1/3@2 4@2", "Any" },
        { "Walk-ups and walk-downs", "Chromatic walk-down",   "1@2 5/7@2 b7.7@2 6.7@2 b6.7@2 5.7@2 1", "Any" },
        { "Walk-ups and walk-downs", "4 down to 1",           "4@2 3.m7@2 2.m7@2 1@2", "Any" },

        // Turnarounds
        { "Turnarounds", "1-6-2-5",                    "1@2 6.7@2 2.m7@2 5.7@2", "Any" },
        { "Turnarounds", "3-b3 dim-2-5",               "3.m7@2 b3.dim7@2 2.m7@2 5.7@2", "Any" },
        { "Turnarounds", "1-#1 dim-2-5",               "1@2 #1.dim7@2 2.m7@2 5.7@2", "Any" },
        { "Turnarounds", "Tritone turnaround",         "1@2 b3.7@2 2.m7@2 b2.7@2", "Any" },
        { "Turnarounds", "1-b7-4-5",                   "1@2 b7@2 4@2 5.13sus@2", "Any" },
        { "Turnarounds", "5 sus to 5 b9",              "5.13sus@2 5.7b9@2", "Any" },

        // Endings and tags
        { "Endings and tags", "Classic gospel ending", "4@2 #4.dim7@2 1/5@2 6.m7@2 2.m7@2 5.7@2 1@8", "Any" },
        { "Endings and tags", "Triumphant b6-b7-1",    "b6@2 b7@2 1@8", "Any" },
        { "Endings and tags", "Minor plagal Amen",     "4.m6 1.69@8", "Any" },
        { "Endings and tags", "Deceptive tag",         "2.m7@2 5.7@2 6.m7@2 2.m7@2 5.7@2 1@8", "Any" },
        { "Endings and tags", "3-6-2-5 double tag",    "3.m7@2 6.7@2 2.m7@2 5.13sus@2 3.m7@2 6.7@2 2.m7@2 5.7@2 1@8", "Any" },
        { "Endings and tags", "Backdoor ending",       "4.m7@2 b7.9@2 1.maj9@8", "Any" },
        { "Endings and tags", "Sus hold ending",       "4/5 1.add9@8", "Any" },

        // Key changes
        { "Key changes", "Half-step lift (new key b2)", "1 5.7 b6.7 b2@8", "Any" },
        { "Key changes", "Whole-step lift (new key 2)", "1 5.7 6.7 2@8", "Any" },
        { "Key changes", "Pivot into 4",               "1 5.m7@2 1.7@2 4@8", "Any" },
        { "Key changes", "Up a minor third",           "1 b7.7 b3@8", "Any" },
        { "Key changes", "Sus lift (new key b2)",      "5.13sus@2 5.7@2 b6.13sus@2 b6.7@2 b2@8", "Any" },

        // Hymn and church
        { "Hymn and church", "Doxology cadence",       "1 4 1/3 5 1", "Hymn" },
        { "Hymn and church", "Amen",                   "4 1", "Hymn" },
        { "Hymn and church", "Hymn 1-4-1-5-1",         "1@2 4@2 1@2 5@2 1", "Hymn" },
        { "Hymn and church", "Traditional hymn",       "1 1/3 4 1/5 5.7 1", "Hymn" },
        { "Hymn and church", "Psalm chant",            "1 6 2 5 1", "Hymn" },

        // Contemporary worship
        { "Contemporary worship", "1-5-6-4",           "1 5 6 4", "Mid-tempo" },
        { "Contemporary worship", "6-4-1-5",           "6 4 1 5", "Mid-tempo" },
        { "Contemporary worship", "4-1-5-6",           "4 1 5 6", "Mid-tempo" },
        { "Contemporary worship", "1-4-6-5",           "1 4 6 5", "Mid-tempo" },
        { "Contemporary worship", "Anthem build",      "4 5 6 1/3", "Mid-tempo" },
        { "Contemporary worship", "2-4-1-5",           "2.m7 4 1 5", "Mid-tempo" },

        // Neo-soul gospel
        { "Neo-soul gospel", "4-3-6-2-5",              "4.maj9 3.m7 6.m9 2.m9@2 5.13sus@2", "Groove" },
        { "Neo-soul gospel", "Altered 2-5-1",          "2.m9@2 5.7alt@2 1.maj9", "Groove" },
        { "Neo-soul gospel", "4 maj9 to 3 sharp 9",    "4.maj9 3.7#9 6.m9 b6.13", "Groove" },
        { "Neo-soul gospel", "Floating 1-4",           "1.maj9 4.maj7#11", "Groove" },
        { "Neo-soul gospel", "Smooth 6-2-5-1",         "6.m11 2.m9 5.13b9 1.69", "Groove" },
        { "Neo-soul gospel", "Minor 2-5 into 6",       "7.m7b5@2 3.7b9@2 6.m9", "Groove" },

        // Minor-key gospel
        { "Minor-key gospel", "1m-4m-5-1m",            "1.m 4.m 5.7 1.m", "Minor key" },
        { "Minor-key gospel", "1m-b6-b7-1m",           "1.m b6 b7 1.m", "Minor key" },
        { "Minor-key gospel", "1m-b6-4m-5",            "1.m7 b6.maj7 4.m7 5.7b9", "Minor key" },
        { "Minor-key gospel", "Minor 2-5-1",           "2.m7b5@2 5.7b9@2 1.m9", "Minor key" },
        { "Minor-key gospel", "Minor walk-down",       "1.m@2 1.m/b7@2 b6@2 5.7@2", "Minor key" },
        { "Minor-key gospel", "Dorian vamp",           "1.m7 4.7", "Minor key" },
        { "Minor-key gospel", "Minor shout",           "1.m@2 4.m@2 5.7@2 1.m@2", "Minor key" },

        // African and Cameroonian
        { "African and Cameroonian", "Makossa-style 1-5 vamp",       "1 5", "Makossa" },
        { "African and Cameroonian", "Makossa-style 1-4-5-1",        "1@2 4@2 5@2 1@2", "Makossa" },
        { "African and Cameroonian", "Assiko / palm-wine 1-5-5-1",   "1 5.7 5.7 1", "Assiko" },
        { "African and Cameroonian", "Highlife 1-4-5-1",             "1 4 5.7 1", "Highlife" },
        { "African and Cameroonian", "Highlife turnaround 1-6-4-5",  "1 6 4 5", "Highlife" },
        { "African and Cameroonian", "Bikutsi minor vamp (6/8)",     "1.m b7 1.m b7", "Bikutsi 6/8" },
        { "African and Cameroonian", "Bikutsi major vamp (6/8)",     "1 b7 4 1", "Bikutsi 6/8" },
        { "African and Cameroonian", "Bend-skin vamp (6/8)",         "1 4 1 5", "Bend-skin 6/8" },
        { "African and Cameroonian", "Seben half-time 1-4-5-1",      "1@2 4@2 5@2 1@2", "Rumba / seben" },
        { "African and Cameroonian", "Seben double stroke 1-4-5-4",  "1@2 4@2 5@2 4@2", "Rumba / seben" },
        { "African and Cameroonian", "Seben 3 stroke 1-5-4-5",       "1@2 5@2 4@2 5@2", "Rumba / seben" },
        { "African and Cameroonian", "Seben 4 stroke 1-b7-4-5",      "1@2 b7@2 4@2 5@2", "Rumba / seben" },
        { "African and Cameroonian", "Seben seventh 1-5.7",          "1@2 5.7@2", "Rumba / seben" },
        { "African and Cameroonian", "Soukous 1-1-4-4",              "1 1 4 4", "Soukous" },
        { "African and Cameroonian", "Rumba 1 to 4 over 5",          "1 4/5", "Rumba" },
        { "African and Cameroonian", "Afro-worship 6-4-1-5",         "6 4 1 5", "Afro-worship" },
    };
    return p;
}

juce::StringArray categories()
{
    juce::StringArray c;
    for (const auto& p : library())
        c.addIfNotAlreadyThere (p.category);
    return c;
}

//==============================================================================
static bool parseDegree (juce::String token, int& semis, bool& sharp)
{
    static const int major[] = { 0, 2, 4, 5, 7, 9, 11 };
    sharp = false;
    int offset = 0;

    while (token.startsWithChar ('b') || token.startsWithChar ('#'))
    {
        if (token[0] == '#') { ++offset; sharp = true; }
        else --offset;
        token = token.substring (1);
    }

    const auto n = token.getIntValue();
    if (n < 1 || n > 7)
        return false;

    semis = ((major[n - 1] + offset) % 12 + 12) % 12;
    return true;
}

static juce::String diatonicQuality (int semis, bool minorKey)
{
    if (! minorKey)
    {
        switch (semis) { case 2: case 4: case 9: return "m"; case 11: return "dim"; default: return ""; }
    }
    switch (semis) { case 0: case 5: case 7: return "m"; case 2: return "dim"; default: return ""; }
}

std::vector<TimedChord> parse (const juce::String& text, double beatsPerBar, bool minorKey)
{
    std::vector<TimedChord> result;
    juce::StringArray tokens;
    tokens.addTokens (text, " ", {});
    tokens.removeEmptyStrings();

    for (auto token : tokens)
    {
        double beats = beatsPerBar;
        if (token.containsChar ('@'))
        {
            beats = token.fromFirstOccurrenceOf ("@", false, false).getDoubleValue();
            token = token.upToFirstOccurrenceOf ("@", false, false);
        }

        juce::String bassPart;
        if (token.containsChar ('/'))
        {
            bassPart = token.fromFirstOccurrenceOf ("/", false, false);
            token = token.upToFirstOccurrenceOf ("/", false, false);
        }

        auto degreePart = token.upToFirstOccurrenceOf (".", false, false);
        auto qualityPart = token.containsChar ('.') ? token.fromFirstOccurrenceOf (".", false, false) : juce::String ("?");

        Chord c;
        if (! parseDegree (degreePart, c.root, c.sharp))
            continue;

        c.quality = qualityPart == "?" ? diatonicQuality (c.root, minorKey)
                  : qualityPart == "M" ? juce::String() : qualityPart;

        if (bassPart.isNotEmpty())
        {
            bool unused = false;
            if (! parseDegree (bassPart, c.bass, unused))
                c.bass = -1;
        }

        result.push_back ({ c, beats > 0.0 ? beats : beatsPerBar });
    }
    return result;
}

static juce::String degreeToken (int semis, bool sharp)
{
    static const char* flatT[]  = { "1", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7" };
    static const char* sharpT[] = { "1", "#1", "2", "#2", "3", "4", "#4", "5", "#5", "6", "#6", "7" };
    return sharp ? sharpT[semis % 12] : flatT[semis % 12];
}

juce::String toText (const std::vector<TimedChord>& chords)
{
    juce::StringArray tokens;
    for (const auto& tc : chords)
    {
        auto t = degreeToken (tc.chord.root, tc.chord.sharp) + "." + (tc.chord.quality.isEmpty() ? juce::String ("M") : tc.chord.quality);
        if (tc.chord.bass >= 0)
            t << "/" << degreeToken (tc.chord.bass, false);
        t << "@" << juce::String (tc.beats);
        tokens.add (t);
    }
    return tokens.joinIntoString (" ");
}

//==============================================================================
Chord enrich (const Chord& c, bool minorKey)
{
    auto r = c;
    const auto& q = c.quality;

    if (q.isEmpty())
    {
        if (! minorKey && c.root == 0)       r.quality = c.bass >= 0 ? "add9" : "maj9";
        else if (c.root == 5 && ! minorKey)  r.quality = c.bass >= 0 ? "" : "maj9";
        else if (c.root == 7 && c.bass < 0)  r.quality = "9";
        else if (c.bass < 0)                 r.quality = "add9";
    }
    else if (q == "m")
    {
        r.quality = (c.root == 4 && ! minorKey) ? "m7" : "m9";
    }
    else if (q == "m7")
    {
        if (! (c.root == 4 && ! minorKey)) r.quality = "m9";
    }
    else if (q == "maj7")  r.quality = "maj9";
    else if (q == "dim")   r.quality = "dim7";
    else if (q == "sus4")  r.quality = "9sus4";
    else if (q == "7")
    {
        // Secondary dominants into minor chords get the b9; others the 9 or 13.
        const auto target = (c.root + 5) % 12;
        const bool minorTarget = ! minorKey ? (target == 2 || target == 4 || target == 9) : (target == 0 || target == 5 || target == 7);
        r.quality = minorTarget ? "7b9" : (c.root == 7 ? "13" : "9");
    }
    return r;
}

std::vector<TimedChord> applyLevel (const std::vector<TimedChord>& in, Level level, bool minorKey)
{
    if (level == Level::simple)
        return in;

    std::vector<TimedChord> rich;
    for (const auto& tc : in)
        rich.push_back ({ enrich (tc.chord, minorKey), tc.beats });

    if (level == Level::rich)
        return rich;

    // Advanced: approach each chord from the beat before.
    std::vector<TimedChord> out;
    for (size_t i = 0; i < rich.size(); ++i)
    {
        auto a = rich[i];
        if (i + 1 < rich.size() && a.beats >= 2.0)
        {
            const auto& b = rich[i + 1].chord;
            const auto target = b.root;
            const bool bMinor = isMinorQuality (b.quality);
            Chord approach;

            if (target == a.chord.root)
            {
                out.push_back (a);
                continue;
            }

            if (bMinor)                                              approach = ch ((target + 7) % 12, "7b9");
            else if (target == 0 && a.chord.root != 7)               approach = ch (1, "7");          // tritone sub of 5
            else if (target == 0)                                    approach = ch (7, "7b9");
            else if (target == 7)                                    approach = ch (6, "dim7", -1, true);
            else if (target == 5)                                    approach = ch (0, "7");
            else if ((target - a.chord.root + 12) % 12 == 2)         approach = ch ((a.chord.root + 1) % 12, "dim7", -1, true);
            else                                                     approach = ch ((target + 7) % 12, "7");

            if (approach.root != a.chord.root)
            {
                a.beats -= 1.0;
                out.push_back (a);
                out.push_back ({ approach, 1.0 });
                continue;
            }
        }
        out.push_back (a);
    }

    if (! out.empty() && out.back().chord.root == 0 && out.back().chord.bass < 0 && ! minorKey)
        out.back().chord.quality = "69";

    return out;
}

//==============================================================================
juce::String midiNoteName (int midiNote, const Key& key)
{
    return noteName (((midiNote % 12) + 12) % 12, key.flats) + juce::String (midiNote / 12 - 1);
}

std::vector<int> chordPitchClasses (const Chord& c, const Key& key)
{
    std::vector<int> out;
    for (auto i : findQuality (c.quality).intervals)
        out.push_back ((key.tonic + c.root + i) % 12);
    if (c.bass >= 0)
        out.push_back ((key.tonic + c.bass) % 12);
    std::sort (out.begin(), out.end());
    out.erase (std::unique (out.begin(), out.end()), out.end());
    return out;
}

bool isInKey (int pitchClass, const Key& key)
{
    static const int major[] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minor[] = { 0, 2, 3, 5, 7, 8, 10, 11 };     // natural minor plus the raised 7th
    const auto rel = ((pitchClass - key.tonic) % 12 + 12) % 12;
    if (key.minor) return std::find (std::begin (minor), std::end (minor), rel) != std::end (minor);
    return std::find (std::begin (major), std::end (major), rel) != std::end (major);
}

}
