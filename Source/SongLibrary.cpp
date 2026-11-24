#include "SongLibrary.h"

//==============================================================================
SongLibrary::SongLibrary()
{
    root = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Gospel Production Studio")
               .getChildFile ("Songs");
    root.createDirectory();
    reload();
}

void SongLibrary::reload()
{
    songs.clear();

    for (const auto& entry : juce::RangedDirectoryIterator (root, false, "*", juce::File::findDirectories))
    {
        const auto file = entry.getFile().getChildFile ("song.json");
        if (! file.existsAsFile())
            continue;

        const auto parsed = juce::JSON::parse (file.loadFileAsString());
        if (! parsed.isObject())
            continue;

        auto song = Song::fromVar (parsed);
        if (song.id.isEmpty())
            song.id = entry.getFile().getFileName();

        songs.push_back (std::move (song));
    }

    sortSongs();
    sendChangeMessage();
}

const Song* SongLibrary::find (const juce::String& id) const
{
    for (const auto& s : songs)
        if (s.id == id)
            return &s;

    return nullptr;
}

juce::String SongLibrary::createSong()
{
    Song song;
    song.id           = juce::Uuid().toString();
    song.dateReceived = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
    song.created      = juce::Time::getCurrentTime();
    song.modified     = song.created;
    song.sections     = { { "Verse", {} }, { "Chorus", {} } };

    save (song);
    songs.push_back (song);
    sortSongs();
    sendChangeMessage();
    return song.id;
}

void SongLibrary::update (const Song& song)
{
    for (auto& s : songs)
    {
        if (s.id == song.id)
        {
            s = song;
            save (s);
            sortSongs();
            sendChangeMessage();
            return;
        }
    }
}

void SongLibrary::remove (const juce::String& id)
{
    getSongFolder (id).moveToTrash();

    songs.erase (std::remove_if (songs.begin(), songs.end(),
                                 [&id] (const Song& s) { return s.id == id; }),
                 songs.end());
    sendChangeMessage();
}

bool SongLibrary::save (const Song& song)
{
    const auto folder = getSongFolder (song.id);
    folder.createDirectory();

    const auto target = folder.getChildFile ("song.json");
    juce::TemporaryFile temp (target);

    if (! temp.getFile().replaceWithText (juce::JSON::toString (song.toVar())))
        return false;

    return temp.overwriteTargetFileWithTemporary();
}

void SongLibrary::sortSongs()
{
    // Newest received first, then by title.
    std::stable_sort (songs.begin(), songs.end(), [] (const Song& a, const Song& b)
    {
        if (a.dateReceived != b.dateReceived)
            return a.dateReceived > b.dateReceived;
        return a.title.compareIgnoreCase (b.title) < 0;
    });
}
