# Gospel Production Studio

**Current build: version 1.3.1.** Runs on Tracktion Engine. All 22 steps of the original plan are done; now working through the improvement plan (Steps 23 to 49) from the professional review.

Stage 0 set up the GitHub build and produced the first version of the studio: the **Studio Shell**. It opens with the professional interface, connects to your audio device, plays a test tone, and detects your keyboard (YPT-240), which you can then play.

---

## How it is built: GitHub (same as the Academy)

GitHub's computers compile the studio and produce a ready-to-install **Setup .exe**. You do **not** need Visual Studio on your laptop.

### Step 1: Create the repository (one time only)

1. On github.com, create a new repository named **GospelProductionStudio** (Private is fine).
2. Upload **everything** in this folder, including the hidden **`.github`** folder (it holds the build workflow). From a laptop, drag the whole folder contents onto the upload page.

### Step 2: Build and install

1. Open the repository, go to **Actions**, choose **Build Windows Installer**, then click **Run workflow**.
2. Wait about 15 to 25 minutes. A green tick means it succeeded.
3. Open **Releases** (on the repository's main page), download **GospelProductionStudio-Setup-0.1.0.exe**, and run it.
4. If Windows SmartScreen appears, click **More info**, then **Run anyway** (the app is not code-signed yet).

If the workflow shows a red X, open the failed step, copy the error lines (or take a screenshot) and send them to Claude.

### Updating later

For each new step, upload the changed files Claude sends and run the workflow again. Each run creates a new Release.

### Optional: build on the laptop

`setup.bat` and `build.bat` build the studio locally. This needs Visual Studio Community with **Desktop development with C++** and **C++ CMake tools**, plus Git for Windows. It is only needed for fast testing later; GitHub is the normal route.

---

## Automatic updates

From version 0.4.0 the app updates itself from your GitHub Releases:

- A few seconds after it opens, the app quietly checks GitHub. If a newer build exists, a gold bar appears: **Update Now** downloads the new Setup, installs it and reopens the app.
- The **Updates** button (top right) checks at any time and tells you if you are up to date.
- Each GitHub run has its own build number, so even a fix-only rebuild is offered as an update.

**Important:** the app can only see releases of a **public** repository. If yours is private, make it public: GitHub > your repository > **Settings** > **General** > scroll to **Danger Zone** > **Change visibility** > **Public**.

---

## The plan and where we are

| Plan step | Status |
|---|---|
| Stage 0 and 1: Steps 1 to 8 | Done |
| 9. Audio recording with takes and punch-in | Done |
| 10. Comping and audio editing | Done |
| 11. VST3 plugin hosting | Done |
| 12. Vocal chain (EQ, de-esser, compression, reverb) with presets | **Done in 0.18.0** (de-esser, one-click vocal chains, ASIO) |
| 13. Pitch and timing correction | **Done in 0.19.0** (Vocal tuning + timing correction) |
| 14. Full mixer, buses and sidechain | **Done in 0.20.0** (group buses, sidechain ducking) |
| 15. Automation | Done |
| 16. Analysers and comparison against a reference track | **Done in 0.21.0** |
| 17. Mastering page with loudness meters | Done |
| 18. Release package and song details for distributors | **Done in 0.22.0**. Stage 3 complete |
| 19. Harmony generator and choir stacks | **Done in 0.23.0** |
| 20. Backing players that follow your chords | **Done in 0.24.0** (and a built-in guitar) |
| 21. African and Cameroonian groove pack | **Done in 0.25.0** (grooves, percussion, balafon, band styles) |
| 22. Chord detection, stem separation and the help assistant | **Done in 1.0.0**. The plan is complete |

Built along the way, outside the plan: the drum grid, My Sounds and loops that follow the tempo. The older test sections below keep the numbers they were delivered with (for example "Step 16 test: Vocal tuning" is plan Step 13).

---

## Learn (Part 2) test: the whole course and the handbook

| # | Test | Pass when |
|---|------|-----------|
| 1 | Click Learn | 10 modules and 34 lessons: Recording, Harmony and choir, Editing and fixing, Mixing, Mastering and release, Keeping your work safe have joined |
| 2 | Lesson 5.1 with a microphone and interface | "+ Track" audio track, Input, R: each task ticks by itself |
| 3 | Lesson 5.2: record a few lines and stop | Both tasks tick when the recording appears |
| 4 | Lesson 8.1, step 2, "Show me" | The mixer opens by itself and "+ Bus" glows |
| 5 | Lesson 9.1, step 3, "Show me" (mixer closed) | The mixer opens, then the Mastering page, then "Set loudness for the whole song" glows |
| 6 | Lesson 9.3, step 3, "Show me" | Export, then the release package open by themselves; "Make the release package" glows |
| 7 | Lesson 10.1: save a version called "Before the choir" | The task ticks |
| 8 | A step whose button only appears after an earlier step (for example 6.1 step 3 before opening the harmony window), "Show me" | A short note: follow the step before first |
| 9 | Learn, "Print the handbook" | The handbook opens in your web browser: contents, every lesson with "Try it" boxes, and a quick reference. Ctrl+P, "Save as PDF" |

---

## Learn (Part 1) test: the course inside the app

| # | Test | Pass when |
|---|------|-----------|
| 1 | Look at the top bar | A gold "Learn" button beside Help |
| 2 | Click Learn | The course: 4 modules, 14 lessons, a progress bar "0 of 14 lessons done", and lesson 1.1 described on the right |
| 3 | "Start this lesson" | The page closes and a card appears at the bottom right: "Lesson 1.1, Step 1 of 7" |
| 4 | "Show me" | The Song Library button glows gold for a few seconds |
| 5 | Next, then do each "Your turn" (click Studio, Engine & Devices, open Help...) | Each task gets a green tick by itself, and Next turns gold |
| 6 | Press Next without doing a task | A gentle "Not done yet" note; pressing Next again skips it |
| 7 | Drag the card by its top edge | It moves and stays where you put it |
| 8 | Finish the lesson | "Lesson 1.1 done" with "Next lesson"; in Learn, 1.1 has a green tick and the bar moves |
| 9 | Lesson 2.1 | You make the "Practice Song" yourself (+ New Song, title, key G major, 80 BPM): each step ticks |
| 10 | Lesson 4.2, step 4, "Show me" | The band window opens by itself and "Make the band" glows |
| 11 | Close the app in the middle of the course, start again, Learn | Your ticks are still there; the first lesson not done is selected |
| 12 | In Learn, "Start the course again" | All ticks cleared; your songs are untouched |

---

## Step 34 test: A livelier band

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open a song with chords and sections (Verse 1, Chorus, Verse 2, Chorus), click "Band..." | Three new switches: "Answer the singer", "Drum variety", "The song grows", all ticked |
| 2 | Make the band, play the whole song | The first verse has no guitar or organ (only piano, bass, drums); they come in at the chorus |
| 3 | Listen to each new section | A cymbal crash on the first beat of every section; the band gets a little louder in the bar before it |
| 4 | Listen to a chorus closely | Soft "ghost" taps on the snare between the main beats; an open hi-hat at the end of every 4 bars |
| 5 | Listen to the last chorus | Louder than the first, the piano sparkles an octave higher, a crash every 4 bars |
| 6 | No melody yet: listen at the end of each 4-bar phrase in a verse or chorus | A short run on the piano (or guitar, or a bass walk), ending on a note of the chord |
| 7 | Record or draw a melody on a track called "Melody" (or "Lead", "Vocal"), then Make the band again | The runs now fall in the gaps where the singer breathes, never on top of the singing |
| 8 | Untick the three switches, Make the band | The band plays as before Step 34 (steady, the same every bar) |
| 9 | A long song (5 minutes or more) | The piano stays in the middle of the keyboard to the end (it used to climb higher and higher; fixed) |

---

## Steps 43-44 test: Versions, backups and recovery

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open a song in the Studio | A "Versions" button at the right of the song title |
| 2 | Versions | A list with "When the song was opened" (and earlier ones) |
| 3 | "Save a version now...", type "Before the choir", Enter | It appears at the top, in gold, with your name |
| 4 | Make a change (delete a track), then Versions, choose "Before the choir", "Go back to this version", "Go back" | The song is back as it was, the deleted track returns. The list also has "Before going back..." with your changed song |
| 5 | Work 10+ minutes on a song | A "During work (every 10 minutes)" version appears |
| 6 | **Power cut test:** open a song, wait 15 s, then switch the laptop off at the button (or end the app in Task Manager) | Starting again says "Welcome back... did not close properly"; "Open the song" opens it with your work |
| 7 | **Power cut while recording:** arm a track, record 20 s, then end the app in Task Manager during the recording | Starting again mentions the recording; after "Open the song" it is on a track "Rescued recording" where you started |
| 8 | Close the app normally, start again | No "Welcome back" message |
| 9 | Song folder (Documents/Gospel Production Studio/Songs/...) | A "Versions" folder with small .tracktionedit files |

---

## Step 33 test: Editable harmony notes

| # | Test | Pass when |
|---|------|-----------|
| 1 | Right-click a sung clip > Harmony and choir stacks | The panel is taller; under the picture: "Click a coloured note in the picture to change it" and buttons ▲ ▼ Chord note ▲ Chord note ▼ Automatic, Reset all edits |
| 2 | Click a coloured note (e.g. a blue Tenor note) | A gold frame around it; the line says e.g. "Tenor, note 1 of 16 (melody A3): sings D3" |
| 3 | Press ▲ twice (or the Up key) | The note moves up; the line says "sings E3, moved by hand (the app chose D3)"; a white edge marks it |
| 4 | "Chord note ▼" (or Shift + Down) | It jumps to the next lower note of the chord |
| 5 | Drag a note up or down | It follows the mouse in semitones |
| 6 | Left / Right keys | Go to the previous / next note of that part |
| 7 | "Make the harmonies", play | The parts sing your changed notes; the rest as before |
| 8 | Close the panel and open it again | Your changes are still there; "Automatic" undoes one, "Reset all edits" all |

---

## Step 28 test: Recording delay

You need: your audio interface, and one cable (guitar or mic cable) from **Output 1** to **Input 1**.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Engine & Devices: a new row "Recording delay fix: not measured" and a button "Check recording delay..." | Both are there |
| 2 | Speakers down, cable in, Input 1 gain at the middle, press "Start the check" | After about 3 seconds: "Measured round trip: ... ms. Your driver reports ... ms. So recordings land ... ms late" |
| 3 | Press "Measure again" two or three times | The numbers stay the same (within 0.1 ms) |
| 4 | Press "Use ... ms correction", remove the cable | "Recording delay fix" on the Engine page shows the value |
| 5 | Close and reopen the app | The correction is still there |
| 6 | Record yourself clapping along to the click, zoom in | The claps sit on the beat lines (before, they were slightly after) |
| 7 | Without the cable, press Start | It says the test sound did not come back (nothing breaks) |

Change the interface, the driver (ASIO/Windows) or the buffer size? Measure again.

---

## Licence

Gospel Production Studio is free software under the **GNU AGPL version 3** (LICENSE.md). It is built on
JUCE (AGPLv3), Tracktion Engine (GPLv3), the ASIO SDK (GPLv3) and LAME (LGPL); all credits are in NOTICES.md,
which the installer also copies next to the app. Because the source code is in this public repository,
sharing the app and its releases follows these licences at no cost.

---

## Logo test

| # | Test | Pass when |
|---|------|-----------|
| 1 | Run the new Setup file | The installer window shows the gold level-meter cross |
| 2 | Desktop shortcut, Start menu, taskbar while the app runs | All show the logo instead of the plain Windows program icon |
| 3 | Top-left of the app | The logo, "GOSPEL" and "PRODUCTION STUDIO" in gold |
| 4 | Settings > Apps (uninstall list) | The logo next to Gospel Production Studio |

The logo files for posters, flyers and social media are in the Branding folder (see Branding/README.txt).

---

## Step 32 test: Real drummers

The GitHub build now reads the Groove MIDI Dataset (Google LLC, CC BY 4.0: about 1,150 performances by 10 drummers) and builds the feels into the app. In the build log, the step "Real-drummer feel" lists each style with how late or early its snare is, e.g. "Gospel: 40 performances; snare on 2 and 4 +6 ms". Please send me that part of the log.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Drum grid on a Drum Kit track: Humanise | A menu: "A little, at random", then "Feel of real drummers" with Gospel, Soul, Funk, Highlife, Afrobeat, Afro-Cuban, Reggae... and "Back to the grid" |
| 2 | Start from "Praise (mid tempo)", choose "Gospel feel", play | It sounds played: snare and hi-hat sit like a real drummer's; the bottom line says how many performances the feel comes from |
| 3 | A Cameroon groove (e.g. Makossa) with "Afrobeat feel" or "Highlife feel" | Works on African grooves too |
| 4 | "Back to the grid" | The timing is exact again |
| 5 | Groove, "Played by real drummers", Gospel 1 | Two bars played by a real drummer fill the clip |
| 6 | Drum Kit track with the African Percussion pack downloaded: a groove with congas or shakers | The congas and shakers are the recordings (before this step they were built-in on the Drum Kit) |
| 7 | Undo after a feel | Back to before |

---

## Step 31 test: Recorded African percussion

**First, once:** run "Build sound packs" again on GitHub (Actions tab). Its log lists, for each percussion sound, how many recordings it found, e.g. "63 conga open -> 6 recordings" or "-> built-in". Please send me that part of the log.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Before downloading: a track with African Percussion | Shows "African Percussion (built-in sound)" and sounds exactly as before |
| 2 | Sounds: download "African Percussion" (Percussion group) | Downloads; size shown |
| 3 | Play a Makossa or Assiko groove on the African Percussion track (drum grid, "Groove") | Congas, shakers, bells and claps are real recordings; djembe and talking drum are the built-in sounds |
| 4 | Play soft and hard notes on the conga rows | Hard hits sound different (not only louder) |
| 5 | Balafon track: play a scale | Recorded balafon; any note outside its range still sounds (built-in) |
| 6 | Open a song from before with percussion or balafon | Same volume; recorded where the pack has the sound |
| 7 | Listen honestly | Tell me any sound that is too loud, too soft or the wrong instrument |

---

## Step 30 test: Recorded guitars

**First, once:** on GitHub open Actions, choose "Build sound packs", press "Run workflow" and wait for the green tick (it rebuilds all the packs on GitHub's computers, about 30-60 minutes). Then build the installer as usual.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Before downloading: "+ Track", choose Clean Electric Guitar | The track is called "Guitar" and shows "Clean Electric Guitar (built-in sound)". It plays the same guitar as before |
| 2 | Sounds (top right) | A "GUITARS" group with Clean Electric Guitar and Nylon Guitar, each with its size |
| 3 | Download Clean Electric Guitar, then play the guitar track | The "(built-in sound)" note goes away and you hear a real recorded electric guitar. Soft and hard notes sound different, not just quieter |
| 4 | Make the band (Band...) with Guitar ticked, play it | The band's guitar uses the recorded guitar |
| 5 | Open a song made before this version that has a guitar track | Same volume as before; it now plays the recorded guitar if the pack is downloaded |
| 6 | Download Nylon Guitar; change a track to Nylon Guitar | A soft classical guitar for worship ballads |
| 7 | Listen honestly | Tell me if any notes sound out of tune, too quiet or noisy: the nylon recording is older (2008), and I can swap or fix notes |

---

## Step 27 test: Tests on GitHub

Every run of "Build Windows Installer" now also runs the app's automatic tests (about 120 checks) on Windows.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Run the workflow from the Actions tab and wait for it to finish | A green tick. The run's page shows "Automatic tests passed" with "Summary: ... ok, 0 FAILED" |
| 2 | Open the Releases page | The new release is there and says "All automatic tests passed on Windows" |
| 3 | On the run's page, under Artifacts | "test-results-1.0.5" holds the full list of checks |

**If you see a red cross:** the run's page says which checks FAILED. The Release is not published, so the app's Updates button will not offer that build. The installer is still under Artifacts for checking. Send me the failed lines (or the test-results file) and I will fix them.

---

## Step 26 test: Small fixes

| # | Test | Pass when |
|---|------|-----------|
| 1 | Look at the top-left title and the bottom line | Only the page name (STUDIO, SONG LIBRARY) and "Press F1 for help · Space to play or stop": no "Stage" or "Step 22 of 22" |
| 2 | Click "+ Track" and choose Gospel Organ in a song that already has an Organ track | The new track is called "Organ 2" |
| 3 | Change that track's instrument to Grand Piano | The track is renamed "Piano" (or "Piano 2") |
| 4 | Rename a track yourself (e.g. "Sister Grace solo"), then change its instrument | Your name stays |
| 5 | Open a song from an earlier version that has a track called "Instrument 6" | It now shows the instrument's name, e.g. "Organ" |
| 6 | Open the Mixer on your 1366 x 768 laptop with 12 or more tracks | Strips narrow a little so about 14 fit; the rest are a short scroll away |
| 7 | GitHub build log | No warnings from the app's own code |

---

## Step 25 test: Disk clean-up

| # | Test | Pass when |
|---|------|-----------|
| 1 | Split a recording into stems twice, delete the first set of stem tracks, save, close, open the song again | The song line shows "Cleaned away 4 old files (... freed)" for a few seconds, and the song's Audio/Stems folder only holds the stems still on tracks |
| 2 | Open the same song again | No clean-up message: nothing left to clean |
| 3 | Check your own recordings, imported songs and loops in the Audio folder | All still there: only unused files in the Stems, Harmony, Tempo and Timed folders are ever removed |
| 4 | Remove all the harmony parts of a song, close and open it | The empty Harmony folder is gone too |

---

## Step 24 test: Safe closing

| # | Test | Pass when |
|---|------|-----------|
| 1 | Nothing running | Close the app: it closes at once, as before, and the song is saved |
| 2 | Still working | Start "Split into stems" on a long song (or Make the harmonies, an export, a sound pack download), then close the app: a box says what is still working, with "Close when finished", "Stop and close" and "Keep working" |
| 3 | Close when finished | The title bar says "closing when the work is finished...". When it is done (the stems appear), the app saves and closes by itself |
| 4 | Stop and close | The app stops the work and closes within a few seconds. Open the song again: no half-made stems or harmonies are left |
| 5 | Keep working | The box closes and the work carries on |
| 6 | Recording | Close while recording: the take is finished and kept in the song |
| 7 | After a power cut | Files that were half-written when the computer switched off are cleaned away the next time the song opens |

---

## Step 23 test: Recordings follow tempo changes

| # | Test | Pass when |
|---|------|-----------|
| 1 | The question | In a song with a recording, change the Tempo: a box asks "Stretch them" or "Leave them". It is asked once per song |
| 2 | Stretch them | Change the tempo with + a few times: after about a second the song line says "Stretching N recordings to ... BPM", then the clips get shorter or longer and stay on the same bars. Play: the voice stays with the band and the click, at the same pitch |
| 3 | Back again | Return to the tempo you recorded at: the untouched original recording plays again |
| 4 | Leave them | Right-click the Tempo > "Stay where they are in time", change the tempo: recordings do not move at all (for an imported song you want to learn) |
| 5 | Loops | Loops that follow the tempo still follow it as before |
| 6 | Timing correction | A recording with timing correction, stretched to a new tempo, then "Remove timing correction": the uncorrected take comes back, stretched to the current tempo |

---

## Plan Step 22 test: Chord detection, stems and the help assistant

| # | Test | Pass when |
|---|------|-----------|
| 1 | Find the chords | Import a song (or record one) at bar 1, with the song's Tempo set to match. Right-click the clip > "Find the chords (put them on the Chord Track)...": after a few seconds the panel lists the chords bar by bar with their numbers, the key the music seems to be in, and the recording's tempo |
| 2 | Tempo warning | If the song's tempo is different from the recording's, the panel says so (set the Tempo, line the clip up with bar 1, try again) |
| 3 | On the Chord Track | "Put them on the Chord Track" replaces the chords in that range. Ctrl+Z takes them away. Chords marked "not sure" are worth checking by ear |
| 4 | MIDI | Right-click a MIDI clip > "Find the chords...": works the same, from the notes |
| 5 | Stems | Right-click a stereo song > "Split into stems (voice, drums, bass, other)..." > "Split into stems": the percentage counts up, then four tracks appear under it (Voice, Drums, Bass, Other) and the original is muted. Solo each one |
| 6 | Stems add up | Un-mute the original and mute the four stems, then swap: it sounds the same |
| 7 | Help | "Help" at the top (or F1): type a question in English or French ("how do I record my voice", "comment enregistrer", "makossa", "distrokid"). The best answers are listed; click one to read the steps |

---

## Plan Step 21 test: African and Cameroonian groove pack

| # | Test | Pass when |
|---|------|-----------|
| 1 | Groove menu | Open a Drums clip (drum grid) > "Groove": grooves are grouped under Church, Cameroon, West Africa, Central Africa and Fill. Other time signatures are in their own submenu |
| 2 | Cameroon | Makossa (classic), Makossa (modern dance), Assiko (Bassa, bottle), Bend skin, Mangambeu (Bamileke); in 6/8 Bikutsi (6/8, fast); in 12/8 Njang (North-West) |
| 3 | West and Central Africa | Highlife (Ghana classic), Ghana praise (kpanlogo feel), Afrobeat, Nigerian praise (Igbo, ogene), Yoruba praise (talking drum), Soukous / Ndombolo, Rumba congolaise, African 12/8 worship (bell); fills: djembe and talking drum, congas |
| 4 | Percussion rows | Choosing an African groove adds rows under Kick: djembe, congas, talking drum (high, low, rise, fall), bells, shaker, tambourine, claves (bottle), woodblock. "Show the African percussion rows" in the Groove menu shows them on any clip |
| 5 | Sound | Press Play: the Drum Kit plays the congas, shakers, bells and talking drum itself (no download needed). The talking drum rise and fall really bend the pitch |
| 6 | Own track | "+ Track" > "African Percussion" for a separate percussion track (its own fader), or "Balafon" for a melodic balafon you play on the keys |
| 7 | Band styles | "Band..." has new styles: Bikutsi praise, Assiko praise, Bend skin praise, Mangambeu praise, Njang worship (12/8), Nigerian praise (Igbo), Yoruba praise (talking drum), Afrobeat praise, Soukous praise, Congolese rumba worship, African 12/8 worship. Makossa praise and Highlife praise now use the new grooves |
| 8 | Fills | With an African style, the fill before a new section is djembe slaps ending with a talking drum call, then a crash |

---

## Plan Step 20 test: Backing band that follows your chords

Put chords on the Chord Track (a progression from the palette is quickest) and, if you like, Verse and Chorus in the Sections lane.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | "Band..." (next to Snap): the panel says how many chords it will play, in which key and bars, and lists your sections |
| 2 | Styles | Praise (mid tempo), Worship ballad, Gospel shout (fast), Praise shuffle, Makossa praise, Highlife praise, Afro-worship, Hymn (traditional), 6/8 worship, 12/8 slow gospel. Styles for another time signature are greyed out. Each has a one-line description with a tempo range |
| 3 | Players | Bass, Piano, Organ, Guitar, Drums. Each shows the track it will play on, or "new track" |
| 4 | Make | "Make the band": one clip "Band: <style>" on each player's track; missing tracks are added (Guitar gets the new Gospel Guitar). Press Play: everyone follows the chords |
| 5 | Sections | With "Follow the sections": verses and intro lighter (fewer notes, softer), choruses, bridges and vamps fuller |
| 6 | Fills | In the bar before each new section: the bass walks up to the new chord, the piano plays a run up to it, the drums play a fill and a crash lands on the new section |
| 7 | Ending | The last chord: everyone plays it together and holds it, with a crash |
| 8 | Editable | Open any band clip in the piano roll (or the drum grid): every note can be changed |
| 9 | Again | Change chords or style and press "Make the band" again: the band clips are replaced, no extra tracks. "Chords to notes" clips on those tracks are replaced too; your own clips are kept and the panel tells you if they overlap |
| 10 | Remove | "Remove the band" removes only the band's clips; Ctrl+Z brings them back |
| 11 | Guitar | "+ Track" > "Clean Electric Guitar" or "Nylon Guitar": play the on-screen keyboard. Short notes sound muted ("chick"), long notes ring |

---

## Plan Step 19 test: Harmony generator and choir stacks

Record (or import) one lead vocal, with chords on the Chord Track.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | Right-click the vocal clip > "Harmony and choir stacks...". The panel shows the key, how many chords it follows and "Melody: N notes, from ... to ..." |
| 2 | Preview | The box shows the melody in gold and each ticked part in its colour. Tick and untick parts: the preview changes at once |
| 3 | Ready-made | "Choose a ready-made choir...": Full choir, Big choir, Gospel trio, Close harmony, Duet, Thirds and sixths, Lead sings low, Unison choir. Each ticks its parts and sets the singers |
| 4 | Make | "Make the harmonies": the percentage counts up, then one track per part appears under the lead (Alto, Tenor, Bass...), all through a Choir bus (or the bus the lead already uses) |
| 5 | In tune | Play: every part sings the words of the lead on its own notes, in tune with the chords. The bass follows slash chords (C/E gives E) |
| 6 | A real choir | Singers 3, Tight - loose 70 %: each part is wider in stereo and sounds like several people. Tight 0 % with 1 singer sounds like one clean studio singer |
| 7 | Tone | Tenor, bass and octave below sound deeper and darker than the lead, not like the lead slowed down |
| 8 | Follow new chords | Change a chord on the Chord Track, open the panel again, "Make the harmonies": the same tracks are updated (no new tracks), following the new chord. Your fader levels and effects on them stay |
| 9 | No chords | On a song with an empty Chord Track, the parts follow the key (the panel says so) |
| 10 | Remove | "Remove the harmony tracks" removes them; Ctrl+Z brings them back |
| 11 | Refused | A clip with several takes, or one played faster, says what to do first |

---

## Plan Step 18 test: Release package for distributors

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | Export > "Release package for distributors...". Left: the song's details. Right: cover, "Ready for the stores?" and "Make the release package" |
| 2 | Filled in | Title, lyrics (from the song's lyrics), release date (4 weeks from today) are filled in. The artist name is remembered for the next song |
| 3 | Details | Version, Featuring, Songwriters (real full names), Producer, Mixed and mastered by, Label, Sub-genre (Contemporary Gospel, Praise & Worship, African Gospel, Makossa Gospel...), Language (English, French, Pidgin, Ewondo, Duala...), ISRC, UPC, Clean/Explicit, copyright lines, preview start |
| 4 | Checks | Red lines first: "feat." in the title, a missing songwriter, a wrong ISRC (12 characters, like CM-ABC-26-00001) or UPC (the last digit is checked), a cover that is not square or too small. Orange: release less than a week away, cover under 3000 |
| 5 | Cover | "Choose cover art...": a square JPG or PNG. Bigger than 3000 x 3000 is saved as 3000 x 3000 |
| 6 | Make it | "Make the release package": a few seconds later a folder in Documents/Gospel Production Studio/Releases with: the master WAV (24-bit, 44.1 kHz), a 320 kbps MP3 with the title, artist, songwriter, copyright, ISRC and cover inside, "Cover (3000x3000).jpg", "Lyrics.txt", "Song details.txt" |
| 7 | Audio checked | After it is made: length, peaks (red if the master clips), loudness, and silence at the start (orange if more than 2 s) |
| 8 | Accents | A title like "Jesus est la" with French accents shows correctly in the MP3 on a phone |
| 9 | Saved | Close and open the page again: everything you typed is still there (it is saved with the song) |

---

## Plan Step 16 test: Analysers and reference track

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | Mixer > Master strip > "Analyser". The page shows Spectrum, Tone balance, Stereo and Loudness |
| 2 | Live | Press Play on the page: the gold curve moves with the music (deep bass on the left, air on the right); Loudness "Now" and "Since you pressed Play" fill in |
| 3 | Whole song | "Measure my whole mix": a few seconds later the white curve shows the whole song and "Measured: -14.5 LUFS, 132 s" |
| 4 | Reference | "Choose a reference song..." (an MP3 or WAV of a gospel song you like). Its blue curve appears, with "Reference -9.8 LUFS, played down 4.7 dB to match your mix" |
| 5 | A/B | While playing, click "B: Reference" and "A: Your mix": the switch is smooth (no click) and both play equally loud, so you compare the sound and not the volume |
| 6 | Line up | "Reference starts at": move it so the reference's chorus plays when your chorus plays |
| 7 | In words | Below: "Bass: 3.2 dB more than the reference...", "Loudness: your mix is 4.7 dB quieter... Mastering can bring it up", "Stereo: narrower" |
| 8 | Tone balance | Five bars (Bass, Low mids, Mids, Presence, Air): green = close to the reference, orange = a little off, red = far off |
| 9 | Safe | Export the song with "B: Reference" on: the file has only your mix. Leaving the page always switches back to A. The reference is remembered with the song (not copied into it) |

---

## Plan Step 14 test: Buses and sidechain

| # | Test | Pass when |
|---|------|-----------|
| 1 | New bus | Mixer > "+ Bus" (top right): Choir bus, Vocal bus, Band bus, Drum bus or an empty Bus. It appears after the tracks, with its own effects and fader ("Choir bus" comes with EQ and a glue compressor) |
| 2 | Send into it | Under each track's name in the Mixer, click "-> Master" and choose the bus. The track now shows "-> Choir bus"; the bus shows "Bus - 2 tracks". Also from the track menu in the timeline: "Output" |
| 3 | One fader | Moving the bus fader turns all its tracks up or down together; muting the bus mutes them all; effects on the bus act on the blend |
| 4 | Remove | Click the bus name > "Remove the bus": its tracks go straight to the master again. Ctrl+Z works |
| 5 | Export | Stems: tracks in a bus come out together as one stem named after the bus |
| 6 | Ducking | Right-click the Bass track > "Duck under another track (sidechain)" > Drums. The bass dips a little on each kick (the header says "(ducks)") |
| 7 | Voice over music | On a music track or bus, duck under the voice track with "Music dips under the voice (smooth)": the music drops while someone speaks and comes back after |
| 8 | How | "How": Gentle, Kick pushes the bass (tight), Music dips under the voice (smooth), Strong pump (dance praise). "Off" removes it |

---

## Plan Step 13 test: Timing correction

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | Right-click a recorded clip (voice, guitar, drums) > "Timing correction (line up to the beat)...". A panel opens next to the clip |
| 2 | Note starts | "Found N note starts": grey marks show where each word or note starts now, gold marks where it will go, over the grid lines |
| 3 | Grid | "Line up to": 1/4, 1/8, 1/8 triplets (shuffle, 12/8), 1/16, 1/16 triplets |
| 4 | Strength | 100 % = exactly on the grid (stiff); 40-60 % tightens but keeps the human feel |
| 5 | Leave alone | Note starts already within 10 ms (or what you choose) are not touched |
| 6 | Find | "More note starts" for soft singing, "Fewer" if it finds breaths or noises |
| 7 | Apply | Press Apply, play: late and early words are now tighter, the voice sounds the same (nothing cut, no clicks) |
| 8 | Again | Apply again with other settings: it always starts from the original recording. "Remove timing correction" brings the original back; Ctrl+Z also works |
| 9 | Safe | A clip played faster or slower, a loop that follows the tempo, or a clip with several takes says what to do first instead |

---

## Plan Step 12 test: Vocal chain, de-esser and ASIO

| # | Test | Pass when |
|---|------|-----------|
| 1 | Vocal chain | Right-click a vocal track > "Vocal chain": choose "Lead vocal: warm worship". The track gets EQ, Compressor and De-esser, and its Rev and Dly knobs in the Mixer are set (-12 and -24 dB) |
| 2 | Kinds of voice | Also: bright and modern, low voice (men), high voice (women, children), choir and backing vocals, preaching and spoken word |
| 3 | Change | Choosing another chain replaces the first one (never two sets). "Remove the vocal chain" takes it off. Ctrl+Z undoes |
| 4 | Fine tune | Each part opens with its own presets ("Starting point..."); effects you added yourself are left alone |
| 5 | De-esser | Mixer > "+ Effect" > De-esser. Sing "sister, Jesus saves": the bar "Turning the S down" moves on S sounds only, and the S is softer while the vowels sound the same |
| 6 | Settings | Frequency (about 5.5 kHz for men, 7.5 kHz for women), Threshold (lower = acts more), Range (the most it turns down) |
| 7 | ASIO | Audio Setup > "Audio device type" now lists **ASIO**. With your audio interface's ASIO driver, latency drops below 10 ms. (If ASIO is not listed, open the GitHub build log, step "Download the ASIO SDK", and send it to Claude) |

---

## Stage 2, Step 18 test: Your own sounds and loops

| # | Test | Pass when |
|---|------|-----------|
| 1 | My Sounds | "+ Track" > "My Sounds (your own samples)": a new track and its window open, saying "No sounds yet" |
| 2 | Add | "Add sounds..." (choose several files) or drag audio files onto the window: each sound gets its own key, from C3 up. The files are copied into the song's "Sounds" folder |
| 3 | Hit | A choir "hey!", horn stab or cymbal swell set to "Hit" plays all the way through on its key, even with a short tap |
| 4 | Melodic | One note of a flute or a voice "ooh" set to "Melodic": it covers two octaves and plays in tune ("recorded on C5" shows the note it found) |
| 5 | Edit | Change a sound's key, volume or name (double-click the name); "Play" lets you hear it; "Remove"; Ctrl+Z brings it back |
| 6 | Drop on track | Drag audio files onto the My Sounds track in the timeline: they are added to its sounds |
| 7 | Import a loop | Right-click an audio track > "Import a loop (follows the song's tempo)...": a loop named like "Shaker 96bpm" is placed at the playhead and fills at least 4 bars |
| 8 | Tempo | Change the song tempo: after a moment the loop is remade and still fits its bars, with no drift (thin lines show where it repeats) |
| 9 | Its tempo | Right-click the loop > Loop: "twice as fast", "half as fast" or "Type its tempo..." if the guess was wrong |
| 10 | Key | Loop > "Its key" (found from names like "Guitar riff G"), then "Follow the song's key": change the song key and the loop moves to it |
| 11 | Repeat | Loop > Repeat 1, 2, 4, 8 times or "to the end of the song" |
| 12 | Drop | Drag an audio file from your folder onto an audio track (or empty space for a new track). A loop with its tempo in the name follows the tempo by itself |
| 13 | Stop | Loop > "Follows the song's tempo (click to stop)": it plays at its own speed again |

---

## Stage 2, Step 17 test: Drum programming

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | Double-click a clip on the Drums track (or right-click the lane > "New empty clip here"): the drum grid opens with Crash, Ride, Hats, Toms, Snare, Clap, Kick, Tambourine, Shaker, Cowbell |
| 2 | Hits | Click a box: the drum plays and the box lights up. Click again: it goes. Drag along a row to paint many. Shift-click: accent (white dot). Right-click: Accent / Normal / Soft / Ghost |
| 3 | Hear | Click a drum name on the left: you hear that drum |
| 4 | Groove | "Start from a groove": Gospel 16ths (ghost notes), Praise shuffle, Church shuffle, Makossa, Highlife, Afro-worship... 6/8 worship and Bikutsi are under "Other time signatures" (or show directly in a 6/8 song) |
| 5 | Length | The bars box (1, 2, 4, 8, 16 bars) makes the clip longer. "Repeat bar" copies bar 1 (or the chosen bar) to the end |
| 6 | Fill | Click a bar number at the top to choose it, then "Add a fill": Snare roll, Toms down, Triplet run, Kick and snare build, Stop (break), Crash on the next 1 |
| 7 | Swing | Swing 0 to 100 %: the off-beats move late (66 % is a full shuffle). The boxes stay where they are; play to hear it |
| 8 | Feel | "Humanise": small changes in timing and strength so it does not sound like a machine |
| 9 | Play | Press Play: the column being played is highlighted and the grid follows the song |
| 10 | Undo | Ctrl+Z / Ctrl+Y work inside the grid. Esc or "Close" returns to the tracks |
| 11 | Piano roll | "Piano roll" opens the same clip in the piano roll; its "Drum grid" button brings you back |

---

## Stage 2, Step 16 test: Vocal tuning

| # | Test | Pass when |
|---|------|-----------|
| 1 | Add | Right-click Lead Vocal > "Vocal tuning (correct the pitch)...": the Vocal tuning window opens. (Also in Effects and in the Mixer's "+ Effect") |
| 2 | Key | "Key" says "Follow the song" with the song's key, for example "Ab major". Change the song's key: it follows |
| 3 | Watch | Play a recorded vocal: the grey line is what was sung, the gold line is what you hear; "Singing G4 +23 cents -> heard G4 (in tune)" |
| 4 | Natural | Strength 80 %, Speed "natural": flat or sharp notes come into tune, but slides and vibrato stay human |
| 5 | Runs | "Notes: key + blue notes" lets gospel runs use the b3 and b7 without being pulled to the wrong note |
| 6 | Robotic | Speed all the way right ("robotic"), Strength 100 %: the hard, stepped effect |
| 7 | Voice | "Lower voice (men)" for low singers, so the low notes are found |
| 8 | Off | "On" button switches it off to compare; Ctrl+Z undoes changes; settings are saved with the song |
| 9 | In time | The tuned voice stays in time with the band (the delay is compensated automatically) |

---

## Stage 2, Step 15 test: Mastering

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | Mixer > on the Master strip press **Mastering** (gold when it is on). The Mastering page opens with "Back" |
| 2 | Start | Choose a starting point, for example "Worship ballad": Mastering turns on and the sliders move |
| 3 | Set loudness | Choose the loudness ("Streaming -14 LUFS" for YouTube, Spotify and Boomplay; "Loud -11" for an uptempo song) and press **Set loudness for the whole song**: after a few seconds it says "Done: the whole song measures ... LUFS" |
| 4 | Meters | Play the song: the big number shows the loudness now; "Highest peak" never goes above -1 dB |
| 5 | Compare | Press "Compare: hear before" while playing: you hear the song without mastering at the same loudness, so you judge the real difference. Press again for after |
| 6 | Tone | Warmth (body), Clarity (voices forward), Air (sparkle): each changes the sound gently. Double-click a slider to reset it |
| 7 | Glue and width | Glue makes the band sound together (the "Glue working" bar moves on loud parts); Width makes it wider |
| 8 | Push | Push up: louder; if the "Limiter working" bar goes past about 6 dB, a note says to lower Push |
| 9 | Export | Export the song: the Export page says "Loudness is set by Mastering", and the MP3 sounds like the mastered song |
| 10 | Off | Untick "Mastering on": the song plays as mixed. Everything is saved with the song |

---

## Stage 2, Step 14 test: Bounce and export

**Before building:** this step changes the Windows workflow once (it adds the MP3 encoder). Replace the text of `.github/workflows/build-windows.yml` with `build-windows-workflow.txt`.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | In the Studio press **Export**: the Export page opens with "Back" |
| 2 | MP3 of the song | "The whole song", "MP3 320 kbps", "Ready to share" ticked, fill in Artist, press Export: a progress bar, then "Done!". "Open folder" shows the MP3 in Documents\Gospel Production Studio\Exports\(song name) |
| 3 | Play it | The MP3 plays on the phone and in WhatsApp, at a similar loudness to songs on streaming services, with the title and artist shown |
| 4 | Smaller file | "MP3 192 kbps": the file is about 40% smaller |
| 5 | Studio master | "WAV 24-bit": a large WAV with the full quality |
| 6 | Stems | "Each track on its own", WAV 24-bit: one file per track ("Song - Lead Vocal.wav" and so on), all the same length from the start of the song, so another studio can line them up |
| 7 | Loop | Set the loop around the chorus, choose "Only the loop range": just the chorus is saved |
| 8 | Stop | Press Stop during a long export: it stops and no half-made file is left |
| 9 | Too loud | Push a fader far up and export "as mixed" (Ready to share unticked): a message says which part clips and what to do |
| 10 | Freeze | Right-click a heavy track (many plugins) > "Freeze": it shows FROZEN and the computer works less. "Unfreeze" to change it again |
| 11 | Names | Exporting again never overwrites: the new file is called "Song (2).mp3" |

---

## Fixes in 0.12.1

| # | Fix | Check |
|---|-----|-------|
| 1 | Sound Library, Plugins and Audio & MIDI Setup open as pages inside the window, with a "Back" button (Esc works too). They can no longer open behind the main window | Open each one from the top bar, then press Back |
| 2 | Plugin windows always stay in front of the main window | Open a VST3 plugin's window, click the main window: the plugin stays visible |
| 3 | The organ no longer distorts into noise on chords: the overdrive was far too hot. Drive now adds warmth at the normal setting and growl only when turned up | Play big chords on the Organ track: clean and warm. Turn Drive up in the organ panel for grit |

---

## Stage 2, Step 13 test: Automation

| # | Test | Pass when |
|---|------|-----------|
| 1 | Show a lane | Click **A** on a track: a lane opens under it showing "Volume" as a dashed line |
| 2 | Draw | Click in the lane at bar 9 (high), bar 13 (low): a line joins the points. Play: the volume follows the line, and the fader moves by itself |
| 3 | Edit points | Drag a point (it shows the value, for example "-12.0 dB"); hold Shift to move without snapping; double-click or right-click a point to remove it |
| 4 | Choose the setting | Click "Volume" in the track header: choose Pan, Reverb send, Delay send, or a setting of an effect or plugin (for example EQ > Mid gain 2). A dot marks settings that already have a line |
| 5 | Last chorus lift | On Choir draw the volume a little higher from the last chorus; on Lead Vocal raise "Reverb send" on the final note |
| 6 | Record by hand | Turn on **Write** (red), play the song and move a fader in the Mixer (or a track's volume slider): when you let go, the movement has become points in the lane. Turn Write off when done |
| 7 | Clear | Right-click in the lane > "Remove all points on this line" |
| 8 | Hide | Click A again (or right-click > Hide): the lane closes, the automation still plays. The A turns gold on tracks that have automation |
| 9 | Undo and save | Ctrl+Z undoes point changes; switching songs and back keeps everything |

---

## Stage 2, Step 12 test: The mixer

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | In the Studio press **Mixer** (or X): every track has a strip, then Reverb and Delay (shared), and Master on the right. "Tracks" (or X) goes back |
| 2 | Faders | Play the song and move a fader: that part gets louder or softer, the meter moves, the dB value shows. Double-click: back to 0 dB |
| 3 | Meters | Green while normal, amber near the top, red when too loud. The little bar at the top of a meter turns red after clipping (click it to clear) |
| 4 | Pan | Turn Pan on the organ left and the piano right: they separate in headphones |
| 5 | Shared reverb | Turn "Rev" up on Lead Vocal and Choir: they sound as if in the same church. Turn it fully left (or double-click) for "off" |
| 6 | Shared delay | Turn "Dly" up on Lead Vocal: echoes in time with the song |
| 7 | Reverb room | Click "Reverb" on the Reverb strip: choose a starting point (Small room, Church, Big cathedral) |
| 8 | Built-in effects | On Lead Vocal click "+ Effect" > "EQ (4 bands)", then choose the starting point "Lead vocal: clear"; add "Compressor" > "Lead vocal: gentle" |
| 9 | Plugin effects | "+ Effect" also lists your VST3 effects (Step 11) |
| 10 | Effect options | Right-click an effect on a strip: open, turn off (it shows crossed out), move earlier or later, remove |
| 11 | Master | "+ Effect" on Master works on the whole song (for example a gentle compressor) |
| 12 | Mute and solo | M and S on a strip work, and match the tracks view |
| 13 | Undo | Ctrl+Z undoes fader, pan, send and effect changes |
| 14 | Saved | Switch to another song and back: every fader, send and effect is as you left it |

---

## Stage 2, Step 11 test: Plugins (VST3)

Before testing: install at least one free VST3 instrument (for example Spitfire LABS) and one VST3 effect (for example Valhalla Supermassive or TDR Nova) with their own installers. They go into C:\Program Files\Common Files\VST3.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Scan | Press **Plugins** at the top, then "Scan for plugins": it says "Checking 1 of N..." and then lists each plugin as Instrument or Effect, "Ready" |
| 2 | A faulty plugin | If a plugin crashes or hangs while being checked, the studio keeps running: that plugin shows "Failed" and is skipped next time ("Try failed again" retries it) |
| 3 | Other folders | "Add a folder..." for plugins kept somewhere else (for example D:\VST3): they are found too |
| 4 | Plugin instrument | Right-click Keys > Change instrument > "Your plugins (VST3)" > pick one: its own window opens, the track shows its name, and your MIDI keyboard plays it |
| 5 | Open it again | Click the instrument name under the track name: the plugin's window opens |
| 6 | New plugin track | "+ Track" > "Plugin instrument (VST3)": a new track with that instrument |
| 7 | Effect | Right-click Lead Vocal > Effects > pick a reverb: its window opens and the track shows "+1 FX" |
| 8 | Effect order | Add a second effect, then Effects > "1. ..." > "Move later": the order changes (you hear the difference with EQ then reverb, or reverb then EQ) |
| 9 | Bypass and remove | Effects > the effect > "Turn off (bypass)" shows "(off)"; "Remove" takes it away; Ctrl+Z brings it back |
| 10 | Saved with the song | Change a knob in a plugin, switch to another song and back: the plugin and its settings are still there |
| 11 | Song from another computer | If a plugin is not installed, the track says "(not installed)" in orange, nothing crashes, and its settings stay safe in the song until you install it |

---

## Stage 2, Step 10 test: Comping and audio editing

| # | Test | Pass when |
|---|------|-----------|
| 1 | Comp editor | Record 3 takes in a loop, right-click the clip > "Comp takes...": each take is a row with its waveform and a Listen button |
| 2 | Pick the best parts | Drag across take 2 where it was best, click take 3 for another part: the Comp row shows T1, T2, T3 blocks |
| 3 | Make the comp | Press "Make comp": a new take "Comp" is added and selected; playing the song plays the chosen parts with smooth joins |
| 4 | Split | Click a clip, put the playhead where you want to cut and press S (or right-click > Split): two clips, no click at the cut |
| 5 | Trim | Drag a clip's left or right edge: the clip gets shorter or longer |
| 6 | Fades | Drag the small square at a clip's top-left (fade in) or top-right (fade out): a shaded corner appears and the sound fades. Right-click > Fades for the shape |
| 7 | Crossfade | Drag one clip so it overlaps another on the same track: the overlap blends smoothly |
| 8 | Clip gain | Right-click > "Clip gain, pitch and speed...": move Gain; the clip shows "dB" and the waveform grows or shrinks |
| 9 | Normalise | Right-click > Normalise: a quiet clip becomes full level (peak -1 dB) |
| 10 | Pitch | In the same window, move Pitch by +2: the clip sounds a tone higher, same speed; the clip shows "+2.0" |
| 11 | Speed | Move Speed to 90% with "Keep pitch" ticked: slower, same key, the clip gets longer. Untick it for the tape effect (slower and lower) |
| 12 | Undo | Ctrl+Z reverses each edit |

---

## Stage 2, Step 9 test: Audio recording

Before testing: plug in the audio interface, open **Audio & MIDI Setup**, choose it, and tick its input channels.
Use headphones while recording so the speakers do not leak into the microphone.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Choose an input | Right-click Lead Vocal > Input: your interface's inputs are listed; choose the microphone input. The track shows "In: ..." |
| 2 | Mono or stereo | Input > "Inputs as single channels" for a microphone or guitar; "stereo pairs" for a stereo keyboard |
| 3 | Arm and meter | Press R on Lead Vocal: it turns red and a level meter moves when you sing. Green is good, orange is loud, red is too loud: turn the interface gain down |
| 4 | Hear yourself | Monitoring > Auto: you hear your voice while armed. If you hear a delay, choose Off and use the interface's direct monitoring knob |
| 5 | Record | Press Record (R): after the count-in, a red band grows on the armed track; Stop: the recording appears as a clip with its waveform |
| 6 | Voice and keys together | Arm Lead Vocal (R) and also Keys (R): one recording captures both |
| 7 | Files are safe | The recordings are WAV files in the song's own folder (Documents\Gospel Production Studio\Songs\...) |
| 8 | Takes | Turn on Loop, arm a track and record through the loop 3 times: one clip says "Take 3/3". Right-click it > Takes: pick the best take and hear it |
| 9 | Keep a take | Right-click > "Keep this take, delete the others..." keeps the one you hear |
| 10 | Punch in/out | Set the loop range over the bar to fix, turn **Punch** on (the range turns red), start a bar or two before it and record: only the red range is replaced |
| 11 | Voice memo | Right-click an audio track > "Place the song's voice memo at the playhead": the memo you sang in the Song Library becomes a clip |
| 12 | Import | Right-click an audio track > "Import audio file at the playhead...": WAV, MP3, FLAC or OGG becomes a clip (a copy is kept with the song) |
| 13 | Undo | Ctrl+Z removes a recording or an import |

---

## Stage 1, Step 8 test: Instruments and grooves

**One-time setup on GitHub (no data from your phone):** create the workflow file
`.github/workflows/build-sound-packs.yml` (copy it from `build-sound-packs-workflow.txt`),
upload the `tools` folder, then in Actions run **Build sound packs** once. It makes the
release "sounds-v1" with the packs. The repository must be public for the app to download them.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Sound Library | The **Sound Library** button (top right) lists Grand Piano, Grand Piano (full detail), Electric Bass, Drum Kit and Upright Piano, each with its size |
| 2 | Download once | **Download** shows progress, then INSTALLED. Close and reopen the app: still installed, nothing downloads again. **Remove** frees the space |
| 3 | New song layout | A new song opens with Keys (Grand Piano), Organ (Gospel Organ), Bass (Electric Bass), Drums (Drum Kit), Lead Vocal and Choir |
| 4 | Grand Piano | Soft and hard playing sound different (velocity layers); the sustain pedal holds notes; letting go gives the gentle damper sound |
| 5 | Instrument settings | Click the instrument name under a track name (or right-click the track > Instrument settings) |
| 6 | Organ drawbars | Drag the drawbars down and up while holding a chord: the sound changes at once. Double-click a drawbar for 0/8 |
| 7 | Organ presets | Gospel Full, Gospel Classic, Praise Break (Shout), Sweet Ballad, Soft Worship Pad, Comping Stabs, Soul Classic, Flute Lead, Bright Choir Lift |
| 8 | Leslie | Slow / Fast buttons, or the **mod wheel** on your keyboard: the horn speeds up in about a second, the drum takes a few seconds (the light shows Slow/Fast) |
| 9 | Percussion | Turn on Percussion: the first note of a phrase "pings"; notes played legato do not re-trigger it |
| 10 | Piano / bass / drums settings | Volume, Tone, Release and Touch change the sound |
| 11 | Change instrument | Right-click a track > Change instrument. **+ Track** offers every instrument. Older songs keep Studio Keys until you change it |
| 12 | Not downloaded yet | A track whose pack is not installed says "(not downloaded)" in orange and plays the simple Studio Keys sound |
| 13 | Grooves | Right-click the Drums lane > Insert groove: the groove fills from that bar to the end of the section. 6/8 and 12/8 grooves appear in 6/8 and 12/8 songs |
| 14 | Drum mapping | With a groove playing, the kick, snare, hi-hat and toms sound in the right places |
| 15 | Saved | Instrument choice and all settings come back when the song is reopened |

---

## Stage 1, Step 7 test: Piano roll

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open | Double-click a clip (or right-click > Open in piano roll, or select it and press Enter). A new empty clip opens straight away |
| 2 | Room for notes | While the piano roll is open the metronome card folds away. **Split** shows the timeline above (on big screens); **Close** or Esc returns |
| 3 | Plays the right sound | Opening a clip arms its track: clicking the keys on the left, or a note, plays that track's sound |
| 4 | Chords behind the notes | The chord strip is under the bar numbers; the gold rows are the tones of each chord, the brighter row is its root |
| 5 | Check notes | White dot = chord tone. Red = clashes (outside the chord and the key). Notes pushed a little early are judged by the chord they sound over |
| 6 | Fit to chords | Moves red notes onto the nearest chord tone (selected notes, or all) |
| 7 | Draw | Press B (Draw): click to add a note, drag to set its length. V goes back to Select, where double-click adds a note |
| 8 | Edit notes | Drag to move (snaps to the grid, any pitch), drag an edge to resize, Alt-drag copies, right-click deletes, drag on empty space box-selects, Shift adds |
| 9 | Grid and length | Grid 1/4 to 1/32, triplets, bar or off; new notes use the last length (or the length chosen) |
| 10 | Velocity | Drag the bars in the lane at the bottom; starting on a selected note changes all selected notes together |
| 11 | Edit menu | Quantise (100%, 75%, 50%, with ends), Transpose, Velocity, Legato, Humanise, Select clashing notes, Copy, Paste, Duplicate |
| 12 | Keys | Up / Down transpose (Shift = octave), Left / Right move by the grid, Q quantise, Ctrl+A / C / V / D, Delete, Ctrl+Z / Ctrl+Y. Space still plays |
| 13 | Clip grows | Drawing past the end of the clip extends it to the next bar line |
| 14 | Zoom and scroll | Ctrl + wheel zooms across, Alt + wheel zooms the note height, the wheel scrolls notes, Shift + wheel scrolls across; **Fit** shows the whole clip |
| 15 | Playback | While playing, the gold playhead moves and the view follows it |
| 16 | Timeline follows | Changes appear at once in the clip on the timeline, and are saved with the song |

---

## Stage 1, Step 6 test: Chord Track and gospel palette

| # | Test | Pass when |
|---|------|-----------|
| 1 | Chords lane | Under Sections there is a **Chords** lane; the **Palette** button on it shows or hides the chord palette on the right |
| 2 | Key follows the song | The palette title shows the song's key; change the key at the top and every chord name changes with it (numbers stay the same) |
| 3 | Display choice | The box at the top of the palette switches between **Numbers and names**, **Numbers only** and **Names only**, on the palette and on the timeline |
| 4 | Hear a chord | One click on a chip plays the chord on the Keys sound; Shift-click plays it as an arpeggio |
| 5 | Add at the playhead | Double-click a chip: the chord is placed at the playhead (length from **Length**) and the playhead moves on, ready for the next one |
| 6 | Next suggestions | The gold **Next** row shows strong moves after the chord you clicked, added, or the one under the playhead |
| 7 | Palette groups | In the key, Sevenths, Gospel favourites, Passing diminished, Secondary dominants, Tritone substitutions, Borrowed and backdoor, Tension and endings |
| 8 | Build any chord | The row under the palette (degree, chord type, slash bass) with **Hear** and **Add** makes any chord |
| 9 | Progressions | The Progressions tab has 12 categories (including **African and Cameroonian**: makossa, assiko, highlife, bikutsi, bend-skin, seben, soukous). Choose Simple, Rich or Advanced gospel, **Hear** it, then **Insert at playhead** (it starts on the next bar line) |
| 10 | Edit on the timeline | Drag a chord to move it, drag its edges to change its length; double-click a chord to hear it as an arpeggio |
| 11 | Chord menu | Right-click a chord: Hear, Change chord type (7 groups), Slash bass, Reharmonise, Save, Delete |
| 12 | Reharmonise | Click the first chord, Shift-click the last, right-click: **Advanced gospel** adds approach chords (for example 3(7b9) before 6m, #4°7 before 5) |
| 13 | My Progressions | Save selected chords with a name; they appear in My Progressions in any key, and stay after the app is closed |
| 14 | Voicings | RH (Close, Spread, Gospel rootless, Shell, Fourths, Upper structure, Cluster) and LH (Root, Root + 5th, Octave) change how chords sound |
| 15 | Chords to notes | **Chords to notes...** writes the chords as real notes on Keys and Bass; press Play to hear the band follow the chords. Run it again after changes: it replaces, never doubles |
| 16 | Delete and undo | Select a chord and press Delete; Ctrl+Z brings it back |
| 17 | Saved with the song | Close and reopen the app: the chords are still on the song |

Detecting chords automatically from a recording stays in **Step 22**, as planned.

---

## Stage 1, Step 5 test: Tracks and timeline

| # | Test | Pass when |
|---|------|-----------|
| 1 | Starter tracks | A **new** song opened in the Studio has Keys, Bass, Drums, Lead Vocal and Choir |
| 2 | Sections | The Sections lane shows the song's lyrics sections (Intro, Verse 1, Chorus 1...) as coloured blocks |
| 3 | Ruler | Clicking the ruler moves the gold playhead; while playing, the timeline follows it |
| 4 | Loop by dragging | Dragging along the lower half of the ruler sets the loop; its edges can be dragged; the loop fields update |
| 5 | Record a take | Record (R) on the armed track (red R): after stopping, the take appears as a clip with your notes drawn inside |
| 6 | Arm another track | Clicking R on another instrument track makes the keyboard play and record there |
| 7 | Move clips | Drag a clip along the track and onto another track; it snaps to the beat (or bar / half beat / off) |
| 8 | Trim clips | Drag a clip's left or right edge to shorten or lengthen it |
| 9 | Clip menu | Right-click a clip: Rename, Duplicate, Copy, Delete |
| 10 | Shortcuts | Ctrl+D duplicate, Ctrl+C / Ctrl+V copy and paste at the playhead, Delete removes, Ctrl+Z / Ctrl+Y undo and redo |
| 11 | Empty clip | Double-click an instrument lane to add a one-bar clip (it is filled in the piano roll, Step 7) |
| 12 | Tracks | + Track adds instrument or audio tracks; right-click a track name for Rename, Colour, Move up/down, Delete |
| 13 | Mute / Solo / Volume | M and S work while playing; the volume slider changes the level (double-click for 0 dB) |
| 14 | Zoom and scroll | Ctrl + mouse wheel zooms, Shift + wheel scrolls sideways, **Fit** shows the whole song |
| 15 | Sections editing | Sections can be moved, resized, renamed, recoloured; + Section adds a new one |
| 16 | Autosave | Changes are saved every 10 seconds and when the app closes |
| 17 | Updates | **Updates** button answers (up to date, or offers the new version) |

---

## Stage 1, Step 4 test: Studio (transport, metronome, tempo, key)

Open a song in the Song Library and click **Open in Studio**.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Open in Studio | The Studio tab opens with the song's title, key, time signature and tempo |
| 2 | Play / Stop | Play (Space) counts bars and beats (1.1, 1.2...) and the beat lights move; Stop halts; Stop again returns to 1.1 |
| 3 | Metronome | The click is heard in time; beat 1 sounds higher and louder when "Accent beat 1" is on |
| 4 | Click sounds | Click, Woodblock, Rim and Cowbell each sound different; the volume slider works |
| 5 | Tempo | Changing tempo changes the click speed at once; the Song Library shows the new tempo |
| 6 | Tap tempo | Tapping Tap 4 times in time sets the tempo |
| 7 | 6/8 and 12/8 | 6/8 shows 6 beats (1 and 4 larger) and clicks all six; tempo is counted in dotted quarters |
| 8 | Key | Changing the key here updates the song in the Library (and the other way round) |
| 9 | Keyboard | The YPT-240 and the on-screen keys play the Studio Keys sound on every page |
| 10 | Record | Record (R) gives the count-in (1 bar by default), then records what you play on the keyboard |
| 11 | Playback | After stopping, Play from the start plays back what you recorded |
| 12 | Loop | Loop (L) repeats bars 1 to 4 (or the bars you set) |
| 13 | Clear Takes | Removes the recorded takes after asking |
| 14 | Saved | Close and reopen the app, open the song in the Studio: tempo, metronome settings and takes are kept |

The timeline that shows your takes as notes arrives in Step 5. The Song Library and Engine & Devices tests from earlier steps still apply.

---

## Stage 1, Step 3 test: Song Library

Your songs are saved in **Documents\\Gospel Production Studio\\Songs**, with one folder per song (song.json + memo.wav).

| # | Test | Pass when |
|---|------|-----------|
| 1 | Opens on Library | The app opens on the **Song Library** tab; the **Engine & Devices** tab shows the Stage 0 cards |
| 2 | New song | **+ New Song** adds "Untitled song" with today's date and puts the cursor in the title |
| 3 | Details | Title, date, key, tempo, time signature, theme, status, scripture and notes can all be set |
| 4 | Autosave | Edits save on their own within about 1 second; closing and reopening the app keeps everything |
| 5 | Lyrics | Sections can be added (Verse, Chorus, Bridge, Vamp...), typed into, moved up/down and removed |
| 6 | Numbering | Two verses show as "Verse 1" and "Verse 2" automatically |
| 7 | Voice memo | **Record Memo** records from the mic, shows the timer; **Stop Recording** saves it; **Play** plays it back |
| 8 | Replace memo | Recording again asks before replacing the old memo |
| 9 | Search | Typing a word from the lyrics finds the song |
| 10 | Filters | Status and theme filters narrow the list; the count shows "x of y songs" |
| 11 | Status colours | The list shows a coloured status chip for each song |
| 12 | Delete | **Delete Song** asks first, then moves the song to the Recycle Bin |
| 13 | Ten songs | Enter 10 of your real received songs: the list stays fast and tidy |

If the memo records silence, open **Audio & MIDI Setup** and choose the microphone input, and check Windows **Settings > Privacy > Microphone** allows desktop apps.

---

## Stage 0 test (Engine & Devices tab)

Tick each item on **your own computer**.

| # | Test | Pass when |
|---|------|-----------|
| 1 | Build | GitHub workflow finishes with a green tick |
| 2 | Install | The Setup .exe installs and the app opens from the desktop icon |
| 3 | Interface | Dark studio interface with gold accents; window resizes cleanly |
| 4 | Audio device | The Audio Engine card shows **Running**, your device name, and the sample rate |
| 5 | Test tone | **Test Tone** plays a steady tone and the Output meter moves; clicking it again stops the tone |
| 6 | Keyboard detected | The YPT-240 (USB) shows **Connected** in the MIDI Keyboards card |
| 7 | Keyboard plays | Pressing keys lights the on-screen keys gold, plays sound, and shows the note name |
| 8 | Velocity | Soft and hard key presses show different velocity numbers and volumes |
| 9 | Sustain | If a sustain pedal is connected, notes ring while it is held |
| 10 | Hot-plug | Unplugging the keyboard and plugging it back in: it reconnects without restarting the app |
| 11 | Setup window | **Audio & MIDI Setup** opens, and you can change the device and buffer size |
| 12 | Memory | Close and reopen the app: your audio settings are remembered |
| 13 | Latency | Shown in green or amber at buffer size 256 or lower |
| 14 | Microphone (when the interface arrives) | Speaking into the mic moves the Input meter |
| 15 | All Notes Off | Stops any stuck notes and the test tone |

**Note on latency:** with laptop sound (Windows Audio), latency of 20 to 40 ms is normal. The audio interface with its ASIO driver brings it under 10 ms. ASIO support is switched on in Stage 2.

---

## Project layout

```
GospelProductionStudio\
  CMakeLists.txt           build recipe
  .github\workflows\       GitHub build: makes the Setup .exe
  installer\               Windows installer script
  setup.bat, build.bat     optional local build
  Source\
    Main.cpp               application and main window
    MainComponent.*        main window, pages, audio and MIDI
    Song.*, SongLibrary.*  song data and safe saving
    MemoRecorder.*         voice memo record and playback
    SongListPanel.*        library list, search, filters
    SongEditor.*           song details, voice memo, lyrics
    SectionEditor.h        one lyrics section card
    LibraryPage.h          Song Library page
    StudioEngine.*         Tracktion Engine: devices, song project, transport, metronome
    StudioKeysPlugin.h     Studio Keys instrument inside the engine
    ClickSounds.*          metronome sounds (click, woodblock, rim, cowbell)
    StudioPage.*           Studio page: transport, tempo, key, metronome, loop
    TransportButton.h      drawn transport buttons
    Timeline.*             tracks, ruler, clips, sections, chords lane, loop, zoom, undo
    Theory.*               gospel harmony: chord types, numbers and names, voicings, palette, progression library
    ChordTrack.*           chords stored in the song, and Chords to notes
    ChordPanel.*           chord palette, progressions and My Progressions
    PianoRoll.*            piano roll: notes, velocity, quantise, chord check
    DrumGrid.*             drum grid: boxes per drum, grooves, fills, swing, humanise
    MySounds.*             your own samples on keys (hits and melodic sounds)
    Loops.*                loops that follow the song's tempo and key
    Sfz.*                  SFZ instruments: reading, sample loading, shared cache
    SamplerPlugin.*        Gospel Sampler: piano, bass and drum packs
    OrganPlugin.*          Gospel Organ: drawbars, percussion, vibrato, Leslie
    Instruments.*          the instrument list and where sound packs live
    SoundLibrary.*         Sound Library window: download packs once
    InstrumentPanel.*      instrument settings window (drawbars etc.)
    Grooves.*              drum grooves: church, Cameroon, West and Central Africa, fills
    Takes.*                choosing takes on recorded clips
    AudioEdit.*            split, fades, normalise, pitch and speed, comp plans and comp rendering
    CompEditor.*           the comping window: pick the best parts of each take
    PluginHost.*           VST3 plugins: safe scanning, the Plugins window, plugin windows
    Mixing.*               shared reverb and delay, sends, built-in effects and their starting points
    MixerView.*            the mixer: channel strips, meters, master
    Automation.*           which settings can change during the song, recording by hand
    AutomationLane.*       the line under a track: draw, drag and remove points
    Exporter.*             renders the mix, stems or loop in the background; loudness, clipping check
    ExportPage.*           the Export page
    Mp3Encoder.*           MP3 files (LAME), when the build includes it
    Loudness.h             loudness in LUFS, as streaming services measure it
    MasterPlugin.*         mastering: tone, glue, width, look-ahead limiter, compare
    MasteringPage.*        the Mastering page and "Set loudness for the whole song"
    VocalTunePlugin.*      vocal tuning: pitch finder, correction to the key, its window
    Timing.*               timing correction: line a recording up with the beat
    Harmony.*              harmony generator and choir stacks from one sung melody
    TempoFollow.*          recordings stretched (or kept in place) when the song's tempo changes
    BackgroundWork.*       all background work in one list: asked about, stopped and waited for when closing
    SongFiles.*            which files a song uses; deletes unused stem, harmony, tempo and timing copies on opening
    WavFiles.h             writes WAV files the current JUCE way
    SongSafety.*           versions of each song, repairing a half-saved song file, recovery after a power cut (Steps 43-44)
    LatencyCheck.*         measures the recording delay with a cable and corrects every recording (Step 28)
    Branding/              the logo: icon (SVG, PNG, Windows .ico) and full logo for dark and light backgrounds
    RealFeel.*             real-drummer feels and grooves (RealFeel.cpp is made by tools/make_feel.py in the GitHub build)
    Backing.*              the backing band: styles, players, fills and endings from the Chord Track
    GuitarPlugin.*         Gospel Guitar: plucked-string guitar (clean electric and nylon), no download
    Percussion.*           African percussion and balafon by synthesis (also played by the Drum Kit)
    ChordDetect.*          chord detection from recordings and MIDI clips (chroma, key, tempo, best path)
    Stems.*                stem separation: voice, drums, bass and other (harmonic/percussive + stereo)
    Help.*                 the help assistant: questions in everyday words, English and French
    RenderTest.*           developer check: renders each instrument to WAV
  tools\make_sound_packs.py builds the sound packs (run by the GitHub workflow)
    Updater.*              automatic updates from GitHub Releases
    Dialogs.h              confirmation boxes
    StudioLookAndFeel.h    colours, fonts and the professional theme
    LevelMeter.h           input and output meters
    StudioKeys.h           simple built-in sound (used until a pack is downloaded)
  libs\                    downloaded engine (created by setup.bat)
  build\                   compiled output (created by build.bat)
```

---

## What comes next

**Step 35: Key changes.** A key change at any bar (for the classic gospel lift up a semitone or a tone in the last chorus): the chord palette, the band, the harmony singers and the chord names all follow the new key from that bar, with a one-click "Lift the last chorus" button.

(Stage 6, your test week: Step 28 hardware checks and Step 29 listening review, is waiting for when you have your audio interface and keyboard ready.)
