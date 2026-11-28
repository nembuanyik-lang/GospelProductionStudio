#include "Release.h"
#include <juce_graphics/juce_graphics.h>

namespace Release
{
    namespace
    {
        const char* fieldNames[] = { "title", "version", "artist", "featured", "songwriters", "producer", "engineer", "label",
                                     "genre", "subGenre", "language", "releaseDate", "isrc", "upc", "pLine", "cLine",
                                     "clipStart", "lyrics", "coverPath" };

        std::vector<juce::String*> fields (Info& i)
        {
            return { &i.title, &i.version, &i.artist, &i.featured, &i.songwriters, &i.producer, &i.engineer, &i.label,
                     &i.genre, &i.subGenre, &i.language, &i.releaseDate, &i.isrc, &i.upc, &i.pLine, &i.cLine,
                     &i.clipStart, &i.lyrics, &i.coverPath };
        }

        juce::String fmtTime (double s)
        {
            const auto t = juce::roundToInt (s);
            return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
        }
    }

    juce::var Info::toVar() const
    {
        auto* o = new juce::DynamicObject();
        auto copy = *this;
        const auto list = fields (copy);
        for (size_t k = 0; k < list.size(); ++k)
            o->setProperty (fieldNames[k], *list[k]);
        o->setProperty ("explicit", explicitLyrics);
        return juce::var (o);
    }

    Info Info::fromSong (const Song& song, const juce::String& defaultArtist)
    {
        Info i;
        const auto& v = song.release;
        if (v.isObject())
        {
            const auto list = fields (i);
            for (size_t k = 0; k < list.size(); ++k)
                if (v.hasProperty (fieldNames[k]))
                    *list[k] = v[fieldNames[k]].toString();
            i.explicitLyrics = (bool) v.getProperty ("explicit", false);
        }
        if (i.title.isEmpty())  i.title = song.title;
        if (i.artist.isEmpty()) i.artist = defaultArtist;
        if (i.lyrics.isEmpty()) i.lyrics = lyricsFromSong (song);
        if (i.releaseDate.isEmpty())
            i.releaseDate = (juce::Time::getCurrentTime() + juce::RelativeTime::days (28)).formatted ("%Y-%m-%d");
        if (i.pLine.isEmpty() && i.artist.isNotEmpty()) i.pLine = juce::String::fromUTF8 ("\xe2\x84\x97 ") + i.year() + " " + i.artist;
        if (i.cLine.isEmpty() && i.artist.isNotEmpty()) i.cLine = juce::String::fromUTF8 ("\xc2\xa9 ") + i.year() + " " + i.artist;
        return i;
    }

    juce::String Info::displayTitle() const
    {
        return version.trim().isEmpty() ? title.trim() : title.trim() + " (" + version.trim() + ")";
    }

    juce::String Info::year() const
    {
        return checkDate (releaseDate) ? releaseDate.substring (0, 4) : juce::Time::getCurrentTime().formatted ("%Y");
    }

    juce::StringArray subGenres()
    {
        return { "Contemporary Gospel", "Praise & Worship", "African Gospel", "Traditional Gospel", "Choir & Hymns",
                 "Urban Gospel", "Gospel Highlife", "Makossa Gospel", "Instrumental Worship" };
    }

    juce::StringArray languages()
    {
        return { "English", "French", "Pidgin English", "English and French", "Ewondo", "Duala", "Bamileke (Ghomala')",
                 "Lingala", "Yoruba", "Igbo", "Swahili", "Instrumental (no words)" };
    }

    juce::String lyricsFromSong (const Song& song)
    {
        juce::StringArray parts;
        for (const auto& s : song.sections)
            if (s.lyrics.trim().isNotEmpty())
                parts.add (s.lyrics.trim());
        return parts.joinIntoString ("\n\n");
    }

    //==========================================================================
    bool checkIsrc (const juce::String& text, juce::String& clean)
    {
        clean = text.toUpperCase().removeCharacters (" -");
        if (clean.length() != 12)
            return false;
        for (int i = 0; i < 12; ++i)
        {
            const auto c = clean[i];
            const bool letter = c >= 'A' && c <= 'Z', digit = c >= '0' && c <= '9';
            if (i < 2 && ! letter) return false;                    // country
            if (i >= 2 && i < 5 && ! (letter || digit)) return false; // registrant
            if (i >= 5 && ! digit) return false;                    // year and number
        }
        return true;
    }

    bool checkUpc (const juce::String& text, juce::String& clean)
    {
        clean = text.removeCharacters (" -");
        if ((clean.length() != 12 && clean.length() != 13) || ! clean.containsOnly ("0123456789"))
            return false;
        // GS1 check digit: from the right, weights 3 and 1
        int sum = 0;
        const int n = clean.length();
        for (int i = n - 2, w = 3; i >= 0; --i, w = (w == 3 ? 1 : 3))
            sum += (clean[i] - '0') * w;
        return (10 - sum % 10) % 10 == clean[n - 1] - '0';
    }

    bool checkDate (const juce::String& d)
    {
        if (d.length() != 10 || d[4] != '-' || d[7] != '-')
            return false;
        const auto y = d.substring (0, 4).getIntValue(), m = d.substring (5, 7).getIntValue(), day = d.substring (8, 10).getIntValue();
        return y >= 1900 && y <= 2100 && m >= 1 && m <= 12 && day >= 1 && day <= 31;
    }

    CoverInfo checkCover (const juce::File& file)
    {
        CoverInfo c;
        if (! file.existsAsFile())
        {
            c.problem = "No cover art chosen yet.";
            return c;
        }
        c.found = true;
        c.format = file.getFileExtension().toLowerCase().trimCharactersAtStart (".");
        auto image = juce::ImageFileFormat::loadFrom (file);
        if (! image.isValid())
        {
            c.problem = "The cover could not be opened. Use a JPG or PNG picture.";
            return c;
        }
        c.width = image.getWidth();
        c.height = image.getHeight();
        if (c.format != "jpg" && c.format != "jpeg" && c.format != "png")
            c.problem = "Stores take JPG or PNG covers. This one will be saved as JPG.";
        if (c.width != c.height)
            c.problem = "The cover must be square. This one is " + juce::String (c.width) + " x " + juce::String (c.height) + ".";
        else if (c.width < 1400)
            c.problem = "The cover is too small (" + juce::String (c.width) + " x " + juce::String (c.height) + "). Stores need 3000 x 3000.";
        c.usable = c.width == c.height && c.width >= 1400;
        return c;
    }

    bool writeCover (const juce::File& source, const juce::File& dest, juce::String& problem)
    {
        const auto info = checkCover (source);
        if (! info.usable)
        {
            problem = info.problem;
            return false;
        }
        // Drawn onto a fresh, cleared 3000 x 3000 picture (so no edge pixel is left undefined)
        const auto original = juce::ImageFileFormat::loadFrom (source);
        juce::Image image (juce::Image::RGB, 3000, 3000, true);
        {
            juce::Graphics g (image);
            g.fillAll (juce::Colours::black);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (original, 0, 0, 3000, 3000, 0, 0, original.getWidth(), original.getHeight());
        }

        dest.deleteFile();
        juce::FileOutputStream out (dest);
        juce::JPEGImageFormat jpeg;
        jpeg.setQuality (0.95f);
        if (! out.openedOk() || ! jpeg.writeImageToStream (image, out))
        {
            problem = "The cover could not be written.";
            return false;
        }
        return true;
    }

    double leadingSilenceSeconds (const juce::File& file)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
        if (r == nullptr)
            return 0.0;
        const auto limit = (juce::int64) juce::jmin ((double) r->lengthInSamples, r->sampleRate * 30.0);
        juce::AudioBuffer<float> b ((int) r->numChannels, 4096);
        for (juce::int64 pos = 0; pos < limit; pos += 4096)
        {
            const auto n = (int) juce::jmin ((juce::int64) 4096, limit - pos);
            r->read (&b, 0, n, pos, true, true);
            for (int c = 0; c < b.getNumChannels(); ++c)
            {
                const auto* d = b.getReadPointer (c);
                for (int i = 0; i < n; ++i)
                    if (std::abs (d[i]) > 0.001f)          // -60 dB
                        return (pos + i) / r->sampleRate;
            }
        }
        return limit / r->sampleRate;
    }

    //==========================================================================
    std::vector<Check> check (const Info& i, const Analysis::FileAnalysis* master, double silence)
    {
        std::vector<Check> out;
        auto add = [&out] (Level l, const juce::String& t) { out.push_back ({ l, t }); };

        // The song
        if (i.title.trim().isEmpty())
            add (Level::problem, "Title: missing.");
        else if (i.title.containsIgnoreCase ("feat") || i.title.containsIgnoreCase ("ft.") || i.title.contains (" x "))
            add (Level::problem, "Title: put other artists in \"Featuring\", not in the title (stores refuse \"feat.\" in titles).");
        else if (i.title == i.title.toUpperCase() && i.title.length() > 3)
            add (Level::warning, "Title: written all in capitals. Stores ask for normal capitals (\"Jehovah Is Faithful\").");
        else
            add (Level::ok, "Title: " + i.displayTitle());

        if (i.version.containsIgnoreCase ("official") || i.version.containsIgnoreCase ("audio"))
            add (Level::problem, "Version: words like \"Official\" or \"Audio\" are not allowed. Use \"Live\", \"Acoustic\", \"Radio edit\" or leave it empty.");

        if (i.artist.trim().isEmpty())
            add (Level::problem, "Artist: missing.");
        else
            add (Level::ok, "Artist: " + i.artist.trim() + (i.featured.trim().isNotEmpty() ? ", featuring " + i.featured.trim() : juce::String()));

        if (i.songwriters.trim().isEmpty())
            add (Level::problem, "Songwriters: stores need the real full name of whoever wrote the words and music.");
        else if (! i.songwriters.contains (" "))
            add (Level::warning, "Songwriters: use full real names (first and last name), not a stage name.");
        else
            add (Level::ok, "Songwriters: " + i.songwriters.trim());

        add (i.language.isNotEmpty() ? Level::ok : Level::problem, "Language of the words: " + (i.language.isNotEmpty() ? i.language : juce::String ("missing")));
        add (Level::ok, "Genre: " + i.genre + " / " + i.subGenre);

        // Dates and codes
        if (! checkDate (i.releaseDate))
            add (Level::problem, "Release date: write it as YYYY-MM-DD (for example 2026-12-25).");
        else
        {
            const auto rel = juce::Time (i.releaseDate.substring (0, 4).getIntValue(), i.releaseDate.substring (5, 7).getIntValue() - 1,
                                         i.releaseDate.substring (8, 10).getIntValue(), 0, 0);
            const auto days = (rel - juce::Time::getCurrentTime()).inDays();
            if (days < 7)
                add (Level::warning, "Release date: less than a week away. Distributors need time; 3 to 4 weeks lets you pitch it to playlists.");
            else if (days < 21)
                add (Level::info, "Release date: " + i.releaseDate + ". 3 to 4 weeks ahead is best for playlist pitching.");
            else
                add (Level::ok, "Release date: " + i.releaseDate);
        }

        juce::String clean;
        if (i.isrc.trim().isEmpty())
            add (Level::info, "ISRC: empty. Your distributor gives one for free (keep it for any later re-release).");
        else if (! checkIsrc (i.isrc, clean))
            add (Level::problem, "ISRC: \"" + i.isrc + "\" is not valid. It is 12 characters, like CM-ABC-26-00001.");
        else
            add (Level::ok, "ISRC: " + clean);

        if (i.upc.trim().isEmpty())
            add (Level::info, "UPC: empty. Your distributor gives one for free.");
        else if (! checkUpc (i.upc, clean))
            add (Level::problem, "UPC: \"" + i.upc + "\" is not valid (12 or 13 digits, and the last digit must match).");
        else
            add (Level::ok, "UPC: " + clean);

        if (i.pLine.trim().isEmpty() || i.cLine.trim().isEmpty())
            add (Level::warning, juce::String::fromUTF8 ("Copyright: fill in both \xe2\x84\x97 (the recording) and \xc2\xa9 (the song)."));
        else
            add (Level::ok, "Copyright: " + i.pLine + "  /  " + i.cLine);

        if (i.explicitLyrics)
            add (Level::info, "Marked explicit. Gospel songs are normally \"Clean\".");

        // Lyrics and cover
        if (i.language.startsWith ("Instrumental"))
            add (Level::ok, "Lyrics: none (instrumental).");
        else if (i.lyrics.trim().isEmpty())
            add (Level::warning, "Lyrics: empty. Add them so Spotify, Apple Music and Boomplay can show them.");
        else
            add (Level::ok, "Lyrics: " + juce::String (juce::StringArray::fromLines (i.lyrics.trim()).size()) + " lines.");

        const auto cover = checkCover (juce::File (i.coverPath));
        if (! cover.found)
            add (Level::problem, "Cover art: choose a square picture, 3000 x 3000 (JPG or PNG).");
        else if (! cover.usable)
            add (Level::problem, "Cover art: " + cover.problem);
        else if (cover.width < 3000)
            add (Level::warning, "Cover art: " + juce::String (cover.width) + " x " + juce::String (cover.height) + ". Most stores ask for 3000 x 3000.");
        else
            add (Level::ok, "Cover art: " + juce::String (cover.width) + " x " + juce::String (cover.height)
                            + (cover.width > 3000 ? " (saved as 3000 x 3000)" : ""));
        if (cover.found)
            add (Level::info, "Cover art rules: no website names, phone numbers, prices or blurry pictures; the names on it must match the artist and title.");

        // The master audio
        if (master != nullptr && master->ok)
        {
            if (master->seconds < 30.0)
                add (Level::warning, "Length: " + fmtTime (master->seconds) + ". Songs under 30 seconds earn nothing on most stores.");
            else
                add (Level::ok, "Length: " + fmtTime (master->seconds));
            if (master->truePeakDb > -0.1f)
                add (Level::problem, "Peaks: the master reaches " + juce::String (master->truePeakDb, 1) + " dB and clips. Turn on Mastering (it keeps peaks under -1 dB).");
            else if (master->truePeakDb > -1.0f)
                add (Level::warning, "Peaks: " + juce::String (master->truePeakDb, 1) + " dB. Stores prefer -1 dB or lower so the MP3 versions don't distort.");
            else
                add (Level::ok, "Peaks: " + juce::String (master->truePeakDb, 1) + " dB");
            if (master->lufs < -20.0f)
                add (Level::warning, "Loudness: " + juce::String (master->lufs, 1) + " LUFS is quiet next to other songs. Mastering can bring it to about -14.");
            else
                add (Level::ok, "Loudness: " + juce::String (master->lufs, 1) + " LUFS");
            if (silence > 2.0)
                add (Level::warning, "Silence at the start: " + juce::String (silence, 1) + " s. Stores like the music to start within 2 seconds.");
        }
        else
        {
            add (Level::info, "Audio: checked when the package is made (length, peaks, loudness, silence at the start).");
        }
        return out;
    }

    bool hasProblems (const std::vector<Check>& checks)
    {
        for (auto& c : checks)
            if (c.level == Level::problem)
                return true;
        return false;
    }

    //==========================================================================
    juce::String detailsText (const Info& i, const Analysis::FileAnalysis& m, const Song& song)
    {
        juce::String t;
        auto line = [&t] (const juce::String& name, const juce::String& value)
        {
            t << name.paddedRight (' ', 26) << (value.isEmpty() ? juce::String ("-") : value) << "\n";
        };
        t << "SONG DETAILS FOR DISTRIBUTORS\n";
        t << "Made with Gospel Production Studio on " << juce::Time::getCurrentTime().formatted ("%Y-%m-%d") << "\n\n";
        line ("Title", i.title);
        line ("Version", i.version);
        line ("Primary artist", i.artist);
        line ("Featuring", i.featured);
        line ("Songwriters / composers", i.songwriters);
        line ("Producer", i.producer);
        line ("Mixing & mastering", i.engineer);
        line ("Label", i.label.isNotEmpty() ? i.label : i.artist);
        line ("Genre", i.genre);
        line ("Sub-genre", i.subGenre);
        line ("Language of the lyrics", i.language);
        line ("Explicit", i.explicitLyrics ? "Yes" : "No (clean)");
        line ("Release date", i.releaseDate);
        line ("ISRC", i.isrc.isNotEmpty() ? i.isrc : "(the distributor assigns it)");
        line ("UPC", i.upc.isNotEmpty() ? i.upc : "(the distributor assigns it)");
        line (juce::String::fromUTF8 ("\xe2\x84\x97 line"), i.pLine);
        line (juce::String::fromUTF8 ("\xc2\xa9 line"), i.cLine);
        line ("Preview / TikTok start", i.clipStart);
        t << "\n";
        line ("Key", song.key);
        line ("Tempo", juce::String (song.tempo) + " BPM, " + song.timeSignature);
        if (m.ok)
        {
            line ("Length", fmtTime (m.seconds));
            line ("Loudness", juce::String (m.lufs, 1) + " LUFS");
            line ("Peak", juce::String (m.truePeakDb, 1) + " dB");
        }
        if (song.scripture.isNotEmpty())
            line ("Scripture", song.scripture);
        t << "\nFILES\n";
        t << "  " << i.displayTitle() << ".wav     master, 24-bit 44.1 kHz WAV (upload this one)\n";
        t << "  " << i.displayTitle() << ".mp3     320 kbps MP3 with the details and cover inside (for WhatsApp, radio, your website)\n";
        t << "  Cover (3000x3000).jpg\n";
        t << "  Lyrics.txt\n";
        return t;
    }

    juce::var detailsJson (const Info& i, const Analysis::FileAnalysis& m, const Song& song)
    {
        auto v = i.toVar();
        if (auto* o = v.getDynamicObject())
        {
            o->removeProperty ("coverPath");
            o->setProperty ("key", song.key);
            o->setProperty ("tempo", song.tempo);
            o->setProperty ("timeSignature", song.timeSignature);
            if (m.ok)
            {
                o->setProperty ("lengthSeconds", m.seconds);
                o->setProperty ("loudnessLufs", m.lufs);
                o->setProperty ("peakDb", m.truePeakDb);
            }
        }
        return v;
    }
}
