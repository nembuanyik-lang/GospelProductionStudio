#include "ChordTrack.h"
#include "StudioEngine.h"

namespace ChordTrack
{
static const juce::Identifier trackId ("GPS_CHORDTRACK"), chordId ("CHORD"),
    rootId ("root"), sharpId ("sharp"), qualityId ("quality"), bassId ("bass"), startId ("start"), lengthId ("length");

static const juce::String generatedName ("Chords (from Chord Track)");

static juce::ValueTree getTree (te::Edit& edit)
{
    return edit.state.getOrCreateChildWithName (trackId, nullptr);
}

Event fromNode (const juce::ValueTree& v)
{
    Event e;
    e.chord.root    = v.getProperty (rootId, 0);
    e.chord.sharp   = v.getProperty (sharpId, false);
    e.chord.quality = v.getProperty (qualityId, "").toString();
    e.chord.bass    = v.getProperty (bassId, -1);
    e.start         = v.getProperty (startId, 0.0);
    e.length        = v.getProperty (lengthId, 4.0);
    e.node          = v;
    return e;
}

std::vector<Event> getEvents (te::Edit& edit)
{
    std::vector<Event> events;
    auto tree = getTree (edit);
    for (int i = 0; i < tree.getNumChildren(); ++i)
        if (tree.getChild (i).hasType (chordId))
            events.push_back (fromNode (tree.getChild (i)));

    std::sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.start < b.start; });
    return events;
}

static void writeChord (juce::ValueTree v, const Theory::Chord& c, juce::UndoManager* um)
{
    v.setProperty (rootId, c.root, um);
    v.setProperty (sharpId, c.sharp, um);
    v.setProperty (qualityId, c.quality, um);
    v.setProperty (bassId, c.bass, um);
}

juce::ValueTree add (te::Edit& edit, const Theory::Chord& c, double start, double length)
{
    auto* um = &edit.getUndoManager();
    juce::ValueTree v (chordId);
    writeChord (v, c, nullptr);
    v.setProperty (startId, start, nullptr);
    v.setProperty (lengthId, length, nullptr);
    getTree (edit).appendChild (v, um);
    return v;
}

void setChord (te::Edit& edit, juce::ValueTree node, const Theory::Chord& c)
{
    writeChord (node, c, &edit.getUndoManager());
}

void setTiming (te::Edit& edit, juce::ValueTree node, double start, double length)
{
    node.setProperty (startId, juce::jmax (0.0, start), &edit.getUndoManager());
    node.setProperty (lengthId, juce::jmax (0.25, length), &edit.getUndoManager());
}

void remove (te::Edit& edit, juce::ValueTree node)
{
    getTree (edit).removeChild (node, &edit.getUndoManager());
}

void clearRange (te::Edit& edit, double start, double end)
{
    for (auto& e : getEvents (edit))
        if (e.start >= start - 1.0e-6 && e.start < end - 1.0e-6)
            remove (edit, e.node);
}

int indexAt (const std::vector<Event>& events, double beat)
{
    for (int i = (int) events.size(); --i >= 0;)
        if (beat >= events[(size_t) i].start - 1.0e-6 && beat < events[(size_t) i].start + events[(size_t) i].length)
            return i;
    return -1;
}

double insert (te::Edit& edit, const std::vector<Theory::TimedChord>& chords, double start)
{
    double total = 0.0;
    for (const auto& c : chords)
        total += c.beats;

    clearRange (edit, start, start + total);

    auto beat = start;
    for (const auto& c : chords)
    {
        add (edit, c.chord, beat, c.beats);
        beat += c.beats;
    }
    return beat;
}

std::vector<Theory::TimedChord> toTimed (const std::vector<Event>& events)
{
    std::vector<Theory::TimedChord> out;
    for (const auto& e : events)
        out.push_back ({ e.chord, e.length });
    return out;
}

//==============================================================================
static te::AudioTrack* findTrack (te::Edit& edit, const juce::String& name, bool instrumentOnly)
{
    for (auto* t : te::getAudioTracks (edit))
        if (t->getName().equalsIgnoreCase (name) && (! instrumentOnly || StudioEngine::isInstrumentTrack (*t)))
            return t;
    return nullptr;
}

juce::String chordsToNotes (te::Edit& edit, const Theory::Key& key, const NotesOptions& o)
{
    const auto events = getEvents (edit);
    if (events.empty())
        return "The Chord Track is empty.";

    auto& tempo = edit.tempoSequence;
    auto toTime = [&tempo] (double b) { return tempo.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, b))); };

    const auto first = events.front().start;
    const auto last  = events.back().start + events.back().length;
    juce::StringArray done;

    auto makeClip = [&] (te::AudioTrack& track) -> te::MidiClip*
    {
        // Replace an earlier generated clip on this track.
        const auto existing = track.getClips();      // copy: clips are removed while looping
        for (auto* c : existing)
            if (c->getName() == generatedName)
                c->removeFromParent();

        auto clip = track.insertMIDIClip (generatedName, te::TimeRange (toTime (first), toTime (last)), nullptr);
        return clip.get();
    };

    if (o.keys)
    {
        te::AudioTrack* keys = findTrack (edit, "Keys", true);
        if (keys == nullptr)
            keys = findTrack (edit, "Piano", true);
        if (keys == nullptr)
            for (auto* t : te::getAudioTracks (edit))
                if (keys == nullptr && StudioEngine::isInstrumentTrack (*t))
                    keys = t;

        if (keys != nullptr)
            if (auto* clip = makeClip (*keys))
            {
                auto& seq = clip->getSequence();
                int centre = 0;
                for (size_t i = 0; i < events.size(); ++i)
                {
                    const auto& e = events[i];
                    const auto notes = Theory::voice (e.chord, key, o.voicing, o.leftHand, centre);
                    centre = Theory::rightHandCentre (notes);

                    auto start = e.start - first;
                    auto length = e.length;
                    if (o.anticipate && i > 0)                      { start -= 0.5; length += 0.5; }
                    if (o.anticipate && i + 1 < events.size())      length -= 0.5;

                    for (auto n : notes)
                        seq.addNote (n, te::BeatPosition::fromBeats (juce::jmax (0.0, start)),
                                     te::BeatDuration::fromBeats (juce::jmax (0.25, length - 0.05)),
                                     88, 0, &edit.getUndoManager());
                }
                done.add (keys->getName());
            }
    }

    if (o.bass)
    {
        if (auto* bass = findTrack (edit, "Bass", false))
            if (auto* clip = makeClip (*bass))
            {
                auto& seq = clip->getSequence();
                for (size_t i = 0; i < events.size(); ++i)
                {
                    const auto& e = events[i];
                    auto start = e.start - first;
                    auto length = e.length;
                    if (o.anticipate && i > 0)                      { start -= 0.5; length += 0.5; }
                    if (o.anticipate && i + 1 < events.size())      length -= 0.5;

                    seq.addNote (Theory::bassNote (e.chord, key), te::BeatPosition::fromBeats (juce::jmax (0.0, start)),
                                 te::BeatDuration::fromBeats (juce::jmax (0.25, length - 0.05)), 100, 0, &edit.getUndoManager());
                }
                done.add (bass->getName());
            }
    }

    return done.isEmpty() ? juce::String ("No Keys or Bass track was found.")
                          : "Chords written to " + done.joinIntoString (" and ") + ".";
}
}
