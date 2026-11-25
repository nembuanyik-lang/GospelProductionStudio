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
    Sfz.*                  SFZ instruments: reading, sample loading, shared cache
    SamplerPlugin.*        Gospel Sampler: piano, bass and drum packs
    OrganPlugin.*          Gospel Organ: drawbars, percussion, vibrato, Leslie
    Instruments.*          the instrument list and where sound packs live
    SoundLibrary.*         Sound Library window: download packs once
    InstrumentPanel.*      instrument settings window (drawbars etc.)
    Grooves.*              starter drum grooves
    Takes.*                choosing takes on recorded clips
    AudioEdit.*            split, fades, normalise, pitch and speed, comp plans and comp rendering
    CompEditor.*           the comping window: pick the best parts of each take
    PluginHost.*           VST3 plugins: safe scanning, the Plugins window, plugin windows
    Mixing.*               shared reverb and delay, sends, built-in effects and their starting points
    MixerView.*            the mixer: channel strips, meters, master
    Automation.*           which settings can change during the song, recording by hand
    AutomationLane.*       the line under a track: draw, drag and remove points
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

**Stage 2, Step 14: Bouncing and exporting.** Save the finished song as a WAV or MP3 for WhatsApp, radio or streaming, export each track separately (stems) for another studio, and freeze heavy tracks to save computer power.
