#include "StudioPage.h"
#include "ClickSounds.h"
#include "Dialogs.h"

//==============================================================================
StudioPage::StudioPage (StudioEngine& e, SongLibrary& lib)
    : engine (e), library (lib)
{
    // ---- Transport ----------------------------------------------------------
    startButton.onClick  = [this] { engine.returnToStart(); };
    playButton.onClick   = [this] { engine.togglePlay(); };
    stopButton.onClick   = [this] { engine.stop(); };
    recordButton.onClick = [this] { engine.toggleRecord(); };
    loopButton.onClick   = [this] { engine.setLooping (! engine.isLooping()); applyLoop(); };

    for (auto* b : { &startButton, &playButton, &stopButton, &recordButton, &loopButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    // ---- Tempo, time signature, key -----------------------------------------
    tempoSlider.setRange (30.0, 240.0, 1.0);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 96, 34);
    tempoSlider.onValueChange = [this]
    {
        const auto bpm = (int) tempoSlider.getValue();
        updateSong ([bpm] (Song& s) { s.tempo = bpm; });
    };
    addAndMakeVisible (tempoSlider);

    tapButton.setTooltip ("Tap four or more times in time with the song");
    tapButton.onClick = [this] { tapTempo(); };
    addAndMakeVisible (tapButton);

    timeSigBox.addItemList (Song::timeSignatures(), 1);
    timeSigBox.onChange = [this]
    {
        const auto sig = timeSigBox.getText();
        updateSong ([sig] (Song& s) { s.timeSignature = sig; });
    };
    addAndMakeVisible (timeSigBox);

    keyBox.addItemList (Song::keys(), 1);
    keyBox.onChange = [this]
    {
        const auto key = keyBox.getText();
        updateSong ([key] (Song& s) { s.key = key; });
    };
    addAndMakeVisible (keyBox);

    // ---- Metronome ----------------------------------------------------------
    clickButton.onClick = [this] { engine.setClickEnabled (! engine.isClickEnabled()); };
    addAndMakeVisible (clickButton);

    clickSoundBox.addItemList (ClickSounds::getNames(), 1);
    clickSoundBox.onChange = [this] { engine.setClickSound (clickSoundBox.getSelectedItemIndex()); };
    addAndMakeVisible (clickSoundBox);

    clickVolume.setRange (0.2, 1.0, 0.01);
    clickVolume.onValueChange = [this] { engine.setClickVolume ((float) clickVolume.getValue()); };
    addAndMakeVisible (clickVolume);

    accentToggle.onClick = [this] { engine.setClickAccent (accentToggle.getToggleState()); };
    addAndMakeVisible (accentToggle);

    countInBox.addItemList (StudioEngine::countInNames(), 1);
    countInBox.onChange = [this] { engine.setCountIn (countInBox.getSelectedItemIndex()); };
    addAndMakeVisible (countInBox);

    // ---- Loop range ---------------------------------------------------------
    loopStart.setRange (1.0, 999.0, 1.0);
    loopStart.setValue (1.0, juce::dontSendNotification);
    loopStart.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 30);
    loopStart.onValueChange = [this] { applyLoop(); };
    addAndMakeVisible (loopStart);

    loopLength.setRange (1.0, 64.0, 1.0);
    loopLength.setValue (4.0, juce::dontSendNotification);
    loopLength.setTextValueSuffix (" bars");
    loopLength.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 80, 30);
    loopLength.onValueChange = [this] { applyLoop(); };
    addAndMakeVisible (loopLength);

    // ---- Takes --------------------------------------------------------------
    clearTakesButton.onClick = [this]
    {
        Dialogs::confirm (this, "Clear takes",
                          "Remove everything recorded on the Keys track for this song?",
                          "Clear", "Keep", [this] { engine.clearTakes(); });
    };
    addAndMakeVisible (clearTakesButton);

    for (juce::Button* b : { (juce::Button*) &tapButton, (juce::Button*) &clickButton,
                             (juce::Button*) &accentToggle, (juce::Button*) &clearTakesButton })
        b->setWantsKeyboardFocus (false);

    setWantsKeyboardFocus (true);
    library.addChangeListener (this);
    refreshFromSong();
    startTimerHz (30);
}

StudioPage::~StudioPage()
{
    library.removeChangeListener (this);
}

//==============================================================================
const Song* StudioPage::getOpenSong() const
{
    return songId.isNotEmpty() ? library.find (songId) : nullptr;
}

void StudioPage::openSong (const juce::String& id)
{
    const auto* song = library.find (id);
    if (song == nullptr)
        return;

    songId = id;
    engine.openSong (*song, library.getSongFolder (id).getChildFile ("project.tracktionedit"));
    applyLoop();
    refreshFromSong();
}

void StudioPage::songWasDeleted (const juce::String& id)
{
    if (id == songId)
    {
        engine.closeSong();
        songId = {};
        refreshFromSong();
    }
}

void StudioPage::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The song was edited (here or in the Library): keep the engine in step.
    if (songId.isNotEmpty() && library.find (songId) == nullptr)
    {
        songWasDeleted (songId);
        return;
    }

    if (const auto* song = getOpenSong())
        engine.applySongSettings (*song);

    refreshFromSong();
}

void StudioPage::updateSong (std::function<void (Song&)> change)
{
    if (const auto* existing = getOpenSong())
    {
        auto song = *existing;
        change (song);
        song.modified = juce::Time::getCurrentTime();
        library.update (song);          // engine follows via changeListenerCallback
    }
}

void StudioPage::refreshFromSong()
{
    const auto* song = getOpenSong();
    const auto enabled = song != nullptr;

    for (juce::Component* c : { (juce::Component*) &tempoSlider, (juce::Component*) &tapButton,
                                (juce::Component*) &timeSigBox, (juce::Component*) &keyBox,
                                (juce::Component*) &recordButton, (juce::Component*) &clearTakesButton,
                                (juce::Component*) &loopStart, (juce::Component*) &loopLength })
        c->setEnabled (enabled);

    if (song != nullptr)
    {
        tempoSlider.setValue (song->tempo, juce::dontSendNotification);
        timeSigBox.setText (song->timeSignature, juce::dontSendNotification);
        keyBox.setText (song->key, juce::dontSendNotification);
    }

    refreshMetronomeControls();
    repaint();
}

void StudioPage::refreshMetronomeControls()
{
    clickButton.setLit (engine.isClickEnabled());
    clickSoundBox.setSelectedItemIndex (engine.getClickSound(), juce::dontSendNotification);
    clickVolume.setValue (engine.getClickVolume(), juce::dontSendNotification);
    accentToggle.setToggleState (engine.isClickAccent(), juce::dontSendNotification);
    countInBox.setSelectedItemIndex (engine.getCountIn(), juce::dontSendNotification);
    loopButton.setLit (engine.isLooping());
}

void StudioPage::applyLoop()
{
    engine.setLoopBars ((int) loopStart.getValue(), (int) loopLength.getValue());
}

void StudioPage::tapTempo()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (! tapTimes.empty() && now - tapTimes.back() > 2000.0)
        tapTimes.clear();

    tapTimes.push_back (now);
    if (tapTimes.size() > 6)
        tapTimes.erase (tapTimes.begin());

    if (tapTimes.size() >= 3)
    {
        const auto average = (tapTimes.back() - tapTimes.front()) / (double) (tapTimes.size() - 1);
        if (average > 0.0)
            tempoSlider.setValue (juce::roundToInt (60000.0 / average), juce::sendNotificationAsync);
    }
}

bool StudioPage::handleShortcut (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)                     { engine.togglePlay(); return true; }
    if (key == juce::KeyPress::homeKey)                      { engine.returnToStart(); return true; }
    if (songId.isNotEmpty() && key.getTextCharacter() == 'r'){ engine.toggleRecord(); return true; }
    if (key.getTextCharacter() == 'l')                       { engine.setLooping (! engine.isLooping()); applyLoop(); return true; }
    if (key.getTextCharacter() == 'c')                       { engine.setClickEnabled (! engine.isClickEnabled()); return true; }
    return false;
}

//==============================================================================
void StudioPage::timerCallback()
{
    const auto pos = engine.getPosition();
    const auto playing = engine.isPlaying();
    const auto recording = engine.isRecording();
    const auto takes = engine.getNumTakes();

    playButton.setLit (playing && ! recording);
    recordButton.setLit (recording);
    loopButton.setLit (engine.isLooping());
    clickButton.setLit (engine.isClickEnabled());

    if (pos.bar != lastPosition.bar || pos.beat != lastPosition.beat || pos.countIn != lastPosition.countIn
        || pos.beatsInBar != lastPosition.beatsInBar
        || (int) (pos.seconds * 10) != (int) (lastPosition.seconds * 10)
        || playing != lastPlaying || recording != lastRecording || takes != lastTakes)
    {
        lastPosition = pos;
        lastPlaying = playing;
        lastRecording = recording;
        lastTakes = takes;
        repaint (positionArea);
        repaint (beatArea);
        repaint (takesArea);
    }
}

//==============================================================================
void StudioPage::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    const auto* song = getOpenSong();

    // ---- Song header --------------------------------------------------------
    auto headerBox = headerArea;
    g.setColour (text);
    g.setFont (studioFont (24.0f, true));
    g.drawText (song != nullptr ? song->title : juce::String ("No song open"),
                headerBox.removeFromTop (34), juce::Justification::bottomLeft, true);

    g.setColour (song != nullptr ? gold : textDim);
    g.setFont (studioFont (13.5f));
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    g.drawText (song != nullptr
                    ? song->key + dot + song->timeSignature + dot + juce::String (song->tempo)
                          + (song->isCompoundMeter() ? " BPM (dotted quarter)" : " BPM") + dot + song->theme
                    : juce::String ("In the Song Library, choose a song and click Open in Studio. "
                                    "The keyboard still plays meanwhile."),
                headerBox, juce::Justification::topLeft, true);

    // ---- Cards --------------------------------------------------------------
    for (auto cardArea : { transportCard, metronomeCard })
    {
        g.setColour (card);
        g.fillRoundedRectangle (cardArea.toFloat(), 10.0f);
        g.setColour (cardBorder);
        g.drawRoundedRectangle (cardArea.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    g.setFont (studioFont (12.0f));
    g.setColour (textDim);
    for (const auto& [area, labelText] : labels)
        g.drawText (labelText, area, juce::Justification::bottomLeft);

    if (song != nullptr && song->isCompoundMeter())
    {
        g.setColour (gold);
        g.drawText ("Tempo (dotted quarter)", tempoLabelArea, juce::Justification::bottomLeft);
    }

    // ---- Position display ---------------------------------------------------
    {
        auto area = positionArea;
        g.setColour (header);
        g.fillRoundedRectangle (area.toFloat(), 8.0f);

        auto inner = area.reduced (14, 6);
        const auto& p = lastPosition;
        const auto countIn = p.countIn && lastRecording;

        g.setFont (studioFont (30.0f, true));
        g.setColour (countIn ? bad : (lastPlaying ? gold : text));
        g.drawText (countIn ? juce::String ("COUNT-IN")
                            : juce::String (juce::jmax (1, p.bar)) + "." + juce::String (juce::jmax (1, p.beat)),
                    inner.removeFromTop (38), juce::Justification::centredLeft);

        const auto secs = juce::jmax (0.0, p.seconds);
        const auto mins = (int) (secs / 60.0);
        g.setFont (studioFont (13.0f));
        g.setColour (textDim);
        g.drawText ("Bar.Beat     " + juce::String (mins) + ":" + juce::String (secs - mins * 60.0, 1).paddedLeft ('0', 4),
                    inner, juce::Justification::centredLeft);
    }

    // ---- Beat indicator -----------------------------------------------------
    {
        auto area = beatArea;
        const auto& p = lastPosition;
        const auto beats = juce::jlimit (1, 16, p.beatsInBar);
        const auto activeBeat = (lastPlaying || lastRecording) ? p.beat : 0;

        g.setFont (studioFont (15.0f, true));
        g.setColour (lastRecording ? bad : (lastPlaying ? good : textDim));
        g.drawText (lastRecording ? (p.countIn ? "COUNT-IN" : "RECORDING")
                                  : (lastPlaying ? "PLAYING" : "STOPPED"),
                    area.removeFromTop (30), juce::Justification::centred);

        const auto size = juce::jmin (54, (area.getWidth() - 40) / beats - 12);
        const auto totalW = beats * (size + 12) - 12;
        auto row = juce::Rectangle<int> (totalW, size).withCentre ({ area.getCentreX(), area.getY() + size / 2 + 16 });

        for (int i = 1; i <= beats; ++i)
        {
            auto dotArea = row.removeFromLeft (size).toFloat();
            row.removeFromLeft (12);

            // In 6/8, 9/8 and 12/8 the main pulses (beats 1, 4, 7, 10) are drawn larger.
            const auto compound = song != nullptr && song->isCompoundMeter();
            const auto isFirst = i == 1 || (compound && (i - 1) % 3 == 0);
            const auto isOn = i == activeBeat;
            const auto colour = i == 1 ? gold : text;

            g.setColour (isOn ? colour : card.brighter (0.05f));
            g.fillEllipse (dotArea.reduced (isFirst ? 0.0f : 5.0f));
            g.setColour (isOn ? colour : cardBorder.brighter (0.2f));
            g.drawEllipse (dotArea.reduced (isFirst ? 0.5f : 5.5f), 1.5f);

            g.setColour (isOn ? header : textDim);
            g.setFont (studioFont (13.0f, true));
            g.drawText (juce::String (i), dotArea.toNearestInt(), juce::Justification::centred);
        }
    }

    // ---- Takes ---------------------------------------------------------------
    {
        g.setFont (studioFont (13.5f));
        g.setColour (textDim);
        const auto takes = lastTakes < 0 ? 0 : lastTakes;
        const auto message = song == nullptr
            ? juce::String ("Open a song to record takes.")
            : (takes == 0 ? juce::String ("Keys track: no takes yet. Press Record (R) and play your keyboard.")
                          : "Keys track: " + juce::String (takes) + (takes == 1 ? " take" : " takes")
                                + " recorded. Press Play to hear them. The timeline to see and edit takes arrives in Step 5.");
        g.drawFittedText (message, takesArea, juce::Justification::centredLeft, 2);
    }
}

void StudioPage::resized()
{
    labels.clear();
    auto area = getLocalBounds().reduced (24, 14);

    headerArea = area.removeFromTop (56);
    area.removeFromTop (10);

    // ---- Transport card -----------------------------------------------------
    transportCard = area.removeFromTop (100);
    auto t = transportCard.reduced (16, 12);

    auto buttons = t.removeFromLeft (5 * 58 + 4 * 8);
    buttons = buttons.withSizeKeepingCentre (buttons.getWidth(), 58);
    for (auto* b : { &startButton, &playButton, &stopButton, &recordButton, &loopButton })
    {
        b->setBounds (buttons.removeFromLeft (58));
        buttons.removeFromLeft (8);
    }

    t.removeFromLeft (18);
    positionArea = t.removeFromLeft (230);
    t.removeFromLeft (18);

    auto field = [this] (juce::Rectangle<int>& row, int width, const juce::String& label)
    {
        auto col = row.removeFromLeft (width);
        row.removeFromLeft (12);
        auto labelArea = col.removeFromTop (22);
        labels.push_back ({ labelArea, label });
        return col.withSizeKeepingCentre (col.getWidth(), 36);
    };

    {
        auto col = t.removeFromLeft (250);
        t.removeFromLeft (12);
        tempoLabelArea = col.removeFromTop (22);
        labels.push_back ({ tempoLabelArea, "Tempo" });
        auto r = col.withSizeKeepingCentre (col.getWidth(), 36);
        tapButton.setBounds (r.removeFromRight (60));
        r.removeFromRight (8);
        tempoSlider.setBounds (r);
    }
    timeSigBox.setBounds (field (t, 100, "Time signature"));
    keyBox.setBounds (field (t, juce::jmin (150, juce::jmax (110, t.getWidth())), "Key"));

    area.removeFromTop (12);

    // ---- Metronome card -----------------------------------------------------
    metronomeCard = area.removeFromTop (88);
    auto m = metronomeCard.reduced (16, 10);

    {
        auto col = m.removeFromLeft (58);
        m.removeFromLeft (14);
        labels.push_back ({ col.removeFromTop (22), "Click" });
        clickButton.setBounds (col.withSizeKeepingCentre (58, 40));
    }
    clickSoundBox.setBounds (field (m, 130, "Click sound"));
    clickVolume.setBounds (field (m, 150, "Click volume"));
    accentToggle.setBounds (field (m, 140, " "));
    countInBox.setBounds (field (m, 130, "Count-in (recording)"));
    loopStart.setBounds (field (m, 130, "Loop from bar"));
    loopLength.setBounds (field (m, 150, "Loop length"));

    area.removeFromTop (12);

    // ---- Beat indicator and takes -------------------------------------------
    takesArea = area.removeFromBottom (44);
    clearTakesButton.setBounds (takesArea.removeFromRight (130).withSizeKeepingCentre (130, 34));
    takesArea.removeFromRight (12);
    beatArea = area;
}
