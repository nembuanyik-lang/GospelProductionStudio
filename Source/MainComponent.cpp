#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // ---- Settings file (remembers audio device, MIDI choices) ---------------
    juce::PropertiesFile::Options options;
    options.applicationName     = "GospelProductionStudio";
    options.filenameSuffix      = ".settings";
    options.folderName          = "GospelProductionStudio";
    options.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (options);

    // ---- Test instrument ----------------------------------------------------
    for (int i = 0; i < 24; ++i)
        synth.addVoice (new StudioKeysVoice());
    synth.addSound (new StudioKeysSound());

    // ---- On-screen 88-key keyboard ------------------------------------------
    keyboard.setAvailableRange (21, 108);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setScrollButtonsVisible (false);
    addAndMakeVisible (keyboard);

    // ---- Pages --------------------------------------------------------------
    addChildComponent (libraryPage);

    for (auto* tab : { &libraryTab, &engineTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (1001);
        addAndMakeVisible (tab);
    }
    libraryTab.onClick = [this] { showPage (Page::library); };
    engineTab.onClick  = [this] { showPage (Page::engine); };

    // ---- Buttons ------------------------------------------------------------
    setupButton.onClick = [this] { showAudioSettings(); };
    addAndMakeVisible (setupButton);

    toneButton.setClickingTogglesState (true);
    toneButton.onClick = [this] { toneOn.store (toneButton.getToggleState()); };
    addAndMakeVisible (toneButton);

    panicButton.onClick = [this] { allNotesOff(); };
    addAndMakeVisible (panicButton);

    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    // ---- Audio device: restore last setup, 2 in / 2 out ---------------------
    auto savedSetup = settings->getXmlValue ("audioSetup");
    setAudioChannels (2, 2, savedSetup.get());

    // ---- MIDI: listen to every connected keyboard ---------------------------
    deviceManager.addMidiInputDeviceCallback ({}, &midiCollector);
    enableNewMidiInputs();
    midiListConnection = juce::MidiDeviceListConnection::make ([this] { enableNewMidiInputs(); });

    showPage (Page::library);
    setSize (1280, 800);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    libraryPage.flush();
    memoRecorder.stopRecording();
    memoRecorder.stopPlayback();
    saveSettings();
    deviceManager.removeMidiInputDeviceCallback ({}, &midiCollector);
    shutdownAudio();
}

//==============================================================================
//  Audio
//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    tonePhase = 0.0;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    midiCollector.reset (sampleRate);
    memoRecorder.prepare (sampleRate, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    auto& buffer = *info.buffer;

    // 1. Measure the input (microphone / guitar) before clearing it.
    float inPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, info.startSample, info.numSamples));

    if (inPeak > inputPeak.load())
        inputPeak.store (inPeak);

    // Voice memo recording takes the input before it is cleared.
    memoRecorder.processInput (buffer, info.startSample, info.numSamples);

    // No live input monitoring in Stage 0: output starts silent.
    info.clearActiveBufferRegion();

    // 2. MIDI from the keyboard(s) and the on-screen keys.
    juce::MidiBuffer incomingMidi;
    midiCollector.removeNextBlockOfMessages (incomingMidi, info.numSamples);

    for (const auto metadata : incomingMidi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            lastNote.store (message.getNoteNumber());
            lastVelocity.store (message.getVelocity());
        }
        midiEventCount.fetch_add (1);
    }

    keyboardState.processNextMidiBuffer (incomingMidi, info.startSample, info.numSamples, true);
    synth.renderNextBlock (buffer, incomingMidi, info.startSample, info.numSamples);

    // 3. Test tone: A 440 Hz sine at -18 dBFS.
    if (toneOn.load())
    {
        const auto delta = juce::MathConstants<double>::twoPi * 440.0 / currentSampleRate;
        const auto gain  = juce::Decibels::decibelsToGain (-18.0f);

        for (int i = 0; i < info.numSamples; ++i)
        {
            const auto sample = gain * (float) std::sin (tonePhase);
            tonePhase += delta;
            if (tonePhase >= juce::MathConstants<double>::twoPi)
                tonePhase -= juce::MathConstants<double>::twoPi;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addSample (ch, info.startSample + i, sample);
        }
    }

    // 4. Voice memo playback.
    memoRecorder.renderPlayback (buffer, info.startSample, info.numSamples);

    // 5. Measure the output.
    float outPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, info.startSample, info.numSamples));

    if (outPeak > outputPeak.load())
        outputPeak.store (outPeak);
}

void MainComponent::releaseResources()
{
    memoRecorder.release();
}

//==============================================================================
//  Pages
//==============================================================================
void MainComponent::showPage (Page newPage)
{
    if (page == Page::library && newPage != Page::library)
        libraryPage.flush();

    page = newPage;
    const auto isEngine = page == Page::engine;

    libraryTab.setToggleState (! isEngine, juce::dontSendNotification);
    engineTab.setToggleState (isEngine, juce::dontSendNotification);

    libraryPage.setVisible (! isEngine);
    for (auto* c : { (juce::Component*) &toneButton, (juce::Component*) &panicButton,
                     (juce::Component*) &inputMeter, (juce::Component*) &outputMeter })
        c->setVisible (isEngine);

    resized();
    repaint();
}

//==============================================================================
//  Devices and settings
//==============================================================================
void MainComponent::enableNewMidiInputs()
{
    // Switch on any keyboard that appears (plugged in now or later).
    // Devices the user switched off in the setup window stay off.
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (! knownMidiInputs.contains (device.identifier))
        {
            knownMidiInputs.add (device.identifier);
            deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
        }
    }

    repaint();
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager,
                                                             0, 2,     // inputs
                                                             2, 2,     // outputs
                                                             true,     // MIDI inputs
                                                             false,    // MIDI output
                                                             true,     // stereo pairs
                                                             false);   // show advanced
    selector->setSize (600, 480);

    juce::DialogWindow::LaunchOptions dialog;
    dialog.content.setOwned (selector);
    dialog.dialogTitle                  = "Audio & MIDI Setup";
    dialog.dialogBackgroundColour       = StudioColours::card;
    dialog.escapeKeyTriggersCloseButton = true;
    dialog.useNativeTitleBar            = true;
    dialog.resizable                    = false;
    dialog.launchAsync();
}

void MainComponent::saveSettings()
{
    if (auto state = deviceManager.createStateXml())
        settings->setValue ("audioSetup", state.get());

    settings->saveIfNeeded();
}

void MainComponent::allNotesOff()
{
    for (int channel = 1; channel <= 16; ++channel)
        keyboardState.allNotesOff (channel);

    synth.allNotesOff (0, false);

    toneButton.setToggleState (false, juce::dontSendNotification);
    toneOn.store (false);
}

//==============================================================================
//  Screen
//==============================================================================
void MainComponent::timerCallback()
{
    // Peak hold with smooth fall-off.
    shownInput  = juce::jmax (inputPeak.exchange (0.0f),  shownInput  * 0.85f);
    shownOutput = juce::jmax (outputPeak.exchange (0.0f), shownOutput * 0.85f);

    inputMeter.setLevel (shownInput);
    outputMeter.setLevel (shownOutput);

    if (page == Page::engine)
        repaint (0, 0, getWidth(), keyboard.getY());
}

void MainComponent::drawCard (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title) const
{
    using namespace StudioColours;

    g.setColour (card);
    g.fillRoundedRectangle (area.toFloat(), 10.0f);
    g.setColour (cardBorder);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 10.0f, 1.0f);

    auto titleArea = area.reduced (20, 0).removeFromTop (48);
    g.setColour (gold);
    g.fillRect (titleArea.getX(), titleArea.getBottom() - 12, 28, 2);
    g.setColour (text);
    g.setFont (studioFont (16.0f, true));
    g.drawText (title, titleArea.withTrimmedBottom (14), juce::Justification::bottomLeft);
}

void MainComponent::drawRow (juce::Graphics& g, juce::Rectangle<int>& area, const juce::String& label,
                             const juce::String& value, juce::Colour valueColour) const
{
    auto row = area.removeFromTop (30);

    g.setColour (StudioColours::cardBorder.withAlpha (0.6f));
    g.drawHorizontalLine (row.getBottom() - 1, (float) row.getX(), (float) row.getRight());

    g.setFont (studioFont (13.5f));
    g.setColour (StudioColours::textDim);
    g.drawText (label, row.removeFromLeft (row.getWidth() / 2), juce::Justification::centredLeft);

    g.setFont (studioFont (13.5f, true));
    g.setColour (valueColour);
    g.drawText (value, row, juce::Justification::centredRight);
}

void MainComponent::paint (juce::Graphics& g)
{
    using namespace StudioColours;

    g.fillAll (background);

    // ---- Header -------------------------------------------------------------
    auto headerBox = headerArea;
    g.setColour (header);
    g.fillRect (headerBox);
    g.setColour (gold);
    g.fillRect (headerBox.removeFromBottom (2));

    auto titleArea = headerBox.reduced (24, 0);
    g.setColour (text);
    g.setFont (studioFont (24.0f, true));
    g.drawText ("Gospel Production Studio", titleArea.removeFromTop (44).withTrimmedTop (10),
                juce::Justification::bottomLeft);
    g.setColour (gold);
    g.setFont (studioFont (13.0f));
    g.drawText (juce::String::fromUTF8 (page == Page::library ? "STAGE 1  \xc2\xb7  SONG LIBRARY" : "STAGE 0  \xc2\xb7  ENGINE & DEVICES"), titleArea, juce::Justification::topLeft);

    if (page == Page::engine)
    {
    // ---- Audio engine card --------------------------------------------------
    drawCard (g, audioCard, "Audio Engine");
    {
        auto rows = audioCard.reduced (20, 0).withTrimmedTop (56).withTrimmedBottom (16);

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            const auto sr      = device->getCurrentSampleRate();
            const auto buffer  = device->getCurrentBufferSizeSamples();
            const auto latency = sr > 0 ? 1000.0 * (device->getInputLatencyInSamples()
                                                    + device->getOutputLatencyInSamples()
                                                    + 2 * buffer) / sr
                                        : 0.0;
            const auto cpu     = deviceManager.getCpuUsage() * 100.0;

            drawRow (g, rows, "Status",      "Running", good);
            drawRow (g, rows, "Driver",      device->getTypeName(), text);
            drawRow (g, rows, "Device",      device->getName(), text);
            drawRow (g, rows, "Sample rate", juce::String (sr / 1000.0, 1) + " kHz", text);
            drawRow (g, rows, "Buffer size", juce::String (buffer) + " samples", text);
            drawRow (g, rows, "Round-trip latency", juce::String (latency, 1) + " ms",
                     latency <= 15.0 ? good : (latency <= 30.0 ? warn : bad));
            drawRow (g, rows, "Engine load", juce::String (cpu, 1) + " %",
                     cpu < 50.0 ? good : (cpu < 80.0 ? warn : bad));
        }
        else
        {
            drawRow (g, rows, "Status", "No audio device", bad);
            g.setColour (textDim);
            g.setFont (studioFont (13.5f));
            g.drawFittedText ("Open Audio & MIDI Setup and choose your speakers, headphones or audio interface.",
                              rows.removeFromTop (60), juce::Justification::centredLeft, 3);
        }
    }

    // ---- MIDI card ----------------------------------------------------------
    drawCard (g, midiCard, "MIDI Keyboards");
    {
        auto rows = midiCard.reduced (20, 0).withTrimmedTop (56).withTrimmedBottom (16);
        const auto devices = juce::MidiInput::getAvailableDevices();

        if (devices.isEmpty())
        {
            drawRow (g, rows, "Keyboard", "None detected", warn);
            g.setColour (textDim);
            g.setFont (studioFont (13.5f));
            g.drawFittedText ("Connect the keyboard with a USB cable. It appears here automatically.",
                              rows.removeFromTop (50), juce::Justification::centredLeft, 3);
        }
        else
        {
            for (const auto& device : devices)
            {
                const auto enabled = deviceManager.isMidiInputDeviceEnabled (device.identifier);
                drawRow (g, rows, device.name, enabled ? "Connected" : "Switched off",
                         enabled ? good : textDim);
            }
        }

        const auto note = lastNote.load();
        drawRow (g, rows, "Last note played",
                 note >= 0 ? juce::MidiMessage::getMidiNoteName (note, true, true, 4)
                               + "   (velocity " + juce::String (lastVelocity.load()) + ")"
                           : juce::String ("Play a key..."),
                 note >= 0 ? gold : textDim);
        drawRow (g, rows, "MIDI messages received", juce::String (midiEventCount.load()), text);
    }

    // ---- Levels card --------------------------------------------------------
    drawCard (g, levelCard, "Levels");
    {
        auto dbText = [] (float db) { return db <= -59.9f ? juce::String ("-inf dB")
                                                           : juce::String (db, 1) + " dB"; };
        g.setFont (studioFont (13.5f));

        g.setColour (textDim);
        g.drawText ("Input  (microphone / guitar)", inputLabelArea, juce::Justification::centredLeft);
        g.setColour (text);
        g.drawText (dbText (inputMeter.getLevelDb()), inputLabelArea, juce::Justification::centredRight);

        g.setColour (textDim);
        g.drawText ("Output  (speakers / headphones)", outputLabelArea, juce::Justification::centredLeft);
        g.setColour (text);
        g.drawText (dbText (outputMeter.getLevelDb()), outputLabelArea, juce::Justification::centredRight);
    }

    }

    // ---- Footer -------------------------------------------------------------
    g.setColour (header);
    g.fillRect (footerArea);
    g.setColour (textDim);
    g.setFont (studioFont (12.0f));
    g.drawText (juce::String::fromUTF8 ("Stage 1  \xc2\xb7  Step 3 of 22  \xc2\xb7  Songs saved in Documents\\Gospel Production Studio\\Songs"),
                footerArea.reduced (24, 0), juce::Justification::centredLeft);
    g.drawText ("v" + juce::String (JUCE_APPLICATION_VERSION_STRING),
                footerArea.reduced (24, 0), juce::Justification::centredRight);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    headerArea = area.removeFromTop (76);
    footerArea = area.removeFromBottom (30);

    // Header: title on the left, page tabs, setup on the right.
    auto headerInner = headerArea.reduced (24, 20);
    setupButton.setBounds (headerInner.removeFromRight (200));
    headerInner.removeFromLeft (340);
    libraryTab.setBounds (headerInner.removeFromLeft (150));
    headerInner.removeFromLeft (8);
    engineTab.setBounds (headerInner.removeFromLeft (170));

    // Keyboard along the bottom: 52 white keys across the full width.
    // Smaller on the Song Library page to leave room for lyrics.
    auto keyboardArea = area.removeFromBottom (page == Page::library ? 96 : 140).reduced (24, 10);
    keyboard.setBounds (keyboardArea);
    keyboard.setKeyWidth ((float) keyboardArea.getWidth() / 52.0f);

    libraryPage.setBounds (area);

    // Engine & Devices: three cards.
    auto body = area.reduced (24, 20);
    const int gap = 18;
    const int cardWidth = (body.getWidth() - 2 * gap) / 3;

    audioCard = body.removeFromLeft (cardWidth);
    body.removeFromLeft (gap);
    midiCard  = body.removeFromLeft (cardWidth);
    body.removeFromLeft (gap);
    levelCard = body;

    // Levels card contents.
    auto inner = levelCard.reduced (20, 0).withTrimmedTop (60).withTrimmedBottom (20);

    inputLabelArea = inner.removeFromTop (24);
    inner.removeFromTop (6);
    inputMeter.setBounds (inner.removeFromTop (18));
    inner.removeFromTop (22);

    outputLabelArea = inner.removeFromTop (24);
    inner.removeFromTop (6);
    outputMeter.setBounds (inner.removeFromTop (18));

    panicButton.setBounds (inner.removeFromBottom (38));
    inner.removeFromBottom (10);
    toneButton.setBounds (inner.removeFromBottom (38));
}
