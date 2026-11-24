#include "SongEditor.h"
#include "Dialogs.h"
#include <map>
#include <tuple>

//==============================================================================
SongEditor::SongEditor (SongLibrary& lib, MemoRecorder& rec)
    : library (lib), memo (rec)
{
    auto dirtyOnText = [this] { markDirty(); };

    titleEdit.setFont (studioFont (24.0f, true));
    titleEdit.setTextToShowWhenEmpty ("Song title", StudioColours::textDim);
    titleEdit.onTextChange = dirtyOnText;

    dateEdit.setInputRestrictions (10, "0123456789-");
    dateEdit.setTextToShowWhenEmpty ("YYYY-MM-DD", StudioColours::textDim);
    dateEdit.onTextChange = dirtyOnText;

    todayButton.onClick = [this]
    {
        dateEdit.setText (juce::Time::getCurrentTime().formatted ("%Y-%m-%d"), true);
    };

    keyBox.addItemList (Song::keys(), 1);
    timeSigBox.addItemList (Song::timeSignatures(), 1);
    themeBox.addItemList (Song::themes(), 1);
    statusBox.addItemList (Song::statuses(), 1);
    for (auto* box : { &keyBox, &timeSigBox, &themeBox, &statusBox })
        box->onChange = [this] { markDirty(); };

    tempoSlider.setRange (30.0, 240.0, 1.0);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 90, 30);
    tempoSlider.onValueChange = [this] { markDirty(); };

    scriptureEdit.setTextToShowWhenEmpty ("e.g. Psalm 150:6", StudioColours::textDim);
    scriptureEdit.onTextChange = dirtyOnText;

    notesEdit.setTextToShowWhenEmpty ("Notes: inspiration, where it came, arrangement ideas...",
                                      StudioColours::textDim);
    notesEdit.onTextChange = dirtyOnText;

    deleteSongButton.setColour (juce::TextButton::textColourOffId, StudioColours::bad);
    deleteSongButton.onClick = [this] { deleteSong(); };

    openStudioButton.setColour (juce::TextButton::buttonColourId, StudioColours::gold);
    openStudioButton.setColour (juce::TextButton::textColourOffId, StudioColours::header);
    openStudioButton.setTooltip ("Open this song as a project in the Studio");
    openStudioButton.onClick = [this]
    {
        flush();
        if (hasSong && onOpenInStudio)
            onOpenInStudio (song.id);
    };

    recordButton.onClick     = [this] { toggleRecord(); };
    playButton.onClick       = [this] { togglePlay(); };
    deleteMemoButton.onClick = [this] { deleteMemo(); };

    addSectionButton.onClick = [this]
    {
        juce::PopupMenu menu;
        const auto types = Song::sectionTypes();
        for (int i = 0; i < types.size(); ++i)
            menu.addItem (i + 1, types[i]);

        juce::Component::SafePointer<SongEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addSectionButton),
                            [safe, types] (int result)
                            {
                                if (safe != nullptr && result > 0)
                                    safe->addSection (types[result - 1]);
                            });
    };

    sectionsViewport.setViewedComponent (&sectionsHolder, false);
    sectionsViewport.setScrollBarsShown (true, false);

    for (juce::Component* c : { (juce::Component*) &titleEdit, (juce::Component*) &dateEdit,
                                (juce::Component*) &todayButton, (juce::Component*) &keyBox,
                                (juce::Component*) &tempoSlider, (juce::Component*) &timeSigBox,
                                (juce::Component*) &themeBox, (juce::Component*) &statusBox,
                                (juce::Component*) &scriptureEdit, (juce::Component*) &notesEdit,
                                (juce::Component*) &deleteSongButton, (juce::Component*) &openStudioButton,
                                (juce::Component*) &recordButton,
                                (juce::Component*) &playButton, (juce::Component*) &deleteMemoButton,
                                (juce::Component*) &addSectionButton, (juce::Component*) &sectionsViewport })
        addChildComponent (c);

    library.addChangeListener (this);
    startTimerHz (10);
}

SongEditor::~SongEditor()
{
    library.removeChangeListener (this);
    stopTimer();
    flush();
}

void SongEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Tempo, key and time signature can also be changed in the Studio.
    if (! hasSong || dirty)
        return;

    const auto* latest = library.find (song.id);
    if (latest == nullptr || latest->modified == song.modified)
        return;

    song.tempo         = latest->tempo;
    song.key           = latest->key;
    song.timeSignature = latest->timeSignature;
    song.modified      = latest->modified;

    keyBox.setText (song.key, juce::dontSendNotification);
    tempoSlider.setValue (song.tempo, juce::dontSendNotification);
    timeSigBox.setText (song.timeSignature, juce::dontSendNotification);
}

//==============================================================================
//  Loading and saving
//==============================================================================
void SongEditor::loadSong (const juce::String& id)
{
    if (hasSong && song.id == id)
        return;

    flush();
    memo.stopRecording();
    memo.stopPlayback();

    const auto* found = library.find (id);
    if (found == nullptr)
    {
        clear();
        return;
    }

    song    = *found;
    hasSong = true;
    dirty   = false;
    memoSecondsStale = true;

    titleEdit.setText (song.title, false);
    dateEdit.setText (song.dateReceived, false);
    keyBox.setText (song.key, juce::dontSendNotification);
    tempoSlider.setValue (song.tempo, juce::dontSendNotification);
    timeSigBox.setText (song.timeSignature, juce::dontSendNotification);
    themeBox.setText (song.theme, juce::dontSendNotification);
    statusBox.setText (song.status, juce::dontSendNotification);
    scriptureEdit.setText (song.scripture, false);
    notesEdit.setText (song.notes, false);

    for (auto* c : getChildren())
        c->setVisible (true);

    rebuildSections();
    updateMemoControls();
    resized();
    repaint();
}

void SongEditor::clear()
{
    flush();
    memo.stopRecording();
    memo.stopPlayback();

    hasSong = false;
    dirty   = false;
    sectionEditors.clear();

    for (auto* c : getChildren())
        c->setVisible (false);

    repaint();
}

void SongEditor::focusTitle()
{
    if (hasSong)
    {
        titleEdit.grabKeyboardFocus();
        titleEdit.selectAll();
    }
}

void SongEditor::markDirty()
{
    if (! hasSong)
        return;

    dirty = true;
    lastEditMs = juce::Time::getMillisecondCounter();
}

void SongEditor::readFieldsIntoSong()
{
    const auto title = titleEdit.getText().trim();
    song.title         = title.isNotEmpty() ? title : juce::String ("Untitled song");
    song.dateReceived  = dateEdit.getText().trim();
    song.key           = keyBox.getText();
    song.tempo         = (int) tempoSlider.getValue();
    song.timeSignature = timeSigBox.getText();
    song.theme         = themeBox.getText();
    song.status        = statusBox.getText();
    song.scripture     = scriptureEdit.getText().trim();
    song.notes         = notesEdit.getText();

    song.sections.clear();
    for (auto* ed : sectionEditors)
        song.sections.push_back (ed->getSection());
}

void SongEditor::flush()
{
    if (! hasSong || ! dirty)
        return;

    readFieldsIntoSong();
    song.modified = juce::Time::getCurrentTime();
    dirty = false;
    library.update (song);
}

void SongEditor::timerCallback()
{
    if (dirty && juce::Time::getMillisecondCounter() - lastEditMs > 700)
        flush();

    if (hasSong)
        updateMemoControls();
}

//==============================================================================
//  Lyrics sections
//==============================================================================
void SongEditor::rebuildSections (int focusIndex)
{
    sectionEditors.clear();

    for (int i = 0; i < (int) song.sections.size(); ++i)
    {
        auto* ed = new SectionEditor();
        ed->setSection (song.sections[(size_t) i]);

        juce::Component::SafePointer<SongEditor> safe (this);
        ed->onChange   = [this] { markDirty(); rebuildLabelsOnly(); };
        ed->onMoveUp   = [safe, i] { juce::MessageManager::callAsync ([safe, i] { if (safe) safe->moveSection (i, -1); }); };
        ed->onMoveDown = [safe, i] { juce::MessageManager::callAsync ([safe, i] { if (safe) safe->moveSection (i, +1); }); };
        ed->onRemove   = [safe, i] { juce::MessageManager::callAsync ([safe, i] { if (safe) safe->removeSection (i); }); };

        sectionsHolder.addAndMakeVisible (ed);
        sectionEditors.add (ed);
    }

    rebuildLabelsOnly();
    layoutSections();

    if (juce::isPositiveAndBelow (focusIndex, sectionEditors.size()))
    {
        auto* ed = sectionEditors[focusIndex];
        sectionsViewport.setViewPosition (0, ed->getY());
        ed->focusLyrics();
    }
}

void SongEditor::rebuildLabelsOnly()
{
    // "Verse 1", "Verse 2"... numbers appear only when a type is used more than once.
    std::map<juce::String, int> totals, seen;
    for (auto* ed : sectionEditors)
        ++totals[ed->getSection().type];

    for (int i = 0; i < sectionEditors.size(); ++i)
    {
        const auto type = sectionEditors[i]->getSection().type;
        const auto n = ++seen[type];
        sectionEditors[i]->setLabel (totals[type] > 1 ? type + " " + juce::String (n) : type,
                                     i == 0, i == sectionEditors.size() - 1);
    }
}

void SongEditor::layoutSections()
{
    const auto width = juce::jmax (200, sectionsViewport.getMaximumVisibleWidth() - 4);
    int y = 0;

    for (auto* ed : sectionEditors)
    {
        ed->setBounds (0, y, width, 196);
        y += 208;
    }

    sectionsHolder.setSize (width, juce::jmax (y, 1));
}

void SongEditor::addSection (const juce::String& type)
{
    readFieldsIntoSong();
    song.sections.push_back ({ type, {} });
    markDirty();
    rebuildSections ((int) song.sections.size() - 1);
}

void SongEditor::moveSection (int index, int delta)
{
    readFieldsIntoSong();
    const auto target = index + delta;

    if (! juce::isPositiveAndBelow (index, (int) song.sections.size())
        || ! juce::isPositiveAndBelow (target, (int) song.sections.size()))
        return;

    std::swap (song.sections[(size_t) index], song.sections[(size_t) target]);
    markDirty();
    rebuildSections (target);
}

void SongEditor::removeSection (int index)
{
    readFieldsIntoSong();
    if (! juce::isPositiveAndBelow (index, (int) song.sections.size()))
        return;

    auto doRemove = [this, index]
    {
        readFieldsIntoSong();
        if (juce::isPositiveAndBelow (index, (int) song.sections.size()))
        {
            song.sections.erase (song.sections.begin() + index);
            markDirty();
            rebuildSections();
        }
    };

    if (song.sections[(size_t) index].lyrics.trim().isEmpty())
    {
        doRemove();
        return;
    }

    juce::Component::SafePointer<SongEditor> safe (this);
    Dialogs::confirm (this, "Remove section", "Remove this section and its lyrics?", "Remove", "Keep",
                      [safe, doRemove] { if (safe != nullptr) doRemove(); });
}

void SongEditor::deleteSong()
{
    if (! hasSong)
        return;

    juce::Component::SafePointer<SongEditor> safe (this);
    const auto id = song.id;

    Dialogs::confirm (this, "Delete song",
                      "Move \"" + song.title + "\" and its voice memo to the Recycle Bin?\n"
                      "You can restore it from the Recycle Bin if needed.",
                      "Delete", "Cancel",
                      [safe, id]
                      {
                          if (safe == nullptr)
                              return;

                          safe->memo.stopRecording();
                          safe->memo.stopPlayback();
                          safe->dirty   = false;
                          safe->hasSong = false;

                          if (safe->onSongDeleted)
                              safe->onSongDeleted (id);   // let the Studio close it first

                          safe->library.remove (id);
                          safe->clear();
                      },
                      juce::MessageBoxIconType::WarningIcon);
}

//==============================================================================
//  Voice memo
//==============================================================================
void SongEditor::toggleRecord()
{
    if (! hasSong)
        return;

    if (memo.isRecording())
    {
        memo.stopRecording();
        updateMemoControls();
        return;
    }

    const auto file = library.getMemoFile (song.id);
    juce::Component::SafePointer<SongEditor> safe (this);

    auto start = [safe, file]
    {
        if (safe == nullptr)
            return;

        if (! safe->memo.startRecording (file))
            Dialogs::message (safe.getComponent(), "Cannot record",
                              "Recording could not start. Open Audio & MIDI Setup and choose an input, "
                              "and check that Windows allows apps to use the microphone "
                              "(Settings > Privacy > Microphone).");

        safe->updateMemoControls();
    };

    if (file.existsAsFile())
    {
        Dialogs::confirm (this, "Replace voice memo",
                          "This song already has a voice memo. Record a new one and replace it?",
                          "Record New", "Cancel", start);
    }
    else
    {
        start();
    }
}

void SongEditor::togglePlay()
{
    if (memo.isPlaying())
        memo.stopPlayback();
    else
        memo.startPlayback (library.getMemoFile (song.id));

    updateMemoControls();
}

void SongEditor::deleteMemo()
{
    juce::Component::SafePointer<SongEditor> safe (this);
    const auto file = library.getMemoFile (song.id);

    Dialogs::confirm (this, "Delete voice memo", "Move this voice memo to the Recycle Bin?", "Delete", "Cancel",
                      [safe, file]
                      {
                          if (safe == nullptr)
                              return;

                          safe->memo.stopPlayback();
                          file.moveToTrash();
                          safe->memoSecondsStale = true;
                          safe->updateMemoControls();
                      });
}

void SongEditor::updateMemoControls()
{
    const auto file = library.getMemoFile (song.id);
    const auto recording = memo.isRecording();
    const auto playing = memo.isPlaying();

    if (! recording && playing != wasPlaying)
        memoSecondsStale = true;
    wasPlaying = playing;

    if (recording != wasRecording)
    {
        memoSecondsStale = true;
        wasRecording = recording;
    }

    if (memoSecondsStale && ! recording)
    {
        memoSeconds = memo.getFileLengthSeconds (file);
        memoSecondsStale = false;
    }

    const auto hasMemo = ! recording && file.existsAsFile();

    juce::String newText;
    juce::Colour newColour = StudioColours::textDim;

    if (recording)
    {
        newText   = "Recording   " + formatSeconds (memo.getRecordedSeconds());
        newColour = StudioColours::bad;
    }
    else if (hasMemo)
    {
        newText   = "Voice memo   " + formatSeconds (memoSeconds);
        if (playing)
            newText << "   (playing " << formatSeconds (memo.getPlaybackPosition()) << ")";
        newColour = StudioColours::text;
    }
    else
    {
        newText = "No voice memo yet. Record the melody the moment the song comes.";
    }

    recordButton.setButtonText (recording ? "Stop Recording" : "Record Memo");
    recordButton.setColour (juce::TextButton::buttonColourId,
                            recording ? StudioColours::bad : StudioColours::card.brighter (0.08f));
    playButton.setButtonText (playing ? "Stop" : "Play");
    playButton.setEnabled (hasMemo);
    deleteMemoButton.setEnabled (hasMemo);

    if (newText != memoText || newColour != memoColour)
    {
        memoText   = newText;
        memoColour = newColour;
        repaint (memoTextArea);
    }
}

juce::String SongEditor::formatSeconds (double seconds)
{
    const auto total = (int) seconds;
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}

//==============================================================================
//  Drawing and layout
//==============================================================================
void SongEditor::paint (juce::Graphics& g)
{
    using namespace StudioColours;

    if (! hasSong)
    {
        g.setColour (textDim);
        g.setFont (studioFont (18.0f, true));
        g.drawText ("No song selected", getLocalBounds().withTrimmedBottom (40), juce::Justification::centred);
        g.setFont (studioFont (14.0f));
        g.drawText ("Choose a song on the left, or click + New Song to capture a new one.",
                    getLocalBounds().withTrimmedTop (20), juce::Justification::centred);
        return;
    }

    g.setFont (studioFont (12.5f));
    g.setColour (textDim);
    for (const auto& [labelArea, labelText] : fieldLabels)
        g.drawText (labelText, labelArea, juce::Justification::bottomLeft);

    // Voice memo card
    g.setColour (card);
    g.fillRoundedRectangle (memoArea.toFloat(), 8.0f);
    g.setColour (cardBorder);
    g.drawRoundedRectangle (memoArea.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (memoColour);
    g.setFont (studioFont (14.0f, memo.isRecording()));
    g.drawText (memoText, memoTextArea, juce::Justification::centredLeft);

    // Lyrics header
    g.setColour (text);
    g.setFont (studioFont (17.0f, true));
    g.drawText ("Lyrics", lyricsHeaderArea, juce::Justification::centredLeft);
    g.setColour (gold);
    g.fillRect (lyricsHeaderArea.getX(), lyricsHeaderArea.getBottom() - 2, 28, 2);
}

void SongEditor::resized()
{
    fieldLabels.clear();
    auto area = getLocalBounds().reduced (24, 16);

    // Title row
    auto titleRow = area.removeFromTop (46);
    deleteSongButton.setBounds (titleRow.removeFromRight (120).reduced (0, 6));
    titleRow.removeFromRight (10);
    openStudioButton.setBounds (titleRow.removeFromRight (150).reduced (0, 6));
    titleRow.removeFromRight (16);
    titleEdit.setBounds (titleRow);
    area.removeFromTop (10);

    const int labelH = 20, fieldH = 32, gap = 14;

    auto placeRow = [&] (std::initializer_list<std::tuple<juce::String, float, juce::Component*>> fields)
    {
        auto labels = area.removeFromTop (labelH);
        auto row    = area.removeFromTop (fieldH);
        area.removeFromTop (gap);

        float totalWeight = 0.0f;
        for (const auto& f : fields) totalWeight += std::get<1> (f);

        const auto available = row.getWidth() - gap * ((int) fields.size() - 1);
        int index = 0;

        for (const auto& [name, weight, comp] : fields)
        {
            const auto isLast = index == (int) fields.size() - 1;
            const auto w = isLast ? row.getWidth() : (int) (available * weight / totalWeight);

            auto fieldArea = row.removeFromLeft (w);
            fieldLabels.push_back ({ labels.removeFromLeft (w), name });

            if (comp == &dateEdit)
            {
                todayButton.setBounds (fieldArea.removeFromRight (70));
                fieldArea.removeFromRight (6);
            }
            comp->setBounds (fieldArea);

            if (! isLast)
            {
                row.removeFromLeft (gap);
                labels.removeFromLeft (gap);
            }
            ++index;
        }
    };

    placeRow ({ { "Date received", 2.2f, &dateEdit },
                { "Key",           1.4f, &keyBox },
                { "Tempo",         1.4f, &tempoSlider },
                { "Time signature",1.0f, &timeSigBox } });

    placeRow ({ { "Theme",         1.3f, &themeBox },
                { "Status",        1.2f, &statusBox },
                { "Scripture",     2.5f, &scriptureEdit } });

    placeRow ({ { "Notes",         1.0f, &notesEdit } });

    // Voice memo card
    memoArea = area.removeFromTop (60);
    auto memoInner = memoArea.reduced (14, 12);
    recordButton.setBounds (memoInner.removeFromLeft (150));
    memoInner.removeFromLeft (8);
    playButton.setBounds (memoInner.removeFromLeft (90));
    memoInner.removeFromLeft (8);
    deleteMemoButton.setBounds (memoInner.removeFromRight (120));
    memoInner.removeFromRight (8);
    memoTextArea = memoInner.withTrimmedLeft (10);
    area.removeFromTop (16);

    // Lyrics
    lyricsHeaderArea = area.removeFromTop (36);
    addSectionButton.setBounds (lyricsHeaderArea.withTrimmedLeft (lyricsHeaderArea.getWidth() - 150).reduced (0, 2));
    area.removeFromTop (10);

    sectionsViewport.setBounds (area);
    layoutSections();
}
