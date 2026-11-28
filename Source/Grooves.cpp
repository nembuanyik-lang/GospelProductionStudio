#include "Grooves.h"

namespace Grooves
{
    using namespace Drum;

    const std::vector<Groove>& all()
    {
        static const std::vector<Groove> list {
            // ---- 4/4 ---------------------------------------------------------
            { "Gospel ballad",            "4/4", 4, 4, 4, {
                { kick,      "x-------x-x-----" },
                { rim,       "----x-------x---" },
                { closedHat, "o-o-o-o-o-o-o-o-" } } },

            { "Praise (mid tempo)",       "4/4", 4, 4, 4, {
                { kick,      "x-----x-x-----x-" },
                { snare,     "----X--g----X---" },
                { closedHat, "x-x-x-x-x-x-x-x-" } } },

            { "Gospel shout (fast)",      "4/4", 4, 4, 4, {
                { kick,      "x---x---x---x---" },
                { snare,     "----X-g-----X-g-" },
                { pedalHat,  "x---x---x---x---" },
                { openHat,   "--x---x---x---x-" } } },

            { "Four on the floor praise", "4/4", 4, 4, 4, {
                { kick,      "x---x---x---x---" },
                { snare,     "----X-------X---" },
                { closedHat, "x---x---x---x---" },
                { openHat,   "--x---x---x---x-" } } },

            { "Afro-worship",             "4/4", 4, 4, 4, {
                { kick,      "x-----x-x-----x-" },
                { rim,       "---x--x----x--x-" },
                { closedHat, "x-xxx-xxx-xxx-xx" } } },

            { "Makossa-style",            "4/4", 4, 4, 4, {
                { kick,      "x---x--xx---x---" },
                { snare,     "----x-------x---" },
                { closedHat, "XoxoXoxoXoxoXoxo" } } },

            { "Highlife-style",           "4/4", 4, 4, 4, {
                { kick,      "x-------x-x-----" },
                { snare,     "----x-------x---" },
                { rideBell,  "x--x--x---x-x---" },
                { closedHat, "o-o-o-o-o-o-o-o-" } } },

            { "Hymn (soft)",              "4/4", 4, 4, 4, {
                { kick,      "x-------o-------" },
                { rim,       "----x-------x---" },
                { ride,      "o-o-o-o-o-o-o-o-" } } },

            { "Gospel 16ths (ghost notes)", "4/4", 4, 4, 4, {
                { kick,      "x--x--x---x--x--" },
                { snare,     "--g-X--g-gg-X--g" },
                { closedHat, "xoxoxoxoxoxoxoxo" } } },

            { "Praise shuffle",           "4/4", 4, 4, 3, {
                { kick,      "x-----x-x---" },
                { snare,     "---X-g---X-g" },
                { closedHat, "x-ox-ox-ox-o" } } },

            { "Church shuffle (fast)",    "4/4", 4, 4, 3, {
                { kick,      "x--x--x--x--" },
                { snare,     "---X-g---X-g" },
                { ride,      "x-xx-xx-xx-x" },
                { pedalHat,  "---x-----x--" } } },

            { "Fill: toms (1 bar)",       "4/4", 4, 4, 4, {
                { snare,     "x-x-x-x---------" },
                { highTom,   "--------xx------" },
                { midTom,    "----------xx----" },
                { lowTom,    "------------xx--" },
                { floorTom,  "--------------xx" },
                { kick,      "x-------x-------" } } },

            // ---- Cameroon (4/4) ------------------------------------------------
            { "Makossa (classic)",        "Cameroon", 4, 4, 4, {
                { kick,      "x-----x-x--x----" },
                { snare,     "----X-------X---" },
                { closedHat, "XoxoXoxoXoxoXoxo" },
                { congaMute, "--x---x---x---x-" },
                { congaOpen, "---x----x--x---x" } } },

            { "Makossa (modern dance)",   "Cameroon", 4, 4, 4, {
                { kick,      "x---x---x---x---" },
                { snare,     "----X-------X---" },
                { openHat,   "--x---x---x---x-" },
                { shaker,    "xoxoxoxoxoxoxoxo" },
                { congaOpen, "---x--x----x--x-" } } },

            { "Assiko (Bassa, bottle)",   "Cameroon", 4, 4, 4, {
                { kick,      "x---x---x---x---" },
                { snare,     "----x-------x---" },
                { claves,    "x-xxx-xxx-xxx-xx" },
                { shaker,    "xoxoxoxoxoxoxoxo" } } },

            { "Bend skin",                "Cameroon", 4, 4, 4, {
                { kick,      "x---x---x---x---" },
                { snare,     "----X--g----X--g" },
                { tambourine, "--x---x---x---x-" },
                { cabasa,    "oooooooooooooooo" },
                { congaOpen, "x--x--x---x-x---" } } },

            { "Mangambeu (Bamileke)",     "Cameroon", 4, 4, 4, {
                { kick,      "x-----x-x-----x-" },
                { snare,     "----x-------x---" },
                { shaker,    "x-xxx-xxx-xxx-xx" },
                { bellHigh,  "x--x--x---x-x---" },
                { congaOpen, "--x--x----x--x--" } } },

            // ---- Cameroon (6/8 and 12/8) ---------------------------------------
            { "Bikutsi (6/8, fast)",      "Cameroon", 6, 8, 2, {
                { kick,      "x-----x-----" },
                { snare,     "----x-----x-" },
                { closedHat, "X-x-x-X-x-x-" },
                { claves,    "x-xx-xx-xx-x" },
                { shaker,    "xoxoxoxoxoxo" } } },

            { "Njang (North-West, 12/8)", "Cameroon", 12, 8, 1, {
                { bellLow,   "x-x-xx-x-x-x" },
                { shaker,    "xoxxoxxoxxox" },
                { djembeBass, "x-----x-----" },
                { djembeTone, "---x-----xx-" },
                { djembeSlap, "-----x-----x" },
                { talkLow,   "x--------x--" } } },

            // ---- West Africa and Nigeria -----------------------------------------
            { "Highlife (Ghana classic)", "West Africa", 4, 4, 4, {
                { kick,      "x-------x-x-----" },
                { snare,     "----x-------x---" },
                { bellHigh,  "x-x-xx-x-x-xx-x-" },
                { congaOpen, "---x--x----x--x-" },
                { shaker,    "xoxoxoxoxoxoxoxo" } } },

            { "Ghana praise (kpanlogo feel)", "West Africa", 4, 4, 4, {
                { kick,      "x-------x-------" },
                { snare,     "----X-------X---" },
                { bellHigh,  "x--x--x---x-x---" },
                { congaOpen, "--xx--x---xx--x-" },
                { congaMute, "x---x---x---x---" },
                { shaker,    "oxoxoxoxoxoxoxox" } } },

            { "Afrobeat",                 "West Africa", 4, 4, 4, {
                { kick,      "x-----x---x-----" },
                { snare,     "----X-------X--g" },
                { closedHat, "x-xxx-xxx-xxx-xx" },
                { shaker,    "xoxoxoxoxoxoxoxo" },
                { cowbell,   "x--x--x---x-x---" },
                { congaOpen, "---x--x----x--x-" },
                { congaMute, "x-------x-------" } } },

            { "Nigerian praise (Igbo, ogene)", "West Africa", 4, 4, 4, {
                { kick,      "x---x---x---x---" },
                { snare,     "----x-------x---" },
                { bellLow,   "x-x-x-xxx-x-x-xx" },
                { shaker,    "xoxoxoxoxoxoxoxo" },
                { congaOpen, "--xx--xx--xx--xx" } } },

            { "Yoruba praise (talking drum)", "West Africa", 4, 4, 4, {
                { kick,      "x-----x-x-------" },
                { snare,     "----X-------X---" },
                { talkHigh,  "x--x--x---x-x---" },
                { talkLow,   "x-------x-------" },
                { talkRise,  "-------x--------" },
                { talkFall,  "--------------x-" },
                { shaker,    "xoxoxoxoxoxoxoxo" } } },

            // ---- Central Africa --------------------------------------------------
            { "Soukous / Ndombolo",       "Central Africa", 4, 4, 4, {
                { kick,      "x---x---x---x---" },
                { snare,     "----X--x----X-x-" },
                { closedHat, "xoxoxoxoxoxoxoxo" },
                { congaOpen, "--x---x---x---x-" },
                { shaker,    "oooooooooooooooo" } } },

            { "Rumba congolaise",         "Central Africa", 4, 4, 4, {
                { kick,      "x-----x-x-----x-" },
                { claves,    "x--x--x---x-x---" },
                { congaMute, "x---x---x---x---" },
                { congaOpen, "------xx------xx" },
                { closedHat, "x-x-x-x-x-x-x-x-" } } },

            // ---- African worship (12/8) and fills --------------------------------
            { "African 12/8 worship (bell)", "West Africa", 12, 8, 1, {
                { kick,      "x-----x-----" },
                { bellHigh,  "x-x-xx-x-x-x" },
                { cabasa,    "o-oo-oo-oo-o" },
                { djembeBass, "x-----x-----" },
                { djembeTone, "--x--x--x--x" } } },

            { "Fill: djembe and talking drum", "Fill", 4, 4, 4, {
                { djembeBass, "x---------------" },
                { djembeSlap, "x-x-x-x---------" },
                { djembeTone, "--------xx-x----" },
                { talkFall,  "------------x---" },
                { talkRise,  "--------------x-" } } },

            { "Fill: congas",             "Fill", 4, 4, 4, {
                { congaOpen, "x-x-x-x-xxxx----" },
                { congaLow,  "------------xxxx" },
                { congaMute, "-x-x-x-x--------" } } },

            // ---- 6/8 and 12/8 --------------------------------------------------
            { "6/8 gospel ballad",        "6/8", 6, 8, 2, {
                { kick,      "x-------x---" },
                { snare,     "------x-----" },
                { closedHat, "x-x-x-x-x-x-" } } },

            { "Bikutsi-style 6/8",        "6/8", 6, 8, 2, {
                { kick,      "x-----x-----" },
                { snare,     "----x-----x-" },
                { closedHat, "X-x-x-X-x-x-" } } },

            { "6/8 worship (driving)",    "6/8", 6, 8, 2, {
                { kick,      "x-----x-x---" },
                { snare,     "------X-----" },
                { closedHat, "x-o-o-x-o-o-" },
                { crash,     "x-----------" } } },

            { "12/8 slow gospel",         "12/8", 12, 8, 1, {
                { kick,      "x-----x-----" },
                { snare,     "---X-----X--" },
                { closedHat, "Xxxxxxxxxxxx" } } },
        };
        return list;
    }

    const Groove* find (const juce::String& name)
    {
        for (auto& g : all())
            if (name == g.name) return &g;
        return nullptr;
    }

    bool usesPercussion (const Groove& g)
    {
        for (auto& l : g.lanes)
            if (l.note == clap || (l.note >= tambourine && l.note != crash && l.note != ride && l.note != rideBell && l.note != 51)) return true;
        return false;
    }

    bool fits (const Groove& g, int numerator, int denominator)
    {
        return g.numerator == numerator && g.denominator == denominator;
    }

    int numSteps (const Groove& g)
    {
        int n = 0;
        for (const auto& l : g.lanes)
            n = juce::jmax (n, (int) std::strlen (l.steps));
        return n;
    }

    void writeTo (te::MidiList& seq, const Groove& g, double startBeat, double lengthBeats, double beatsPerBar, juce::UndoManager* um)
    {
        const auto stepBeats = 1.0 / g.stepsPerBeat;
        const auto patternBeats = numSteps (g) * stepBeats;
        const auto barLength = juce::jmax (patternBeats, beatsPerBar);
        const auto end = startBeat + lengthBeats - 1.0e-6;

        for (double bar = startBeat; bar < end; bar += barLength)
            for (const auto& lane : g.lanes)
            {
                const auto len = (int) std::strlen (lane.steps);
                for (int i = 0; i < len; ++i)
                {
                    const auto c = lane.steps[i];
                    const int velocity = c == 'X' ? 118 : c == 'x' ? 96 : c == 'o' ? 66 : c == 'g' ? 38 : 0;
                    const auto beat = bar + i * stepBeats;
                    if (velocity == 0 || beat >= end)
                        continue;
                    seq.addNote (lane.note, te::BeatPosition::fromBeats (beat), te::BeatDuration::fromBeats (stepBeats * 0.9),
                                 velocity, 0, um);
                }
            }
    }
}
