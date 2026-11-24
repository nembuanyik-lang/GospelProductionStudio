# Gospel Production Studio

**Stage 0: Foundation and Setup** (Steps 1 and 2 of 22)

This stage sets up the GitHub build and produces the first version of the studio: the **Studio Shell**. It opens with the professional interface, connects to your audio device, plays a test tone, and detects your keyboard (YPT-240), which you can then play.

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

## Stage 0 test (must pass before Stage 1)

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
    MainComponent.*        Studio Shell screen, audio and MIDI
    StudioLookAndFeel.h    colours, fonts and the professional theme
    LevelMeter.h           input and output meters
    StudioKeys.h           Stage 0 test instrument (replaced in Step 8)
  libs\                    downloaded engine (created by setup.bat)
  build\                   compiled output (created by build.bat)
```

---

## What comes next

**Stage 1, Step 3: Song library and lyrics editor.** Every received song gets its title, date, key, tempo, theme and lyrics, and each song can be opened as a project.
