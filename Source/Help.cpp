#include "Help.h"
#include "StudioLookAndFeel.h"

namespace Help
{
    const std::vector<Topic>& topics()
    {
        static const std::vector<Topic> t {
            // ---- Songs ----------------------------------------------------------
            { "How do I start a new song?", "new song create nouveau chanson start begin",
              "1. Click Song Library at the top.\n2. Press \"+ New Song\".\n3. Type the title, then choose Key, Tempo, Time signature, Theme and Status.\n4. Press \"Open in Studio\" to start recording and arranging.\n\nEverything is saved by itself.", "Songs" },
            { "How do I keep a melody idea before I forget it?", "voice memo idea melody record quickly memo idee",
              "1. Song Library: choose the song (or make a new one).\n2. Press \"Record Memo\" and sing or play the idea.\n3. Press it again to stop. \"Play\" listens back.\n\nThe memo stays with the song. You can write the lyrics underneath, section by section (Verse, Chorus, Bridge...).", "Songs" },
            { "How do I write the lyrics?", "lyrics words paroles sections verse chorus",
              "In the Song Library, under the song's details:\n1. Press \"+ Add Section\" and choose Verse, Chorus, Bridge...\n2. Type the words in the box.\n3. Use Up and Down to change the order.\n\nThe lyrics are used again in the Release package (Lyrics.txt).", "Songs" },
            { "How do I find a song again?", "search find song library filter status theme chercher",
              "Song Library: type in the search box (title, lyrics or scripture). Use the two boxes under it to show only one status (Idea, Written, Recorded...) or one theme.", "Songs" },

            // ---- Recording -------------------------------------------------------
            { "How do I record my voice?", "record voice sing vocal microphone enregistrer voix micro chanter",
              "1. Plug in your microphone (an audio interface or the laptop's input) and check it in Audio Setup.\n2. In the Studio, on the Lead Vocal track press R (record arm). The meter moves when you sing.\n3. Choose a Count-in (1 bar is good) and turn on the Click if you want the beat.\n4. Press the red Record button, sing, then press Stop.\n\nRecord again over the same place: each pass becomes a new take.", "Recording" },
            { "How do I record only one part again (punch-in)?", "punch in fix mistake correct part replace re-record",
              "1. Set the loop around the part to fix (Loop from bar, Loop length).\n2. Press \"Punch\".\n3. Arm the track (R) and press Record: only the loop part is recorded; the rest of the take is kept.", "Recording" },
            { "How do I choose the best take?", "takes best take choose comp comping meilleure prise",
              "Right-click the clip > \"Takes\" and pick one, or \"Comp takes (choose the best of each part)...\" to take the verse from take 2 and the chorus from take 3. \"Keep this take, delete the others...\" tidies up.", "Recording" },
            { "My recording is late or out of time. How do I fix the timing?", "timing late early rhythm beat quantise audio correction tempo decale",
              "Right-click the recording > \"Timing correction (line up to the beat)...\".\n1. Choose the grid (1/8, 1/16, triplets).\n2. Strength 40-60 % tightens but keeps the feel; 100 % is exactly on the grid.\n3. Press Apply. \"Remove timing correction\" brings back the original.", "Recording" },
            { "How do I make my voice in tune (pitch correction)?", "tune pitch autotune off key flat sharp justesse accorder voix",
              "Right-click the vocal track > \"Vocal tuning (correct the pitch)...\".\n- Strength: how much it pulls to the right note.\n- Speed: fast is the modern effect, slow is natural.\n- It follows the song's key.\nStart with Strength 60-80 % and Speed 60 ms.", "Recording" },
            { "How do I make harmonies or a choir from my voice?", "harmony choir alto tenor bass backing vocals harmonie chorale voix",
              "Right-click your sung clip > \"Harmony and choir stacks...\".\n1. Tick the parts (Alto, Tenor, Bass, Third above...) or choose a ready-made choir.\n2. Singers in each part: 2-3 for a choir sound. Tight - loose for how live it sounds.\n3. Press \"Make the harmonies\". Each part gets its own track through a Choir bus.\n\nThe parts follow the chords on your Chord Track, so put the chords there first.", "Recording" },
            { "How do I import an audio file or a loop?", "import audio file wav mp3 loop drag drop importer",
              "Drag the file from your folder onto a track, or right-click a track > \"Import audio file at the playhead...\". For a loop that should follow the song's tempo and key: \"Import a loop (follows the song's tempo)...\".", "Recording" },

            // ---- Chords and notes --------------------------------------------------
            { "How do I put chords in the song?", "chords chord track palette progression accords grille",
              "1. In the Studio, the Chords lane is under the Sections.\n2. Open the Chord palette (right side). Click a chord to hear it, then \"Add\" or drag it onto the Chords lane.\n3. \"Progressions\" has ready gospel progressions: choose one and place it.\n\nChords are stored as numbers in the key (1, 4, 5...), so changing the Key moves them all.", "Chords" },
            { "How do I find the chords of a recording or a song I want to learn?", "find chords detect recognise chord detection learn song accords trouver reconnaitre",
              "1. Import the song (or record it) onto a track, starting at bar 1.\n2. Set the song's Tempo to the recording's tempo (the panel tells you if it differs).\n3. Right-click the clip > \"Find the chords (put them on the Chord Track)...\".\n4. Read the list (\"not sure\" means check by ear) and press \"Put them on the Chord Track\".\n\nIt also works on MIDI clips. It hears major, minor, 7, maj7, m7, sus4, dim and slash chords; rich gospel chords come out simplified.", "Chords" },
            { "How do I turn the chords into piano and bass notes?", "chords to notes piano bass write notes voicing",
              "Chord palette > \"Chords to notes...\": it writes the Chord Track as notes on the Keys and Bass tracks, with the voicing you choose (Gospel, Shell, Spread...). For a full band with rhythm, use \"Band...\" instead.", "Chords" },
            { "How do I edit notes (piano roll)?", "piano roll midi notes edit velocity quantise",
              "Double-click a MIDI clip (or right-click > Open in piano roll). Click to add a note, drag to move or lengthen, right-click to delete. Quantise lines notes up with the grid; the chord check shows notes outside the chord.", "Chords" },
            { "How do I change the key or the tempo?", "key tempo bpm change transpose tonalite vitesse",
              "At the top of the Studio: Key (chords and band follow it) and Tempo (type it, use - and +, or Tap in time). The Time signature is next to it.", "Chords" },

            // ---- Band, drums and instruments -------------------------------------------
            { "How do I get a backing band to play my chords?", "band backing players accompaniment orchestra musicians groupe accompagnement",
              "1. Put chords on the Chord Track (and Verse/Chorus in the Sections lane).\n2. Press \"Band...\" next to Snap.\n3. Choose a style (Praise, Worship ballad, Makossa, Bikutsi, Assiko, Njang...) and the players (Bass, Piano, Organ, Guitar, Drums).\n4. Press \"Make the band\".\n\nVerses are lighter and choruses fuller, there are fills before each new section and an ending on the last chord. Every note can be edited.", "Band" },
            { "How do I program drums?", "drums beat programming groove rhythm batterie rythme",
              "Double-click a clip on the Drums track: the drum grid opens.\n- Click a box to add a hit, Shift-click for an accent.\n- \"Groove\" starts from a ready groove (Church, Cameroon, West Africa, Central Africa).\n- \"Add a fill\", \"Humanise\", \"Repeat bar\", and Swing for a shuffle feel.", "Band" },
            { "Where are the makossa, bikutsi and other African rhythms?", "makossa bikutsi assiko bend skin njang mangambeu highlife afrobeat soukous rumba african cameroon rythmes africains",
              "In the drum grid press \"Groove\": the Cameroon group has Makossa, Assiko, Bend skin, Mangambeu, Bikutsi (6/8) and Njang (12/8); West Africa has Highlife, Ghana praise, Afrobeat, Igbo and Yoruba praise; Central Africa has Soukous and Rumba. In \"Band...\" the same styles play with bass, piano, organ and guitar.\n\nTip: choose the right time signature first (6/8 for bikutsi, 12/8 for njang).", "Band" },
            { "How do I add congas, shakers, talking drum or bells?", "percussion congas shaker talking drum bell djembe tambourine percussions",
              "The Drum Kit plays them itself. In the drum grid: Groove > \"Show the African percussion rows\". For a separate track: \"+ Track\" > \"African Percussion\". There is also a Balafon you play on the keys.", "Band" },
            { "How do I choose or change an instrument?", "instrument sound piano organ guitar bass change sound instrument",
              "Click the instrument name under the track's name (or right-click the track). Choose Grand Piano, Gospel Organ, Electric Bass, Clean Electric or Nylon Guitar, Drum Kit, African Percussion, Balafon... For the organ, the panel has drawbars and presets.", "Band" },
            { "Can I change the notes the harmony parts sing?", "harmony edit note change move alto tenor bass soprano wrong note voice choir part",
              "Yes. Right-click the lead vocal clip > Harmony and choir stacks. In the picture, click a coloured note of a part: the line under the picture says which note it sings. Use the arrows (or the Up and Down keys) to move it a semitone, \"Chord note\" (or Shift + arrow) to jump to the next note of the chord, or drag it up or down. Left and Right keys go to the next note. A white edge shows the notes you moved. \"Automatic\" gives the chosen note back to the app; \"Reset all edits\" clears them all. Then press \"Make the harmonies\" to hear them sung. Your changes are kept with the clip.", "Recording" },
            { "Licence and credits", "licence license credits copyright open source agpl gpl source code who made sounds",
              "Gospel Production Studio is free software under the GNU AGPL version 3, built on JUCE (AGPLv3), Tracktion Engine (GPLv3), the ASIO SDK (GPLv3) and LAME (LGPL). Its source code is at github.com/nembuanyik-lang/GospelProductionStudio. Sounds: Salamander Grand Piano by Alexander Holm (CC BY 3.0), MuldjordKit drums by Lars Muldjord and the DrumGizmo team (CC BY 4.0), FreePats and the Versilian Community Sample Library (CC0). Real-drummer feels: Groove MIDI Dataset by Google LLC (CC BY 4.0). The full list is in NOTICES.md in the app's folder.", "Setup" },
            { "How do I learn to use the app?", "learn course lesson lessons tutorial teach training beginner start how to use step by step guide practice handbook",
              "Click \"Learn\" at the top of the window. The course has 10 modules (getting started, songs, chords, the band, recording, harmony, editing, mixing, mastering and release, keeping your work safe) with short lessons. Start a lesson and a small card shows one step at a time: \"Show me\" makes the right button glow, and \"Your turn\" tasks tick themselves when you have done them. Lessons use a \"Practice Song\", so your real songs are never changed. Your progress is kept, so you can stop and continue later. Drag the card by its top edge if it covers something. \"Print the handbook\" on the Learn page opens every lesson in your web browser: press Ctrl+P to print it or save it as a PDF.", "Setup" },
            { "How do I make the band sound more alive?", "band livelier alive answer singer fills runs licks ghost notes crash open hihat build grows sparse verse last chorus lift same every bar boring",
              "Click \"Band...\" in the Studio. Three switches make the band sound like musicians listening to each other. \"Answer the singer\": short piano or guitar runs, or a bass walk, in the gaps where the singer rests (from a track called Melody, Lead or Vocal; with no melody yet, at the end of each 4-bar phrase). \"Drum variety\": a crash at each new section, an open hi-hat at the end of each phrase, soft ghost notes on the snare in choruses. \"The song grows\": the first verse is sparer (no guitar or organ yet), the band builds into each new section, and the last chorus lifts. Untick them for a steady band. Then click \"Make the band\" again.", "Band" },
            { "How do I make the drums feel like a real drummer?", "humanise humanize feel real drummer groove timing late early accents gospel highlife afrobeat swing",
              "In the drum grid, click Humanise. \"A little, at random\" adds small changes. The \"feel\" choices (Gospel, Soul, Highlife, Afrobeat...) move each hit the way real drummers play that style: the snare a little late or early, the hi-hat stronger on the beat. It works on any groove, African ones too. \"Back to the grid\" puts the timing back. For grooves played by real drummers, click Groove, then \"Played by real drummers\". The feels and grooves come from the Groove MIDI Dataset by Google LLC (Magenta), licensed CC BY 4.0.", "Band" },
            { "How do I get recorded congas, shakers, bells and balafon?", "percussion recorded real congas shakers bells agogo balafon download pack djembe talking drum",
              "Open Sounds (top right) and download \"African Percussion\". African Percussion tracks, the drum grid's percussion rows and the Balafon then play recordings of congas, shakers, cabasa, tambourine, agogo bells, cowbell, claves, woodblock and claps. The djembe and talking drum keep the built-in sound (there is no free recording of them yet), and so does any sound the pack does not have. Until it is downloaded, everything plays the built-in percussion.", "Band" },
            { "How do I get the real recorded guitars?", "guitar recorded real samples electric nylon download pack sound library built-in",
              "Open Sounds (top right) and download \"Clean Electric Guitar\" or \"Nylon Guitar\" in the Guitars group. Until a guitar pack is downloaded, guitar tracks and the band's guitar play the built-in guitar, so songs always have a guitar. Once it is downloaded, every Clean Electric or Nylon Guitar track plays the recorded guitar, including songs you made before.", "Band" },
            { "Why is my piano or drum kit silent or simple?", "download sound pack missing silent sound library telecharger",
              "The real piano, bass and drum sounds are downloaded once: press \"Sounds\" at the top and download the packs (do it on Wi-Fi). Until then a simple keyboard sound plays.", "Band" },
            { "How do I use my own samples?", "my sounds samples own sounds sampler echantillons",
              "Choose the instrument \"My Sounds\", then drag your WAV files onto the panel. One-shot sounds go on keys one by one; a melodic sound is spread over the keyboard from its note.", "Band" },
            { "How do I use VST3 plugins?", "vst vst3 plugin third party kontakt effects external",
              "Press \"Plugins\" at the top and scan the computer. Then use a plugin as an instrument (track instrument menu) or as an effect in the track's effects.", "Band" },

            // ---- Mixing --------------------------------------------------------------
            { "How do I make the vocal sound professional?", "vocal chain eq compressor de-esser reverb mix voice voix pro",
              "Right-click the vocal track > Vocal chain and choose one (for example \"Gospel lead\"). It adds EQ, de-esser, compressor and reverb with good settings. Then adjust: more reverb for worship, less for fast praise.", "Mixing" },
            { "How do I open the mixer?", "mixer faders volume pan levels melangeur",
              "Press \"Mixer\" (top right of the Studio). Each track has a fader, pan, mute, solo, effects and sends to the shared reverb and delay. The master strip has the meter and the Analyser.", "Mixing" },
            { "How do I group tracks (choir bus, drum bus)?", "bus group buses choir bus submix groupe",
              "In the mixer press \"+ Bus\" and choose Choir, Vocal, Band or Drum. Then click a track's output line (under its fader) and choose the bus. One fader then controls the whole group.", "Mixing" },
            { "How do I make the bass duck under the kick (sidechain)?", "sidechain duck ducking pump kick bass",
              "Right-click the track that should get quieter (for example Bass) > \"Duck under another track (sidechain)\" and choose what it ducks under (Drums) and how (Kick tight, Gentle...).", "Mixing" },
            { "How do I make the volume change during the song (automation)?", "automation volume change fade write draw automatisation",
              "Press A on the track to show its line, choose the setting (Volume, Pan...) and click to add points. Or press \"Write\" and move the fader while the song plays.", "Mixing" },
            { "How do I compare my mix with a professional song?", "reference analyser compare mix spectrum loudness comparer",
              "Mixer > master strip > \"Analyser\". Choose a reference song, then switch A (your mix) and B (the reference) while playing. The tone balance bars tell you what is too strong or too weak.", "Mixing" },
            { "How do I split a song into voice, drums and bass (stems)?", "stems separate split voice drums bass instrumental karaoke remove vocals separer",
              "Right-click a stereo recording > \"Split into stems (voice, drums, bass, other)...\" and press \"Split into stems\". Four tracks appear under it; the original is muted. Mute the Voice to sing along.\n\nIt works without internet, so it is less clean than online AI tools: some of each part is heard in the others.", "Mixing" },

            // ---- Release -------------------------------------------------------------
            { "How do I export an MP3 or WAV?", "export mp3 wav bounce render share whatsapp exporter",
              "Press \"Export\" (top of the Studio). Choose the whole song or the loop, WAV or MP3, and press Export. The file goes to Documents/Gospel Production Studio/Exports. \"Each track on its own (stems...)\" exports every track separately.", "Release" },
            { "How do I master my song (make it loud and polished)?", "mastering loud loudness lufs master polish",
              "Open the Mixer and press \"Mastering\" on the Master strip. Choose a starting point, then \"Set loudness for the whole song\" (-14 LUFS is right for streaming). Use Compare to hear before and after.", "Release" },
            { "How do I prepare the song for DistroKid, Boomplay or Audiomack?", "release distributor distrokid boomplay audiomack tunecore isrc upc cover streaming sortie",
              "Export > \"Release package for distributors...\". Fill in the details (real songwriter names, ISRC if you have one), choose a square cover (3000 x 3000 is best), fix any red lines, then press \"Make the release package\". You get the master WAV, a tagged MP3, the cover, lyrics and a details sheet in one folder.", "Release" },

            // ---- Setup and problems -----------------------------------------------------
            { "How do I choose my sound card or microphone input?", "audio setup sound card interface asio input output microphone carte son",
              "Press \"Audio Setup\" at the top. Choose the device (ASIO if your interface has it), the input for your microphone and the output for your headphones or speakers.", "Setup" },
            { "There is a delay when I play or sing (latency).", "latency delay lag slow echo buffer retard",
              "Audio Setup: choose an ASIO driver if your interface has one, and a smaller buffer size (128 or 256). If the sound crackles, make the buffer bigger again. Close other programs.", "Setup" },
            { "My recordings are slightly behind the beat.", "recording late behind beat offset delay timing out of time sloppy latency correction cable loopback",
              "Every audio interface delays the sound a little, and drivers do not always report it exactly, so recordings can land a few milliseconds late. Open Engine & Devices and press \"Check recording delay...\". Connect a cable from Output 1 of your interface to Input 1, turn the speakers down, and press Start: the app measures the delay and, when you press \"Use ... correction\", moves every new recording by that much. Measure again if you change the interface, the driver or the buffer size. Without a cable you can set the correction by hand.", "Setup" },
            { "The sound crackles or stops.", "crackle glitch dropout noise cpu slow craquement",
              "Audio Setup: make the buffer size bigger (512). Plug in the laptop's charger, close other programs, and freeze heavy plugins by exporting the track if needed.", "Setup" },
            { "I hear nothing.", "no sound silent nothing hear muet rien",
              "1. Audio Setup: check the output device and press the Test Tone on the Engine & Devices page.\n2. Check that no track is muted (M) or soloed (S) by mistake, and the master fader is up.\n3. For instruments, check the sound packs are downloaded (Sounds).", "Setup" },
            { "How do I use a MIDI keyboard?", "midi keyboard controller clavier",
              "Plug it in before starting the app (or check Audio Setup > MIDI inputs). The armed track (R) plays it. The on-screen keyboard (Show keyboard) also works with the computer keys.", "Setup" },
            { "How do I use the click (metronome) and count-in?", "click metronome count in beat clic",
              "Studio: the Click button turns it on; choose the sound and volume next to it. Count-in gives 1 or 2 bars before recording starts.", "Setup" },
            { "How do I loop a part of the song?", "loop repeat cycle part boucle",
              "Press the loop button next to Record, then set \"Loop from bar\" and \"Loop length\". Playback repeats that part; recording in a loop makes takes.", "Setup" },
            { "How do I undo a mistake?", "undo redo mistake ctrl z annuler",
              "Ctrl+Z undoes, Ctrl+Y redoes (also the Undo and Redo buttons above the tracks). It works for notes, clips, chords, mixing and the band.", "Setup" },
            { "What are the keyboard shortcuts?", "shortcuts keys keyboard space raccourcis",
              "Space: play / stop.  R (on a track): record arm.  S: split a clip at the playhead.  Ctrl+Z / Ctrl+Y: undo / redo.  Delete: delete the selected clip.  Ctrl + mouse wheel: zoom.  Esc: close a page.  F1: this help.", "Setup" },
            { "How do I update the app?", "update new version upgrade mise a jour",
              "Press \"Updates\" at the top. When a new version is on GitHub you are asked to install it. Your songs are kept.", "Setup" },
            { "Where are my files saved?", "files saved folder documents backup where sauvegarde dossier",
              "Documents/Gospel Production Studio: Songs (each song in its own folder, with its recordings and its Versions), Exports and Releases. The app keeps versions of each song on this laptop; to be safe if the laptop itself is lost or broken, also copy that folder to a USB key or Google Drive now and then.", "Setup" },
            { "How do I go back to an earlier version of my song?", "version versions backup undo go back restore earlier yesterday mistake deleted lost old copy history",
              "In the Studio, press \"Versions\" beside the song title. The app keeps a copy when you open the song, every 10 minutes while you work, and when you close it; \"Save a version now...\" keeps one with your own name (e.g. \"Before the choir\"). Choose one and press \"Go back to this version\": the song as it is now is kept as a version first, so you can always come back. Named versions are kept until you delete them; the automatic ones are thinned out after a month.", "Songs" },
            { "The power went off (or the app crashed). Is my song lost?", "power cut crash closed suddenly lost song recover recovery battery died blackout nepa",
              "No. The song is saved every few seconds, so at most the last few seconds are missing. When you start the app again it says it did not close properly and offers to open the song. If you were recording, the recording is rescued onto a new track called \"Rescued recording\" at the place where you started. If the song file was being saved at that very moment, it is repaired from the copy being written or from the newest version.", "Songs" },
        };
        return t;
    }

    namespace
    {
        juce::StringArray words (const juce::String& text)
        {
            static const juce::StringArray stop { "how", "do", "i", "a", "an", "the", "to", "my", "is", "it", "in", "on", "of", "and", "or", "can",
                                                  "what", "where", "why", "for", "with", "me", "you", "comment", "je", "le", "la", "les", "un",
                                                  "une", "de", "du", "des", "mon", "ma", "mes", "est", "et", "pour", "faire", "make", "get" };
            juce::StringArray out;
            auto clean = text.toLowerCase().replaceCharacters ("?!.,;:()'\"/-", "            ");
            for (auto w : juce::StringArray::fromTokens (clean, " ", ""))
            {
                w = w.trim();
                if (w.length() < 2 || stop.contains (w)) continue;
                out.add (w);
            }
            return out;
        }

        bool matches (const juce::String& word, const juce::StringArray& candidates)
        {
            for (auto& c : candidates)
                if (c == word || (word.length() >= 4 && c.startsWith (word)) || (c.length() >= 4 && word.startsWith (c)))
                    return true;
            return false;
        }
    }

    std::vector<int> search (const juce::String& question, int maxResults)
    {
        const auto& list = topics();
        std::vector<int> result;
        const auto q = words (question);
        if (q.isEmpty())
        {
            for (int i = 0; i < (int) list.size(); ++i) result.push_back (i);
            return result;
        }
        std::vector<std::pair<float, int>> scored;
        for (int i = 0; i < (int) list.size(); ++i)
        {
            const auto& t = list[(size_t) i];
            const auto title = words (t.question), keys = words (t.keywords), body = words (t.answer);
            float s = 0.0f;
            for (auto& w : q)
            {
                if (matches (w, title)) s += 3.0f;
                if (matches (w, keys))  s += 2.0f;
                if (matches (w, body))  s += 0.5f;
            }
            if (s > 0.0f) scored.push_back ({ s, i });
        }
        std::stable_sort (scored.begin(), scored.end(), [] (auto& a, auto& b) { return a.first > b.first; });
        for (auto& [s, i] : scored)
        {
            if ((int) result.size() >= maxResults) break;
            result.push_back (i);
        }
        return result;
    }

    //==========================================================================
    namespace
    {
        class HelpComponent final : public juce::Component, private juce::ListBoxModel
        {
        public:
            HelpComponent()
            {
                using namespace StudioColours;
                prompt.setText ("Ask a question in your own words (English or French), for example \"how do I record my voice\" or \"accords\".",
                                juce::dontSendNotification);
                prompt.setColour (juce::Label::textColourId, textDim);
                prompt.setFont (studioFont (13.0f));
                addAndMakeVisible (prompt);

                box.setTextToShowWhenEmpty ("Type your question here...", textDim);
                box.setFont (studioFont (15.0f));
                box.onTextChange = [this] { update(); };
                box.onReturnKey = [this] { if (! shown.empty()) list.selectRow (0); };
                addAndMakeVisible (box);

                list.setModel (this);
                list.setRowHeight (30);
                list.setColour (juce::ListBox::backgroundColourId, card);
                addAndMakeVisible (list);

                answer.setMultiLine (true);
                answer.setReadOnly (true);
                answer.setScrollbarsShown (true);
                answer.setFont (studioFont (15.0f));
                answer.setColour (juce::TextEditor::backgroundColourId, card);
                addAndMakeVisible (answer);

                update();
            }

            void visibilityChanged() override
            {
                if (isShowing()) box.grabKeyboardFocus();
            }

            void update()
            {
                shown = search (box.getText());
                list.updateContent();
                list.repaint();
                if (! shown.empty()) list.selectRow (0);
                else answer.setText ("No answer found. Try other words: for example \"record\", \"chords\", \"band\", \"export\", \"mixer\".", false);
            }

            int getNumRows() override   { return (int) shown.size(); }

            void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
            {
                using namespace StudioColours;
                if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
                const auto& t = topics()[(size_t) shown[(size_t) row]];
                if (selected) { g.setColour (gold.withAlpha (0.18f)); g.fillRect (0, 0, w, h); }
                g.setColour (textDim);
                g.setFont (studioFont (11.0f));
                g.drawText (t.area.toUpperCase(), 8, 0, 70, h, juce::Justification::centredLeft);
                g.setColour (selected ? gold : text);
                g.setFont (studioFont (13.5f, selected));
                g.drawText (t.question, 80, 0, w - 86, h, juce::Justification::centredLeft, true);
            }

            void selectedRowsChanged (int row) override
            {
                if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
                const auto& t = topics()[(size_t) shown[(size_t) row]];
                answer.setText (t.question + "\n\n" + t.answer, false);
            }

            void paint (juce::Graphics& g) override   { g.fillAll (StudioColours::background); }

            void resized() override
            {
                auto r = getLocalBounds();
                prompt.setBounds (r.removeFromTop (24));
                box.setBounds (r.removeFromTop (36));
                r.removeFromTop (10);
                list.setBounds (r.removeFromLeft (juce::jmax (320, r.getWidth() * 45 / 100)));
                r.removeFromLeft (12);
                answer.setBounds (r);
            }

        private:
            juce::Label prompt;
            juce::TextEditor box, answer;
            juce::ListBox list;
            std::vector<int> shown;
        };
    }

    std::unique_ptr<juce::Component> createComponent()
    {
        return std::make_unique<HelpComponent>();
    }
}
