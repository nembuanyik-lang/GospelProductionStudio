#pragma once

#include "Song.h"
#include "Analysis.h"

//==============================================================================
//  Release package (plan Step 18): everything a distributor asks for.
//
//  DistroKid, TuneCore, CD Baby, Boomplay, Audiomack and the others all want
//  the same things: the master audio, square cover art (3000 x 3000), the
//  song's details (artists, songwriters, genre, language, codes, copyright)
//  and the lyrics. This checks them the way the stores do, and writes one
//  folder with everything ready to upload.
//==============================================================================
namespace Release
{
    struct Info
    {
        juce::String title, version, artist, featured, songwriters, producer, engineer, label;
        juce::String genre { "Christian & Gospel" }, subGenre { "Contemporary Gospel" }, language { "English" };
        juce::String releaseDate, isrc, upc, pLine, cLine, clipStart { "0:30" }, lyrics, coverPath;
        bool explicitLyrics = false;

        juce::var toVar() const;
        // From the song's saved details, filling the gaps from the song itself
        static Info fromSong (const Song&, const juce::String& defaultArtist);
        juce::String displayTitle() const;          // "Title (Live)"
        juce::String year() const;
    };

    juce::StringArray subGenres();
    juce::StringArray languages();

    juce::String lyricsFromSong (const Song&);      // plain text, a blank line between parts

    // Codes: true if valid; 'clean' gets the code without dashes or spaces
    bool checkIsrc (const juce::String&, juce::String& clean);
    bool checkUpc (const juce::String&, juce::String& clean);
    bool checkDate (const juce::String&);           // YYYY-MM-DD

    struct CoverInfo
    {
        bool found = false, usable = false;
        int width = 0, height = 0;
        juce::String format, problem;
    };
    CoverInfo checkCover (const juce::File&);
    // Writes the cover as a 3000 x 3000 JPEG (resized if bigger). False with a reason if it can't.
    bool writeCover (const juce::File& source, const juce::File& dest, juce::String& problem);

    double leadingSilenceSeconds (const juce::File& audio);

    enum class Level { ok, info, warning, problem };
    struct Check { Level level; juce::String text; };

    // master: the analysis of the rendered master (nullptr before it exists)
    std::vector<Check> check (const Info&, const Analysis::FileAnalysis* master = nullptr, double leadingSilence = -1.0);
    bool hasProblems (const std::vector<Check>&);

    juce::String detailsText (const Info&, const Analysis::FileAnalysis& master, const Song&);
    juce::var detailsJson (const Info&, const Analysis::FileAnalysis& master, const Song&);
}
