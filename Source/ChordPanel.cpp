#include "ChordPanel.h"
#include "Dialogs.h"

namespace
{
    juce::File savedFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("GospelProductionStudio").getChildFile ("MyProgressions.json");
    }

    const juce::StringArray lengthNames { "Length: 1 beat", "Length: 2 beats", "Length: 1 bar", "Length: 2 bars" };
}

//==============================================================================
//  Chip
//==============================================================================
void ChordChip::paintButton (juce::Graphics& g, bool over, bool down)
{
    using namespace StudioColours;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto current = panel.isCurrent (chord);

    auto fill = isSuggestion ? gold.withAlpha (0.16f) : card.brighter (0.07f);
    if (current) fill = gold.withAlpha (0.35f);
    if (over)    fill = fill.brighter (0.1f);
    if (down)    fill = fill.darker (0.1f);

    g.setColour (fill);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (current ? gold : (isSuggestion ? gold.withAlpha (0.7f) : cardBorder.brighter (0.2f)));
    g.drawRoundedRectangle (b, 6.0f, current ? 1.8f : 1.0f);

    const auto key = panel.getKey();
    const auto display = panel.getDisplay();
    auto area = b.reduced (6.0f, 3.0f).toNearestInt();

    g.setColour (StudioColours::text);
    if (display == Theory::Display::both)
    {
        g.setFont (studioFont (13.5f, true));
        g.drawText (Theory::numberLabel (chord), area.removeFromTop (area.getHeight() / 2 + 2), juce::Justification::centredBottom, true);
        g.setColour (StudioColours::textDim);
        g.setFont (studioFont (11.5f));
        g.drawText (Theory::nameLabel (chord, key), area, juce::Justification::centredTop, true);
    }
    else
    {
        g.setFont (studioFont (14.0f, true));
        g.drawText (Theory::label (chord, key, display), area, juce::Justification::centred, true);
    }
}

void ChordChip::clicked (const juce::ModifierKeys& mods)
{
    panel.chipClicked (chord, mods.isShiftDown());
}

void ChordChip::mouseDoubleClick (const juce::MouseEvent&)
{
    panel.addChordAtPlayhead (chord);
}

//==============================================================================
//  List models
//==============================================================================
struct ChordPanel::ProgressionList final : public juce::ListBoxModel
{
    explicit ProgressionList (ChordPanel& p) : panel (p) {}

    int getNumRows() override   { return (int) panel.visibleProgressions.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, getNumRows()))
            return;

        const auto& p = Theory::library()[(size_t) panel.visibleProgressions[(size_t) row]];
        if (selected)
        {
            g.setColour (StudioColours::gold.withAlpha (0.18f));
            g.fillRoundedRectangle (0.0f, 1.0f, (float) w, (float) h - 2.0f, 5.0f);
        }

        g.setColour (StudioColours::text);
        g.setFont (studioFont (13.5f, true));
        g.drawText (p.name, 10, 4, w - 20, 18, juce::Justification::centredLeft, true);

        // Preview in the current display, simple level
        const auto chords = Theory::parse (p.text, panel.timeline.getBeatsPerBar(), panel.getKey().minor);
        juce::StringArray parts;
        for (const auto& c : chords)
            parts.add (panel.getDisplay() == Theory::Display::names ? Theory::nameLabel (c.chord, panel.getKey())
                                                                   : Theory::numberLabel (c.chord));
        g.setColour (StudioColours::textDim);
        g.setFont (studioFont (12.0f));
        g.drawText (parts.joinIntoString ("  "), 10, 22, w - 20, 16, juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override
    {
        panel.insertProgression (panel.selectedLibraryChords());
    }

    ChordPanel& panel;
};

struct ChordPanel::MyList final : public juce::ListBoxModel
{
    explicit MyList (ChordPanel& p) : panel (p) {}

    int getNumRows() override   { return (int) panel.saved.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, getNumRows()))
            return;

        const auto& s = panel.saved[(size_t) row];
        if (selected)
        {
            g.setColour (StudioColours::gold.withAlpha (0.18f));
            g.fillRoundedRectangle (0.0f, 1.0f, (float) w, (float) h - 2.0f, 5.0f);
        }

        g.setColour (StudioColours::text);
        g.setFont (studioFont (13.5f, true));
        g.drawText (s.name, 10, 4, w - 20, 18, juce::Justification::centredLeft, true);

        juce::StringArray parts;
        for (const auto& c : Theory::parse (s.text, panel.timeline.getBeatsPerBar(), panel.getKey().minor))
            parts.add (Theory::label (c.chord, panel.getKey(), panel.getDisplay() == Theory::Display::both
                                                                  ? Theory::Display::numbers : panel.getDisplay()));
        g.setColour (StudioColours::textDim);
        g.setFont (studioFont (12.0f));
        g.drawText (parts.joinIntoString ("  "), 10, 22, w - 20, 16, juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow (row, getNumRows()))
            panel.insertProgression (Theory::parse (panel.saved[(size_t) row].text,
                                                    panel.timeline.getBeatsPerBar(), panel.getKey().minor));
    }

    ChordPanel& panel;
};

//==============================================================================
//  Panel
//==============================================================================
ChordPanel::ChordPanel (StudioEngine& e, TimelineView& t)
    : engine (e), timeline (t)
{
    auto noFocus = [] (juce::Component& c) { c.setWantsKeyboardFocus (false); };

    // Display
    displayBox.addItemList ({ "Numbers and names", "Numbers only", "Names only" }, 1);
    displayBox.setSelectedItemIndex (StudioEngine::getSetting ("chordDisplay", 0), juce::dontSendNotification);
    displayBox.setTooltip ("How chords are shown everywhere in the Studio");
    displayBox.onChange = [this]
    {
        const auto i = displayBox.getSelectedItemIndex();
        StudioEngine::setSetting ("chordDisplay", i);
        timeline.chordDisplay = i == 1 ? Theory::Display::numbers : i == 2 ? Theory::Display::names : Theory::Display::both;
        timeline.refreshAll();
        refresh();
    };
    addAndMakeVisible (displayBox);
    timeline.chordDisplay = displayBox.getSelectedItemIndex() == 1 ? Theory::Display::numbers
                          : displayBox.getSelectedItemIndex() == 2 ? Theory::Display::names : Theory::Display::both;

    // Tabs
    int index = 0;
    for (auto* b : { &paletteTab, &progressionsTab, &mineTab })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (2001);
        noFocus (*b);
        b->onClick = [this, index] { showTab (index); };
        addAndMakeVisible (b);
        ++index;
    }

    // Palette
    paletteViewport.setComponentID ("chords.palette");
    paletteViewport.setViewedComponent (&paletteContent, false);
    paletteViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (paletteViewport);

    for (int i = 0; i < 12; ++i)
        degreeBox.addItem (Theory::degreeLabel (i, i == 6), i + 1);
    degreeBox.setSelectedItemIndex (0, juce::dontSendNotification);

    for (size_t i = 0; i < Theory::qualities().size(); ++i)
    {
        const auto& q = Theory::qualities()[i];
        const juce::String suffix = juce::String::fromUTF8 (q.suffix);
        qualityBox.addItem (suffix.isEmpty() ? juce::String ("major") : suffix, (int) i + 1);
    }
    qualityBox.setSelectedItemIndex (0, juce::dontSendNotification);

    bassBox.addItem ("no slash", 1);
    for (int i = 0; i < 12; ++i)
        bassBox.addItem ("/" + Theory::degreeLabel (i, false), i + 2);
    bassBox.setSelectedItemIndex (0, juce::dontSendNotification);

    degreeBox.setTooltip ("Scale degree of the chord root");
    qualityBox.setTooltip ("Chord type");
    bassBox.setTooltip ("Slash bass note (inversion or pedal)");
    for (auto* box : { &degreeBox, &qualityBox, &bassBox })
    {
        box->onChange = [this] { updateBuilderChord(); };
        addAndMakeVisible (box);
    }

    hearBuiltButton.onClick = [this] { updateBuilderChord(); chipClicked (current, false); };
    addBuiltButton.onClick  = [this] { updateBuilderChord(); addChordAtPlayhead (current); };
    for (auto* b : { &hearBuiltButton, &addBuiltButton }) { noFocus (*b); addAndMakeVisible (b); }

    // Progressions
    categoryBox.addItem ("All categories", 1);
    categoryBox.addItemList (Theory::categories(), 2);
    categoryBox.setSelectedItemIndex (0, juce::dontSendNotification);
    categoryBox.onChange = [this]
    {
        visibleProgressions.clear();
        const auto cat = categoryBox.getSelectedItemIndex() > 0 ? categoryBox.getText() : juce::String();
        for (size_t i = 0; i < Theory::library().size(); ++i)
            if (cat.isEmpty() || Theory::library()[i].category == cat)
                visibleProgressions.push_back ((int) i);
        progressionList.updateContent();
        progressionList.selectRow (0);
        progressionList.repaint();
    };
    addChildComponent (categoryBox);

    levelBox.addItemList (Theory::levelNames(), 1);
    levelBox.setSelectedItemIndex (StudioEngine::getSetting ("chordLevel", 1), juce::dontSendNotification);
    levelBox.onChange = [this] { StudioEngine::setSetting ("chordLevel", levelBox.getSelectedItemIndex()); };
    addChildComponent (levelBox);

    progressionModel = std::make_unique<ProgressionList> (*this);
    progressionList.setModel (progressionModel.get());
    progressionList.setRowHeight (44);
    progressionList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addChildComponent (progressionList);

    hearProgressionButton.onClick   = [this] { hearProgression (selectedLibraryChords()); };
    insertProgressionButton.onClick = [this] { insertProgression (selectedLibraryChords()); };
    insertProgressionButton.setColour (juce::TextButton::buttonColourId, StudioColours::gold);
    insertProgressionButton.setColour (juce::TextButton::textColourOffId, StudioColours::header);
    for (auto* b : { &hearProgressionButton, &insertProgressionButton }) { noFocus (*b); addChildComponent (b); }

    // My Progressions
    myModel = std::make_unique<MyList> (*this);
    myList.setModel (myModel.get());
    myList.setRowHeight (44);
    myList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addChildComponent (myList);

    saveMineButton.onClick   = [this] { saveSelectionAsProgression(); };
    insertMineButton.onClick = [this]
    {
        const auto row = myList.getSelectedRow();
        if (juce::isPositiveAndBelow (row, (int) saved.size()))
            insertProgression (Theory::parse (saved[(size_t) row].text, timeline.getBeatsPerBar(), getKey().minor));
    };
    deleteMineButton.onClick = [this]
    {
        const auto row = myList.getSelectedRow();
        if (! juce::isPositiveAndBelow (row, (int) saved.size()))
            return;
        juce::Component::SafePointer<ChordPanel> safe (this);
        Dialogs::confirm (this, "Delete progression", "Delete \"" + saved[(size_t) row].name + "\" from My Progressions?",
                          "Delete", "Keep", [safe, row]
                          {
                              if (safe == nullptr) return;
                              safe->saved.erase (safe->saved.begin() + row);
                              safe->storeSaved();
                              safe->myList.updateContent();
                              safe->myList.repaint();
                          });
    };
    for (auto* b : { &saveMineButton, &insertMineButton, &deleteMineButton }) { noFocus (*b); addChildComponent (b); }

    // Shared
    lengthBox.setComponentID ("chords.length");
    lengthBox.addItemList (lengthNames, 1);
    lengthBox.setSelectedItemIndex (StudioEngine::getSetting ("chordLength", 2), juce::dontSendNotification);
    lengthBox.onChange = [this] { StudioEngine::setSetting ("chordLength", lengthBox.getSelectedItemIndex()); };

    for (int i = 0; i < Theory::voicingNames().size(); ++i)
        voicingBox.addItem ("RH: " + Theory::voicingNames()[i], i + 1);
    voicingBox.setTooltip ("Right-hand voicing used for hearing chords and for Chords to notes");
    lengthBox.setTooltip ("How long a chord lasts when you add it from the palette");
    voicingBox.setSelectedItemIndex (StudioEngine::getSetting ("chordVoicing", 2), juce::dontSendNotification);
    voicingBox.onChange = [this] { StudioEngine::setSetting ("chordVoicing", voicingBox.getSelectedItemIndex()); };

    for (int i = 0; i < Theory::leftHandNames().size(); ++i)
        leftHandBox.addItem ("LH: " + Theory::leftHandNames()[i], i + 1);
    leftHandBox.setTooltip ("Left-hand (bass) pattern");
    leftHandBox.setSelectedItemIndex (StudioEngine::getSetting ("chordLeftHand", 0), juce::dontSendNotification);
    leftHandBox.onChange = [this] { StudioEngine::setSetting ("chordLeftHand", leftHandBox.getSelectedItemIndex()); };

    anticipateToggle.setToggleState (StudioEngine::getSetting ("chordAnticipate", 0) != 0, juce::dontSendNotification);
    anticipateToggle.onClick = [this] { StudioEngine::setSetting ("chordAnticipate", anticipateToggle.getToggleState() ? 1 : 0); };
    noFocus (anticipateToggle);

    notesButton.setTooltip ("Write the Chord Track as notes on the Keys and Bass tracks");
    notesButton.onClick = [this] { chordsToNotes(); };
    noFocus (notesButton);

    for (juce::Component* c : { (juce::Component*) &lengthBox, (juce::Component*) &voicingBox,
                                (juce::Component*) &leftHandBox, (juce::Component*) &anticipateToggle,
                                (juce::Component*) &notesButton })
        addAndMakeVisible (c);

    loadSaved();
    categoryBox.onChange();
    paletteTab.setToggleState (true, juce::dontSendNotification);
    rebuildPalette();
    showTab (0);
    startTimerHz (4);
}

ChordPanel::~ChordPanel()
{
    alive->store (false);
}

//==============================================================================
void ChordPanel::showTab (int index)
{
    tab = index;
    const auto p = tab == 0, g = tab == 1, m = tab == 2;

    for (juce::Component* c : { (juce::Component*) &paletteViewport, (juce::Component*) &degreeBox,
                                (juce::Component*) &qualityBox, (juce::Component*) &bassBox,
                                (juce::Component*) &hearBuiltButton, (juce::Component*) &addBuiltButton })
        c->setVisible (p);

    for (juce::Component* c : { (juce::Component*) &categoryBox, (juce::Component*) &levelBox,
                                (juce::Component*) &progressionList, (juce::Component*) &hearProgressionButton,
                                (juce::Component*) &insertProgressionButton })
        c->setVisible (g);

    for (juce::Component* c : { (juce::Component*) &myList, (juce::Component*) &saveMineButton,
                                (juce::Component*) &insertMineButton, (juce::Component*) &deleteMineButton })
        c->setVisible (m);

    paletteTab.setToggleState (p, juce::dontSendNotification);
    progressionsTab.setToggleState (g, juce::dontSendNotification);
    mineTab.setToggleState (m, juce::dontSendNotification);

    resized();
    repaint();
}

void ChordPanel::refresh()
{
    if (hasCurrent && ! suggestions.empty())
        suggestions = Theory::suggestionsAfter (current, getKey());
    rebuildPalette();
    progressionList.repaint();
    myList.repaint();
    repaint();
}

void ChordPanel::showSuggestionsFor (const Theory::Chord* chord)
{
    if (chord != nullptr)
    {
        current = *chord;
        hasCurrent = true;
        suggestions = Theory::suggestionsAfter (*chord, getKey());
    }
    else
    {
        suggestions.clear();
    }
    schedulePaletteRebuild();
}

void ChordPanel::schedulePaletteRebuild()
{
    // Chips may be in the middle of their own click when this is asked for.
    juce::Component::SafePointer<ChordPanel> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->rebuildPalette(); });
}

void ChordPanel::timerCallback()
{
    // Follow the chord under the playhead for the "Next" suggestions.
    auto* ed = timeline.getEdit();
    if (ed == nullptr || ! timeline.selectedChords.empty())
        return;

    const auto events = ChordTrack::getEvents (*ed);
    const auto index = ChordTrack::indexAt (events, timeline.getPlayheadBeat() - 0.001);
    if (index != lastPlayheadIndex)
    {
        lastPlayheadIndex = index;
        if (index >= 0)
        {
            current = events[(size_t) index].chord;
            hasCurrent = true;
            suggestions = Theory::suggestionsAfter (current, getKey());
            schedulePaletteRebuild();
        }
    }
}

//==============================================================================
void ChordPanel::rebuildPalette()
{
    chips.clear();
    groupTitles.clear();

    const auto key = getKey();

    for (const auto& s : suggestions)
    {
        auto* chip = new ChordChip (*this, s, "next", true);
        paletteContent.addAndMakeVisible (chip);
        chips.add (chip);
    }

    for (const auto& group : Theory::paletteFor (key))
        for (const auto& pc : group.chords)
        {
            auto* chip = new ChordChip (*this, pc.chord, pc.hint, false);
            chip->setTooltip (pc.hint);
            paletteContent.addAndMakeVisible (chip);
            chips.add (chip);
        }

    layoutPalette();
}

void ChordPanel::layoutPalette()
{
    groupTitles.clear();
    const auto width = juce::jmax (200, paletteViewport.getMaximumVisibleWidth());
    const auto chipH = getDisplay() == Theory::Display::both ? 36 : 28;
    const int gap = 6;
    int y = 4, chipIndex = 0;

    auto layoutRow = [&] (const juce::String& title, int count, int minW)
    {
        groupTitles.push_back ({ juce::Rectangle<int> (2, y, width - 4, 18), title });
        y += 20;
        const auto perRow = juce::jmax (1, (width + gap) / (minW + gap));
        const auto w = (width - gap * (perRow - 1)) / perRow;
        for (int i = 0; i < count; ++i, ++chipIndex)
        {
            const auto col = i % perRow, row = i / perRow;
            chips[chipIndex]->setBounds (col * (w + gap), y + row * (chipH + gap), w, chipH);
        }
        y += ((count + perRow - 1) / perRow) * (chipH + gap) + 4;
    };

    // The "Next" row always keeps its place (one row), so the chips below never jump.
    {
        const auto perRow = juce::jmax (1, (width + gap) / (62 + gap));
        const auto shown = juce::jmin ((int) suggestions.size(), perRow);
        const auto title = suggestions.empty()
                             ? juce::String ("Next: click a chord to see strong moves after it")
                             : "Next after " + Theory::label (hasCurrent ? current : suggestions.front(), getKey(),
                                                              getDisplay() == Theory::Display::names ? Theory::Display::names
                                                                                                    : Theory::Display::numbers);
        groupTitles.push_back ({ juce::Rectangle<int> (2, y, width - 4, 18), title });
        y += 20;
        const auto w = (width - gap * (perRow - 1)) / perRow;
        for (int i = 0; i < (int) suggestions.size(); ++i, ++chipIndex)
        {
            chips[chipIndex]->setVisible (i < shown);
            chips[chipIndex]->setBounds (i * (w + gap), y, w, chipH);
        }
        y += chipH + gap + 4;
    }

    for (const auto& group : Theory::paletteFor (getKey()))
        layoutRow (group.name, (int) group.chords.size(), 62);

    paletteContent.setSize (width, y);
    paletteContent.repaint();
}

void ChordPanel::updateBuilderChord()
{
    current.root = degreeBox.getSelectedItemIndex();
    current.sharp = current.root == 6 || current.root == 1;
    current.quality = Theory::qualities()[(size_t) juce::jmax (0, qualityBox.getSelectedItemIndex())].id;
    current.bass = bassBox.getSelectedItemIndex() <= 0 ? -1 : bassBox.getSelectedItemIndex() - 1;
    hasCurrent = true;
    for (auto* c : chips) c->repaint();
}

double ChordPanel::chordLengthBeats() const
{
    switch (lengthBox.getSelectedItemIndex())
    {
        case 0:  return 1.0;
        case 1:  return 2.0;
        case 3:  return timeline.getBeatsPerBar() * 2.0;
        default: return timeline.getBeatsPerBar();
    }
}

//==============================================================================
void ChordPanel::chipClicked (const Theory::Chord& chord, bool arpeggio)
{
    current = chord;
    hasCurrent = true;
    timeline.auditionChord (chord, arpeggio);

    // Mirror the chord in the builder so it can be tweaked.
    degreeBox.setSelectedItemIndex (chord.root, juce::dontSendNotification);
    for (size_t i = 0; i < Theory::qualities().size(); ++i)
        if (chord.quality == Theory::qualities()[i].id)
            qualityBox.setSelectedItemIndex ((int) i, juce::dontSendNotification);
    bassBox.setSelectedItemIndex (chord.bass < 0 ? 0 : chord.bass + 1, juce::dontSendNotification);

    suggestions = Theory::suggestionsAfter (chord, getKey());
    schedulePaletteRebuild();
}

void ChordPanel::addChordAtPlayhead (const Theory::Chord& chord)
{
    auto* ed = timeline.getEdit();
    if (ed == nullptr)
        return;

    const auto start = timeline.snap (timeline.getPlayheadBeat());
    const auto length = chordLengthBeats();

    timeline.beginUndoStep();
    ChordTrack::clearRange (*ed, start, start + length);
    auto node = ChordTrack::add (*ed, chord, start, length);

    engine.setPositionSeconds (timeline.beatToTime (start + length).inSeconds());
    timeline.auditionChord (chord);
    timeline.selectedChords = { node };
    showSuggestionsFor (&chord);
    timeline.selectedChords.clear();
    timeline.refreshAll();
}

std::vector<Theory::TimedChord> ChordPanel::selectedLibraryChords() const
{
    const auto row = progressionList.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, (int) visibleProgressions.size()))
        return {};

    const auto& p = Theory::library()[(size_t) visibleProgressions[(size_t) row]];
    const auto chords = Theory::parse (p.text, timeline.getBeatsPerBar(), getKey().minor);
    return Theory::applyLevel (chords, (Theory::Level) levelBox.getSelectedItemIndex(), getKey().minor);
}

void ChordPanel::insertProgression (const std::vector<Theory::TimedChord>& chords)
{
    auto* ed = timeline.getEdit();
    if (ed == nullptr || chords.empty())
        return;

    // Progressions start on the bar at (or after) the playhead.
    const auto bpb = timeline.getBeatsPerBar();
    const auto start = std::ceil (timeline.getPlayheadBeat() / bpb - 1.0e-6) * bpb;

    timeline.beginUndoStep();
    const auto end = ChordTrack::insert (*ed, chords, start);
    engine.setPositionSeconds (timeline.beatToTime (end).inSeconds());
    timeline.refreshAll();
}

void ChordPanel::hearProgression (std::vector<Theory::TimedChord> chords)
{
    auto* ed = timeline.getEdit();
    if (ed == nullptr || chords.empty())
        return;

    const auto secondsPerBeat = ed->tempoSequence.toTime (te::BeatPosition::fromBeats (1.0)).inSeconds();
    const auto key = getKey();
    const auto voicing = (Theory::Voicing) voicingBox.getSelectedItemIndex();
    const auto lh = (Theory::LeftHand) leftHandBox.getSelectedItemIndex();

    auto flag = alive;
    auto* eng = &engine;
    int delayMs = 0, centre = 0;

    for (const auto& c : chords)
    {
        const auto notes = Theory::voice (c.chord, key, voicing, lh, centre);
        centre = Theory::rightHandCentre (notes);
        const auto durationMs = (int) (c.beats * secondsPerBeat * 1000.0);

        juce::Timer::callAfterDelay (juce::jmax (1, delayMs), [flag, eng, notes, durationMs]
        {
            if (flag->load())
                eng->audition (notes, false, juce::jmax (150, durationMs - 40));
        });
        delayMs += durationMs;
    }
}

void ChordPanel::chordsToNotes()
{
    auto* ed = timeline.getEdit();
    if (ed == nullptr)
        return;

    juce::PopupMenu menu;
    menu.addItem (1, "Keys and Bass");
    menu.addItem (2, "Keys only");
    menu.addItem (3, "Bass only");

    juce::Component::SafePointer<ChordPanel> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&notesButton), [safe] (int result)
    {
        if (safe == nullptr || result == 0 || safe->timeline.getEdit() == nullptr)
            return;

        ChordTrack::NotesOptions o;
        o.keys = result != 3;
        o.bass = result != 2;
        o.anticipate = safe->anticipateToggle.getToggleState();
        o.voicing = (Theory::Voicing) safe->voicingBox.getSelectedItemIndex();
        o.leftHand = (Theory::LeftHand) safe->leftHandBox.getSelectedItemIndex();

        safe->timeline.beginUndoStep();
        const auto report = ChordTrack::chordsToNotes (*safe->timeline.getEdit(), safe->getKey(), o);
        safe->timeline.refreshAll();
        Dialogs::message (safe.getComponent(), "Chords to notes",
                          report + "\nPress Play to hear them. If you change the chords or the key, run this again.",
                          juce::MessageBoxIconType::InfoIcon);
    });
}

//==============================================================================
void ChordPanel::saveSelectionAsProgression()
{
    const auto events = timeline.getSelectedChordEvents();
    if (events.empty())
    {
        Dialogs::message (this, "My Progressions",
                          "Select chords on the Chord Track first: click the first chord, then Shift-click the last one.",
                          juce::MessageBoxIconType::InfoIcon);
        return;
    }

    const auto text = Theory::toText (ChordTrack::toTimed (events));
    auto* window = new juce::AlertWindow ("Save progression", "Give this progression a name:", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", "My progression " + juce::String ((int) saved.size() + 1));
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<ChordPanel> safe (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window, text] (int result)
    {
        const auto name = window->getTextEditorContents ("name").trim();
        if (safe == nullptr || result != 1 || name.isEmpty())
            return;

        safe->saved.push_back ({ name, text });
        safe->storeSaved();
        safe->myList.updateContent();
        safe->showTab (2);
        safe->myList.selectRow ((int) safe->saved.size() - 1);
    }), true);
}

void ChordPanel::loadSaved()
{
    saved.clear();
    const auto json = juce::JSON::parse (savedFile().loadFileAsString());
    if (auto* arr = json.getArray())
        for (const auto& item : *arr)
            saved.push_back ({ item["name"].toString(), item["text"].toString() });
}

void ChordPanel::storeSaved()
{
    juce::Array<juce::var> arr;
    for (const auto& s : saved)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", s.name);
        o->setProperty ("text", s.text);
        arr.add (juce::var (o));
    }
    savedFile().getParentDirectory().createDirectory();
    savedFile().replaceWithText (juce::JSON::toString (juce::var (arr)));
}

//==============================================================================
void ChordPanel::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    g.setColour (card);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (cardBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);

    g.setColour (text);
    g.setFont (studioFont (15.0f, true));
    g.drawText ("Chord palette", titleArea, juce::Justification::centredLeft);

    const auto key = getKey();
    g.setColour (gold);
    g.setFont (studioFont (12.5f));
    g.drawText ("Key of " + Theory::noteName (key.tonic, key.flats) + (key.minor ? " minor" : " major"),
                titleArea, juce::Justification::centredRight);

    g.setColour (textDim);
    g.setFont (studioFont (11.5f));
    for (const auto& [area, labelText] : labels)
        g.drawText (labelText, area, juce::Justification::bottomLeft);

    // Group titles inside the scrolling palette
    if (tab == 0)
    {
        g.saveState();
        g.reduceClipRegion (paletteViewport.getBounds());
        const auto offset = paletteViewport.getPosition() - paletteViewport.getViewPosition();
        for (const auto& [area, title] : groupTitles)
        {
            g.setColour (title.startsWith ("Next") ? gold : textDim);
            g.setFont (studioFont (12.0f, true));
            g.drawText (title, area + offset, juce::Justification::bottomLeft, true);
        }
        g.restoreState();
    }
}

void ChordPanel::resized()
{
    labels.clear();
    auto area = getLocalBounds().reduced (12, 10);

    titleArea = area.removeFromTop (20);
    area.removeFromTop (6);
    displayBox.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);

    auto tabs = area.removeFromTop (28);
    const auto tw = (tabs.getWidth() - 8) / 3;
    paletteTab.setBounds (tabs.removeFromLeft (tw));
    tabs.removeFromLeft (4);
    progressionsTab.setBounds (tabs.removeFromLeft (tw));
    tabs.removeFromLeft (4);
    mineTab.setBounds (tabs);
    area.removeFromTop (6);

    // Shared controls at the bottom (two compact rows)
    auto shared = area.removeFromBottom (62);
    {
        auto row1 = shared.removeFromTop (28);
        lengthBox.setBounds (row1.removeFromLeft ((row1.getWidth() - 8) * 30 / 100));
        row1.removeFromLeft (4);
        leftHandBox.setBounds (row1.removeFromRight ((row1.getWidth() - 4) * 38 / 100));
        row1.removeFromRight (4);
        voicingBox.setBounds (row1);

        shared.removeFromTop (6);
        auto row2 = shared.removeFromTop (28);
        anticipateToggle.setBounds (row2.removeFromLeft (row2.getWidth() / 2));
        notesButton.setBounds (row2);
    }
    area.removeFromBottom (8);

    // Palette tab
    {
        auto p = area;
        auto builder = p.removeFromBottom (28);
        p.removeFromBottom (6);
        degreeBox.setBounds (builder.removeFromLeft (62));
        builder.removeFromLeft (4);
        addBuiltButton.setBounds (builder.removeFromRight (50));
        builder.removeFromRight (4);
        hearBuiltButton.setBounds (builder.removeFromRight (52));
        builder.removeFromRight (4);
        bassBox.setBounds (builder.removeFromRight (76));
        builder.removeFromRight (4);
        qualityBox.setBounds (builder);
        paletteViewport.setBounds (p);
        layoutPalette();
    }

    // Progressions tab
    {
        auto g = area;
        auto top = g.removeFromTop (28);
        levelBox.setBounds (top.removeFromRight (140));
        top.removeFromRight (6);
        categoryBox.setBounds (top);
        g.removeFromTop (6);
        auto buttons = g.removeFromBottom (32);
        insertProgressionButton.setBounds (buttons.removeFromRight (buttons.getWidth() * 2 / 3));
        buttons.removeFromRight (6);
        hearProgressionButton.setBounds (buttons);
        g.removeFromBottom (6);
        progressionList.setBounds (g);
    }

    // My Progressions tab
    {
        auto m = area;
        auto buttons = m.removeFromBottom (32);
        const auto w = (buttons.getWidth() - 12) / 3;
        saveMineButton.setBounds (buttons.removeFromLeft (w + 30));
        buttons.removeFromLeft (6);
        insertMineButton.setBounds (buttons.removeFromLeft ((buttons.getWidth() - 6) / 2));
        buttons.removeFromLeft (6);
        deleteMineButton.setBounds (buttons);
        m.removeFromBottom (6);
        myList.setBounds (m);
    }
}
