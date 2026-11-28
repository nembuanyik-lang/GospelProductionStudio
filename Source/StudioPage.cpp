#include "TempoFollow.h"
#include "InstrumentPanel.h"
#include "StudioPage.h"
#include "Automation.h"
#include "ExportPage.h"
#include "MasteringPage.h"
#include "AnalyserPage.h"
#include "ReleasePage.h"
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
    loopButton.onClick   = [this] { engine.setLooping (! engine.isLooping()); };

    for (auto* b : { &startButton, &playButton, &stopButton, &recordButton, &loopButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    // ---- Tempo, time signature, key -----------------------------------------
    tempoSlider.setComponentID ("studio.tempo");
    keyBox.setComponentID ("studio.key");
    timeSigBox.setComponentID ("studio.timesig");
    countInBox.setComponentID ("studio.countin");
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
    loopStart.setComponentID ("studio.loopstart");
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

    // ---- Timeline -----------------------------------------------------------
    addAndMakeVisible (timeline);

    // ---- Chord Track and palette (Step 6) -----------------------------------
    timeline.getKey = [this]
    {
        if (const auto* song = getOpenSong())
            return Theory::keyFromSong (song->getKeyPitchClass(), song->isMinorKey(), song->key);
        return Theory::Key();
    };
    chordPanel = std::make_unique<ChordPanel> (engine, timeline);
    addAndMakeVisible (*chordPanel);
    chordPanelOpen = StudioEngine::getSetting ("chordPanelOpen", 1) != 0;
    chordPanel->setVisible (chordPanelOpen);

    timeline.onTogglePalette = [this]
    {
        chordPanelOpen = ! chordPanelOpen;
        StudioEngine::setSetting ("chordPanelOpen", chordPanelOpen ? 1 : 0);
        chordPanel->setVisible (chordPanelOpen);
        resized();
    };
    timeline.onChordSelected = [this] (const Theory::Chord* c) { chordPanel->showSuggestionsFor (c); };
    timeline.onSaveSelection = [this] { chordPanel->saveSelectionAsProgression(); };

    // ---- Piano roll (Step 7) -------------------------------------------------
    pianoRoll = std::make_unique<PianoRoll> (engine, timeline);
    addChildComponent (*pianoRoll);
    pianoRoll->onClose = [this] { closePianoRoll(); };
    pianoRoll->onSplitChanged = [this] { resized(); repaint(); };
    pianoRoll->onDrumGrid = [this] (te::MidiClip* c)
    {
        te::MidiClip::Ptr clip = c;
        juce::Component::SafePointer<StudioPage> safe (this);
        juce::MessageManager::callAsync ([safe, clip]
        {
            if (safe != nullptr && clip != nullptr)
            {
                clip->state.removeProperty ("gpsPianoRoll", nullptr);
                safe->openDrumGrid (clip.get());
            }
        });
    };

    // Drum grid (Step 17)
    drumGrid = std::make_unique<DrumGrid> (engine, timeline);
    addChildComponent (*drumGrid);
    drumGrid->onClose = [this] { closeDrumGrid(); };
    drumGrid->onPianoRoll = [this] (te::MidiClip* c)
    {
        te::MidiClip::Ptr clip = c;
        juce::Component::SafePointer<StudioPage> safe (this);
        juce::MessageManager::callAsync ([safe, clip]
        {
            if (safe != nullptr && clip != nullptr)
                safe->openPianoRoll (clip.get());
        });
    };

    // ---- Comp editor (Step 10) ------------------------------------------------
    compEditor = std::make_unique<CompEditor> (engine, timeline);
    addChildComponent (*compEditor);
    compEditor->onClose = [this] { closeCompEditor(); };
    timeline.onOpenComp = [this] (te::WaveAudioClip* clip) { openCompEditor (clip); };

    // ---- Mixer (Step 12) -------------------------------------------------------
    mixer = std::make_unique<MixerView> (engine);
    addChildComponent (*mixer);
    mixerButton.setClickingTogglesState (false);
    mixerButton.setWantsKeyboardFocus (false);
    mixerButton.setTooltip ("Mixer: volume, pan, effects and reverb for every track (X)");
    mixerButton.onClick = [this] { setMixerOpen (! isMixerOpen()); };
    addAndMakeVisible (mixerButton);

    exportButton.setWantsKeyboardFocus (false);
    exportButton.setTooltip ("Save the song as MP3 or WAV, or each track on its own (stems)");
    exportButton.onClick = [this]
    {
        auto* ed = engine.getEdit();
        const auto* song = getOpenSong();
        if (ed == nullptr || song == nullptr || onShowPage == nullptr)
            return;
        engine.save();
        onShowPage ("Export  " + juce::String::fromUTF8 ("\xc2\xb7") + "  " + song->title,
                    std::make_unique<ExportPage> (*ed, song->title));
    };
    addAndMakeVisible (exportButton);

    versionsButton.setTooltip ("Copies of this song kept automatically, and your own: go back to any of them");
    versionsButton.onClick = [this]
    {
        const auto* song = getOpenSong();
        const auto file = engine.getSongFile();
        if (song == nullptr || file == juce::File() || ! onShowPage)
            return;
        engine.save();
        juce::Component::SafePointer<StudioPage> safe (this);
        onShowPage ("Versions  " + juce::String::fromUTF8 ("\xc2\xb7") + "  " + song->title,
                    SongSafety::createVersionsPage (file,
                                                    [safe] { if (safe != nullptr) safe->engine.save(); },
                                                    [safe] (const SongSafety::Version& v) { if (safe != nullptr) safe->goBackToVersion (v); }));
    };
    addChildComponent (versionsButton);

    ReleasePage::onOpenRequest = [this]
    {
        auto* ed = engine.getEdit();
        const auto* song = getOpenSong();
        if (ed == nullptr || song == nullptr || onShowPage == nullptr)
            return;
        engine.save();
        juce::Component::SafePointer<StudioPage> safe (this);
        onShowPage ("Release package  " + juce::String::fromUTF8 ("\xc2\xb7") + "  " + song->title,
                    std::make_unique<ReleasePage> (*ed, *song, [safe] (const juce::var& details)
                    {
                        if (safe != nullptr)
                            safe->updateSong ([details] (Song& s) { s.release = details; });
                    }));
    };
    AnalyserPlugin::onOpenRequest = [this]
    {
        auto* ed = engine.getEdit();
        const auto* song = getOpenSong();
        if (ed == nullptr || song == nullptr || onShowPage == nullptr)
            return;
        onShowPage ("Analyser  " + juce::String::fromUTF8 ("\xc2\xb7") + "  " + song->title, std::make_unique<AnalyserPage> (*ed));
    };
    MasterPlugin::onOpenRequest = [this]
    {
        auto* ed = engine.getEdit();
        const auto* song = getOpenSong();
        if (ed == nullptr || song == nullptr || onShowPage == nullptr)
            return;
        onShowPage ("Mastering  " + juce::String::fromUTF8 ("\xc2\xb7") + "  " + song->title, std::make_unique<MasteringPage> (*ed));
    };

    // Before a song closes, every screen lets go of it (clips, tracks, plugins, windows)
    // Recordings and tempo changes (Step 23)
    engine.onAskTempoMode = [this] { askTempoMode(); };
    engine.onTempoStatus = [this] (const juce::String& t) { tempoStatus = t; repaint(); };
    tempoSlider.setTooltip ("Tempo of the song. Right-click: what happens to your recordings when the tempo changes");
    tempoSlider.addMouseListener (this, true);

    engine.onEditAboutToChange = [this]
    {
        juce::ModalComponentManager::getInstance()->cancelAllModalComponents();    // clip settings bubbles
        InstrumentPanel::closeAll();
        if (pianoRoll != nullptr)  { pianoRoll->setClip (nullptr);  pianoRoll->setVisible (false); }
        if (drumGrid != nullptr)   { drumGrid->setClip (nullptr);   drumGrid->setVisible (false); }
        if (compEditor != nullptr) { compEditor->setClip (nullptr); compEditor->setVisible (false); }
        timeline.releaseEdit();
        if (mixer != nullptr) mixer->releaseEdit();
        if (! isMixerOpen()) timeline.setVisible (true);
    };
    timeline.onOpenClip = [this] (te::MidiClip* clip) { openPianoRoll (clip); };
    timeline.getMemoFile = [this] { return songId.isNotEmpty() ? library.getMemoFile (songId) : juce::File(); };

    // ---- Punch in/out (Step 9) -----------------------------------------------
    punchButton.setClickingTogglesState (true);
    punchButton.setWantsKeyboardFocus (false);
    punchButton.setTooltip ("Punch in/out: when recording, only the loop range (the gold band on the ruler, shown red) is recorded. "
                            "Start playing a little before it to hear your cue.");
    punchButton.onClick = [this]
    {
        engine.setPunchEnabled (punchButton.getToggleState());
        timeline.repaint();
    };
    punchButton.setColour (juce::TextButton::buttonOnColourId, StudioColours::bad);
    addAndMakeVisible (punchButton);

    writeButton.setClickingTogglesState (true);
    writeButton.setWantsKeyboardFocus (false);
    writeButton.setTooltip ("Write automation: while the song plays, move a fader or knob (here or in the Mixer) and "
                            "the movement is recorded. Let go and the song plays back what you did. Turn off when done.");
    writeButton.onClick = [this]
    {
        if (auto* ed = engine.getEdit())
            Automation::setWriting (*ed, writeButton.getToggleState());
    };
    writeButton.setColour (juce::TextButton::buttonOnColourId, StudioColours::bad);
    addAndMakeVisible (writeButton);

    for (juce::Button* b : { (juce::Button*) &tapButton, (juce::Button*) &clickButton,
                             (juce::Button*) &accentToggle })
        b->setWantsKeyboardFocus (false);

    setWantsKeyboardFocus (true);
    library.addChangeListener (this);
    refreshFromSong();
    startTimerHz (30);
}

StudioPage::~StudioPage()
{
    engine.onAskTempoMode = nullptr;
    engine.onTempoStatus = nullptr;
    MasterPlugin::onOpenRequest = nullptr;
    AnalyserPlugin::onOpenRequest = nullptr;
    ReleasePage::onOpenRequest = nullptr;
    library.removeChangeListener (this);
}

//==============================================================================
//  Recordings and tempo changes (Step 23)
//==============================================================================
void StudioPage::askTempoMode()
{
    juce::Component::SafePointer<StudioPage> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions{}
        .withIconType (juce::MessageBoxIconType::QuestionIcon)
        .withTitle ("The tempo changed: what about your recordings?")
        .withMessage ("Your recordings were made at the old tempo.\n\n"
                      "Stretch them: they are made faster or slower (same pitch) so they stay with the beat and the band. "
                      "Best when you recorded to this song's click.\n\n"
                      "Leave them: they stay exactly where they are in time. Best when you are setting the tempo to match "
                      "a song you imported to learn.\n\n"
                      "Your original recordings are always kept. You can change this later: right-click the Tempo.")
        .withButton ("Stretch them", 1)
        .withButton ("Leave them", 2)
        .withAssociatedComponent (getTopLevelComponent()),
        [safe] (int result)
        {
            if (safe == nullptr) return;
            if (auto* ed = safe->engine.getEdit())
            {
                TempoFollow::setMode (*ed, result == 2 ? TempoFollow::Mode::keep : TempoFollow::Mode::stretch);
                safe->engine.tempoModeChosen();
            }
        });
}

void StudioPage::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && e.eventComponent != this
        && (e.eventComponent == &tempoSlider || tempoSlider.isParentOf (e.eventComponent)))
        showTempoMenu();
}

void StudioPage::showTempoMenu()
{
    auto* ed = engine.getEdit();
    if (ed == nullptr || ! engine.hasOpenSong()) return;
    const auto mode = TempoFollow::getMode (*ed);
    juce::PopupMenu m;
    m.addSectionHeader ("When the tempo changes, my recordings...");
    m.addItem (1, "Stretch to stay with the beat (same pitch)", true, mode == TempoFollow::Mode::stretch);
    m.addItem (2, "Stay where they are in time", true, mode == TempoFollow::Mode::keep);
    m.addItem (3, "Ask me the next time", true, mode == TempoFollow::Mode::ask);
    juce::Component::SafePointer<StudioPage> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (tempoSlider), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (auto* e = safe->engine.getEdit())
        {
            TempoFollow::setMode (*e, r == 1 ? TempoFollow::Mode::stretch : r == 2 ? TempoFollow::Mode::keep : TempoFollow::Mode::ask);
            if (r != 3) safe->engine.tempoModeChosen();
        }
    });
}

//==============================================================================
const Song* StudioPage::getOpenSong() const
{
    return songId.isNotEmpty() ? library.find (songId) : nullptr;
}

void StudioPage::setMixerOpen (bool open)
{
    if (open == isMixerOpen() || mixer == nullptr)
        return;

    if (open)
    {
        if (pianoRoll != nullptr && pianoRoll->isVisible()) closePianoRoll();
        if (drumGrid != nullptr && drumGrid->isVisible())   closeDrumGrid (true);
        if (compEditor != nullptr && compEditor->isOpen())   closeCompEditor();
    }
    mixer->setVisible (open);
    timeline.setVisible (! open);
    if (! open)
        timeline.refreshAll();
    mixerButton.setButtonText (open ? "Tracks" : "Mixer");
    mixerButton.setTooltip (open ? "Back to the tracks (X)" : "Mixer: volume, pan, effects and reverb for every track (X)");
    resized();
    repaint();
}

bool StudioPage::isDrumClip (te::MidiClip& clip) const
{
    if (auto* track = dynamic_cast<te::AudioTrack*> (clip.getTrack()))
        return StudioEngine::getInstrumentId (*track) == "drums" || track->getName().containsIgnoreCase ("drum");
    return false;
}

void StudioPage::openPianoRoll (te::MidiClip* clip)
{
    setMixerOpen (false);
    if (clip == nullptr)
        return;

    // Drum clips open in the drum grid, unless you chose the piano roll for this clip
    const bool drums = isDrumClip (*clip);
    if (drums && ! (bool) clip->state.getProperty ("gpsPianoRoll", false))
    {
        openDrumGrid (clip);
        return;
    }
    if (drumGrid != nullptr && drumGrid->isOpen())
        closeDrumGrid (true);
    pianoRoll->setDrumGridAvailable (drums);

    // The keyboard plays (and records) on the clip's own track while editing it.
    if (auto* track = dynamic_cast<te::AudioTrack*> (clip->getTrack()))
        if (StudioEngine::isInstrumentTrack (*track))
            engine.setArmedTrack (track);

    if (compEditor != nullptr && compEditor->isOpen())
    {
        compEditor->setClip (nullptr);
        compEditor->setVisible (false);
    }
    pianoRoll->setVisible (true);
    resized();
    repaint();
    pianoRoll->setClip (clip);
    timeline.refreshAll();
}

void StudioPage::openDrumGrid (te::MidiClip* clip)
{
    setMixerOpen (false);
    if (clip == nullptr)
        return;

    if (auto* track = dynamic_cast<te::AudioTrack*> (clip->getTrack()))
        if (StudioEngine::isInstrumentTrack (*track))
            engine.setArmedTrack (track);     // so clicking a drum name plays it

    if (pianoRoll->isOpen() || pianoRoll->isVisible())
    {
        pianoRoll->setClip (nullptr);
        pianoRoll->setVisible (false);
    }
    if (compEditor != nullptr && compEditor->isOpen())
    {
        compEditor->setClip (nullptr);
        compEditor->setVisible (false);
    }
    drumGrid->setVisible (true);
    resized();
    repaint();
    drumGrid->setClip (clip);
    drumGrid->grabKeyboardFocus();
    timeline.refreshAll();
}

void StudioPage::closeDrumGrid (bool now)
{
    auto close = [] (StudioPage& p)
    {
        if (p.drumGrid == nullptr)
            return;
        p.drumGrid->setClip (nullptr);
        p.drumGrid->setVisible (false);
        if (! p.isMixerOpen()) p.timeline.setVisible (true);
        p.resized();
        p.repaint();
        p.timeline.refreshAll();
    };
    if (now)
    {
        close (*this);
        return;
    }
    juce::Component::SafePointer<StudioPage> safe (this);
    juce::MessageManager::callAsync ([safe, close] { if (safe != nullptr) close (*safe); });
}

void StudioPage::openCompEditor (te::WaveAudioClip* clip)
{
    setMixerOpen (false);
    if (clip == nullptr)
        return;
    if (drumGrid != nullptr && drumGrid->isOpen())
        closeDrumGrid (true);
    if (pianoRoll->isOpen())
    {
        pianoRoll->setClip (nullptr);
        pianoRoll->setVisible (false);
    }
    compEditor->setVisible (true);
    resized();
    repaint();
    compEditor->setClip (clip);
}

void StudioPage::closeCompEditor()
{
    juce::Component::SafePointer<StudioPage> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe == nullptr || safe->compEditor == nullptr)
            return;
        safe->compEditor->setClip (nullptr);
        safe->compEditor->setVisible (false);
        safe->timeline.setVisible (true);
        safe->resized();
        safe->repaint();
        safe->timeline.refreshAll();
    });
}

void StudioPage::closePianoRoll()
{
    // Called from the piano roll's own button: hide it after the click has finished.
    juce::Component::SafePointer<StudioPage> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe == nullptr || safe->pianoRoll == nullptr)
            return;
        safe->pianoRoll->setClip (nullptr);
        safe->pianoRoll->setVisible (false);
        safe->timeline.setVisible (true);
        safe->resized();
        safe->repaint();
        safe->timeline.refreshAll();
    });
}

void StudioPage::openSong (const juce::String& id)
{
    const auto* song = library.find (id);
    if (song == nullptr)
        return;

    if (pianoRoll != nullptr && pianoRoll->isOpen())
    {
        pianoRoll->setClip (nullptr);
        pianoRoll->setVisible (false);
        timeline.setVisible (true);
    }
    if (drumGrid != nullptr && drumGrid->isOpen())
        closeDrumGrid (true);
    if (compEditor != nullptr && compEditor->isOpen())
    {
        compEditor->setClip (nullptr);
        compEditor->setVisible (false);
    }

    songId = id;
    engine.openSong (*song, library.getSongFolder (id).getChildFile ("project.tracktionedit"));
    engine.ensureSections (*song);
    timeline.editChanged();
    refreshFromSong();
    resized();                 // the Versions button appears now that a song is open
}

//==============================================================================
//  Versions and recovery (Steps 43-44)
//==============================================================================
void StudioPage::goBackToVersion (const SongSafety::Version& v)
{
    const auto* open = getOpenSong();
    const auto file = engine.getSongFile();
    if (open == nullptr || file == juce::File())
        return;
    const auto song = *open;
    engine.save();
    engine.discardSongWithoutSaving();         // the song as it is now was just saved: restore() keeps it as a version
    SongSafety::restore (file, v);
    engine.openSong (song, file);
    engine.ensureSections (song);
    timeline.editChanged();
    refreshFromSong();
    resized();
    repaint();
}

int StudioPage::rescueRecordings (const SongSafety::Session::Previous& p)
{
    auto* edit = engine.getEdit();
    if (edit == nullptr)
        return 0;
    SongSafety::snapshot (engine.getSongFile(), "rescue");
    const auto n = SongSafety::Session::rescueRecordings (*edit, p);
    if (n > 0)
    {
        engine.save();
        timeline.editChanged();
        repaint();
    }
    return n;
}

void StudioPage::songWasDeleted (const juce::String& id)
{
    if (id == songId)
    {
        if (pianoRoll != nullptr && pianoRoll->isOpen())
        {
            pianoRoll->setClip (nullptr);
            pianoRoll->setVisible (false);
            timeline.setVisible (true);
        }
        if (drumGrid != nullptr && drumGrid->isOpen())
            closeDrumGrid (true);
        if (compEditor != nullptr && compEditor->isOpen())
        {
            compEditor->setClip (nullptr);
            compEditor->setVisible (false);
        }
        engine.closeSong();
        songId = {};
        timeline.editChanged();
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
                                (juce::Component*) &recordButton,
                                (juce::Component*) &loopStart, (juce::Component*) &loopLength })
        c->setEnabled (enabled);

    if (song != nullptr)
    {
        tempoSlider.setValue (song->tempo, juce::dontSendNotification);
        timeSigBox.setText (song->timeSignature, juce::dontSendNotification);
        keyBox.setText (song->key, juce::dontSendNotification);
    }

    punchButton.setToggleState (engine.isPunchEnabled(), juce::dontSendNotification);
    if (auto* ed = engine.getEdit())
        writeButton.setToggleState (Automation::isWriting (*ed), juce::dontSendNotification);

    if (chordPanel != nullptr)
        chordPanel->refresh();
    timeline.repaint();
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
    if (key.getTextCharacter() == 'l')                       { engine.setLooping (! engine.isLooping()); return true; }
    if (key.getTextCharacter() == 'c')                       { engine.setClickEnabled (! engine.isClickEnabled()); return true; }
    if (key.getTextCharacter() == 'x' && songId.isNotEmpty()){ setMixerOpen (! isMixerOpen()); return true; }
    return false;
}

//==============================================================================
void StudioPage::timerCallback()
{
    if (++autosaveTicks >= 300)      // every 10 seconds at 30 Hz
    {
        autosaveTicks = 0;
        engine.autosaveIfChanged();
    }

    const auto pos = engine.getPosition();
    const auto playing = engine.isPlaying();
    const auto recording = engine.isRecording();
    const auto takes = engine.getNumTakes();

    // Keep the loop fields in step with a loop dragged on the ruler.
    if (! loopStart.isMouseButtonDown() && ! loopLength.isMouseButtonDown())
    {
        const auto [first, count] = engine.getLoopBars();
        loopStart.setValue (first, juce::dontSendNotification);
        loopLength.setValue (count, juce::dontSendNotification);
    }

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
    }
}

//==============================================================================
void StudioPage::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    const auto* song = getOpenSong();

    // ---- Song header --------------------------------------------------------
    auto headerBox = headerArea.withTrimmedRight (versionsButton.isVisible() ? versionsButton.getWidth() + 12 : 0);
    g.setColour (text);
    g.setFont (studioFont (compact ? 16.0f : 21.0f, true));
    const auto titleText = song != nullptr ? song->title : juce::String ("No song open");
    if (compact)
    {
        // One line: title, then the details beside it
        const auto w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), titleText) + 16;
        g.drawText (titleText, headerBox.removeFromLeft (w), juce::Justification::centredLeft, true);
    }
    else
    {
        g.drawText (titleText, headerBox.removeFromTop (26), juce::Justification::bottomLeft, true);
    }

    g.setColour (song != nullptr ? gold : textDim);
    g.setFont (studioFont (13.5f));
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    g.drawText (song != nullptr
                    ? song->key + dot + song->timeSignature + dot + juce::String (song->tempo)
                          + (song->isCompoundMeter() ? " BPM (dotted quarter)" : " BPM") + dot + song->theme
                          + (tempoStatus.isNotEmpty() ? dot + tempoStatus : juce::String())
                    : juce::String ("In the Song Library, choose a song and click Open in Studio. "
                                    "The keyboard still plays meanwhile."),
                headerBox, compact ? juce::Justification::centredLeft : juce::Justification::topLeft, true);

    // ---- Cards --------------------------------------------------------------
    for (auto cardArea : { transportCard, metronomeCard })
    {
        if (cardArea.isEmpty())
            continue;
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

    // ---- Position display with beat lights ---------------------------------
    {
        auto area = positionArea;
        g.setColour (header);
        g.fillRoundedRectangle (area.toFloat(), 8.0f);

        auto inner = area.reduced (12, 6);
        const auto& p = lastPosition;
        const auto countIn = p.countIn && lastRecording;

        auto top = inner.removeFromTop (30);
        g.setFont (studioFont (26.0f, true));
        g.setColour (countIn ? bad : (lastRecording ? bad : (lastPlaying ? gold : text)));
        g.drawText (countIn ? juce::String ("COUNT-IN")
                            : juce::String (juce::jmax (1, p.bar)) + "." + juce::String (juce::jmax (1, p.beat)),
                    top, juce::Justification::centredLeft);

        const auto secs = juce::jmax (0.0, p.seconds);
        const auto mins = (int) (secs / 60.0);
        g.setFont (studioFont (12.5f));
        g.setColour (textDim);
        g.drawText (juce::String (mins) + ":" + juce::String (secs - mins * 60.0, 1).paddedLeft ('0', 4),
                    top, juce::Justification::centredRight);

        // Beat lights: beat 1 gold, main pulses of 6/8 and 12/8 larger.
        const auto beats = juce::jlimit (1, 16, p.beatsInBar);
        const auto activeBeat = (lastPlaying || lastRecording) ? p.beat : 0;
        const auto compound = song != nullptr && song->isCompoundMeter();
        const auto step = juce::jmin (18.0f, (float) inner.getWidth() / (float) beats);
        auto x = (float) inner.getX();
        const auto cy = (float) inner.getCentreY() + 2.0f;

        for (int i = 1; i <= beats; ++i)
        {
            const auto main = i == 1 || (compound && (i - 1) % 3 == 0);
            const auto d = main ? 11.0f : 8.0f;
            const auto r = juce::Rectangle<float> (d, d).withCentre ({ x + step * 0.5f, cy });
            const auto on = i == activeBeat;

            g.setColour (on ? (i == 1 ? gold : text) : cardBorder.brighter (0.2f));
            g.fillEllipse (r);
            x += step;
        }
    }
}

void StudioPage::resized()
{
    labels.clear();
    auto area = getLocalBounds().reduced (getHeight() < 640 ? 12 : 20, getHeight() < 640 ? 6 : 10);

    // Small screens: thinner cards and a one-line song title
    this->compact = getHeight() < 640 || getWidth() < 1150;

    // While the piano roll is open the song title and the metronome card fold away to give the notes room.
    const auto editorOpen = (pianoRoll != nullptr && pianoRoll->isVisible()) || (compEditor != nullptr && compEditor->isVisible())
                          || (drumGrid != nullptr && drumGrid->isVisible())
                          || isMixerOpen();

    headerArea = editorOpen ? juce::Rectangle<int>() : area.removeFromTop (this->compact ? 24 : 44);
    versionsButton.setVisible (! headerArea.isEmpty() && getOpenSong() != nullptr);
    versionsButton.setBounds (headerArea.withLeft (headerArea.getRight() - 104).withSizeKeepingCentre (104, this->compact ? 24 : 30));
    if (! editorOpen)
        area.removeFromTop (this->compact ? 4 : 6);

    auto field = [this] (juce::Rectangle<int>& row, int width, const juce::String& label)
    {
        auto col = row.removeFromLeft (width);
        row.removeFromLeft (12);
        auto labelArea = col.removeFromTop (this->compact ? 15 : 18);
        labels.push_back ({ labelArea, label });
        return col.withSizeKeepingCentre (col.getWidth(), this->compact ? 28 : 32);
    };

    // ---- Transport card -----------------------------------------------------
    const auto bs = this->compact ? 42 : 50;
    transportCard = area.removeFromTop (this->compact ? 60 : 72);
    auto t = transportCard.reduced (this->compact ? 10 : 12, this->compact ? 5 : 8);

    auto buttons = t.removeFromLeft (5 * bs + 4 * 6);
    buttons = buttons.withSizeKeepingCentre (buttons.getWidth(), bs);
    for (auto* b : { &startButton, &playButton, &stopButton, &recordButton, &loopButton })
    {
        b->setBounds (buttons.removeFromLeft (bs));
        buttons.removeFromLeft (6);
    }
    t.removeFromLeft (8);
    punchButton.setBounds (t.removeFromLeft (62).withSizeKeepingCentre (62, 30));
    t.removeFromLeft (6);
    writeButton.setBounds (t.removeFromLeft (58).withSizeKeepingCentre (58, 30));
    mixerButton.setBounds (t.removeFromRight (this->compact ? 80 : 96).withSizeKeepingCentre (this->compact ? 80 : 96, 34));
    t.removeFromRight (8);
    exportButton.setBounds (t.removeFromRight (this->compact ? 74 : 90).withSizeKeepingCentre (this->compact ? 74 : 90, 34));
    t.removeFromRight (10);

    t.removeFromLeft (this->compact ? 10 : 14);
    positionArea = t.removeFromLeft (this->compact ? 170 : 210);
    t.removeFromLeft (this->compact ? 10 : 14);

    {
        auto col = t.removeFromLeft (this->compact ? 200 : 230);
        t.removeFromLeft (12);
        tempoLabelArea = col.removeFromTop (18);
        labels.push_back ({ tempoLabelArea, "Tempo" });
        auto r = col.withSizeKeepingCentre (col.getWidth(), 32);
        tapButton.setBounds (r.removeFromRight (54));
        r.removeFromRight (6);
        tempoSlider.setBounds (r);
    }
    timeSigBox.setBounds (field (t, 92, "Time signature"));
    keyBox.setBounds (field (t, juce::jlimit (100, 140, t.getWidth()), "Key"));

    area.removeFromTop (this->compact ? 4 : 6);

    // ---- Metronome and loop card --------------------------------------------
    for (juce::Component* c : { (juce::Component*) &clickButton, (juce::Component*) &clickSoundBox,
                                (juce::Component*) &clickVolume, (juce::Component*) &accentToggle,
                                (juce::Component*) &countInBox, (juce::Component*) &loopStart, (juce::Component*) &loopLength })
        c->setVisible (! editorOpen);

    if (editorOpen)
    {
        metronomeCard = {};
    }
    else
    {
        metronomeCard = area.removeFromTop (this->compact ? 52 : 60);
        auto m = metronomeCard.reduced (12, this->compact ? 3 : 6);

        {
            auto col = m.removeFromLeft (50);
            m.removeFromLeft (12);
            labels.push_back ({ col.removeFromTop (this->compact ? 15 : 18), "Click" });
            clickButton.setBounds (col.withSizeKeepingCentre (50, this->compact ? 27 : 30));
        }
        clickSoundBox.setBounds (field (m, 120, "Click sound"));
        clickVolume.setBounds (field (m, 130, "Click volume"));
        accentToggle.setBounds (field (m, 130, " "));
        countInBox.setBounds (field (m, 130, "Count-in (recording)"));
        loopStart.setBounds (field (m, 120, "Loop from bar"));
        loopLength.setBounds (field (m, 140, "Loop length"));
    }

    area.removeFromTop (this->compact ? 5 : 8);

    if (isMixerOpen())
    {
        if (chordPanel != nullptr) chordPanel->setVisible (false);
        timeline.setVisible (false);
        mixer->setBounds (area);
        return;
    }
    if (chordPanel != nullptr) chordPanel->setVisible (chordPanelOpen);

    if (chordPanelOpen && chordPanel != nullptr)
    {
        chordPanel->setBounds (area.removeFromRight (juce::jlimit (340, 400, area.getWidth() / 3)));
        area.removeFromRight (8);
    }

    if (pianoRoll != nullptr && pianoRoll->isVisible())
    {
        // Split: timeline on top, piano roll below. On small screens the piano roll takes the whole area.
        const auto split = pianoRoll->isSplit() && area.getHeight() >= 540;
        timeline.setVisible (split);
        if (split)
        {
            timeline.setBounds (area.removeFromTop (juce::jmax (180, area.getHeight() * 34 / 100)));
            area.removeFromTop (6);
        }
        pianoRoll->setBounds (area);
        return;
    }

    if (drumGrid != nullptr && drumGrid->isVisible())
    {
        // Timeline on top (you see the song), drum grid below. The grid needs about 17 rows.
        const auto gridHeight = 40 + 22 + 17 * 22 + 34;
        const auto split = area.getHeight() >= gridHeight + 150;
        timeline.setVisible (split);
        if (split)
        {
            timeline.setBounds (area.removeFromTop (juce::jmax (150, area.getHeight() - gridHeight - 6)));
            area.removeFromTop (6);
        }
        drumGrid->setBounds (area);
        return;
    }

    if (compEditor != nullptr && compEditor->isVisible())
    {
        // The timeline stays above (you can see and play the song while comping)
        const auto split = area.getHeight() >= 420;
        timeline.setVisible (split);
        if (split)
        {
            timeline.setBounds (area.removeFromTop (juce::jmax (170, area.getHeight() * 38 / 100)));
            area.removeFromTop (6);
        }
        compEditor->setBounds (area);
        return;
    }

    timeline.setVisible (true);
    timeline.setBounds (area);
}
