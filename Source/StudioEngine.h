#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "MemoRecorder.h"
#include "Song.h"

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
    void autosaveIfChanged();                                // called every few seconds

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
    static te::Plugin* getInstrumentPlugin (te::AudioTrack&);
    static juce::String getInstrumentId (te::AudioTrack&);
    static juce::String describeInstrument (te::AudioTrack&);       // "Grand Piano", "Grand Piano (not downloaded)"
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
    te::InputDevice::MonitorMode getMonitorMode (te::AudioTrack&) const;
    void setMonitorMode (te::AudioTrack&, te::InputDevice::MonitorMode);
    bool isPunchEnabled() const;
    void setPunchEnabled (bool);
    double getRecordStartSeconds() const                     { return recordStartSeconds; }
    te::Clip* importAudio (te::AudioTrack&, const juce::File&, double startSeconds);
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

    te::Engine engine { "GospelProductionStudio" };
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioEngine)
};
