#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
//  One part of a song's lyrics: Verse, Chorus, Bridge, Vamp...
//==============================================================================
struct SongSection
{
    juce::String type   { "Verse" };
    juce::String lyrics;
};

//==============================================================================
//  A received song and everything known about it.
//  Saved as song.json inside the song's own folder.
//==============================================================================
struct Song
{
    juce::String id;
    juce::String title          { "Untitled song" };
    juce::String dateReceived;                    // YYYY-MM-DD
    juce::String key            { "C major" };
    int          tempo          { 72 };
    juce::String timeSignature  { "4/4" };
    juce::String theme          { "Worship" };
    juce::String scripture;
    juce::String status         { "Idea" };
    juce::String notes;
    std::vector<SongSection> sections;
    juce::Time   created, modified;

    juce::var toVar() const;
    static Song fromVar (const juce::var& v);

    bool matchesSearch (const juce::String& text) const;

    // Music helpers
    bool   isCompoundMeter() const;      // 6/8, 9/8, 12/8: tempo is counted in dotted quarters
    double getEngineBpm() const;         // quarter-note BPM for the engine
    int    getKeyPitchClass() const;     // C = 0 ... B = 11
    bool   isMinorKey() const;

    // Fixed choices shown in the editor
    static juce::StringArray statuses();
    static juce::StringArray themes();
    static juce::StringArray keys();
    static juce::StringArray timeSignatures();
    static juce::StringArray sectionTypes();
};
