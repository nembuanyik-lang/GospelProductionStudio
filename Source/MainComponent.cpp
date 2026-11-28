#include "Help.h"
#include "InstrumentPanel.h"
#include "MainComponent.h"
#include "ChordTrack.h"
#include "BinaryData.h"
#include "LatencyCheck.h"
#include "Dialogs.h"

//==============================================================================
MainComponent::MainComponent()
{
    logo = juce::Drawable::createFromImageData (BinaryData::logoicon_svg, BinaryData::logoicon_svgSize);

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

    pluginsButton.setWantsKeyboardFocus (false);
    pluginsButton.setTooltip ("Find the VST3 instruments and effects installed on this computer");
    pluginsButton.onClick = [this] { PluginHost::openManager (studio.getEngine()); };

    // Pages inside the window (they used to be separate windows that could hide behind)
    PluginHost::onOpenManagerRequest = [this] { openPanel ("Plugins", PluginHost::createManagerComponent (studio.getEngine())); };
    SoundLibrary::onOpenRequest      = [this] { openPanel ("Sound Library", SoundLibrary::createComponent()); };
    // A page that works on the song (Export) closes before the song does
    {
        auto previous = studio.onEditAboutToChange;
        studio.onEditAboutToChange = [this, previous] { panel.reset(); if (previous) previous(); };
    }
    studioPage.onShowPage            = [this] (const juce::String& t, std::unique_ptr<juce::Component> c) { openPanel (t, std::move (c)); };
    addAndMakeVisible (pluginsButton);

    // Learn: the step-by-step course
    learnButton.setWantsKeyboardFocus (false);
    learnButton.setTooltip ("Learn the app step by step, with lessons that show you where to click");
    learnButton.setColour (juce::TextButton::textColourOffId, StudioColours::gold);
    learnButton.onClick = [this] { showCoursePage(); };
    addAndMakeVisible (learnButton);

    // Names the Learn course uses to find these buttons
    libraryTab.setComponentID ("tab.library");
    studioTab.setComponentID ("tab.studio");
    engineTab.setComponentID ("tab.engine");
    setupButton.setComponentID ("btn.audioSetup");
    updatesButton.setComponentID ("btn.updates");
    soundsButton.setComponentID ("btn.sounds");
    pluginsButton.setComponentID ("btn.plugins");
    helpButton.setComponentID ("btn.help");
    learnButton.setComponentID ("btn.learn");
    keyboardButton.setComponentID ("btn.keyboard");

    helpButton.setWantsKeyboardFocus (false);
    helpButton.setTooltip ("Ask how to do something (F1)");
    helpButton.onClick = [this] { openPanel ("Help", Help::createComponent()); };
    addAndMakeVisible (helpButton);

    // The on-screen keyboard can be hidden to give small screens more room
    keyboardShown = StudioEngine::getSetting ("showKeyboard", 1) != 0;
    keyboardButton.setWantsKeyboardFocus (false);
    keyboardButton.setTooltip ("Show or hide the on-screen keyboard (your MIDI keyboard always works)");
    keyboardButton.onClick = [this]
    {
        keyboardShown = ! keyboardShown;
        StudioEngine::setSetting ("showKeyboard", keyboardShown ? 1 : 0);
        resized();
        repaint();
    };
    addAndMakeVisible (keyboardButton);

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
    delayButton.onClick = [this] { openPanel ("Recording delay", LatencyCheck::createPanel (studio)); };
    delayButton.setTooltip ("Measure how late recordings land (with a cable from an output to an input) and correct it");
    toneButton.setClickingTogglesState (true);
    toneButton.onClick = [this] { studio.setTestTone (toneButton.getToggleState()); };
    panicButton.onClick = [this] { allNotesOff(); };
    for (juce::Component* c : { (juce::Component*) &toneButton, (juce::Component*) &panicButton, (juce::Component*) &delayButton,
                                (juce::Component*) &inputMeter, (juce::Component*) &outputMeter })
        addChildComponent (c);

    setWantsKeyboardFocus (true);
    showPage (Page::library);
    setSize (1280, 800);
    startTimerHz (30);

    if (juce::SystemStats::getEnvironmentVariable ("GPS_TEST_LEARN_AUDIT", {}).isNotEmpty())
        runLearnAudit();
    if (auto showcase = juce::SystemStats::getEnvironmentVariable ("GPS_TEST_SHOWCASE", {}); showcase.isNotEmpty())
    {
        juce::Component::SafePointer<MainComponent> self (this);
        juce::Timer::callAfterDelay (1500, [self, showcase] { if (self != nullptr) self->buildShowcase (juce::File (showcase)); });
    }

    // The app did not close properly last time (power cut, crash): offer to open the song again (Step 43)
    if (auto previous = SongSafety::Session::previousEndedUnexpectedly())
    {
        juce::Component::SafePointer<MainComponent> self (this);
        const auto p = *previous;
        juce::Timer::callAfterDelay (900, [self, p] { if (self != nullptr) self->offerRecovery (p); });
    }
}

void MainComponent::offerRecovery (const SongSafety::Session::Previous& p)
{
    SongSafety::Session::forgetPrevious();
    const auto* song = p.songId.isNotEmpty() ? songLibrary.find (p.songId) : nullptr;
    const auto cutOff = SongSafety::Session::findCutOffRecordings (p, nullptr);

    juce::String message ("Gospel Production Studio did not close properly last time (for example a power cut).\n\n");
    if (song == nullptr)
    {
        message << "No song was open, so nothing was lost.";
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Welcome back", message, "OK");
        return;
    }
    message << "Your song \"" << song->title << "\" was saved automatically every few seconds, so at most the last few seconds of work are missing.";
    if (! cutOff.isEmpty())
        message << "\n\nA recording you were making was found (" << cutOff.size() << (cutOff.size() == 1 ? " file" : " files")
                << "). Opening the song puts it on a new track called \"Rescued recording\", where it was recorded.";
    message << "\n\nThe Versions button beside the song title also has copies from earlier, if you need them.";

    juce::Component::SafePointer<MainComponent> safe (this);
    const auto id = p.songId;
    juce::AlertWindow::showAsync (juce::MessageBoxOptions{}
        .withIconType (juce::MessageBoxIconType::InfoIcon)
        .withTitle ("Welcome back")
        .withMessage (message)
        .withButton ("Open the song", 1)
        .withButton ("Not now", 0),
        [safe, id, p] (int ok)
        {
            if (ok != 1 || safe == nullptr)
                return;
            safe->libraryPage.onOpenInStudio (id);
            const auto n = safe->studioPage.rescueRecordings (p);
            if (n > 0)
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Recording rescued",
                    juce::String (n) + (n == 1 ? " recording was" : " recordings were")
                        + " put on a new track called \"Rescued recording\" at the place where recording started. Listen to it, then move or tidy it as you like.",
                    "OK");
        });
}

MainComponent::~MainComponent()
{
    stopTimer();
    coach.reset();
    PluginHost::onOpenManagerRequest = nullptr;
    SoundLibrary::onOpenRequest = nullptr;
    panel.reset();
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
    panel.reset();

    page = newPage;

    libraryTab.setToggleState (page == Page::library, juce::dontSendNotification);
    studioTab.setToggleState (page == Page::studio, juce::dontSendNotification);
    engineTab.setToggleState (page == Page::engine, juce::dontSendNotification);

    libraryPage.setVisible (page == Page::library);
    studioPage.setVisible (page == Page::studio);

    const auto isEngine = page == Page::engine;
    for (juce::Component* c : { (juce::Component*) &toneButton, (juce::Component*) &panicButton, (juce::Component*) &delayButton,
                                (juce::Component*) &inputMeter, (juce::Component*) &outputMeter })
        c->setVisible (isEngine);

    resized();
    repaint();

    if (page == Page::studio)
        studioPage.grabKeyboardFocus();
}

void MainComponent::prepareToClose()
{
    if (studio.isRecording() || studio.isPlaying())
        studio.stop();                 // a take being recorded is finished and placed in the song
    studio.save();
    studio.songClosing();              // a version "when the song was closed" (Step 44)
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::F1Key)
    {
        if (panel == nullptr) openPanel ("Help", Help::createComponent());
        return true;
    }

    if (panel != nullptr)
    {
        if (key == juce::KeyPress::escapeKey) { closePanel(); return true; }
        return false;
    }

    if (page == Page::studio)
        return studioPage.handleShortcut (key);

    return false;
}

void MainComponent::showAudioSettings()
{
    // MIDI keyboards are handled by the engine automatically, so only audio is shown here.
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (studio.getAudioDeviceManager(),
                                                                          0, 8,     // inputs (microphones, guitar)
                                                                          2, 2,     // outputs
                                                                          false,    // MIDI inputs
                                                                          false,    // MIDI output
                                                                          false,    // inputs listed one by one
                                                                          false);   // show advanced
    openPanel ("Audio & MIDI Setup", std::move (selector));
}

void MainComponent::openPanel (const juce::String& title, std::unique_ptr<juce::Component> content)
{
    if (content == nullptr)
        return;

    panel = std::make_unique<PanelPage> (title, std::move (content), [this] { closePanel(); });
    addAndMakeVisible (*panel);
    panel->setBounds (pageArea);
    panel->toFront (true);
    panel->grabKeyboardFocus();
    if (coach != nullptr)
        Learn::placeCoach (*coach);        // the lesson card stays on top of Help, Audio Setup...
}

//==============================================================================
//  Learn (the course)
//==============================================================================
juce::String MainComponent::currentPage() const
{
    switch (page)
    {
        case Page::library: return "library";
        case Page::studio:  return "studio";
        case Page::engine:  return "engine";
    }
    return {};
}

void MainComponent::showPage (const juce::String& name)
{
    const auto wanted = name == "studio" ? Page::studio : (name == "engine" ? Page::engine : Page::library);
    if (wanted != page || panel != nullptr)
        showPage (wanted);
}

void MainComponent::selectSongInLibrary (const juce::String& id)
{
    libraryPage.selectSong (id);
}

void MainComponent::openSongInStudio (const juce::String& id)
{
    libraryPage.flush();
    libraryPage.selectSong (id);
    libraryPage.onOpenInStudio (id);
}

void MainComponent::showCoursePage()
{
    coach.reset();
    juce::Component::SafePointer<MainComponent> safe (this);
    openPanel ("Learn", Learn::createCoursePage (*this, [safe] (const juce::String& id)
    {
        if (safe != nullptr)
            juce::MessageManager::callAsync ([safe, id] { if (safe != nullptr) safe->startLesson (id); });
    }));
}

void MainComponent::runLearnAudit()
{
    // Walk every step of every lesson, open its page and look for the thing "Show me" makes glow
    struct Item { juce::String lesson; int step; juce::String target, where, opener; };
    auto items = std::make_shared<std::vector<Item>>();
    for (auto& mod : Learn::course())
        for (auto& l : mod.lessons)
            for (size_t i = 0; i < l.steps.size(); ++i)
                items->push_back ({ l.id, (int) i, l.steps[i].target, l.steps[i].where, l.steps[i].opener });

    auto out = std::make_shared<juce::String>();
    auto index = std::make_shared<size_t> (0);
    auto lastLesson = std::make_shared<juce::String>();
    juce::Component::SafePointer<MainComponent> safe (this);
    auto step = std::make_shared<std::function<void()>>();
    *step = [safe, items, out, index, lastLesson, step]
    {
        if (safe == nullptr) return;
        if (*index >= items->size())
        {
            const auto file = juce::File (juce::SystemStats::getEnvironmentVariable ("GPS_TEST_LEARN_AUDIT", {}));
            file.replaceWithText (*out);
            std::cout << *out << std::flush;
            safe->coach.reset();
            juce::JUCEApplicationBase::quit();
            return;
        }
        const auto& it = (*items)[*index];
        const bool newLesson = it.lesson != *lastLesson;
        if (newLesson)
        {
            safe->startLesson (it.lesson);
            *lastLesson = it.lesson;
        }
        if (safe->coach != nullptr)
            Learn::coachGoTo (*safe->coach, it.step);
        const auto* lesson = Learn::findLesson (it.lesson);
        const auto* st = &lesson->steps[(size_t) it.step];
        juce::Timer::callAfterDelay (newLesson ? 900 : 0, [safe, st, items, out, index, step] {
        if (safe == nullptr) return;
        Learn::reveal (*safe, *st, [safe, items, out, index, step] (juce::Component* c)
        {
            if (safe == nullptr) return;
            const auto& item = (*items)[*index];
            juce::String line ("lesson " + item.lesson + " step " + juce::String (item.step + 1) + ": ");
            if (item.target.isEmpty())
                line << "no target";
            else if (c != nullptr)
                line << "found " << item.target << " at " << c->getScreenBounds().toString();
            else
                line << "MISSING " << item.target;
            *out << line << "\n";
            ++*index;
            juce::ModalComponentManager::getInstance()->cancelAllModalComponents();       // the band window
            juce::Timer::callAfterDelay (item.opener.isNotEmpty() ? 700 : 150, [step] { (*step)(); });
        });
        });
    };
    juce::Timer::callAfterDelay (1500, [step] { (*step)(); });
}

void MainComponent::buildShowcase (const juce::File& vocal)
{
    const juce::String title ("You Are Faithful");
    juce::String id;
    for (auto& s : songLibrary.getSongs())
        if (s.title == title) id = s.id;
    if (id.isEmpty())
    {
        id = songLibrary.createSong();
        Song s = *songLibrary.find (id);
        s.title = title;
        s.key = "G major";
        s.tempo = 76;
        s.theme = "Worship";
        s.status = "Arranged";
        s.scripture = "Lamentations 3:22-23";
        s.notes = "Lead: Minister Kemamah. Choir joins on the chorus; lift the last chorus.";
        s.sections.clear();
        auto add = [&s] (const char* type, const char* words) { SongSection x; x.type = type; x.lyrics = words; s.sections.push_back (x); };
        add ("Intro", "");
        add ("Verse", "Morning by morning new mercies I see\nAll I have needed Your hand has supplied\nIn every season You have been with me\nFaithful and true, You are always my guide");
        add ("Chorus", "You are faithful, You are faithful\nYesterday, today and forevermore\nYou are faithful, Lord, You are faithful\nGreat is Your love, I will trust in You");
        add ("Verse", "When I was weary You lifted my head\nWhen I was lonely You stayed by my side\nNever forsaken, by Your Spirit led\nIn Your great mercy I always abide");
        add ("Chorus", "You are faithful, You are faithful\nYesterday, today and forevermore\nYou are faithful, Lord, You are faithful\nGreat is Your love, I will trust in You");
        add ("Vamp", "Faithful, faithful, faithful God\nFaithful, faithful, You never fail");
        songLibrary.update (s);
    }
    openSongInStudio (id);

    juce::Component::SafePointer<MainComponent> safe (this);
    juce::Timer::callAfterDelay (1500, [safe, vocal]
    {
        if (safe == nullptr) return;
        auto* edit = safe->studio.getEdit();
        if (edit == nullptr || ! ChordTrack::getEvents (*edit).empty()) return;

        auto chord = [] (int root, const char* q) { Theory::Chord c; c.root = root; c.quality = q; return c; };
        std::vector<Theory::TimedChord> chords;
        auto bars = [&chords] (std::initializer_list<Theory::Chord> list) { for (auto& c : list) chords.push_back ({ c, 4.0 }); };
        const auto I = chord (0, ""), IV = chord (5, ""), V = chord (7, ""), vi = chord (9, "m"), iii = chord (4, "m"), ii = chord (2, "m");
        bars ({ I, IV, I, V });                                                  // Intro
        for (int verse = 0; verse < 2; ++verse)
        {
            bars ({ I, IV, vi, V, I, IV, V, V });                                // Verse
            bars ({ IV, V, iii, vi, ii, V, I, I });                              // Chorus
        }
        bars ({ IV, V, iii, vi, IV, V, I, I });                                  // Vamp
        ChordTrack::insert (*edit, chords, 0.0);

        if (vocal.existsAsFile())
        {
            te::AudioTrack* t = nullptr;
            for (auto* track : te::getAudioTracks (*edit))
                if (track->getName() == "Lead Vocal") t = track;
            if (t == nullptr)
                t = safe->studio.addTrack (false, "Lead Vocal", juce::Colour (0xffd4a843), nullptr);
            if (t != nullptr)
                safe->studio.importAudio (*t, vocal, edit->tempoSequence.toTime (te::BeatPosition::fromBeats (16.0)).inSeconds());
        }
        safe->studio.save();
        safe->studioPage.openSong (safe->studio.getOpenSongId());     // shows the new chords and track
    });
}

void MainComponent::startLesson (const juce::String& lessonId)
{
    panel.reset();
    coach.reset();
    juce::Component::SafePointer<MainComponent> safe (this);
    coach = Learn::createCoach (*this, lessonId, [safe] { if (safe != nullptr) safe->coach.reset(); });
    if (coach != nullptr)
    {
        addAndMakeVisible (*coach);
        Learn::placeCoach (*coach);
    }
}

void MainComponent::closePanel()
{
    // Later, so the Back button is not deleted while it is still being clicked
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe == nullptr)
            return;
        safe->panel.reset();
        if (safe->page == Page::studio)
            safe->studioPage.grabKeyboardFocus();
    });
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

    // The logo and the name: GOSPEL / PRODUCTION STUDIO
    auto titleArea = headerBox.reduced (compactLayout ? 14 : 22, 0);
    const int mark = compactLayout ? 34 : 44;
    auto markArea = titleArea.removeFromLeft (mark).withSizeKeepingCentre (mark, mark);
    if (logo != nullptr)
        logo->drawWithin (g, markArea.toFloat(), juce::RectanglePlacement::centred, 1.0f);
    titleArea.removeFromLeft (compactLayout ? 9 : 12);

    const auto nameHeight = compactLayout ? 19.0f : 25.0f;
    auto nameArea = titleArea.withSizeKeepingCentre (titleArea.getWidth(), juce::roundToInt (nameHeight + (compactLayout ? 14.0f : 18.0f)));
    g.setColour (text);
    g.setFont (studioFont (nameHeight, true).withExtraKerningFactor (0.03f));
    g.drawText ("GOSPEL", nameArea.removeFromTop (juce::roundToInt (nameHeight)), juce::Justification::bottomLeft);
    g.setColour (gold);
    g.setFont (studioFont (compactLayout ? 8.5f : 10.5f, true).withExtraKerningFactor (compactLayout ? 0.24f : 0.32f));
    g.drawText ("PRODUCTION STUDIO", nameArea.withTrimmedTop (compactLayout ? 2 : 3), juce::Justification::topLeft);

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
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    g.drawText ("Press F1 for help" + dot + "Space to play or stop",
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
            const auto correction = studio.getRecordingCorrectionMs();
            drawRow (g, rows, "Recording delay fix", std::abs (correction) < 0.05 ? juce::String ("not measured")
                                                                                 : juce::String (correction, 1) + " ms",
                     std::abs (correction) < 0.05 ? textDim : good);
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

    // Small laptop screens (for example 1366 x 768) get a compact layout
    compactLayout = getHeight() < 800 || getWidth() < 1300;

    headerArea = area.removeFromTop (compactLayout ? 50 : 76);
    footerArea = area.removeFromBottom (compactLayout ? 24 : 30);

    keyboardButton.setButtonText (keyboardShown ? "Hide keyboard" : "Show keyboard");
    keyboardButton.setBounds (footerArea.reduced (compactLayout ? 16 : 24, 2).removeFromRight (120).translated (-70, 0));

    auto headerInner = headerArea.reduced (compactLayout ? 16 : 24, compactLayout ? 9 : 20);
    setupButton.setButtonText (compactLayout ? "Audio Setup" : "Audio & MIDI Setup");
    soundsButton.setButtonText (compactLayout ? "Sounds" : "Sound Library");
    setupButton.setBounds (headerInner.removeFromRight (compactLayout ? 120 : 180));
    headerInner.removeFromRight (8);
    updatesButton.setBounds (headerInner.removeFromRight (compactLayout ? 90 : 100));
    headerInner.removeFromRight (8);
    soundsButton.setBounds (headerInner.removeFromRight (compactLayout ? 90 : 140));
    headerInner.removeFromRight (8);
    pluginsButton.setBounds (headerInner.removeFromRight (compactLayout ? 80 : 100));
    headerInner.removeFromRight (8);
    helpButton.setBounds (headerInner.removeFromRight (compactLayout ? 64 : 80));
    headerInner.removeFromRight (8);
    learnButton.setBounds (headerInner.removeFromRight (compactLayout ? 70 : 84));
    headerInner.removeFromLeft (compactLayout ? 170 : 320);
    for (auto* tab : { &libraryTab, &studioTab, &engineTab })
    {
        tab->setBounds (headerInner.removeFromLeft (tab == &engineTab ? (compactLayout ? 150 : 160) : (compactLayout ? 104 : 120)));
        headerInner.removeFromLeft (compactLayout ? 6 : 8);
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
    keyboard.setVisible (keyboardShown || page == Page::engine);
    if (keyboard.isVisible())
    {
        const auto kh = page == Page::engine ? (compactLayout ? 100 : 140) : (compactLayout ? 66 : 96);
        auto keyboardArea = area.removeFromBottom (kh).reduced (compactLayout ? 16 : 24, compactLayout ? 6 : 10);
        keyboard.setBounds (keyboardArea);
        keyboard.setKeyWidth ((float) keyboardArea.getWidth() / 52.0f);
    }

    libraryPage.setBounds (area);
    studioPage.setBounds (area);
    pageArea = area;
    if (panel != nullptr)
        panel->setBounds (area);
    if (coach != nullptr)
        Learn::placeCoach (*coach);

    // Engine & Devices: three cards.
    auto body = area.reduced (24, 20);
    const int gap = 18;
    const int cardWidth = (body.getWidth() - 2 * gap) / 3;

    audioCard = body.removeFromLeft (cardWidth);
    delayButton.setBounds (audioCard.reduced (20, 0).removeFromBottom (54).withTrimmedBottom (16));
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
