#include "InstrumentPanel.h"
#include "MainComponent.h"
#include "Dialogs.h"

//==============================================================================
MainComponent::MainComponent()
{
    // ---- Pages --------------------------------------------------------------
    libraryPage.onOpenInStudio = [this] (const juce::String& id)
    {
        studioPage.openSong (id);
        showPage (Page::studio);
    };
    libraryPage.onSongDeleted = [this] (const juce::String& id) { studioPage.songWasDeleted (id); };

    addChildComponent (libraryPage);
    addChildComponent (studioPage);

    for (auto* tab : { &libraryTab, &studioTab, &engineTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (1001);
        tab->setWantsKeyboardFocus (false);
        addAndMakeVisible (tab);
    }
    libraryTab.onClick = [this] { showPage (Page::library); };
    studioTab.onClick  = [this] { showPage (Page::studio); };
    engineTab.onClick  = [this] { showPage (Page::engine); };

    setupButton.setWantsKeyboardFocus (false);
    setupButton.onClick = [this] { showAudioSettings(); };
    addAndMakeVisible (setupButton);

    // ---- Updates ------------------------------------------------------------
    updatesButton.setWantsKeyboardFocus (false);
    updatesButton.setTooltip ("Check GitHub for a newer version (" + Updater::describeCurrent() + ")");
    updatesButton.onClick = [this] { checkForUpdates (true); };
    addAndMakeVisible (updatesButton);

    soundsButton.setWantsKeyboardFocus (false);
    soundsButton.setTooltip ("Download piano, bass and drum sounds (once)");
    soundsButton.onClick = [] { SoundLibrary::openWindow(); };
    addAndMakeVisible (soundsButton);

    updateNowButton.setColour (juce::TextButton::buttonColourId, StudioColours::gold);
    updateNowButton.setColour (juce::TextButton::textColourOffId, StudioColours::header);
    updateNowButton.onClick = [this] { startUpdate(); };
    laterButton.onClick = [this] { setBanner (false); };
    for (auto* b : { &updateNowButton, &laterButton })
    {
        b->setWantsKeyboardFocus (false);
        addChildComponent (b);
    }

    // Quiet check a few seconds after start-up.
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::Timer::callAfterDelay (5000, [safe] { if (safe != nullptr) safe->checkForUpdates (false); });

    // ---- 88-key keyboard ----------------------------------------------------
    keyboard.setAvailableRange (21, 108);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setMidiChannel (StudioEngine::onScreenChannel);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    // ---- Engine & Devices page ----------------------------------------------
    toneButton.setClickingTogglesState (true);
    toneButton.onClick = [this] { studio.setTestTone (toneButton.getToggleState()); };
    panicButton.onClick = [this] { allNotesOff(); };
    for (juce::Component* c : { (juce::Component*) &toneButton, (juce::Component*) &panicButton,
                                (juce::Component*) &inputMeter, (juce::Component*) &outputMeter })
        addChildComponent (c);

    setWantsKeyboardFocus (true);
    showPage (Page::library);
    setSize (1280, 800);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    InstrumentPanel::closeAll();
    libraryPage.flush();
}

//==============================================================================
//  Updates
//==============================================================================
void MainComponent::checkForUpdates (bool userAsked)
{
    if (updater.isBusy())
        return;

    if (userAsked)
        updatesButton.setButtonText ("Checking...");

    juce::Component::SafePointer<MainComponent> safe (this);
    updater.check ([safe, userAsked] (Updater::Result result, const Updater::Release& release, const juce::String& message)
    {
        if (safe == nullptr)
            return;

        safe->updatesButton.setButtonText ("Updates");

        if (result == Updater::Result::updateAvailable)
        {
            safe->pendingRelease = release;
            safe->downloadProgress = -1.0f;
            safe->bannerText = "A new version is ready: " + release.version
                               + (release.build > 0 ? " (build " + juce::String (release.build) + ")" : juce::String())
                               + ".  You have " + Updater::describeCurrent() + ".";
            safe->setBanner (true);
        }
        else if (userAsked)
        {
            Dialogs::message (safe.getComponent(),
                              result == Updater::Result::upToDate ? "Up to date" : "Update check", message);
        }
    });
}

void MainComponent::startUpdate()
{
    if (pendingRelease.downloadUrl.isEmpty() || updater.isBusy())
        return;

    libraryPage.flush();
    downloadProgress = 0.0f;
    bannerText = "Downloading the update...";
    updateNowButton.setEnabled (false);
    laterButton.setEnabled (false);
    repaint (bannerArea);

    juce::Component::SafePointer<MainComponent> safe (this);
    updater.downloadAndInstall (pendingRelease,
        [safe] (float progress)
        {
            if (safe == nullptr) return;
            safe->downloadProgress = progress;
            safe->bannerText = "Downloading the update...  " + juce::String (juce::roundToInt (progress * 100.0f)) + "%";
            safe->repaint (safe->bannerArea);
        },
        [safe] (bool ok, const juce::String& message)
        {
            if (safe == nullptr) return;
            safe->bannerText = message;
            safe->downloadProgress = -1.0f;
            safe->updateNowButton.setEnabled (true);
            safe->laterButton.setEnabled (true);
            safe->repaint (safe->bannerArea);
            if (! ok)
                Dialogs::message (safe.getComponent(), "Update", message);
        });
}

void MainComponent::setBanner (bool visible)
{
    bannerVisible = visible;
    updateNowButton.setVisible (visible);
    laterButton.setVisible (visible);
    resized();
    repaint();
}

//==============================================================================
void MainComponent::showPage (Page newPage)
{
    if (page == Page::library && newPage != Page::library)
        libraryPage.flush();

    page = newPage;

    libraryTab.setToggleState (page == Page::library, juce::dontSendNotification);
    studioTab.setToggleState (page == Page::studio, juce::dontSendNotification);
    engineTab.setToggleState (page == Page::engine, juce::dontSendNotification);

    libraryPage.setVisible (page == Page::library);
    studioPage.setVisible (page == Page::studio);

    const auto isEngine = page == Page::engine;
    for (juce::Component* c : { (juce::Component*) &toneButton, (juce::Component*) &panicButton,
                                (juce::Component*) &inputMeter, (juce::Component*) &outputMeter })
        c->setVisible (isEngine);

    resized();
    repaint();

    if (page == Page::studio)
        studioPage.grabKeyboardFocus();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (page == Page::studio)
        return studioPage.handleShortcut (key);

    return false;
}

void MainComponent::showAudioSettings()
{
    // MIDI keyboards are handled by the engine automatically, so only audio is shown here.
    auto* selector = new juce::AudioDeviceSelectorComponent (studio.getAudioDeviceManager(),
                                                             0, 8,     // inputs (microphones, guitar)
                                                             2, 2,     // outputs
                                                             false,    // MIDI inputs
                                                             false,    // MIDI output
                                                             false,    // inputs listed one by one
                                                             false);   // show advanced
    selector->setSize (600, 420);

    juce::DialogWindow::LaunchOptions dialog;
    dialog.content.setOwned (selector);
    dialog.dialogTitle                  = "Audio & MIDI Setup";
    dialog.dialogBackgroundColour       = StudioColours::card;
    dialog.escapeKeyTriggersCloseButton = true;
    dialog.useNativeTitleBar            = true;
    dialog.resizable                    = false;
    dialog.launchAsync();
}

void MainComponent::allNotesOff()
{
    for (int channel = 1; channel <= 16; ++channel)
        studio.getKeyboardState().allNotesOff (channel);

    toneButton.setToggleState (false, juce::dontSendNotification);
    studio.setTestTone (false);
}

void MainComponent::timerCallback()
{
    shownInput  = juce::jmax (studio.getInputLevel(),  shownInput  * 0.85f);
    shownOutput = juce::jmax (studio.getOutputLevel(), shownOutput * 0.85f);

    if (page == Page::engine)
    {
        inputMeter.setLevel (shownInput);
        outputMeter.setLevel (shownOutput);
        repaint (0, 0, getWidth(), keyboard.getY());
    }
}

//==============================================================================
//  Drawing
//==============================================================================
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

    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    g.setColour (gold);
    g.setFont (studioFont (13.0f));
    g.drawText (page == Page::library ? "STAGE 1" + dot + "SONG LIBRARY"
              : page == Page::studio  ? "STAGE 1" + dot + "STUDIO"
                                      : "STAGE 0" + dot + "ENGINE & DEVICES",
                titleArea, juce::Justification::topLeft);

    if (bannerVisible)
    {
        g.setColour (gold.withAlpha (0.16f));
        g.fillRect (bannerArea);
        g.setColour (gold);
        g.fillRect (bannerArea.removeFromBottom (1));

        if (downloadProgress >= 0.0f)
        {
            g.setColour (gold.withAlpha (0.35f));
            g.fillRect (bannerArea.withWidth (juce::roundToInt (bannerArea.getWidth() * downloadProgress)));
        }

        g.setColour (text);
        g.setFont (studioFont (14.0f, true));
        g.drawText (bannerText, bannerArea.reduced (24, 0), juce::Justification::centredLeft, true);
    }

    if (page == Page::engine)
        paintEnginePage (g);

    // ---- Footer -------------------------------------------------------------
    g.setColour (header);
    g.fillRect (footerArea);
    g.setColour (textDim);
    g.setFont (studioFont (12.0f));
    g.drawText ("Stage 2" + dot + "Step 9 of 22" + dot + "Audio recording",
                footerArea.reduced (24, 0), juce::Justification::centredLeft);
    g.drawText ("v" + juce::String (JUCE_APPLICATION_VERSION_STRING),
                footerArea.reduced (24, 0), juce::Justification::centredRight);
}

void MainComponent::paintEnginePage (juce::Graphics& g)
{
    using namespace StudioColours;

    // ---- Audio engine card --------------------------------------------------
    drawCard (g, audioCard, "Audio Engine");
    {
        auto rows = audioCard.reduced (20, 0).withTrimmedTop (56).withTrimmedBottom (16);

        if (auto* device = studio.getAudioDeviceManager().getCurrentAudioDevice())
        {
            const auto sr      = device->getCurrentSampleRate();
            const auto buffer  = device->getCurrentBufferSizeSamples();
            const auto latency = sr > 0 ? 1000.0 * (device->getInputLatencyInSamples()
                                                    + device->getOutputLatencyInSamples()
                                                    + 2 * buffer) / sr
                                        : 0.0;
            const auto cpu     = studio.getCpuUsage() * 100.0;

            drawRow (g, rows, "Status",      "Running", good);
            drawRow (g, rows, "Engine",      "Tracktion Engine", text);
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
        const auto devices = studio.getPhysicalMidiInputs();

        if (devices.empty())
        {
            drawRow (g, rows, "Keyboard", "None detected", warn);
            g.setColour (textDim);
            g.setFont (studioFont (13.5f));
            g.drawFittedText ("Connect the keyboard with a USB cable. It appears here within a few seconds.",
                              rows.removeFromTop (50), juce::Justification::centredLeft, 3);
        }
        else
        {
            for (const auto& d : devices)
                drawRow (g, rows, d.name, d.enabled ? "Connected" : "Switched off", d.enabled ? good : textDim);
        }

        const auto note = studio.getLastNote();
        drawRow (g, rows, "Last note played",
                 note >= 0 ? juce::MidiMessage::getMidiNoteName (note, true, true, 4)
                               + "   (velocity " + juce::String (studio.getLastVelocity()) + ")"
                           : juce::String ("Play a key..."),
                 note >= 0 ? gold : textDim);
        drawRow (g, rows, "Notes received", juce::String (studio.getMidiMessageCount()), text);
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

void MainComponent::resized()
{
    auto area = getLocalBounds();

    headerArea = area.removeFromTop (76);
    footerArea = area.removeFromBottom (30);

    auto headerInner = headerArea.reduced (24, 20);
    setupButton.setBounds (headerInner.removeFromRight (180));
    headerInner.removeFromRight (8);
    updatesButton.setBounds (headerInner.removeFromRight (100));
    headerInner.removeFromRight (10);
    soundsButton.setBounds (headerInner.removeFromRight (140));
    headerInner.removeFromLeft (320);
    for (auto* tab : { &libraryTab, &studioTab, &engineTab })
    {
        tab->setBounds (headerInner.removeFromLeft (tab == &engineTab ? 160 : 120));
        headerInner.removeFromLeft (8);
    }

    if (bannerVisible)
    {
        bannerArea = area.removeFromTop (44);
        auto b = bannerArea.reduced (24, 6);
        laterButton.setBounds (b.removeFromRight (90));
        b.removeFromRight (8);
        updateNowButton.setBounds (b.removeFromRight (130));
    }

    // Keyboard along the bottom: 52 white keys across the full width.
    auto keyboardArea = area.removeFromBottom (page == Page::engine ? 140 : 96).reduced (24, 10);
    keyboard.setBounds (keyboardArea);
    keyboard.setKeyWidth ((float) keyboardArea.getWidth() / 52.0f);

    libraryPage.setBounds (area);
    studioPage.setBounds (area);

    // Engine & Devices: three cards.
    auto body = area.reduced (24, 20);
    const int gap = 18;
    const int cardWidth = (body.getWidth() - 2 * gap) / 3;

    audioCard = body.removeFromLeft (cardWidth);
    body.removeFromLeft (gap);
    midiCard  = body.removeFromLeft (cardWidth);
    body.removeFromLeft (gap);
    levelCard = body;

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
