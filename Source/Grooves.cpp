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

            { "Fill: toms (1 bar)",       "4/4", 4, 4, 4, {
                { snare,     "x-x-x-x---------" },
                { highTom,   "--------xx------" },
                { midTom,    "----------xx----" },
                { lowTom,    "------------xx--" },
                { floorTom,  "--------------xx" },
                { kick,      "x-------x-------" } } },

            // ---- 6/8 and 12/8 --------------------------------------------------
            { "6/8 gospel ballad",        "6/8", 6, 8, 2, {
                { kick,      "x-------x---" },
                { snare,     "------x-----" },
                { closedHat, "x-x-x-x-x-x-" } } },

            { "Bikutsi-style 6/8",        "6/8", 6, 8, 2, {
                { kick,      "x-----x-----" },
                { snare,     "----x-----x-" },
                { closedHat, "X-x-x-X-x-x-" } } },

            { "12/8 slow gospel",         "12/8", 12, 8, 1, {
                { kick,      "x-----x-----" },
                { snare,     "---X-----X--" },
                { closedHat, "Xxxxxxxxxxxx" } } },
        };
        return list;
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
