# Gospel Production Studio

**Current build: Stage 1, Step 5 (Tracks and timeline) with automatic updates**, version 0.4.0. Runs on Tracktion Engine.

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
    Sfz.*                  SFZ instruments: reading, sample loading, shared cache
    SamplerPlugin.*        Gospel Sampler: piano, bass and drum packs
    OrganPlugin.*          Gospel Organ: drawbars, percussion, vibrato, Leslie
    Instruments.*          the instrument list and where sound packs live
    SoundLibrary.*         Sound Library window: download packs once
    InstrumentPanel.*      instrument settings window (drawbars etc.)
    Grooves.*              starter drum grooves
    Takes.*                choosing takes on recorded clips
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

**Stage 2, Step 10: Comping and audio editing.** Build the best performance from several takes (comping), split, trim, fade and crossfade audio, clip gain, time-stretch and pitch-shift.
