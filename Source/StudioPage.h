#pragma once

#include "StudioEngine.h"
#include "SongLibrary.h"
#include "TransportButton.h"

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

    juce::TextButton clearTakesButton { "Clear Takes" };

    // State
    juce::String songId;
    std::vector<double> tapTimes;
    StudioEngine::Position lastPosition;
    bool lastPlaying = false, lastRecording = false;
    int lastTakes = -1;

    // Layout areas used in paint()
    juce::Rectangle<int> headerArea, transportCard, metronomeCard, beatArea, positionArea, takesArea;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> labels;
    juce::Rectangle<int> tempoLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioPage)
};
