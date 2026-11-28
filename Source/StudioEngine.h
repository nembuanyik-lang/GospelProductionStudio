#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "MemoRecorder.h"
#include "Song.h"
#include "PluginHost.h"

namespace te = tracktion;

//==============================================================================
//  The professional audio engine (Tracktion Engine) and everything the
//  screens need from it:
//
//    - audio and MIDI devices
//    - the open song's project (an "Edit") with its Keys track
//    - transport: play, stop, record, loop, position
//    - tempo, time signature and key, kept in step with the song
//    - metronome and count-in
//    - test tone, voice memo and level meters
//==============================================================================
class StudioEngine final : private juce::ChangeListener,
                           private juce::MidiKeyboardState::Listener
{
public:
    StudioEngine();
    ~StudioEngine() override;

    te::Engine& getEngine()                                  { return engine; }
    juce::AudioDeviceManager& getAudioDeviceManager()        { return engine.getDeviceManager().deviceManager; }

    // Recording delay (Step 28): the driver's reported delay is corrected by the engine;
    // this extra correction (ms, + moves recordings earlier) is what the check measured.
    double getRecordingCorrectionMs();
    void setRecordingCorrectionMs (double ms);
    double getReportedLatencyMs();
    void prepareForLatencyCheck (bool starting);          // silence: stop, no live monitoring, no test tone
    MemoRecorder& getMemo()                                  { return memo; }

    //==========================================================================
    // On-screen keyboard: shows every key played and plays into the engine.
    juce::MidiKeyboardState& getKeyboardState()              { return displayKeyboard; }
    static constexpr int onScreenChannel = 1, mirrorChannel = 16;

    int   getLastNote() const                                { return lastNote.load(); }
    int   getLastVelocity() const                            { return lastVelocity.load(); }
    int   getMidiMessageCount() const                        { return midiCount.load(); }

    struct MidiInputInfo { juce::String name; bool enabled; };
    std::vector<MidiInputInfo> getPhysicalMidiInputs() const;

    //==========================================================================
    void  setTestTone (bool on)                              { toneOn.store (on); }
    bool  isTestToneOn() const                               { return toneOn.load(); }
    float getInputLevel();
    float getOutputLevel();
    float getCpuUsage() const;

    //==========================================================================
    // Song projects
    void openSong (const Song&, const juce::File& editFile);
    void closeSong();                                        // back to the scratch project
    juce::String getOpenSongId() const                       { return openSongId; }
    bool hasOpenSong() const                                 { return openSongId.isNotEmpty(); }
    void applySongSettings (const Song&);                    // tempo, time signature, key
    void save();

    // Called just before the current song is closed: screens must let go of its
    // clips, tracks and plugins here (they are deleted right after).
    std::function<void()> onEditAboutToChange;
    void autosaveIfChanged();

    // Versions and recovery (Steps 43-44)
    juce::File getSongFile() const;
    void discardSongWithoutSaving();               // before going back to a version
    void songClosing();                            // the app is closing: a version "when closed"                                // called every few seconds

    // Recordings and tempo changes (Step 23): asks once per song, then stretches in the background
    std::function<void()> onAskTempoMode;                    // first tempo change in a song with recordings
    std::function<void (const juce::String&)> onTempoStatus; // "Stretching 3 recordings to 100 BPM..." or empty
    void tempoModeChosen();                                  // after the answer: start stretching if asked to

    //==========================================================================
    // Transport
    void togglePlay();
    void stop();
    void toggleRecord();
    void returnToStart();
    bool isPlaying() const;
    bool isRecording() const;
    bool isInCountIn() const;

    void setLooping (bool);
    bool isLooping() const;
    void setLoopBars (int firstBar, int numBars);            // bars counted from 1
    std::pair<int, int> getLoopBars() const;

    struct Position { int bar = 1, beat = 1, beatsInBar = 4; double seconds = 0.0; bool countIn = false; };
    Position getPosition() const;

    int  getNumTakes() const;
    void clearTakes();
    void setPositionSeconds (double seconds);

    //==========================================================================
    // Tracks (Step 5)
    te::Edit* getEdit() const                               { return edit.get(); }
    static bool isInstrumentTrack (te::AudioTrack&);
    te::AudioTrack* addTrack (bool instrument, const juce::String& name, juce::Colour, te::Track* after,
                              const juce::String& instrumentId = "grand");

    // Built-in instruments (Step 8)
    void setInstrument (te::AudioTrack&, const juce::String& instrumentId);

    // Track names follow the instrument (Step 26): "Piano", "Organ 2"... A name you typed is never changed.
    static int upgradeBuiltInInstruments (te::Edit&);                       // Steps 30-31: built-in guitar/percussion -> sampler
    static juce::String trackNameFor (const juce::String& instrumentId);   // "Piano", "Bass", "Percussion"
    static bool hasAutomaticName (te::Track&);                              // "Organ", "Piano 2", "Instrument 6"
    static juce::String uniqueTrackName (te::Edit&, const juce::String& base, const te::Track* except = nullptr);
    static void nameAfterInstrument (te::AudioTrack&, const juce::String& newName);
    static te::Plugin* getInstrumentPlugin (te::AudioTrack&);
    static juce::String getInstrumentId (te::AudioTrack&);
    static juce::String describeInstrument (te::AudioTrack&);       // "Grand Piano", "Grand Piano (not downloaded)"

    // Plugins (Step 11): a VST3 instrument in place of ours, and effects after it
    te::Plugin* setPluginInstrument (te::AudioTrack&, const juce::PluginDescription&);
    te::Plugin* addEffect (te::AudioTrack&, const juce::PluginDescription&);
    te::Plugin* addEffect (te::AudioTrack&, te::Plugin::Ptr);              // built-in effects (Step 12)
    static std::vector<te::Plugin*> getEffects (te::AudioTrack&);
    static void removeEffect (te::Plugin&);
    static void moveEffect (te::Plugin&, int delta);       // -1 earlier, +1 later in the chain
    void deleteTrack (te::AudioTrack&);
    void setArmedTrack (te::AudioTrack*);
    te::AudioTrack* getArmedTrack() const;
    void ensureSections (const Song&);                      // builds the Sections lane from the lyrics

    //==========================================================================
    // Metronome
    void  setClickEnabled (bool);
    bool  isClickEnabled() const;
    void  setClickVolume (float);                           // 0..1
    float getClickVolume() const;
    void  setClickAccent (bool);
    bool  isClickAccent() const;
    void  setClickSound (int index);
    int   getClickSound() const                             { return clickSound; }

    // Chord audition: plays notes on the armed instrument and lights the keyboard.
    void audition (const std::vector<int>& notes, bool arpeggio, int durationMs = 1100);

    // Small app-wide settings (chord display, voicing...)
    static int  getSetting (const juce::String& key, int defaultValue);
    static void setSetting (const juce::String& key, int value);
    static juce::String getTextSetting (const juce::String& key, const juce::String& defaultValue);
    static void setTextSetting (const juce::String& key, const juce::String& value);

    static juce::StringArray countInNames()                 { return { "No count-in", "1 bar", "2 bars", "1 beat", "2 beats" }; }
    void  setCountIn (int index);                           // index into countInNames()
    int   getCountIn() const;

    // ---- Audio recording (Step 9) --------------------------------------------
    std::vector<te::WaveInputDevice*> getAudioInputs() const;          // inputs of the audio interface
    te::WaveInputDevice* getTrackInput (te::AudioTrack&) const;
    void setTrackInput (te::AudioTrack&, te::WaveInputDevice*);        // nullptr = no input
    bool isAudioArmed (te::AudioTrack&) const;
    void setAudioArmed (te::AudioTrack&, bool);
    float getTrackInputLevel (te::AudioTrack&);                         // 0..1, for the header meter
    bool areInputsMono() const;
    void setInputsMono (bool);
    std::vector<std::pair<juce::String, te::InputDevice::MonitorMode>> monitorsBeforeCheck;
    juce::uint32 lastVersionMs = 0;
    bool discardNextLoad = false;
    te::InputDevice::MonitorMode getMonitorMode (te::AudioTrack&) const;
    void setMonitorMode (te::AudioTrack&, te::InputDevice::MonitorMode);
    bool isPunchEnabled() const;
    void setPunchEnabled (bool);
    double getRecordStartSeconds() const                     { return recordStartSeconds; }
    te::Clip* importAudio (te::AudioTrack&, const juce::File&, double startSeconds);
    juce::File copyIntoSong (const juce::File&, const juce::String& subfolder);   // keeps the song's audio with the song
    static juce::String describeInput (te::WaveInputDevice*);

private:
    void prepareAudioInputs();
    te::InputDeviceInstance* findInstance (te::InputDevice&) const;
    std::map<te::WaveInputDevice*, std::unique_ptr<te::LevelMeasurer::Client>> inputMeters;
    double recordStartSeconds = 0.0;
    void loadEdit (const juce::File& file);
    void prepareKeysTrack();
    void createStarterLayout();
    void addInstrument (te::AudioTrack&, const juce::String& instrumentId = "grand");
    void assignInputs();
    void attachMirrors();
    te::AudioTrack* getKeysTrack() const;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity) override;

    struct AuxCallback;
    struct Mirror;

    te::Engine engine { "GospelProductionStudio", PluginHost::createUIBehaviour(), nullptr };
    std::unique_ptr<te::Edit> edit;
    juce::String openSongId;
    juce::File scratchFile;

    MemoRecorder memo;
    std::unique_ptr<AuxCallback> aux;

    juce::MidiKeyboardState displayKeyboard;
    std::vector<std::unique_ptr<Mirror>> mirrors;
    std::shared_ptr<te::MidiInputDevice> onScreenDevice;

    std::atomic<bool> toneOn { false };
    std::atomic<int>  lastNote { -1 }, lastVelocity { 0 }, midiCount { 0 };

    juce::AudioDeviceManager::LevelMeter::Ptr inputLevel, outputLevel;
    int clickSound = 0;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // Stretching recordings to a new tempo, in the background (Step 23)
    struct TempoStretcher;
    std::unique_ptr<TempoStretcher> tempoStretcher;
    void changeTempo (double bpm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioEngine)
};
