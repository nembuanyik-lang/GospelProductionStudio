#pragma once

#include <juce_events/juce_events.h>
#include "Song.h"

//==============================================================================
//  All received songs, stored on the laptop:
//
//    Documents\Gospel Production Studio\Songs\<song id>\song.json
//    Documents\Gospel Production Studio\Songs\<song id>\memo.wav
//
//  Every save is written to a temporary file first and then swapped in,
//  so a power cut during saving can never corrupt a song.
//==============================================================================
class SongLibrary final : public juce::ChangeBroadcaster
{
public:
    SongLibrary();

    juce::File getRootFolder() const                          { return root; }
    juce::File getSongFolder (const juce::String& id) const   { return root.getChildFile (id); }
    juce::File getMemoFile (const juce::String& id) const     { return getSongFolder (id).getChildFile ("memo.wav"); }

    const std::vector<Song>& getSongs() const                 { return songs; }
    const Song* find (const juce::String& id) const;

    juce::String createSong();                  // returns the new song's id
    void update (const Song& song);             // save changes
    void remove (const juce::String& id);       // moves the folder to the Recycle Bin
    void reload();

private:
    bool save (const Song& song);
    void sortSongs();

    juce::File root;
    std::vector<Song> songs;
};
