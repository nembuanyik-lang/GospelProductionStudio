#include "Song.h"

//==============================================================================
juce::var Song::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("formatVersion", 1);
    o->setProperty ("id",            id);
    o->setProperty ("title",         title);
    o->setProperty ("dateReceived",  dateReceived);
    o->setProperty ("key",           key);
    o->setProperty ("tempo",         tempo);
    o->setProperty ("timeSignature", timeSignature);
    o->setProperty ("theme",         theme);
    o->setProperty ("scripture",     scripture);
    o->setProperty ("status",        status);
    o->setProperty ("notes",         notes);
    o->setProperty ("created",       created.toISO8601 (true));
    o->setProperty ("modified",      modified.toISO8601 (true));

    juce::Array<juce::var> sectionArray;
    for (const auto& s : sections)
    {
        auto* so = new juce::DynamicObject();
        so->setProperty ("type",   s.type);
        so->setProperty ("lyrics", s.lyrics);
        sectionArray.add (juce::var (so));
    }
    o->setProperty ("sections", sectionArray);
    if (release.isObject())
        o->setProperty ("release", release);

    return juce::var (o);
}

Song Song::fromVar (const juce::var& v)
{
    Song s;
    auto str = [&v] (const char* name, const juce::String& fallback)
    {
        return v.hasProperty (name) ? v[name].toString() : fallback;
    };

    s.id            = str ("id",            {});
    s.title         = str ("title",         s.title);
    s.dateReceived  = str ("dateReceived",  {});
    s.key           = str ("key",           s.key);
    s.tempo         = v.hasProperty ("tempo") ? (int) v["tempo"] : s.tempo;
    s.timeSignature = str ("timeSignature", s.timeSignature);
    s.theme         = str ("theme",         s.theme);
    s.scripture     = str ("scripture",     {});
    s.status        = str ("status",        s.status);
    s.notes         = str ("notes",         {});
    s.created       = juce::Time::fromISO8601 (str ("created",  {}));
    s.modified      = juce::Time::fromISO8601 (str ("modified", {}));

    if (auto* arr = v["sections"].getArray())
        for (const auto& item : *arr)
            s.sections.push_back ({ item["type"].toString(), item["lyrics"].toString() });
    if (v["release"].isObject())
        s.release = v["release"];

    return s;
}

bool Song::matchesSearch (const juce::String& text) const
{
    if (text.trim().isEmpty())
        return true;

    if (title.containsIgnoreCase (text) || scripture.containsIgnoreCase (text)
        || theme.containsIgnoreCase (text) || notes.containsIgnoreCase (text))
        return true;

    for (const auto& s : sections)
        if (s.lyrics.containsIgnoreCase (text))
            return true;

    return false;
}

//==============================================================================
bool Song::isCompoundMeter() const
{
    return timeSignature == "6/8" || timeSignature == "9/8" || timeSignature == "12/8";
}

double Song::getEngineBpm() const
{
    return isCompoundMeter() ? tempo * 1.5 : (double) tempo;
}

int Song::getKeyPitchClass() const
{
    static const juce::StringArray names { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    static const juce::StringArray alternatives { "B#", "C#", "D", "D#", "Fb", "E#", "Gb", "G", "G#", "A", "A#", "Cb" };

    const auto root = key.upToFirstOccurrenceOf (" ", false, false).trim();
    auto index = names.indexOf (root);
    if (index < 0)
        index = alternatives.indexOf (root);
    return juce::jmax (0, index);
}

bool Song::isMinorKey() const
{
    return key.containsIgnoreCase ("minor");
}

//==============================================================================
juce::StringArray Song::statuses()
{
    return { "Idea", "Written", "Arranged", "Recorded", "Mixed", "Released" };
}

juce::StringArray Song::themes()
{
    return { "Praise", "Worship", "Thanksgiving", "Prayer", "Healing", "Deliverance",
             "Faith", "Salvation", "Holy Spirit", "Christmas", "Easter", "Wedding",
             "Comfort", "Children", "Other" };
}

juce::StringArray Song::keys()
{
    juce::StringArray result;
    for (auto root : { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" })
    {
        result.add (juce::String (root) + " major");
        result.add (juce::String (root) + " minor");
    }
    return result;
}

juce::StringArray Song::timeSignatures()
{
    return { "4/4", "3/4", "6/8", "12/8", "2/4", "9/8" };
}

juce::StringArray Song::sectionTypes()
{
    return { "Intro", "Verse", "Pre-Chorus", "Chorus", "Bridge", "Vamp",
             "Tag", "Interlude", "Spoken", "Outro" };
}
