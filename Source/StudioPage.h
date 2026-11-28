#pragma once

#include "StudioEngine.h"
#include "SongLibrary.h"
#include "SongSafety.h"
#include "TransportButton.h"
#include "Timeline.h"
#include "ChordPanel.h"
#include "PianoRoll.h"
#include "DrumGrid.h"
#include "CompEditor.h"
#include "MixerView.h"

//==============================================================================
//  Studio page (Stage 1, Step 4): the open song's transport, tempo,
//  time signature, key, metronome, count-in and loop.
//==============================================================================
class StudioPage final : public juce::Component,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    StudioPage (StudioEngine&, SongLibrary&);
    ~StudioPage() override;

    void openSong (const juce::String& songId);

    // Steps 43-44
    void goBackToVersion (const SongSafety::Version&);
    int rescueRecordings (const SongSafety::Session::Previous&);
    void mouseDown (const juce::MouseEvent&) override;
    void songWasDeleted (const juce::String& songId);
    bool handleShortcut (const juce::KeyPress&);
    bool keyPressed (const juce::KeyPress& key) override   { return handleShortcut (key); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void refreshFromSong();
    void refreshMetronomeControls();
    void updateSong (std::function<void (Song&)> change);
    void tapTempo();
    void applyLoop();
    const Song* getOpenSong() const;

    StudioEngine& engine;
    SongLibrary&  library;

    // Transport
    TransportButton startButton  { "Return to start (Home)", TransportButton::Icon::toStart, StudioColours::gold };
    TransportButton playButton   { "Play / Pause (Space)",   TransportButton::Icon::play,    StudioColours::good };
    TransportButton stopButton   { "Stop (press twice to return to start)", TransportButton::Icon::stop, StudioColours::gold };
    TransportButton recordButton { "Record (R)",             TransportButton::Icon::record,  StudioColours::bad };
    TransportButton loopButton   { "Loop (L)",               TransportButton::Icon::loop,    StudioColours::gold };
    juce::TextButton punchButton { "Punch" };
    juce::TextButton writeButton { "Write" };         // record automation (Step 13)

    // Song musical settings
    juce::Slider     tempoSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::TextButton tapButton   { "Tap" };
    juce::ComboBox   timeSigBox, keyBox;

    // Metronome
    TransportButton  clickButton { "Metronome (C)", TransportButton::Icon::metronome, StudioColours::gold };
    juce::ComboBox   clickSoundBox, countInBox;
    juce::Slider     clickVolume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ToggleButton accentToggle { "Accent beat 1" };

    // Loop range
    juce::Slider loopStart  { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Slider loopLength { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };

    TimelineView timeline { engine };
    std::unique_ptr<ChordPanel> chordPanel;
    std::unique_ptr<PianoRoll> pianoRoll;
    std::unique_ptr<DrumGrid> drumGrid;
    std::unique_ptr<MixerView> mixer;
    juce::TextButton mixerButton { "Mixer" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton versionsButton { "Versions" };           // Step 44

public:
    // Shows a page inside the main window (set by the main window), Step 14
    std::function<void (const juce::String& title, std::unique_ptr<juce::Component>)> onShowPage;
private:
    void setMixerOpen (bool);
    bool isMixerOpen() const       { return mixer != nullptr && mixer->isVisible(); }
    std::unique_ptr<CompEditor> compEditor;
    void openCompEditor (te::WaveAudioClip*);
    void closeCompEditor();
    bool compact = false;
    void openPianoRoll (te::MidiClip*);
    void closePianoRoll();
    void openDrumGrid (te::MidiClip*);
    void closeDrumGrid (bool now = false);
    bool isDrumClip (te::MidiClip&) const;
    bool chordPanelOpen = true;

    // State
    juce::String songId;
    juce::String tempoStatus;                       // "Stretching 3 recordings to 100 BPM..."
    void askTempoMode();
    void showTempoMenu();
    std::vector<double> tapTimes;
    StudioEngine::Position lastPosition;
    bool lastPlaying = false, lastRecording = false;
    int lastTakes = -1;
    int autosaveTicks = 0;

    // Layout areas used in paint()
    juce::Rectangle<int> headerArea, transportCard, metronomeCard, positionArea;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> labels;
    juce::Rectangle<int> tempoLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioPage)
};
