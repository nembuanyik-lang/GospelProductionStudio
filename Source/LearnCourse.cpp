#include "Learn.h"

//==============================================================================
//  The course itself: modules, lessons and steps (Part 1: modules 1 to 4).
//
//  Targets: "id:..." is a component ID set in the code, "tip:..." matches the
//  start of a tooltip (the round transport buttons), anything else is the text
//  written on a button or tick box.
//==============================================================================
namespace Learn
{
    namespace
    {
        // Tasks used by many lessons
        Check onPage (const juce::String& page)       { return [page] (const Context& c) { return c.host.currentPage() == page && c.host.currentPanel().isEmpty(); }; }
        Check panelOpen (const juce::String& title)   { return [title] (const Context& c) { return c.host.currentPanel() == title; }; }
        Check showing (const juce::String& target)    { return [target] (const Context& c) { return c.isOnScreen (target); }; }
        Check practiceInStudio()                      { return [] (const Context& c) { return c.practiceOpen() && c.host.currentPage() == "studio"; }; }

        Step read (juce::String text, juce::String target = {}, juce::String where = {})
        {
            return { std::move (text), std::move (target), std::move (where), {}, {}, {} };
        }
        Step task (juce::String text, juce::String target, juce::String where, juce::String todo, Check check)
        {
            return { std::move (text), std::move (target), std::move (where), std::move (todo), std::move (check), {} };
        }
        Step openedBy (Step s, juce::String opener)   { s.opener = std::move (opener); return s; }

        std::vector<Module> build()
        {
            std::vector<Module> m;

            //==================================================================
            //  Module 1: Getting started
            //==================================================================
            {
                Module mod { "1. Getting started", "Find your way around, and make sure you can hear the app and play it from your keyboard.", {} };

                mod.lessons.push_back ({ "1.1", "A tour of the screen",
                    "Before you can serve with a tool, you need to know where everything is. This lesson walks you around the main window.", 3, {},
                {
                    read ("Welcome! Along the top of the window are the three main pages of the app. You will use them every day:\n"
                          "Song Library (your songs, lyrics and voice memos), Studio (where the music is made) and Engine & Devices (your sound card and keyboard).",
                          "id:tab.library"),
                    task ("The Song Library is where every song God gives you is kept: the title, key, tempo, lyrics and a voice memo of the melody.",
                          "id:tab.library", {}, "Click \"Song Library\" at the top.", onPage ("library")),
                    task ("The Studio is where you build the song: chords, the backing band, recording your voice, mixing.",
                          "id:tab.studio", {}, "Click \"Studio\".", onPage ("studio")),
                    task ("Engine & Devices shows whether your speakers, headphones and keyboard are working. Come here when there is no sound.",
                          "id:tab.engine", {}, "Click \"Engine & Devices\".", onPage ("engine")),
                    task ("Help answers questions in your own words, any time. You can also press F1.",
                          "id:btn.help", {}, "Open Help, then type a question such as \"record my voice\".", panelOpen ("Help")),
                    task ("Pages like Help open inside the window. To leave them, click \"Back\" at the top left, or press Esc.",
                          {}, {}, "Leave Help with Back or Esc.", [] (const Context& c) { return c.host.currentPanel().isEmpty(); }),
                    read ("Last, the Learn button (where you started this lesson) brings you back to the course whenever you want. "
                          "You can move this card by dragging its top edge if it covers something.", "id:btn.learn"),
                } });

                mod.lessons.push_back ({ "1.2", "Sound check: speakers and headphones",
                    "If you cannot hear the app, nothing else matters. Here you choose where the sound goes and test it.", 3, {},
                {
                    task ("Audio Setup chooses where the sound comes out: the laptop speakers, headphones, or your audio interface. "
                          "With an audio interface, choose its ASIO driver for the shortest delay.",
                          "id:btn.audioSetup", {}, "Open Audio Setup and look at the output device.", panelOpen ("Audio & MIDI Setup")),
                    task ("Close Audio Setup, then go to the Engine & Devices page. The Audio Engine card shows the device in use and the delay (latency): under 15 ms is green and good for recording.",
                          "id:tab.engine", {}, "Go to Engine & Devices.", onPage ("engine")),
                    task ("The test tone plays a steady note (A 440 Hz) so you can check your speakers or headphones.",
                          "Test Tone (A 440 Hz)", "engine", "Turn on the test tone and listen.",
                          [] (const Context& c) { return c.studio().isTestToneOn(); }),
                    task ("Do you hear it? The Output meter on the right moves too. If the meter moves but you hear nothing, the sound is going to the wrong device: check Audio Setup again.",
                          "Test Tone (A 440 Hz)", "engine", "Turn the test tone off again.",
                          [] (const Context& c) { return ! c.studio().isTestToneOn(); }),
                    read ("If a note ever keeps sounding and will not stop, \"All Notes Off\" silences everything at once.", "All Notes Off", "engine"),
                } });

                mod.lessons.push_back ({ "1.3", "Your keyboard",
                    "Your MIDI keyboard is how you play chords, record piano parts and try ideas. The app finds it by itself.", 3, {},
                {
                    task ("Plug your USB or MIDI keyboard in before starting the app (or now: it is found within a few seconds). The MIDI card on Engine & Devices lists it.",
                          "id:tab.engine", {}, "Play a few notes on your keyboard (or on the keyboard at the bottom of the screen with the mouse).",
                          [] (const Context& c) { return c.studio().getMidiMessageCount() > c.start.midiMessages; }),
                    read ("The keyboard along the bottom of the window shows every key you play, and you can click it with the mouse. "
                          "It always plays the Piano (Keys) track of the open song."),
                    read ("On a small screen you can hide it to make room: \"Hide keyboard\" at the bottom. Your real keyboard still works when it is hidden.", "id:btn.keyboard"),
                    read ("Later, before recording with a microphone and interface, use \"Check recording delay...\" here once. It measures your interface so your voice lands exactly in time with the band.",
                          "Check recording delay...", "engine"),
                } });

                m.push_back (std::move (mod));
            }

            //==================================================================
            //  Module 2: Your songs
            //==================================================================
            {
                Module mod { "2. Your songs", "Write down a song the moment it comes: title, key, lyrics and a voice memo of the melody.", {} };

                mod.lessons.push_back ({ "2.1", "Create your practice song",
                    "Every lesson from here uses a \"Practice Song\", so your real songs stay untouched. You make it yourself, the way you will make every new song.", 3, {},
                {
                    task ("In the Song Library, \"+ New Song\" starts a new song. It appears at the top of the list on the left.",
                          "+ New Song", "library", "Click \"+ New Song\".",
                          [] (const Context& c) { return (int) c.host.getLibrary().getSongs().size() > c.start.songs || c.practiceSong() != nullptr; }),
                    task ("The title box is ready for typing.",
                          "id:song.title", "library", "Type the title: Practice Song",
                          [] (const Context& c) { return c.practiceSong() != nullptr; }),
                    task ("Key, tempo and time signature describe how the song is sung. The Studio and the backing band follow them.",
                          "id:song.key", "library", "Set the key to G major.",
                          [] (const Context& c) { auto* s = c.practiceSong(); return s != nullptr && s->key == "G major"; }),
                    task ("Tempo is the speed in beats per minute (BPM). Slow worship is around 60-75, praise around 100-120.",
                          "id:song.tempo", "library", "Set the tempo to 80 BPM.",
                          [] (const Context& c) { auto* s = c.practiceSong(); return s != nullptr && s->tempo == 80; }),
                    read ("Theme, Status (Idea, Written, Arranged...), Scripture and Notes help you find songs later. The search box at the top of the list searches titles, lyrics and scriptures. "
                          "Everything is saved by itself: there is no Save button to forget."),
                } });

                mod.lessons.push_back ({ "2.2", "Lyrics and sections",
                    "A song is made of sections: verses, choruses, a bridge. Writing them here also gives the Studio its map of the song.", 4, "practice:library",
                {
                    task ("\"+ Add Section\" adds a part of the song. Choose its type from the list.",
                          "+ Add Section", "library", "Add a Verse.",
                          [] (const Context& c) { auto* s = c.practiceSong(); if (s == nullptr) return false;
                                                  for (auto& x : s->sections) { if (x.type == "Verse") return true; }
                                                  return false; }),
                    task ("Type the words in the big box under the section name.",
                          "+ Add Section", "library", "Add a Chorus too, and type a line of words in it.",
                          [] (const Context& c) { auto* s = c.practiceSong(); if (s == nullptr) return false;
                                                  for (auto& x : s->sections) { if (x.type == "Chorus" && x.lyrics.trim().isNotEmpty()) return true; }
                                                  return false; }),
                    task ("Songs usually repeat: verse, chorus, verse, chorus.",
                          "+ Add Section", "library", "Make it four sections: Verse, Chorus, Verse, Chorus.",
                          [] (const Context& c) { auto* s = c.practiceSong(); return s != nullptr && s->sections.size() >= 4; }),
                    read ("\"Up\" and \"Down\" change the order, \"Remove\" deletes a section. When you open the song in the Studio, these sections become the coloured Sections lane above the chords."),
                } });

                mod.lessons.push_back ({ "2.3", "Catch the melody: voice memo",
                    "When a melody comes, sing it before it goes. A voice memo is the quickest way, straight from the laptop microphone.", 2, "practice:library",
                {
                    task ("\"Record Memo\" records from your microphone. Click it again (Stop) when you have finished singing.",
                          "Record Memo", "library", "Record a short memo: sing or hum a line, then stop.",
                          [] (const Context& c) { auto* s = c.practiceSong(); return s != nullptr && c.host.getLibrary().getMemoFile (s->id).existsAsFile(); }),
                    read ("\"Play\" plays it back. Each song keeps one memo; recording again replaces it, and \"Delete Memo\" removes it.", "Play", "library"),
                } });

                mod.lessons.push_back ({ "2.4", "Open the song in the Studio",
                    "The Studio is where the written song becomes music.", 2, "practice:library",
                {
                    task ("\"Open in Studio\" takes the selected song to the Studio page.",
                          "Open in Studio", "library", "Open the Practice Song in the Studio.", practiceInStudio()),
                    read ("At the top: the song title with its key, time signature, tempo and theme. Changes you make in the Studio (tempo, key) are saved back to the song."),
                    openedBy (read ("\"Versions\" keeps copies of the song from earlier (when it was opened, every 10 minutes, and any you save yourself). If something goes wrong, you can go back.",
                                    "Versions", "studio"), "Tracks"),
                } });

                m.push_back (std::move (mod));
            }

            //==================================================================
            //  Module 3: Chords
            //==================================================================
            {
                Module mod { "3. Playing and chords", "Control playback, set the tempo and key, and write the chords of your song.", {} };

                mod.lessons.push_back ({ "3.1", "Play, stop, metronome and loop",
                    "The transport buttons are like a music player's, with a metronome and a loop for practising one part again and again.", 4, "practice:studio",
                {
                    task ("The big round buttons control playback. Play starts from the playhead (the line across the timeline).",
                          "tip:Play / Pause", "studio", "Click Play.", [] (const Context& c) { return c.studio().isPlaying(); }),
                    task ("Stop stops. Pressing Stop twice returns to the start. The Space bar also plays and stops.",
                          "tip:Stop", "studio", "Stop playback.", [] (const Context& c) { return ! c.studio().isPlaying(); }),
                    task ("The metronome (click) keeps time while you play or record. Choose its sound and volume beside it; \"Count-in\" gives you a bar of clicks before recording starts.",
                          "tip:Metronome", "studio", "Turn the metronome on, then play to hear it.", [] (const Context& c) { return c.studio().isClickEnabled(); }),
                    task ("The loop plays the same bars again and again: perfect for practising a chorus or recording several takes. \"Loop from bar\" and \"Loop length\" choose which bars.",
                          "tip:Loop", "studio", "Turn the loop on.", [] (const Context& c) { return c.studio().isLooping(); }),
                    read ("Turn the metronome and loop off again whenever you like. Shortcuts: Space play/stop, Home back to the start, C metronome, L loop, R record.", "tip:Return to start", "studio"),
                } });

                mod.lessons.push_back ({ "3.2", "Tempo and key in the Studio",
                    "You can change the speed and key at any time, and everything (chords, band, harmony) follows.", 3, "practice:studio",
                {
                    task ("Tempo: the - and + buttons change it one BPM at a time, or type a number.",
                          "id:studio.tempo", "studio", "Change the tempo.",
                          [] (const Context& c) { auto* s = c.practiceSong(); return s != nullptr && s->tempo != c.start.tempo; }),
                    read ("\"Tap\": click it in time with the music (four taps or more) and the tempo follows your taps. Useful when you know the feel but not the number.", "Tap", "studio"),
                    task ("Key: choose the key the song is sung in. The chord palette on the right changes to the chords of that key.",
                          "id:studio.key", "studio", "Change the key to D major.",
                          [] (const Context& c) { auto* s = c.practiceSong(); return s != nullptr && s->key == "D major"; }),
                } });

                mod.lessons.push_back ({ "3.3", "Writing chords with the palette",
                    "The Chord Track is the heart of the song: the band, harmony singers and chord charts all read it.", 5, "practice:studio",
                {
                    read ("The \"Chords\" lane runs along the timeline under the sections. The Chord palette on the right shows the chords of your key, with numbers (1, 4, 5...) or names.",
                          "id:timeline.palette", "studio"),
                    read ("Click a chord in the palette to hear it. After a click, the palette also suggests strong chords to play next.", "id:chords.palette", "studio"),
                    task ("Double-click a chord to put it on the Chord Track at the playhead. The playhead then moves on, ready for the next chord.",
                          "id:chords.palette", "studio", "Double-click a chord in the palette.",
                          [] (const Context& c) { return c.chordCount() > c.start.chords; }),
                    task ("\"Length\" (under the palette) sets how long each new chord lasts: 1 bar, 2 beats...",
                          "id:chords.length", "studio", "Add chords until the Chord Track has at least four.",
                          [] (const Context& c) { return c.chordCount() >= 4; }),
                    task ("\"Progressions\" has ready-made gospel chord progressions. Choose one, then \"Insert at playhead\".",
                          "Progressions", "studio", "Insert a progression.",
                          [] (const Context& c) { return c.chordCount() >= c.start.chords + 2; }),
                    read ("Made a mistake? \"Undo\" (Ctrl+Z) takes back the last change. On the Chord Track, drag a chord to move it, drag its edge to change its length, or right-click it for more choices.",
                          "Undo", "studio"),
                } });

                m.push_back (std::move (mod));
            }

            //==================================================================
            //  Module 4: The band and drums
            //==================================================================
            {
                Module mod { "4. The band and drums", "Let a backing band play your chords, and write drum parts by hand.", {} };

                mod.lessons.push_back ({ "4.1", "Sections in the Studio",
                    "The band plays verses lighter and choruses fuller, so it needs to know where each section is.", 3, "practice:studio",
                {
                    read ("The Sections lane (the coloured bars at the top of the timeline) came from the lyrics you wrote in the Song Library.", "+ Section", "studio"),
                    task ("\"+ Section\" adds a section at the playhead; double-clicking in the lane does the same. Drag a section's edge to make it longer or shorter, and right-click it to rename it.",
                          "+ Section", "studio", "Make sure the song has at least two sections.",
                          [] (const Context& c) { return c.timelineSectionCount() >= 2; }),
                } });

                mod.lessons.push_back ({ "4.2", "Make the band",
                    "One click gives you bass, piano, organ, guitar and drums playing your chords in a gospel style, ready to sing over.", 5, "practice:studio",
                {
                    task ("\"Band...\" opens the backing band.", "Band...", "studio", "Click \"Band...\".", showing ("Make the band")),
                    openedBy (read ("\"Style\" chooses how the band plays: praise, worship ballad, shout, highlife, makossa... Each style shows its usual tempo. "
                                    "\"Players\" chooses who plays, and on which track.", "id:band.style", "studio"), "Band..."),
                    openedBy (read ("The switches shape the performance: following the sections, fills before each new section, an ending, answering the singer, drum variety, and the song growing from a sparer first verse to a lifted last chorus. Leave them all on to start.",
                                    "Answer the singer (short runs where the singer rests)", "studio"), "Band..."),
                    openedBy (task ("\"Make the band\" writes the parts on the tracks. You can make it again with another style at any time: the old band parts are replaced.",
                                    "Make the band", "studio", "Click \"Make the band\".", [] (const Context& c) { return c.hasBand(); }), "Band..."),
                    task ("Close the band window (click anywhere outside it) and listen.",
                          "tip:Play / Pause", "studio", "Play the song.", [] (const Context& c) { return c.studio().isPlaying(); }),
                    read ("\"Remove the band\" (in the same window) takes the band parts away again. Your own clips on the tracks are never removed."),
                } });

                mod.lessons.push_back ({ "4.3", "Drums by hand: the drum grid",
                    "Sometimes you want a particular beat. The drum grid lets you place every hit yourself.", 5, "practice:studio",
                {
                    task ("\"+ Track\" adds a track. Choose \"Drum Kit\" from the list (skip this if the song already has a Drums track).",
                          "+ Track", "studio", "Have a Drums track.", [] (const Context& c) { return c.hasTrackNamed ("Drums"); }),
                    task ("Double-click in an empty place on the Drums track: a one-bar clip is made and the drum grid opens below.",
                          {}, "studio", "Open the drum grid.", showing ("id:drums.groove")),
                    read ("Each row is a drum (kick, snare, hi-hat...), each column a step in the bar. Click a square to add a hit, click again to remove it.",
                          "id:drums.groove"),
                    read ("\"Groove\" (\"Start from a groove\" on a wide screen) fills the clip with a ready-made gospel or African groove. \"Add a fill\" puts a fill at the end, \"Repeat bar\" copies it on, "
                          "and \"Humanise\" makes it feel like a real drummer (Played by real drummers grooves are there too).", "Humanise"),
                    read ("\"Close\" closes the grid. Double-click a drum clip any time to open it again."),
                } });

                mod.lessons.push_back ({ "4.4", "Mute, solo and volume",
                    "While you work, you often want to hear one instrument alone, or silence one.", 3, "practice:studio",
                {
                    task ("Each track has small buttons at the left: M mutes it (silence), S solos it (only that track plays), R arms it for recording, A shows automation.",
                          "M", "studio", "Mute a track.", [] (const Context& c) { return c.anyTrackMuted(); }),
                    read ("The slider beside the buttons is the track's volume. The Mixer (top right) shows every track side by side with more controls.", "Mixer", "studio"),
                    task ("Muted tracks stay silent until you unmute them, so it is easy to forget.",
                          "M", "studio", "Unmute it again.", [] (const Context& c) { return ! c.anyTrackMuted(); }),
                    read ("Well done: you can now write a song, add chords and have a band play it. Next comes recording your own voice and instruments."),
                } });

                m.push_back (std::move (mod));
            }


            //==================================================================
            //  Module 5: Recording
            //==================================================================
            {
                Module mod { "5. Recording", "Record your voice, guitar and keyboard, keep the best takes, and fix single lines.", {} };

                mod.lessons.push_back ({ "5.1", "Get ready to record",
                    "A good recording starts before you press Record: the right track, the right input, and a healthy level.", 5, "practice:studio",
                {
                    read ("For singing you need a microphone and an audio interface (a sound card with a microphone input), and headphones, "
                          "so the band does not leak into the microphone. Choose the interface in Audio Setup first (lesson 1.2).", "id:btn.audioSetup"),
                    task ("Voices and guitars go on an audio track. \"+ Track\" and choose \"Audio track (vocals, guitar)\" at the bottom of the list.",
                          "+ Track", "studio", "Add an audio track.", [] (const Context& c) { return c.hasAudioTrack(); }),
                    task ("Right-click the new track's name and open \"Input\": choose the input your microphone is plugged into (for example \"Input 1\"). "
                          "\"Inputs as single channels\" is right for a voice or guitar.",
                          {}, "studio", "Choose an input for the audio track.", [] (const Context& c) { return c.audioTrackHasInput(); }),
                    task ("Click the track's R button to arm it (it turns red): the track is now ready to record and its meter shows your voice.",
                          {}, "studio", "Arm the audio track (R).", [] (const Context& c) { return c.audioTrackArmed(); }),
                    read ("Sing the loudest part of the song and watch the meter in the track header. Aim for about three quarters of the way up, never red: "
                          "turn the gain knob on your interface, not the fader in the app. "
                          "\"Monitoring\" in the same right-click menu chooses whether you hear yourself through the app; if your interface has direct monitoring, choose \"Off\"."),
                } });

                mod.lessons.push_back ({ "5.2", "Record a take",
                    "Each time you record over the same place, the app keeps it as a take, so you can sing it several times and keep the best.", 4, "practice:studio",
                {
                    read ("Turn the metronome on and choose a count-in (1 bar is good) so you know exactly when to start singing.", "id:studio.countin", "studio"),
                    task ("Put the playhead where you want to start (click in the ruler), then press Record (or the R key). You hear the count-in, then recording starts.",
                          "tip:Record", "studio", "Record a few lines.", [] (const Context& c) { return c.studio().isRecording() || c.recordingCount() > c.start.recordings; }),
                    task ("Stop when you have finished: the recording appears as a clip on the armed track.",
                          "tip:Stop", "studio", "Stop, and see your recording on the track.",
                          [] (const Context& c) { return ! c.studio().isRecording() && c.recordingCount() > c.start.recordings; }),
                    read ("Record again over the same place: it becomes Take 2. Right-click the clip and choose \"Takes\" to listen to each one, "
                          "and \"Keep this take, delete the others...\" when you have chosen."),
                    read ("To sing a chorus several times in a row, turn the loop on over the chorus and press Record: every pass becomes a take."),
                } });

                mod.lessons.push_back ({ "5.3", "Fix one line: punch-in",
                    "When one line is wrong, you do not need to sing the whole song again. Punch-in records only between two points.", 3, "practice:studio",
                {
                    read ("Set the loop range (\"Loop from bar\" and \"Loop length\") around the line to fix. The punch uses the same range.", "id:studio.loopstart", "studio"),
                    task ("\"Punch\" turns punch-in on.", "Punch", "studio", "Turn Punch on.", [] (const Context& c) { return c.studio().isPunchEnabled(); }),
                    read ("Start playing a few bars before the line and press Record: you hear yourself and the band, but only the punch range is recorded. "
                          "The rest of your take stays as it was. Turn Punch off when you have finished.", "Punch", "studio"),
                } });

                mod.lessons.push_back ({ "5.4", "Comp: the best of each take",
                    "Often take 1 has the best verse and take 3 the best chorus. Comping joins the best parts into one perfect performance.", 3, "practice:studio",
                {
                    read ("Record at least two takes in the same place (lesson 5.2)."),
                    task ("Right-click the recording and choose \"Comp takes (choose the best of each part)...\". The comp editor opens below the timeline with every take.",
                          {}, "studio", "Open the comp editor (needs two takes or more).", showing ("Make comp")),
                    read ("Drag across a take to choose that part of it, then the next part from another take. Listen, then \"Make comp\" joins your choices into one clip.",
                          "Make comp"),
                } });

                mod.lessons.push_back ({ "5.5", "Record your keyboard",
                    "Playing a part on your keyboard records notes, not sound, so you can fix any wrong note afterwards.", 4, "practice:studio",
                {
                    read ("Instrument tracks (Piano, Organ, Bass...) record what you play on your MIDI keyboard. Arm the Piano track with its R button."),
                    task ("Press Record, play a few bars with the band, then Stop.",
                          "tip:Record", "studio", "Record some notes on an instrument track.", [] (const Context& c) { return c.noteCount() > c.start.notes && ! c.studio().isRecording(); }),
                    task ("Double-click the new clip: the piano roll opens below. Each bar is a note: drag to move it, drag its end to make it longer, double-click to add one, and the Delete key removes the selected notes.",
                          {}, "studio", "Open the piano roll.", showing ("Fit to chords")),
                    read ("\"Check notes\" marks notes that clash with the Chord Track, and \"Fit to chords\" moves them onto chord notes. \"Close\" closes the piano roll.", "Check notes"),
                } });

                m.push_back (std::move (mod));
            }

            //==================================================================
            //  Module 6: Harmony and choir
            //==================================================================
            {
                Module mod { "6. Harmony and choir", "Turn one recorded voice into a choir, and choose every harmony note yourself if you want.", {} };

                mod.lessons.push_back ({ "6.1", "Make the harmonies",
                    "From your lead vocal and the Chord Track, the app sings alto, tenor and more, so you can hear the choir before you teach it.", 5, "practice:studio",
                {
                    read ("You need a recorded lead vocal (lesson 5.2) and chords on the Chord Track (lesson 3.3)."),
                    task ("Right-click the vocal recording and choose \"Harmony and choir stacks...\".",
                          {}, "studio", "Open the harmony window.", showing ("Make the harmonies")),
                    read ("Choose a ready-made choir (for example \"Full choir: alto, tenor and bass\" or \"Gospel trio\"), or tick the parts you want yourself.",
                          "Make the harmonies"),
                    task ("\"Make the harmonies\" sings each part on a new track under your vocal.",
                          "Make the harmonies", "studio", "Make the harmonies, then play the song.", [] (const Context& c) { return c.hasHarmony(); }),
                    read ("\"Send them to a Choir bus\" puts all the harmony tracks on one fader, so the whole choir gets louder or softer together. "
                          "\"Remove the harmony tracks\" takes them away again.", "Remove the harmony tracks"),
                } });

                mod.lessons.push_back ({ "6.2", "Choose the harmony notes yourself",
                    "The automatic notes follow the chords, but a choir director sometimes wants a different line. You can change any note.", 4, "practice:studio",
                {
                    read ("Open the harmony window again (right-click the vocal, \"Harmony and choir stacks...\"). The lower part shows each harmony's notes."),
                    read ("Click a note to choose it. The arrow buttons move it one step up or down; \"Chord note\" jumps to the next note of the chord. You can also drag it."),
                    read ("\"Automatic\" gives a note back to the app, and \"Reset all edits\" undoes every change. Press \"Make the harmonies\" again to hear your new lines."),
                } });

                m.push_back (std::move (mod));
            }

            //==================================================================
            //  Module 7: Editing and fixing
            //==================================================================
            {
                Module mod { "7. Editing and fixing", "Cut and move recordings, correct pitch and timing, and fix notes.", {} };

                mod.lessons.push_back ({ "7.1", "Cut, move and copy",
                    "Editing is like working with paper: cut out a piece, move it, copy the chorus to the end.", 4, "practice:studio",
                {
                    read ("Click a clip to select it. Drag it left or right to move it, or to another track. Drag its left or right edge to shorten it."),
                    task ("Put the playhead inside a clip, select the clip and press S (or right-click, \"Split at playhead\"): it becomes two clips.",
                          {}, "studio", "Split a clip.", [] (const Context& c) { return c.clipCount() > c.start.clips; }),
                    read ("Right-click a clip for Copy, Paste here, Duplicate, Rename and Delete. Ctrl+Z (Undo) takes back any mistake.", "Undo", "studio"),
                    read ("\"Clip gain, pitch and speed...\" in the same menu makes one clip louder or softer, without touching the rest of the track."),
                } });

                mod.lessons.push_back ({ "7.2", "Vocal tuning",
                    "Gentle pitch correction pulls slightly flat or sharp notes into tune, while keeping the voice natural.", 3, "practice:studio",
                {
                    task ("Right-click the vocal track's name and choose \"Vocal tuning (correct the pitch)...\".",
                          {}, "studio", "Turn vocal tuning on for a vocal track.", [] (const Context& c) { return c.hasVocalTuning(); }),
                    read ("It follows the song's key. Lower \"Strength\" for a more natural sound; full strength and a fast speed give the modern hard-tuned effect. "
                          "Listen with it on and off to compare."),
                } });

                mod.lessons.push_back ({ "7.3", "Timing correction",
                    "When a recorded part rushes or drags, timing correction lines it up with the beat.", 3, "practice:studio",
                {
                    task ("Right-click a recording and choose \"Timing correction (line up to the beat)...\".",
                          {}, "studio", "Correct the timing of a recording.", [] (const Context& c) { return c.hasTimingCorrection(); }),
                    read ("\"Strength\" chooses how much to correct: lower keeps the human feel, full is exactly on the beat. Press Apply and listen; the original is kept, so you can switch it off again."),
                } });

                mod.lessons.push_back ({ "7.4", "Fix notes in the piano roll",
                    "Any instrument part, played by you or by the band, can be changed note by note.", 3, "practice:studio",
                {
                    task ("Double-click a piano, bass or organ clip (a band part is fine) to open the piano roll.",
                          {}, "studio", "Open the piano roll.", showing ("Fit to chords")),
                    read ("\"Draw\" adds notes with a click, \"Select\" chooses and moves them (the Delete key removes the selected notes). \"Edit\" has quantise, transpose, velocity (how hard notes are played) and more.", "Draw"),
                    read ("\"Check notes\" marks notes that do not fit the chords; \"Fit to chords\" corrects them. \"Close\" when you have finished.", "Check notes"),
                } });

                m.push_back (std::move (mod));
            }

            //==================================================================
            //  Module 8: Mixing
            //==================================================================
            {
                Module mod { "8. Mixing", "Balance the instruments and voices, add effects, and compare with songs you love.", {} };

                mod.lessons.push_back ({ "8.1", "The mixer",
                    "Mixing is making every part heard: the voice clear on top, the band supporting it.", 4, "practice:studio",
                {
                    task ("\"Mixer\" shows every track side by side.", "Mixer", "studio", "Open the mixer.", showing ("+ Bus")),
                    openedBy (read ("Each strip has a fader (volume), a pan knob (left and right), M and S, and a meter. Start with every fader down a little, then bring up the lead voice first and build the band around it.",
                          "+ Bus"), "Mixer"),
                    openedBy (read ("A simple order that works: voice, then bass and drums together, then piano and organ, then guitar. Keep the master meter out of the red. "
                                    "While the mixer is open the same button says \"Tracks\": click it to go back to the timeline.", "Tracks", "studio"), "Mixer"),
                } });

                mod.lessons.push_back ({ "8.2", "Vocal chain and effects",
                    "A vocal chain (EQ, compressor, de-esser, reverb) makes a home recording sound polished in one click.", 3, "practice:studio",
                {
                    task ("Right-click the vocal track's name and open \"Vocal chain\". Choose one that matches the singer (for example \"Lead vocal: warm worship\", or \"Choir and backing vocals\").",
                          {}, "studio", "Put a vocal chain on a vocal track.", [] (const Context& c) { return c.hasVocalChain(); }),
                    read ("\"Effects\" in the same menu adds single effects (built-in, or your VST3 plugins) and opens their windows. "
                          "Every effect can be turned off for a moment to compare, and \"Remove the vocal chain\" takes the whole chain off."),
                } });

                mod.lessons.push_back ({ "8.3", "Buses and ducking",
                    "A bus groups several tracks on one fader, like all the choir voices. Ducking lets the music dip under a voice automatically.", 4, "practice:studio",
                {
                    openedBy (task ("In the mixer, \"+ Bus\" adds a group bus (choir, band, drums...).", "+ Bus", "studio", "Add a bus.", [] (const Context& c) { return c.busCount() > 0; }), "Mixer"),
                    read ("Right-click a track's name and choose \"Output\" to send it into the bus. The bus fader then moves all its tracks together."),
                    read ("\"Duck under another track\" (same menu) makes this track get quieter whenever another plays: the band under a spoken word, or the bass under the kick drum."),
                } });

                mod.lessons.push_back ({ "8.4", "Compare with a reference song",
                    "Comparing your mix with a professional song you love is the quickest way to hear what to change.", 3, "practice:studio",
                {
                    openedBy (task ("In the mixer, the master strip has \"Analyser\".", "Analyser", "studio", "Open the Analyser.",
                          [] (const Context& c) { return c.host.currentPanel().startsWith ("Analyser"); }), "Mixer"),
                    openedBy (read ("\"B: Reference\" loads a finished song; \"A: Your mix\" is yours. Switch between them while playing: the picture shows where your mix has too much or too little bass, middle or treble.",
                          "B: Reference"), "Mixer|Analyser"),
                } });

                m.push_back (std::move (mod));
            }

            //==================================================================
            //  Module 9: Mastering and release
            //==================================================================
            {
                Module mod { "9. Mastering and release", "Give the song its final polish and loudness, export it, and prepare it for distributors.", {} };

                mod.lessons.push_back ({ "9.1", "Mastering",
                    "Mastering is the last polish on the whole song: even, clear and as loud as other songs, without distortion.", 4, "practice:studio",
                {
                    openedBy (task ("Open the mixer and click \"Mastering\" on the master strip.", "Mastering", "studio", "Open the Mastering page.",
                          [] (const Context& c) { return c.host.currentPanel().startsWith ("Mastering"); }), "Mixer"),
                    openedBy (task ("\"Mastering on\" switches it on for the whole song.", "Mastering on", {}, "Turn mastering on.", [] (const Context& c) { return c.masteringOn(); }), "Mixer|Mastering"),
                    openedBy (read ("\"Set loudness for the whole song\" listens through the whole song by itself (it takes a moment) and sets the level to match streaming services.", "Set loudness for the whole song"), "Mixer|Mastering"),
                    openedBy (read ("\"Compare: hear before\" lets you hear the song without mastering, to be sure it is better, not just louder.", "Compare: hear before"), "Mixer|Mastering"),
                } });

                mod.lessons.push_back ({ "9.2", "Export MP3 or WAV",
                    "Export makes a file you can send on WhatsApp, play in church, or give to a radio station.", 3, "practice:studio",
                {
                    task ("\"Export\" opens the export page.", "Export", "studio", "Open the Export page.",
                          [] (const Context& c) { return c.host.currentPanel().startsWith ("Export"); }),
                    openedBy (read ("\"The whole song (mix)\" makes one file. Choose MP3 for sharing or WAV for the best quality. "
                          "\"Each track on its own (stems)\" is for another studio or a mix engineer.", "The whole song (mix)"), "Export"),
                    openedBy (read ("Tick \"Ready to share\" for even loudness, then \"Export\". \"Open folder\" shows the file.", "Open folder"), "Export"),
                } });

                mod.lessons.push_back ({ "9.3", "Release package for distributors",
                    "DistroKid, TuneCore, Boomplay and Audiomack all ask for the same things. The release package puts them together and checks them.", 4, "practice:studio",
                {
                    openedBy (task ("On the Export page, \"Release package for distributors...\".", "Release package for distributors...", {}, "Open the release package.",
                          [] (const Context& c) { return c.host.currentPanel().startsWith ("Release package"); }), "Export"),
                    openedBy (read ("Fill in the details (artist name, songwriter, year, genre), the lyrics come from the Song Library, and \"Choose cover art...\" adds the picture (a square 3000 x 3000 picture is best).",
                          "Choose cover art..."), "Export|Release package for distributors..."),
                    openedBy (read ("\"Make the release package\" checks everything and makes a folder with the master, cover and details, ready to upload.", "Make the release package"), "Export|Release package for distributors..."),
                } });

                m.push_back (std::move (mod));
            }

            //==================================================================
            //  Module 10: Keeping your work safe
            //==================================================================
            {
                Module mod { "10. Keeping your work safe", "Go back to earlier versions, survive power cuts, and keep a backup.", {} };

                mod.lessons.push_back ({ "10.1", "Versions",
                    "Try bold ideas without fear: the app keeps copies of your song, and you can go back to any of them.", 3, "practice:studio",
                {
                    openedBy (task ("\"Versions\" beside the song title lists the copies.", "Versions", "studio", "Open Versions.",
                                    [] (const Context& c) { return c.host.currentPanel().startsWith ("Versions"); }), "Tracks"),
                    openedBy (task ("Before a big change, save your own version with a name you will recognise, such as \"Before the choir\".",
                          "Save a version now...", {}, "Save a version with a name.", [] (const Context& c) { return c.hasNamedVersion(); }), "Tracks|Versions"),
                    openedBy (read ("To go back, choose a version and \"Go back to this version\". The song as it is now is kept too, as \"Before going back\", so nothing is ever lost.",
                          "Go back to this version"), "Tracks|Versions"),
                } });

                mod.lessons.push_back ({ "10.2", "Power cuts and backups",
                    "Power cuts happen. The app is built so they cost you seconds, not songs.", 3, {},
                {
                    read ("The song is saved by itself every few seconds while you work, and a version is kept every 10 minutes. There is no Save button to forget."),
                    read ("If the power goes, start the app again: \"Welcome back\" offers to open the song. A recording that was cut off is rescued onto a track called \"Rescued recording\"."),
                    read ("Once a week, copy the folder Documents\\Gospel Production Studio to a flash drive or Google Drive. If the laptop is ever lost or broken, your songs are safe."),
                    read ("That is the whole course. Well done! Use Help (F1) any time, and come back to any lesson when you need it. "
                          "\"Print the handbook\" on the Learn page gives you all these lessons on paper."),
                } });

                m.push_back (std::move (mod));
            }

            return m;
        }
    }

    const std::vector<Module>& course()
    {
        static const std::vector<Module> c = build();
        return c;
    }

    const Lesson* findLesson (const juce::String& id)
    {
        for (auto& mod : course())
            for (auto& l : mod.lessons)
                if (l.id == id) return &l;
        return nullptr;
    }

    int lessonCount()
    {
        int n = 0;
        for (auto& mod : course()) n += (int) mod.lessons.size();
        return n;
    }

    //==========================================================================
    //  The handbook: the same lessons as a page to print or save as PDF
    //==========================================================================
    namespace
    {
        juce::String html (const juce::String& t)
        {
            return t.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;").replace ("\n", "<br>");
        }
    }

    juce::String handbookHtml (const juce::String& version)
    {
        juce::String h;
        h << "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>Gospel Production Studio Handbook</title><style>\n"
             "@page { size: A4; margin: 18mm 16mm; }\n"
             "body { font-family: 'Segoe UI', Arial, sans-serif; font-size: 11.5pt; line-height: 1.5; color: #1d1d22; max-width: 760px; margin: 0 auto; padding: 24px; }\n"
             "h1 { font-size: 30pt; margin: 0 0 4px; letter-spacing: 0.02em; } .sub { color: #a07a1c; font-weight: 700; letter-spacing: 0.25em; font-size: 10pt; }\n"
             ".cover { border-bottom: 3px solid #c9a033; padding: 40px 0 24px; margin-bottom: 24px; }\n"
             "h2 { font-size: 18pt; border-bottom: 2px solid #c9a033; padding-bottom: 4px; margin-top: 34px; page-break-before: always; }\n"
             "h2.first { page-break-before: auto; }\n"
             "h3 { font-size: 13.5pt; margin: 22px 0 2px; page-break-after: avoid; } .meta { color: #8a8a96; font-size: 9.5pt; margin-bottom: 6px; }\n"
             ".why { font-style: italic; margin: 4px 0 8px; } ol { padding-left: 22px; margin: 6px 0; } li { margin: 0 0 7px; page-break-inside: avoid; }\n"
             ".try { display: block; margin-top: 3px; padding: 4px 8px; background: #f6efd9; border-left: 3px solid #c9a033; font-weight: 600; }\n"
             ".box { border: 1px solid #ddd3b5; background: #fbf8ef; padding: 10px 14px; border-radius: 6px; } table { border-collapse: collapse; }\n"
             "td { padding: 3px 14px 3px 0; vertical-align: top; } .toc { padding-left: 30px; } .toc li { margin: 0 0 3px; } .lesson { page-break-inside: avoid; }\n"
             "@media print { .noprint { display: none; } body { padding: 0; } }\n"
             "</style></head><body>\n";
        h << "<div class=\"cover\"><div class=\"sub\">GOSPEL PRODUCTION STUDIO</div><h1>Handbook</h1>"
             "<div>Every lesson of the Learn course, to read, print and keep beside the laptop. Version " << html (version) << ".</div></div>\n";
        h << "<p class=\"noprint box\">To keep this handbook as a PDF or print it: press <b>Ctrl+P</b>, then choose <b>Save as PDF</b> or your printer.</p>\n";
        h << "<div class=\"box\"><b>How to use this handbook.</b> Each lesson matches a lesson in the app (the Learn button). "
             "Read it here, then do it in the app: the lesson card there shows you each button with \"Show me\". "
             "Lessons use a song called \"Practice Song\", so your real songs are never changed.</div>\n";

        h << "<h2 class=\"first\">Contents</h2><ol class=\"toc\">";
        for (auto& mod : course())
        {
            h << "<li><b>" << html (mod.title.fromFirstOccurrenceOf (". ", false, false)) << "</b>: ";
            juce::StringArray names;
            for (auto& l : mod.lessons) names.add (l.id + " " + l.title);
            h << html (names.joinIntoString (", ")) << "</li>";
        }
        h << "<li><b>Quick reference</b>: shortcuts and where your files are</li></ol>\n";

        for (auto& mod : course())
        {
            h << "<h2>" << html (mod.title) << "</h2><p>" << html (mod.intro) << "</p>\n";
            for (auto& l : mod.lessons)
            {
                h << "<div class=\"lesson\"><h3>" << html (l.id + "  " + l.title) << "</h3><div class=\"meta\">About " << l.minutes << " minutes"
                  << (l.startWith == "practice:studio" ? " &middot; with the Practice Song open in the Studio" : l.startWith == "practice:library" ? " &middot; with the Practice Song in the Song Library" : "")
                  << "</div><div class=\"why\">" << html (l.why) << "</div><ol>";
                for (auto& st : l.steps)
                {
                    h << "<li>" << html (st.text);
                    if (st.task.isNotEmpty())
                        h << "<span class=\"try\">Try it: " << html (st.task) << "</span>";
                    h << "</li>";
                }
                h << "</ol></div>\n";
            }
        }

        h << "<h2>Quick reference</h2><h3>Keyboard shortcuts</h3><table>"
             "<tr><td><b>Space</b></td><td>Play / stop</td></tr><tr><td><b>Home</b></td><td>Back to the start</td></tr>"
             "<tr><td><b>R</b></td><td>Record</td></tr><tr><td><b>C</b></td><td>Metronome on / off</td></tr><tr><td><b>L</b></td><td>Loop on / off</td></tr>"
             "<tr><td><b>S</b></td><td>Split the selected clip at the playhead</td></tr><tr><td><b>Ctrl+Z / Ctrl+Y</b></td><td>Undo / redo</td></tr>"
             "<tr><td><b>F1</b></td><td>Help: ask a question in your own words</td></tr><tr><td><b>Esc</b></td><td>Leave a page such as Help or Export</td></tr></table>"
             "<h3>Where your work is kept</h3><p>Everything is in <b>Documents\\Gospel Production Studio</b>: each song in its own folder under <b>Songs</b>, "
             "with its recordings and a <b>Versions</b> folder of earlier copies. Copy the whole folder to a flash drive or Google Drive once a week.</p>"
             "<h3>When something goes wrong</h3><p>No sound: Engine &amp; Devices, then the test tone (lesson 1.2). A note will not stop: All Notes Off. "
             "A mistake: Ctrl+Z, or Versions (lesson 10.1). A power cut: start the app again and choose \"Open the song\" (lesson 10.2).</p>\n";
        h << "</body></html>\n";
        return h;
    }

    juce::File writeHandbook (const juce::File& file, const juce::String& version)
    {
        file.getParentDirectory().createDirectory();
        return file.replaceWithText (handbookHtml (version), false, false, "\n") ? file : juce::File();
    }

    juce::StringArray checkCourse()
    {
        juce::StringArray problems;
        juce::StringArray ids;
        for (auto& mod : course())
        {
            if (mod.lessons.empty()) problems.add (mod.title + ": no lessons");
            for (auto& l : mod.lessons)
            {
                if (ids.contains (l.id)) problems.add ("lesson " + l.id + " twice");
                ids.add (l.id);
                if (l.steps.size() < 2) problems.add (l.id + ": too few steps");
                if (l.title.isEmpty() || l.why.isEmpty()) problems.add (l.id + ": no title or purpose");
                if (l.startWith.isNotEmpty() && l.startWith != "practice:library" && l.startWith != "practice:studio")
                    problems.add (l.id + ": unknown start " + l.startWith);
                for (size_t i = 0; i < l.steps.size(); ++i)
                {
                    const auto& s = l.steps[i];
                    const auto where = l.id + " step " + juce::String ((int) i + 1);
                    if (s.text.trim().isEmpty()) problems.add (where + ": no text");
                    if (s.task.isNotEmpty() != (s.check != nullptr)) problems.add (where + ": a task needs a check (and a check a task)");
                    if (s.where.isNotEmpty() && s.where != "library" && s.where != "studio" && s.where != "engine") problems.add (where + ": unknown page " + s.where);
                    if (s.text.length() > 420) problems.add (where + ": text too long for the coach card");
                }
            }
        }
        return problems;
    }
}
