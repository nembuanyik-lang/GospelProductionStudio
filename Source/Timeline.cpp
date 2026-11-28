#include "AudioEdit.h"
#include "Takes.h"
#include "InstrumentPanel.h"
#include "SoundLibrary.h"
#include "Grooves.h"
#include "PluginHost.h"
#include "Mixing.h"
#include "AutomationLane.h"
#include "VocalTunePlugin.h"
#include "Loops.h"
#include "Timing.h"
#include "Harmony.h"
#include "Backing.h"
#include "ChordDetect.h"
#include "Stems.h"
#include "MySounds.h"
#include "SamplerPlugin.h"
#include "Timeline.h"
#include "Dialogs.h"

namespace
{
    const juce::Colour palette[] = { juce::Colour (0xffd4a843), juce::Colour (0xff5b8def), juce::Colour (0xffe07a4f),
                                     juce::Colour (0xff4caf7d), juce::Colour (0xffb07ee0), juce::Colour (0xff4fb3c8),
                                     juce::Colour (0xffe05a8a), juce::Colour (0xff8a93a3) };
    const char* paletteNames[] = { "Gold", "Blue", "Orange", "Green", "Purple", "Teal", "Rose", "Grey" };

    void addColourMenu (juce::PopupMenu& menu, int firstId)
    {
        juce::PopupMenu colours;
        for (int i = 0; i < (int) std::size (palette); ++i)
            colours.addColouredItem (firstId + i, paletteNames[i], palette[i]);
        menu.addSubMenu ("Colour", colours);
    }

    bool isArranger (const te::Track* t)   { return t != nullptr && t->isArrangerTrack(); }

    void askForName (juce::Component* parent, const juce::String& title, const juce::String& current,
                     std::function<void (const juce::String&)> onDone)
    {
        auto* window = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon, parent);
        window->addTextEditor ("name", current);
        window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        window->enterModalState (true, juce::ModalCallbackFunction::create ([window, onDone] (int result)
        {
            const auto text = window->getTextEditorContents ("name").trim();
            if (result == 1 && text.isNotEmpty() && onDone)
                onDone (text);
        }), true);
    }
}

//==============================================================================
//  Header for the Chord Track lane
//==============================================================================
class ChordLaneHeader final : public juce::Component
{
public:
    explicit ChordLaneHeader (TimelineView& o) : owner (o)
    {
        paletteButton.setComponentID ("timeline.palette");
        paletteButton.setTooltip ("Show or hide the chord palette");
        paletteButton.setWantsKeyboardFocus (false);
        paletteButton.onClick = [this] { if (owner.onTogglePalette) owner.onTogglePalette(); };
        addAndMakeVisible (paletteButton);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (StudioColours::header);
        g.fillRect (getLocalBounds());
        g.setColour (StudioColours::cardBorder);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        g.setColour (StudioColours::text);
        g.setFont (studioFont (14.0f, true));
        g.drawText ("Chords", 12, 0, 100, getHeight(), juce::Justification::centredLeft);
    }

    void resized() override
    {
        paletteButton.setBounds (getLocalBounds().reduced (8, 6).removeFromRight (86));
    }

private:
    TimelineView& owner;
    juce::TextButton paletteButton { "Palette" };
};

//==============================================================================
//  TimelineView
//==============================================================================
TimelineView::TimelineView (StudioEngine& e)
    : engine (e)
{
    addTrackButton.setTooltip ("Add an instrument or audio track");
    addTrackButton.onClick = [this] { addTrackMenu(); };

    undoButton.setTooltip ("Undo (Ctrl+Z)");
    undoButton.onClick = [this] { if (auto* ed = getEdit()) { ed->undo(); selectedClip = nullptr; refreshAll(); } };
    redoButton.setTooltip ("Redo (Ctrl+Y)");
    redoButton.onClick = [this] { if (auto* ed = getEdit()) { ed->redo(); selectedClip = nullptr; refreshAll(); } };

    fitButton.setTooltip ("Show the whole song");
    fitButton.onClick = [this] { zoomToFit(); };
    bandButton.setTooltip ("Backing band: bass, piano, organ, guitar and drums play your Chord Track in a gospel style");
    bandButton.onClick = [this] { openBand(); };

    snapBox.addItemList ({ "Snap off", "Snap 1/2 beat", "Snap beat", "Snap bar" }, 1);
    snapBox.setSelectedItemIndex (snapMode, juce::dontSendNotification);
    snapBox.onChange = [this] { snapMode = snapBox.getSelectedItemIndex(); };

    zoomSlider.setRange (2.0, 200.0);
    zoomSlider.setSkewFactorFromMidPoint (24.0);
    zoomSlider.setValue (pxPerBeat, juce::dontSendNotification);
    zoomSlider.setTooltip ("Zoom (Ctrl + mouse wheel)");
    zoomSlider.onValueChange = [this] { zoomAround (zoomSlider.getValue() / pxPerBeat, 0.0f); };

    for (juce::Component* c : { (juce::Component*) &addTrackButton, (juce::Component*) &undoButton,
                                (juce::Component*) &redoButton, (juce::Component*) &fitButton, (juce::Component*) &bandButton,
                                (juce::Component*) &snapBox, (juce::Component*) &zoomSlider })
    {
        c->setWantsKeyboardFocus (false);
        addAndMakeVisible (c);
    }

    addAndMakeVisible (ruler);
    addAndMakeVisible (headerHolder);
    addAndMakeVisible (lanesView);

    hScroll.setAutoHide (false);
    hScroll.addListener (this);
    vScroll.addListener (this);
    addAndMakeVisible (hScroll);
    addAndMakeVisible (vScroll);

    setWantsKeyboardFocus (true);
    editChanged();
    startTimerHz (30);
}

TimelineView::~TimelineView()
{
    hScroll.removeListener (this);
    vScroll.removeListener (this);
}

void TimelineView::releaseEdit()
{
    // Everything that points into the song goes, before the song is deleted
    selectedClip = nullptr;
    clipboard = nullptr;
    selectedTrack = nullptr;
    selectedChords.clear();
    lanesView.thumbnails.clear();
    lanesView.thumbnailFiles.clear();
    automationLanes.clear();
    lanes.clear();
    headers.clear();
}

void TimelineView::editChanged()
{
    lanesView.thumbnails.clear();          // waveforms belong to the previous song
    lanesView.thumbnailFiles.clear();
    selectedClip = nullptr;
    selectedTrack = nullptr;
    clipboard = nullptr;
    scrollBeats = 0.0;
    scrollY = 0;
    rebuildHeaders();
}

//==============================================================================
double TimelineView::timeToBeat (te::TimePosition t) const
{
    auto* ed = getEdit();
    return ed != nullptr ? ed->tempoSequence.toBeats (t).inBeats() : 0.0;
}

te::TimePosition TimelineView::beatToTime (double beat) const
{
    auto* ed = getEdit();
    return ed != nullptr ? ed->tempoSequence.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, beat)))
                         : te::TimePosition();
}

double TimelineView::getBeatsPerBar() const
{
    if (auto* ed = getEdit())
        if (auto* sig = ed->tempoSequence.getTimeSig (0))
            return juce::jmax (1, sig->numerator.get());
    return 4.0;
}

double TimelineView::snap (double beat) const
{
    const double grid = snapMode == 1 ? 0.5 : snapMode == 2 ? 1.0 : snapMode == 3 ? getBeatsPerBar() : 0.0;
    return grid > 0.0 ? std::round (beat / grid) * grid : beat;
}

double TimelineView::getPlayheadBeat() const
{
    auto* ed = getEdit();
    return ed != nullptr ? timeToBeat (ed->getTransport().getPosition()) : 0.0;
}

int TimelineView::getLaneHeight (int i) const
{
    if (i == 0) return sectionLaneHeight;
    if (i == chordLane) return chordLaneHeight;
    if (auto* at = dynamic_cast<te::AudioTrack*> (getLane (i)))
        if (Automation::isShown (*at))
            return trackHeight + AutomationLane::height;
    return trackHeight;
}

int TimelineView::getLaneY (int index) const
{
    int y = 0;
    for (int i = 0; i < index; ++i)
        y += getLaneHeight (i);
    return y;
}

int TimelineView::laneAtY (int y) const
{
    int top = 0;
    for (int i = 0; i < getNumLanes(); ++i)
    {
        const auto h = getLaneHeight (i);
        if (y >= top && y < top + h)
            return i;
        top += h;
    }
    return -1;
}

int TimelineView::getContentHeight() const
{
    return getLaneY (getNumLanes());
}

//==============================================================================
juce::String TimelineView::trackSignature() const
{
    juce::String s;
    if (auto* ed = getEdit())
    {
        s << (juce::pointer_sized_int) ed << ":";
        for (auto* t : Mixing::getSongTracks (*ed))
            s << t->itemID.toString() << (Automation::isShown (*t) ? "a" : "") << ",";
    }
    return s;
}

void TimelineView::rebuildHeaders()
{
    automationLanes.clear();
    headers.clear();
    lanes.clear();

    if (auto* ed = getEdit())
    {
        lanes.push_back (ed->getArrangerTrack());
        lanes.push_back (nullptr);                  // the Chord Track lane

        for (auto* t : Mixing::getSongTracks (*ed))
            lanes.push_back (t);
    }

    for (size_t i = 0; i < lanes.size(); ++i)
    {
        juce::Component* h = nullptr;
        if ((int) i == chordLane)    h = new ChordLaneHeader (*this);
        else if (lanes[i] != nullptr) h = new TrackHeader (*this, *lanes[i]);
        else                          h = new juce::Component();

        headerHolder.addAndMakeVisible (h);
        headers.add (h);

        if (auto* at = dynamic_cast<te::AudioTrack*> (lanes[i]); at != nullptr && Automation::isShown (*at))
            lanesView.addAndMakeVisible (automationLanes.add (new AutomationLane (*this, *at)));
        else
            automationLanes.add (nullptr);
    }

    if (selectedTrack != nullptr && std::find (lanes.begin(), lanes.end(), selectedTrack) == lanes.end())
        selectedTrack = nullptr;

    lastSignature = trackSignature();
    resized();
    refreshAll();

    if (onTracksChanged)
        onTracksChanged();
}

void TimelineView::refreshAll()
{
    for (auto* h : headers)
        if (auto* th = dynamic_cast<TrackHeader*> (h))
            th->refresh();

    if (auto* ed = getEdit())
    {
        undoButton.setEnabled (ed->getUndoManager().canUndo());
        redoButton.setEnabled (ed->getUndoManager().canRedo());
    }

    updateScrollbars();
    repaint();
    ruler.repaint();
    lanesView.repaint();
}

void TimelineView::openClip (te::Clip* clip)
{
    if (auto* mc = dynamic_cast<te::MidiClip*> (clip))
    {
        selectClip (mc);
        if (onOpenClip)
            onOpenClip (mc);
    }
}

void TimelineView::selectClip (te::Clip* clip)
{
    selectedClip = clip;
    lanesView.repaint();
}

void TimelineView::selectTrack (te::Track* track)
{
    selectedTrack = isArranger (track) ? nullptr : track;
    for (auto* h : headers)
        h->repaint();
    lanesView.repaint();
}

void TimelineView::beginUndoStep()
{
    if (auto* ed = getEdit())
        ed->getUndoManager().beginNewTransaction();
}

//==============================================================================
void TimelineView::deleteSelectedClip()
{
    if (selectedClip == nullptr)
        return;

    beginUndoStep();
    selectedClip->removeFromParent();
    selectedClip = nullptr;
    refreshAll();
}

void TimelineView::duplicateSelectedClip()
{
    if (selectedClip == nullptr || selectedClip->getClipTrack() == nullptr)
        return;

    beginUndoStep();
    if (auto copy = te::duplicateClip (*selectedClip))
    {
        copy->setStart (selectedClip->getPosition().getEnd(), false, true);
        selectClip (copy.get());
    }
    refreshAll();
}

void TimelineView::copySelectedClip()
{
    clipboard = selectedClip;
}

void TimelineView::pasteClip()
{
    if (clipboard == nullptr || clipboard->getClipTrack() == nullptr)
        return;

    beginUndoStep();
    if (auto copy = te::duplicateClip (*clipboard))
    {
        copy->setStart (beatToTime (snap (getPlayheadBeat())), false, true);

        if (auto* target = dynamic_cast<te::AudioTrack*> (selectedTrack))
            if (! isArranger (clipboard->getTrack()) && target != copy->getTrack())
                copy->moveTo (*target);

        selectClip (copy.get());
    }
    refreshAll();
}

void TimelineView::addTrackMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Instrument track");
    juce::String group;
    for (size_t i = 0; i < Instruments::all().size(); ++i)
    {
        const auto& c = Instruments::all()[i];
        menu.addItem (100 + (int) i, c.name + (c.kind == Instruments::Kind::sampler && juce::String (c.pack).isNotEmpty() && ! SoundPacks::isInstalled (c.pack)
                                                   ? juce::String ("  (download in Sound Library)") : juce::String()));
    }
    const auto pluginInstruments = PluginHost::getInstruments (engine.getEngine());
    if (! pluginInstruments.isEmpty())
    {
        juce::PopupMenu plugins;
        for (int i = 0; i < pluginInstruments.size(); ++i)
            plugins.addItem (1000 + i, pluginInstruments.getReference (i).name);
        menu.addSubMenu ("Plugin instrument (VST3)", plugins);
    }
    menu.addSeparator();
    menu.addItem (2, "Audio track (vocals, guitar)");

    juce::Component::SafePointer<TimelineView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addTrackButton), [safe, pluginInstruments] (int result)
    {
        if (safe == nullptr || result == 0 || safe->getEdit() == nullptr)
            return;

        if (result >= 1000 && result - 1000 < pluginInstruments.size())
        {
            const auto& d = pluginInstruments.getReference (result - 1000);
            const auto count = (int) Mixing::getSongTracks (*safe->getEdit()).size();
            te::Track* after = safe->selectedTrack;
            if (after == nullptr && count > 0)
                after = Mixing::getSongTracks (*safe->getEdit()).getLast();

            safe->beginUndoStep();
            if (auto* track = safe->engine.addTrack (true, StudioEngine::uniqueTrackName (*safe->getEdit(), d.name), palette[count % (int) std::size (palette)], after, "none"))
            {
                if (auto* p = safe->engine.setPluginInstrument (*track, d))
                    PluginHost::showWindow (*p);
                safe->engine.setArmedTrack (track);
                safe->selectedTrack = track;
            }
            safe->rebuildHeaders();
            return;
        }

        const auto instrument = result >= 100;
        const auto* choice = instrument ? &Instruments::all()[(size_t) (result - 100)] : nullptr;
        const auto count = (int) Mixing::getSongTracks (*safe->getEdit()).size();
        const auto name = StudioEngine::uniqueTrackName (*safe->getEdit(), choice != nullptr ? StudioEngine::trackNameFor (choice->id)
                                                                                           : juce::String ("Audio"));

        te::Track* after = safe->selectedTrack;
        if (after == nullptr && count > 0)
            after = Mixing::getSongTracks (*safe->getEdit()).getLast();

        safe->beginUndoStep();
        if (auto* track = safe->engine.addTrack (instrument, name, palette[count % (int) std::size (palette)], after,
                                                 choice != nullptr ? choice->id : "grand"))
        {
            if (instrument)
                safe->engine.setArmedTrack (track);
            safe->selectedTrack = track;

            // My Sounds starts empty: open its window so you can add your sounds
            if (choice != nullptr && juce::String (choice->id) == "mysounds")
            {
                te::Track::Ptr keep (track);
                auto& eng = safe->engine;
                juce::MessageManager::callAsync ([&eng, keep]
                {
                    if (auto* at = dynamic_cast<te::AudioTrack*> (keep.get()))
                        InstrumentPanel::open (eng, *at);
                });
            }
        }
        safe->rebuildHeaders();
    });
}

void TimelineView::openBand()
{
    auto* ed = getEdit();
    if (ed == nullptr)
        return;
    juce::Component::SafePointer<TimelineView> view (this);
    auto content = Backing::createPanel (*ed,
        [view] { return view != nullptr ? view->currentKey() : Theory::Key(); },
        [view] { return view != nullptr ? view->getBeatsPerBar() : 4.0; },
        [view] (int, const juce::String& name, juce::Colour colour, const juce::String& instrumentId) -> te::AudioTrack*
        {
            return view != nullptr ? view->getEngine().addTrack (true, name, colour, nullptr, instrumentId) : nullptr;
        },
        [view]
        {
            if (view == nullptr) return;
            view->refreshAll();
            if (view->onTracksChanged) view->onTracksChanged();
        });
    juce::CallOutBox::launchAsynchronously (std::move (content), bandButton.getScreenBounds(), nullptr);
}

void TimelineView::addSectionAt (double beat)
{
    auto* ed = getEdit();
    if (ed == nullptr || ed->getArrangerTrack() == nullptr)
        return;

    juce::PopupMenu menu;
    const juce::StringArray types { "Intro", "Verse", "Pre-Chorus", "Chorus", "Bridge", "Vamp", "Tag", "Interlude", "Outro" };
    for (int i = 0; i < types.size(); ++i)
        menu.addItem (i + 1, types[i]);

    juce::Component::SafePointer<TimelineView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, beat, types] (int result)
    {
        if (safe == nullptr || result == 0 || safe->getEdit() == nullptr)
            return;

        const auto type = types[result - 1];
        const auto bpb = safe->getBeatsPerBar();
        const auto start = std::floor (beat / bpb) * bpb;
        const auto bars = (type == "Verse" || type == "Chorus" || type == "Bridge" || type == "Vamp") ? 8 : 4;

        safe->beginUndoStep();
        if (auto* clip = safe->getEdit()->getArrangerTrack()->insertNewClip (
                             te::TrackItem::Type::arranger, type,
                             te::TimeRange (safe->beatToTime (start), safe->beatToTime (start + bars * bpb)), nullptr))
            clip->setColour (palette[(result - 1) % (int) std::size (palette)]);

        safe->refreshAll();
    });
}

void TimelineView::zoomAround (double factor, float x)
{
    const auto beatAtX = xToBeat (x);
    pxPerBeat = juce::jlimit (2.0, 200.0, pxPerBeat * factor);
    scrollBeats = juce::jmax (0.0, beatAtX - x / pxPerBeat);
    zoomSlider.setValue (pxPerBeat, juce::dontSendNotification);
    refreshAll();
}

void TimelineView::scrollBy (double beats)
{
    scrollBeats = juce::jmax (0.0, scrollBeats + beats);
    refreshAll();
}

void TimelineView::zoomToFit()
{
    double endBeat = getBeatsPerBar() * 16.0;
    for (auto* t : lanes)
        if (auto* ct = dynamic_cast<te::ClipTrack*> (t))
            for (auto* c : ct->getClips())
                endBeat = juce::jmax (endBeat, timeToBeat (c->getPosition().getEnd()));

    const auto width = juce::jmax (100, lanesView.getWidth());
    pxPerBeat = juce::jlimit (2.0, 200.0, width / (endBeat * 1.05));
    scrollBeats = 0.0;
    zoomSlider.setValue (pxPerBeat, juce::dontSendNotification);
    refreshAll();
}

void TimelineView::updateScrollbars()
{
    double endBeat = getBeatsPerBar() * 32.0;
    for (auto* t : lanes)
        if (auto* ct = dynamic_cast<te::ClipTrack*> (t))
            for (auto* c : ct->getClips())
                endBeat = juce::jmax (endBeat, timeToBeat (c->getPosition().getEnd()) + getBeatsPerBar() * 16.0);

    const auto visibleBeats = juce::jmax (1.0, lanesView.getWidth() / pxPerBeat);
    endBeat = juce::jmax (endBeat, scrollBeats + visibleBeats);
    hScroll.setRangeLimits (0.0, endBeat, juce::dontSendNotification);
    hScroll.setCurrentRange (scrollBeats, visibleBeats, juce::dontSendNotification);

    const auto contentH = getContentHeight();
    const auto viewH = juce::jmax (1, lanesView.getHeight());
    scrollY = juce::jlimit (0, juce::jmax (0, contentH - viewH), scrollY);
    vScroll.setRangeLimits (0.0, juce::jmax (contentH, viewH), juce::dontSendNotification);
    vScroll.setCurrentRange (scrollY, viewH, juce::dontSendNotification);

    for (int i = 0; i < headers.size(); ++i)
    {
        headers[i]->setBounds (0, getLaneY (i) - scrollY, headerHolder.getWidth(), getLaneHeight (i));
        if (auto* al = automationLanes[i])
            al->setBounds (0, getLaneY (i) + trackHeight - scrollY, lanesView.getWidth(), AutomationLane::height);
    }
}

void TimelineView::scrollBarMoved (juce::ScrollBar* bar, double newStart)
{
    if (bar == &hScroll)
        scrollBeats = juce::jmax (0.0, newStart);
    else
        scrollY = juce::roundToInt (newStart);

    refreshAll();
}

//==============================================================================
void TimelineView::timerCallback()
{
    if (trackSignature() != lastSignature)
        rebuildHeaders();

    auto* ed = getEdit();
    if (ed == nullptr)
        return;

    // Loops follow tempo and key changes: once the tempo and key have stayed the same for a moment
    {
        const auto sig = ed->tempoSequence.getTempoAt (te::TimePosition()).getBpm() * 100.0
                         + ed->pitchSequence.getPitchAt (te::TimePosition()).getPitch() * 1.0e6;
        const auto now = juce::Time::getMillisecondCounter();
        if (! juce::exactlyEqual (sig, loopSignature)) { loopSignature = sig; loopSignatureTime = now; }
        if (++loopCheckTick % 10 == 0 && now - loopSignatureTime > 800 && ! juce::Component::isMouseButtonDownAnywhere())
            if (Loops::refreshAll (*ed) > 0)
                refreshAll();
    }

    const auto playhead = getPlayheadBeat();
    const auto playing = ed->getTransport().isPlaying();

    // Follow the playhead while playing.
    if (playing)
    {
        const auto x = beatToX (playhead);
        if (x > lanesView.getWidth() - 40 || x < 0)
        {
            scrollBeats = juce::jmax (0.0, playhead - (lanesView.getWidth() / pxPerBeat) * 0.1);
            updateScrollbars();
        }
    }

    // Input meters on armed audio tracks
    for (auto* h : headers)
        if (auto* th = dynamic_cast<TrackHeader*> (h))
            if (th->audioTrack != nullptr && ! th->isInstrument && engine.isAudioArmed (*th->audioTrack))
                th->repaint();

    static int tick = 0;
    if (playing || std::abs (playhead - lastPlayhead) > 1.0e-6 || (++tick % 6) == 0)
    {
        lastPlayhead = playhead;
        ruler.repaint();
        lanesView.repaint();
    }

    if (tick % 10 == 0)
    {
        for (auto* h : headers)
            if (auto* th = dynamic_cast<TrackHeader*> (h))
                th->refresh();
        undoButton.setEnabled (ed->getUndoManager().canUndo());
        redoButton.setEnabled (ed->getUndoManager().canRedo());
    }
}

bool TimelineView::keyPressed (const juce::KeyPress& key)
{
    const auto cmd = key.getModifiers().isCommandDown();
    // Letters can arrive as upper or lower case key codes depending on the system.
    const auto c = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) key.getKeyCode());

    if (! cmd && c == 's' && ! key.getModifiers().isAnyModifierKeyDown())
    {
        splitAtPlayhead();
        return true;
    }
    if (key == juce::KeyPress::returnKey && selectedClip != nullptr)
    {
        openClip (selectedClip.get());
        return true;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (! selectedChords.empty()) deleteSelectedChords();
        else                          deleteSelectedClip();
        return true;
    }
    if (cmd && (c == 'z'))
    {
        if (auto* ed = getEdit())
            key.getModifiers().isShiftDown() ? ed->redo() : ed->undo();
        selectedClip = nullptr;
        refreshAll();
        return true;
    }
    if (cmd && (c == 'y'))   { if (auto* ed = getEdit()) ed->redo(); selectedClip = nullptr; refreshAll(); return true; }
    if (cmd && (c == 'd'))   { duplicateSelectedClip(); return true; }
    if (cmd && (c == 'c'))   { copySelectedClip(); return true; }
    if (cmd && (c == 'v'))   { pasteClip(); return true; }
    return false;
}

void TimelineView::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    g.setColour (card);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    g.setColour (header);
    g.fillRect (cornerArea);
    g.fillRect (headerHolder.getBounds());

    g.setColour (textDim);
    g.setFont (studioFont (12.0f));
    g.drawText ("Tracks", cornerArea.reduced (12, 0), juce::Justification::centredLeft);

    g.setColour (cardBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
}

void TimelineView::resized()
{
    auto area = getLocalBounds().reduced (1);

    toolbarArea = area.removeFromTop (40).reduced (10, 5);
    {
        auto t = toolbarArea;
        addTrackButton.setBounds (t.removeFromLeft (100));
        t.removeFromLeft (8);
        snapBox.setBounds (t.removeFromLeft (140));
        t.removeFromLeft (8);
        bandButton.setBounds (t.removeFromLeft (80));
        t.removeFromLeft (16);
        redoButton.setBounds (t.removeFromRight (64));
        t.removeFromRight (6);
        undoButton.setBounds (t.removeFromRight (64));
        t.removeFromRight (16);
        fitButton.setBounds (t.removeFromRight (56));
        t.removeFromRight (8);
        zoomSlider.setBounds (t.removeFromRight (180));
    }

    auto bottom = area.removeFromBottom (12);
    auto right = area.removeFromRight (12);

    auto top = area.removeFromTop (rulerHeight);
    cornerArea = top.removeFromLeft (headerWidth);
    ruler.setBounds (top);

    headerHolder.setBounds (area.removeFromLeft (headerWidth));
    lanesView.setBounds (area);

    vScroll.setBounds (right.withTrimmedTop (rulerHeight));
    hScroll.setBounds (bottom.withTrimmedLeft (headerWidth));

    updateScrollbars();
}

//==============================================================================
//  Ruler
//==============================================================================
void TimelineRuler::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    auto* ed = owner.getEdit();

    g.fillAll (header);
    if (ed == nullptr)
        return;

    const auto bpb = owner.getBeatsPerBar();
    const auto pxPerBar = owner.pxPerBeat * bpb;
    const int labelEvery = pxPerBar >= 34 ? 1 : pxPerBar >= 17 ? 2 : pxPerBar >= 8 ? 4 : 8;

    // Loop band (lower half)
    {
        const auto loop = ed->getTransport().getLoopRange();
        const auto x1 = owner.beatToX (owner.timeToBeat (loop.getStart()));
        const auto x2 = owner.beatToX (owner.timeToBeat (loop.getEnd()));
        const auto looping = ed->getTransport().looping.get();
        const auto band = juce::Rectangle<float> (x1, getHeight() * 0.5f + 3.0f, x2 - x1, getHeight() * 0.5f - 7.0f);

        // Red when punch recording is on: this range is what gets recorded
        const auto bandColour = owner.getEngine().isPunchEnabled() ? bad : gold;
        g.setColour (bandColour.withAlpha (looping || owner.getEngine().isPunchEnabled() ? 0.45f : 0.16f));
        g.fillRoundedRectangle (band, 3.0f);
        g.setColour (bandColour.withAlpha (looping ? 1.0f : 0.5f));
        g.fillRect (band.withWidth (3.0f));
        g.fillRect (band.withLeft (band.getRight() - 3.0f));
    }

    // Bars and beats
    const auto firstBar = (int) std::floor (owner.scrollBeats / bpb);
    for (int bar = juce::jmax (0, firstBar); ; ++bar)
    {
        const auto x = owner.beatToX (bar * bpb);
        if (x > getWidth())
            break;

        g.setColour (cardBorder.brighter (0.3f));
        g.drawVerticalLine ((int) x, getHeight() * 0.25f, (float) getHeight());

        if (bar % labelEvery == 0)
        {
            g.setColour (text);
            g.setFont (studioFont (12.0f, true));
            g.drawText (juce::String (bar + 1), (int) x + 4, 2, 40, 18, juce::Justification::centredLeft);
        }

        if (owner.pxPerBeat >= 10)
        {
            g.setColour (cardBorder);
            for (int b = 1; b < (int) bpb; ++b)
            {
                const auto bx = owner.beatToX (bar * bpb + b);
                g.drawVerticalLine ((int) bx, getHeight() * 0.62f, (float) getHeight());
            }
        }
    }

    // Playhead
    const auto px = owner.beatToX (owner.getPlayheadBeat());
    juce::Path tri;
    tri.addTriangle (px - 6.0f, 0.0f, px + 6.0f, 0.0f, px, 9.0f);
    g.setColour (gold);
    g.fillPath (tri);
    g.drawVerticalLine ((int) px, 0.0f, (float) getHeight());
}

void TimelineRuler::mouseDown (const juce::MouseEvent& e)
{
    auto* ed = owner.getEdit();
    if (ed == nullptr)
        return;

    const auto beat = owner.xToBeat ((float) e.x);
    dragStartBeat = beat;

    if (isInLoopStrip (e.y))
    {
        const auto loop = ed->getTransport().getLoopRange();
        origLoopStart = owner.timeToBeat (loop.getStart());
        origLoopEnd   = owner.timeToBeat (loop.getEnd());
        const auto x1 = owner.beatToX (origLoopStart), x2 = owner.beatToX (origLoopEnd);

        if (std::abs (e.x - x1) < 7)            drag = Drag::loopStart;
        else if (std::abs (e.x - x2) < 7)       drag = Drag::loopEnd;
        else if (e.x > x1 && e.x < x2)          drag = Drag::loopMove;
        else                                    drag = Drag::loopNew;
    }
    else
    {
        drag = Drag::position;
        owner.getEngine().setPositionSeconds (owner.beatToTime (owner.snap (beat)).inSeconds());
    }
    repaint();
}

void TimelineRuler::mouseDrag (const juce::MouseEvent& e)
{
    auto* ed = owner.getEdit();
    if (ed == nullptr)
        return;

    const auto beat = owner.xToBeat ((float) e.x);
    const auto delta = beat - dragStartBeat;
    const auto minLen = juce::jmax (1.0, owner.snapMode == 3 ? owner.getBeatsPerBar() : 1.0);
    auto start = origLoopStart, end = origLoopEnd;

    switch (drag)
    {
        case Drag::position:
            owner.getEngine().setPositionSeconds (owner.beatToTime (owner.snap (beat)).inSeconds());
            return;
        case Drag::loopStart: start = juce::jmin (owner.snap (origLoopStart + delta), end - minLen); break;
        case Drag::loopEnd:   end   = juce::jmax (owner.snap (origLoopEnd + delta), start + minLen); break;
        case Drag::loopMove:
        {
            const auto len = origLoopEnd - origLoopStart;
            start = juce::jmax (0.0, owner.snap (origLoopStart + delta));
            end = start + len;
            break;
        }
        case Drag::loopNew:
            start = owner.snap (juce::jmin (dragStartBeat, beat));
            end   = juce::jmax (start + minLen, owner.snap (juce::jmax (dragStartBeat, beat)));
            ed->getTransport().looping = true;
            break;
        case Drag::none: return;
    }

    ed->getTransport().setLoopRange ({ owner.beatToTime (juce::jmax (0.0, start)), owner.beatToTime (end) });
    repaint();
    owner.lanesView.repaint();
}

void TimelineRuler::mouseUp (const juce::MouseEvent&)
{
    drag = Drag::none;
}

void TimelineRuler::mouseMove (const juce::MouseEvent& e)
{
    auto* ed = owner.getEdit();
    if (ed == nullptr || ! isInLoopStrip (e.y))
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    const auto loop = ed->getTransport().getLoopRange();
    const auto x1 = owner.beatToX (owner.timeToBeat (loop.getStart()));
    const auto x2 = owner.beatToX (owner.timeToBeat (loop.getEnd()));
    setMouseCursor (std::abs (e.x - x1) < 7 || std::abs (e.x - x2) < 7 ? juce::MouseCursor::LeftRightResizeCursor
                                                                         : juce::MouseCursor::NormalCursor);
}

void TimelineRuler::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    owner.zoomAround (w.deltaY > 0 ? 1.15 : 1.0 / 1.15, (float) e.x);
}

//==============================================================================
//  Track header
//==============================================================================
TrackHeader::TrackHeader (TimelineView& o, te::Track& t)
    : owner (o), track (t)
{
    audioTrack = dynamic_cast<te::AudioTrack*> (&track);
    isInstrument = audioTrack != nullptr && StudioEngine::isInstrumentTrack (*audioTrack);

    name.setText (isArranger (&track) ? juce::String ("Sections") : track.getName(), juce::dontSendNotification);
    name.setFont (studioFont (14.0f, true));
    name.setColour (juce::Label::textColourId, StudioColours::text);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    if (audioTrack != nullptr)
    {
        name.setEditable (false, true, false);
        name.setInterceptsMouseClicks (true, true);
        name.addMouseListener (this, false);     // right-click on the name opens the track menu too
        name.onTextChange = [this]
        {
            owner.beginUndoStep();
            track.setName (name.getText().trim().isNotEmpty() ? name.getText().trim() : track.getName());
        };

        muteButton.setTooltip ("Mute");
        soloButton.setTooltip ("Solo");
        armButton.setTooltip (isInstrument ? "Play and record this track from the keyboard"
                                           : "Arm for recording from your audio interface (right-click the track to choose the input)");

        muteButton.onClick = [this] { owner.beginUndoStep(); audioTrack->setMute (! audioTrack->isMuted (false)); refresh(); };
        soloButton.onClick = [this] { owner.beginUndoStep(); audioTrack->setSolo (! audioTrack->isSolo (false)); refresh(); };
        armButton.onClick  = [this]
        {
            auto& eng = owner.getEngine();
            if (isInstrument)
            {
                eng.setArmedTrack (audioTrack);
            }
            else
            {
                // First time: give the track the first input nobody else is using
                if (eng.getTrackInput (*audioTrack) == nullptr)
                {
                    const auto inputs = eng.getAudioInputs();
                    if (inputs.empty())
                    {
                        Dialogs::message (&owner, "No audio input",
                                          "No audio interface inputs are available. Open Audio & MIDI Setup and choose your audio "
                                          "interface, then turn on its input channels.");
                        return;
                    }
                    te::WaveInputDevice* chosen = inputs.front();
                    for (auto* in : inputs)
                    {
                        bool used = false;
                        for (auto* other : te::getAudioTracks (*owner.getEdit()))
                            used = used || (other != audioTrack && eng.getTrackInput (*other) == in);
                        if (! used) { chosen = in; break; }
                    }
                    eng.setTrackInput (*audioTrack, chosen);
                }
                eng.setAudioArmed (*audioTrack, ! eng.isAudioArmed (*audioTrack));
            }
            owner.refreshAll();
        };

        for (auto* b : { &muteButton, &soloButton, &armButton })
        {
            b->setWantsKeyboardFocus (false);
            addAndMakeVisible (b);
        }

        // Automation (Step 13)
        autoButton.setTooltip ("Automation: show a line under this track to change the volume, pan, reverb or an effect during the song");
        autoButton.setWantsKeyboardFocus (false);
        autoButton.onClick = [this]
        {
            if (Automation::isShown (*audioTrack)) Automation::hide (*audioTrack);
            else                                   Automation::show (*audioTrack, Automation::getShownKey (*audioTrack));
            owner.refreshAll();
        };
        addAndMakeVisible (autoButton);

        autoParamButton.setTooltip ("Choose what this automation line controls");
        autoParamButton.setWantsKeyboardFocus (false);
        autoParamButton.onClick = [this] { showAutomationMenu(); };
        addChildComponent (autoParamButton);

        volume.setRange (-60.0, 6.0, 0.1);
        volume.setSkewFactorFromMidPoint (-12.0);
        volume.setDoubleClickReturnValue (true, 0.0);
        volume.setTooltip ("Volume (double-click for 0 dB)");
        volume.setColour (juce::Slider::trackColourId, track.getColour().withAlpha (0.8f));
        volume.onValueChange = [this]
        {
            if (auto* v = audioTrack->getVolumePlugin())
                v->setVolumeDb ((float) volume.getValue());
        };
        volume.onDragStart = [this] { owner.beginUndoStep(); Automation::beginGesture (Automation::resolve (*audioTrack, "vol")); };
        volume.onDragEnd   = [this] { Automation::endGesture (Automation::resolve (*audioTrack, "vol")); };
        volume.setWantsKeyboardFocus (false);
        addAndMakeVisible (volume);
    }
    else
    {
        addSectionButton.setTooltip ("Add a song section (Verse, Chorus...)");
        addSectionButton.onClick = [this] { owner.addSectionAt (owner.snap (owner.getPlayheadBeat())); };
        addSectionButton.setWantsKeyboardFocus (false);
        addAndMakeVisible (addSectionButton);
    }

    refresh();
}

void TrackHeader::refresh()
{
    if (audioTrack == nullptr)
        return;

    if (! name.isBeingEdited())
        name.setText (track.getName(), juce::dontSendNotification);

    const auto muted = audioTrack->isMuted (false);
    const auto solo  = audioTrack->isSolo (false);
    const auto armed = isInstrument ? owner.getEngine().getArmedTrack() == audioTrack
                                    : owner.getEngine().isAudioArmed (*audioTrack);

    muteButton.setColour (juce::TextButton::buttonColourId, muted ? StudioColours::warn : StudioColours::card.brighter (0.08f));
    muteButton.setColour (juce::TextButton::textColourOffId, muted ? StudioColours::header : StudioColours::text);
    soloButton.setColour (juce::TextButton::buttonColourId, solo ? StudioColours::gold : StudioColours::card.brighter (0.08f));
    soloButton.setColour (juce::TextButton::textColourOffId, solo ? StudioColours::header : StudioColours::text);
    armButton.setColour (juce::TextButton::buttonColourId, armed ? StudioColours::bad : StudioColours::card.brighter (0.08f));
    armButton.setColour (juce::TextButton::textColourOffId, armed ? StudioColours::text : StudioColours::text);

    const auto shown = Automation::isShown (*audioTrack);
    const auto automated = Automation::countAutomated (*audioTrack) > 0;
    autoButton.setColour (juce::TextButton::buttonColourId, shown ? StudioColours::text.withAlpha (0.85f)
                                                                  : StudioColours::card.brighter (0.08f));
    autoButton.setColour (juce::TextButton::textColourOffId, shown ? StudioColours::header
                                                                   : (automated ? StudioColours::gold : StudioColours::text));
    autoParamButton.setVisible (shown);
    if (shown)
        autoParamButton.setButtonText (Automation::describe (*audioTrack, Automation::getShownKey (*audioTrack))
                                         + juce::String::fromUTF8 ("  \xe2\x96\xbe"));

    if (auto* v = audioTrack->getVolumePlugin())
        if (! volume.isMouseButtonDown())
            volume.setValue (v->getVolumeDb(), juce::dontSendNotification);

    repaint();
}

void TrackHeader::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    auto b = getLocalBounds();

    const auto selected = owner.selectedTrack == &track;
    g.setColour (selected ? card.brighter (0.07f) : (isArranger (&track) ? header : card));
    g.fillRect (b);

    g.setColour (cardBorder);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    if (audioTrack != nullptr)
    {
        g.setColour (track.getColour());
        g.fillRect (b.removeFromLeft (5));

        g.setColour (textDim);
        g.setFont (studioFont (11.5f));
        auto& eng = owner.getEngine();
        const auto armed = isInstrument ? eng.getArmedTrack() == audioTrack : eng.isAudioArmed (*audioTrack);
        juce::String description;
        if (isInstrument)
            description = StudioEngine::describeInstrument (*audioTrack);
        else if (auto* in = eng.getTrackInput (*audioTrack))
            description = "In: " + StudioEngine::describeInput (in);
        else
            description = "Audio  -  no input";
        if (const auto fx = StudioEngine::getEffects (*audioTrack).size(); fx > 0)
            description << "  +" << (int) fx << " FX";
        if (auto* bus = Mixing::getBusOf (*audioTrack))
            description = juce::String::fromUTF8 ("\xe2\x86\x92 ") + bus->getName() + "  -  " + description;
        if (Mixing::getDuckingSource (*audioTrack) != nullptr)
            description << "  (ducks)";
        if (audioTrack->isFrozen (te::Track::individualFreeze))
            description = "FROZEN  -  " + description;
        if (description.contains ("not downloaded") || description.contains ("not installed")) g.setColour (warn);

        auto textArea = juce::Rectangle<int> (14, 26, getWidth() - 20, 14);
        if (! isInstrument && armed)
        {
            // Input level meter while armed
            const auto level = eng.getTrackInputLevel (*audioTrack);
            meterLevel = juce::jmax (level, meterLevel * 0.85f);
            auto meter = textArea.removeFromRight (64).reduced (0, 3).toFloat();
            g.setColour (header);
            g.fillRoundedRectangle (meter, 2.0f);
            g.setColour (meterLevel > 0.95f ? bad : (meterLevel > 0.8f ? warn : good));
            g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * meterLevel), 2.0f);
            textArea.removeFromRight (6);
            g.setColour (textDim);
        }
        g.drawText (description + (armed ? "  -  armed" : ""), textArea, juce::Justification::centredLeft, true);
    }

    if (selected)
    {
        g.setColour (gold.withAlpha (0.8f));
        g.drawRect (getLocalBounds(), 1);
    }
}

void TrackHeader::resized()
{
    auto b = getLocalBounds().reduced (8, 4);

    if (audioTrack == nullptr)
    {
        addSectionButton.setBounds (b.removeFromRight (86).reduced (0, 2));
        name.setBounds (b);
        return;
    }

    b.removeFromLeft (4);
    auto top = b.removeFromTop (22);
    name.setBounds (top);

    b.removeFromTop (16);
    auto row = b.removeFromTop (20);
    for (auto* btn : { &muteButton, &soloButton, &armButton, &autoButton })
    {
        btn->setBounds (row.removeFromLeft (26));
        row.removeFromLeft (4);
    }
    row.removeFromLeft (2);
    volume.setBounds (row);

    if (getHeight() > TimelineView::trackHeight)
        autoParamButton.setBounds (14, TimelineView::trackHeight + 8, getWidth() - 24, 24);
}

void TrackHeader::showAutomationMenu()
{
    if (audioTrack == nullptr)
        return;

    const auto choices = Automation::choicesFor (*audioTrack);
    const auto current = Automation::getShownKey (*audioTrack);
    const auto mark = [] (const Automation::Choice& c) { return c.name + (c.hasPoints ? juce::String::fromUTF8 ("  \xe2\x97\x8f") : juce::String()); };

    juce::PopupMenu menu;
    menu.addSectionHeader ("Mixer");
    std::map<juce::String, juce::PopupMenu> groups;
    std::vector<juce::String> order;
    for (int i = 0; i < (int) choices.size(); ++i)
    {
        const auto& c = choices[(size_t) i];
        if (c.group.isEmpty())
        {
            menu.addItem (i + 1, mark (c), true, c.key == current);
        }
        else
        {
            if (groups.find (c.group) == groups.end())
                order.push_back (c.group);
            groups[c.group].addItem (i + 1, mark (c), true, c.key == current);
        }
    }
    if (! order.empty())
    {
        menu.addSectionHeader ("Effects and plugins");
        for (auto& g : order)
            menu.addSubMenu (g, groups[g]);
    }
    menu.addSeparator();
    menu.addItem (10000, "Remove all points on this line");
    menu.addItem (10001, "Hide the automation lane");

    juce::Component::SafePointer<TrackHeader> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&autoParamButton), [safe, choices] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        auto& t = *safe->audioTrack;
        if (r == 10000)
        {
            if (auto* p = Automation::resolve (t, Automation::getShownKey (t)))
            {
                safe->owner.beginUndoStep();
                p->getCurve().clear (&t.edit.getUndoManager());
            }
        }
        else if (r == 10001)
        {
            Automation::hide (t);
        }
        else if (r - 1 < (int) choices.size())
        {
            Automation::show (t, choices[(size_t) (r - 1)].key);
        }
        safe->owner.refreshAll();
    });
}

void TrackHeader::mouseDown (const juce::MouseEvent& e)
{
    owner.selectTrack (&track);
    if (e.mods.isPopupMenu())
    {
        showMenu();
        return;
    }

    // Clicking the instrument name under the track name opens its settings
    const auto p = e.getEventRelativeTo (this).getPosition();
    if (isInstrument && audioTrack != nullptr && p.y >= 24 && p.y <= 42 && p.x > 12)
        InstrumentPanel::open (owner.getEngine(), *audioTrack);
}

void TrackHeader::showMenu()
{
    if (audioTrack == nullptr)
        return;

    juce::PopupMenu menu, instruments;
    if (isInstrument)
    {
        const auto current = StudioEngine::getInstrumentId (*audioTrack);
        juce::String group;
        for (size_t i = 0; i < Instruments::all().size(); ++i)
        {
            const auto& c = Instruments::all()[i];
            if (group != c.group) { instruments.addSectionHeader (c.group); group = c.group; }
            instruments.addItem (200 + (int) i, c.name, true, current == c.id);
        }
        const auto pluginInstruments = PluginHost::getInstruments (owner.getEngine().getEngine());
        const auto* currentPlugin = StudioEngine::getInstrumentPlugin (*audioTrack);
        instruments.addSectionHeader ("Your plugins (VST3)");
        for (int i = 0; i < pluginInstruments.size(); ++i)
        {
            const auto& d = pluginInstruments.getReference (i);
            const bool ticked = PluginHost::isPlugin (currentPlugin)
                                 && currentPlugin->state[te::IDs::name].toString() == d.name;
            instruments.addItem (3000 + i, d.name + (d.manufacturerName.isNotEmpty() ? "  (" + d.manufacturerName + ")" : juce::String()), true, ticked);
        }
        if (pluginInstruments.isEmpty())
            instruments.addItem (2999, "None yet: open Plugins and press Scan", false, false);
        instruments.addItem (9, "Plugins...");

        menu.addItem (5, PluginHost::isPlugin (currentPlugin) ? "Open the plugin's window..." : "Instrument settings...");
        menu.addSubMenu ("Change instrument", instruments);
        menu.addItem (6, "Sound Library...");
        menu.addSeparator();
    }

    // Effects after the instrument (or on the recording)
    juce::PopupMenu effects;
    const auto effectList = PluginHost::getEffects (owner.getEngine().getEngine());
    const auto trackEffects = StudioEngine::getEffects (*audioTrack);

    for (int i = 0; i < (int) trackEffects.size() && i < 50; ++i)
    {
        auto* fx = trackEffects[(size_t) i];
        juce::PopupMenu one;
        one.addItem (5000 + i * 10, "Open window");
        one.addItem (5001 + i * 10, fx->isEnabled() ? "Turn off (bypass)" : "Turn on again");
        one.addItem (5002 + i * 10, "Move earlier", i > 0);
        one.addItem (5003 + i * 10, "Move later", i + 1 < (int) trackEffects.size());
        one.addSeparator();
        one.addItem (5004 + i * 10, "Remove");
        effects.addSubMenu (juce::String (i + 1) + ". " + PluginHost::describe (*fx), one);
    }
    if (! trackEffects.empty())
        effects.addSeparator();
    effects.addSectionHeader ("Add a built-in effect");
    for (int i = 0; i < (int) Mixing::builtInEffects().size(); ++i)
        effects.addItem (6000 + i, Mixing::builtInEffects()[(size_t) i].name);
    effects.addSectionHeader ("Add a plugin effect (VST3)");
    for (int i = 0; i < effectList.size(); ++i)
    {
        const auto& d = effectList.getReference (i);
        effects.addItem (4000 + i, d.name + (d.manufacturerName.isNotEmpty() ? "  (" + d.manufacturerName + ")" : juce::String()));
    }
    if (effectList.isEmpty())
        effects.addItem (3999, "None yet: open Plugins and press Scan", false, false);
    effects.addSeparator();
    effects.addItem (9, "Plugins...");
    menu.addSubMenu ("Effects" + (trackEffects.empty() ? juce::String() : " (" + juce::String ((int) trackEffects.size()) + ")"), effects);

    // Vocal tuning (Step 16), for sung parts
    if (! StudioEngine::isInstrumentTrack (*audioTrack))
    {
        bool hasTuning = false;
        for (auto* fx : trackEffects)
            hasTuning = hasTuning || dynamic_cast<VocalTunePlugin*> (fx) != nullptr;
        menu.addItem (480, hasTuning ? "Vocal tuning (open)..." : "Vocal tuning (correct the pitch)...");

        // Vocal chain (Step 12): EQ, compressor, de-esser and reverb in one click
        juce::PopupMenu chains;
        const auto current = Mixing::currentVocalChain (*audioTrack);
        for (size_t i = 0; i < Mixing::vocalChains().size(); ++i)
        {
            const auto& c = Mixing::vocalChains()[i];
            chains.addItem (800 + (int) i, juce::String (c.name) + "   (" + c.hint + ")", true, current == c.name);
        }
        chains.addSeparator();
        chains.addItem (799, "Remove the vocal chain", current.isNotEmpty());
        menu.addSubMenu (current.isNotEmpty() ? "Vocal chain: " + current : juce::String ("Vocal chain (EQ, compressor, de-esser, reverb)"), chains);
    }

    // Output: straight to the master or into a group bus; ducking under another track (plan Step 14)
    {
        auto* ed = owner.getEdit();
        juce::PopupMenu output, newBus, duck, duckKinds;
        auto* currentBus = Mixing::getBusOf (*audioTrack);
        output.addItem (829, "Master (no bus)", true, currentBus == nullptr);
        const auto buses = Mixing::getGroupBuses (*ed);
        for (int i = 0; i < buses.size() && i < 20; ++i)
            output.addItem (830 + i, buses[i]->getName() + "  (" + juce::String (Mixing::getBusMembers (*buses[i]).size()) + " tracks)",
                            true, buses[i] == currentBus);
        output.addSeparator();
        for (size_t i = 0; i < Mixing::busKinds().size(); ++i)
            newBus.addItem (850 + (int) i, juce::String ("New ") + Mixing::busKinds()[i].name + "   (" + Mixing::busKinds()[i].hint + ")");
        output.addSubMenu ("Send it into a new bus", newBus);
        menu.addSubMenu (currentBus != nullptr ? "Output: " + currentBus->getName() : juce::String ("Output: Master"), output);

        auto* source = Mixing::getDuckingSource (*audioTrack);
        duck.addItem (869, "Off", true, source == nullptr);
        duck.addSeparator();
        duck.addSectionHeader ("Get quieter whenever this one plays:");
        juce::Array<te::AudioTrack*> sources (Mixing::getSongTracks (*ed));
        sources.addArray (buses);
        for (int i = 0; i < sources.size() && i < 30; ++i)
            if (sources[i] != audioTrack)
                duck.addItem (870 + i, sources[i]->getName(), true, sources[i] == source);
        const auto kind = Mixing::getDuckingKind (*audioTrack);
        for (size_t i = 0; i < Mixing::duckKinds().size(); ++i)
            duckKinds.addItem (900 + (int) i, juce::String (Mixing::duckKinds()[i].name) + "   (" + Mixing::duckKinds()[i].hint + ")",
                               source != nullptr, kind == (int) i);
        duck.addSeparator();
        duck.addSubMenu ("How", duckKinds, source != nullptr);
        menu.addSubMenu (source != nullptr ? "Ducks under: " + source->getName() : juce::String ("Duck under another track (sidechain)"), duck);
    }

    // Freeze (Step 14): play from a recording of the track to save computer power
    if (audioTrack->isFrozen (te::Track::individualFreeze))
        menu.addItem (471, "Unfreeze (to change notes, sounds or effects)");
    else
        menu.addItem (470, "Freeze (saves computer power on heavy tracks)", ! audioTrack->getClips().isEmpty());
    menu.addSeparator();
    juce::PopupMenu inputs, monitoring;
    if (! isInstrument)
    {
        auto& eng = owner.getEngine();
        const auto current = eng.getTrackInput (*audioTrack);
        inputs.addItem (400, "No input", true, current == nullptr);
        const auto list = eng.getAudioInputs();
        for (size_t i = 0; i < list.size(); ++i)
            inputs.addItem (401 + (int) i, StudioEngine::describeInput (list[i]), true, list[i] == current);
        if (list.empty())
            inputs.addItem (399, "No inputs: choose your audio interface in Audio & MIDI Setup", false, false);
        inputs.addSeparator();
        inputs.addItem (450, "Inputs as single channels (mono: vocals, guitar)", true, eng.areInputsMono());
        inputs.addItem (451, "Inputs as stereo pairs (keyboard, stereo sources)", true, ! eng.areInputsMono());

        const auto mode = eng.getMonitorMode (*audioTrack);
        monitoring.addItem (460, "Auto: hear the input while armed", current != nullptr, mode == te::InputDevice::MonitorMode::automatic);
        monitoring.addItem (461, "Always on", current != nullptr, mode == te::InputDevice::MonitorMode::on);
        monitoring.addItem (462, "Off (use your interface's direct monitoring)", current != nullptr, mode == te::InputDevice::MonitorMode::off);

        menu.addSubMenu ("Input", inputs);
        menu.addSubMenu ("Monitoring", monitoring);
        menu.addItem (7, "Import audio file at the playhead...");
        menu.addItem (490, "Import a loop (follows the song's tempo)...");
        menu.addItem (8, "Place the song's voice memo at the playhead", owner.getMemoFile && owner.getMemoFile().existsAsFile());
        menu.addSeparator();
    }
    menu.addItem (1, "Rename...");
    addColourMenu (menu, 100);
    menu.addSeparator();
    menu.addItem (2, "Move up");
    menu.addItem (3, "Move down");
    menu.addSeparator();
    menu.addItem (4, "Delete track...");

    juce::Component::SafePointer<TrackHeader> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        auto& view = safe->owner;
        auto* ed = view.getEdit();
        auto* t = safe->audioTrack;
        if (ed == nullptr)
            return;

        const auto tracks = Mixing::getSongTracks (*ed);
        const auto index = tracks.indexOf (t);

        if (result == 1)
        {
            askForName (&view, "Rename track", t->getName(), [&view, t] (const juce::String& n)
            {
                view.beginUndoStep();
                t->setName (n);
                view.refreshAll();
            });
        }
        else if (result == 5)
        {
            InstrumentPanel::open (view.getEngine(), *t);
        }
        else if (result == 9)
        {
            PluginHost::openManager (view.getEngine().getEngine());
        }
        else if (result >= 3000 && result < 3999)
        {
            const auto list = PluginHost::getInstruments (view.getEngine().getEngine());
            if (result - 3000 < list.size())
            {
                view.beginUndoStep();
                InstrumentPanel::closeAll();
                if (auto* p = view.getEngine().setPluginInstrument (*t, list.getReference (result - 3000)))
                    PluginHost::showWindow (*p);
                view.refreshAll();
            }
        }
        else if (result >= 4000 && result < 5000)
        {
            const auto list = PluginHost::getEffects (view.getEngine().getEngine());
            if (result - 4000 < list.size())
            {
                view.beginUndoStep();
                if (auto* p = view.getEngine().addEffect (*t, list.getReference (result - 4000)))
                    PluginHost::showWindow (*p);
                view.refreshAll();
            }
        }
        else if (result >= 829 && result < 860)
        {
            view.beginUndoStep();
            if (result == 829)
                Mixing::routeToBus (*t, nullptr);
            else if (result < 850)
            {
                const auto buses = Mixing::getGroupBuses (*ed);
                if (juce::isPositiveAndBelow (result - 830, buses.size()))
                    Mixing::routeToBus (*t, buses[result - 830]);
            }
            else if (auto* bus = Mixing::createGroupBus (*ed, result - 850))
                Mixing::routeToBus (*t, bus);
            view.refreshAll();
        }
        else if (result >= 869 && result < 910)
        {
            view.beginUndoStep();
            if (result == 869)
                Mixing::setDucking (*t, nullptr, 0);
            else if (result < 900)
            {
                juce::Array<te::AudioTrack*> sources (Mixing::getSongTracks (*ed));
                sources.addArray (Mixing::getGroupBuses (*ed));
                if (juce::isPositiveAndBelow (result - 870, sources.size()))
                {
                    // Voices duck the music smoothly; drums duck tightly
                    auto* src = sources[result - 870];
                    const auto sourceName = src->getName().toLowerCase();
                    const int kind = Mixing::getDuckingKind (*t) >= 0 ? Mixing::getDuckingKind (*t)
                                   : (sourceName.contains ("kick") || sourceName.contains ("drum")) ? 1
                                   : (sourceName.contains ("voc") || sourceName.contains ("preach") || sourceName.contains ("voice") || sourceName.contains ("speak")) ? 2 : 0;
                    Mixing::setDucking (*t, src, kind);
                }
            }
            else if (auto* src = Mixing::getDuckingSource (*t))
                Mixing::setDucking (*t, src, result - 900);
            view.refreshAll();
        }
        else if (result >= 799 && result < 820)
        {
            view.beginUndoStep();
            if (result == 799) Mixing::removeVocalChain (*t);
            else               Mixing::applyVocalChain (*t, result - 800);
            view.refreshAll();
        }
        else if (result == 480)
        {
            te::Plugin* tuning = nullptr;
            for (auto* fx : StudioEngine::getEffects (*t))
                if (dynamic_cast<VocalTunePlugin*> (fx) != nullptr)
                    tuning = fx;
            if (tuning == nullptr)
            {
                view.beginUndoStep();
                tuning = view.getEngine().addEffect (*t, ed->getPluginCache().createNewPlugin (VocalTunePlugin::xmlTypeName, {}));
                view.refreshAll();
            }
            if (tuning != nullptr)
                PluginHost::showWindow (*tuning);
        }
        else if (result == 470 || result == 471)
        {
            ed->getTransport().stop (false, true);
            const auto r = t->setFrozen (result == 470, te::Track::individualFreeze);
            if (r.failed())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Freeze", r.getErrorMessage());
            view.refreshAll();
        }
        else if (result >= 6000 && result < 6100)
        {
            const auto effectIndex = (size_t) (result - 6000);
            if (effectIndex < Mixing::builtInEffects().size())
            {
                view.beginUndoStep();
                if (auto* p = view.getEngine().addEffect (*t, Mixing::createBuiltIn (*ed, Mixing::builtInEffects()[effectIndex].type)))
                    PluginHost::showWindow (*p);
                view.refreshAll();
            }
        }
        else if (result >= 5000 && result < 5500)
        {
            const auto fxs = StudioEngine::getEffects (*t);
            const auto effectIndex = (size_t) ((result - 5000) / 10);
            const auto action = (result - 5000) % 10;
            if (effectIndex < fxs.size())
            {
                auto* fx = fxs[effectIndex];
                if (action == 0)
                {
                    PluginHost::showWindow (*fx);
                }
                else
                {
                    view.beginUndoStep();
                    if (action == 1)      fx->setEnabled (! fx->isEnabled());
                    else if (action == 2) StudioEngine::moveEffect (*fx, -1);
                    else if (action == 3) StudioEngine::moveEffect (*fx, 1);
                    else if (action == 4) StudioEngine::removeEffect (*fx);
                    view.refreshAll();
                }
            }
        }
        else if (result >= 400 && result < 450)
        {
            const auto list = view.getEngine().getAudioInputs();
            view.getEngine().setTrackInput (*t, result == 400 ? nullptr : list[(size_t) (result - 401)]);
            view.refreshAll();
        }
        else if (result == 450 || result == 451)
        {
            view.getEngine().setInputsMono (result == 450);
            view.refreshAll();
        }
        else if (result >= 460 && result <= 462)
        {
            view.getEngine().setMonitorMode (*t, result == 460 ? te::InputDevice::MonitorMode::automatic
                                                 : result == 461 ? te::InputDevice::MonitorMode::on
                                                                 : te::InputDevice::MonitorMode::off);
        }
        else if (result == 490)
        {
            view.importAudioTo (*t, true);
        }
        else if (result == 7)
        {
            view.importAudioTo (*t);
        }
        else if (result == 8)
        {
            view.beginUndoStep();
            if (auto* c = view.getEngine().importAudio (*t, view.getMemoFile(), view.getEngine().getPosition().seconds))
            {
                c->setName ("Voice memo");
                view.selectClip (c);
            }
            view.refreshAll();
        }
        else if (result == 6)
        {
            SoundLibrary::openWindow();
        }
        else if (result >= 200)
        {
            view.beginUndoStep();
            view.getEngine().setInstrument (*t, Instruments::all()[(size_t) (result - 200)].id);
            view.refreshAll();
        }
        else if (result >= 100)
        {
            view.beginUndoStep();
            t->setColour (palette[result - 100]);
            view.rebuildHeaders();
        }
        else if (result == 2 && index > 0)
        {
            view.beginUndoStep();
            ed->moveTrack (t, te::TrackInsertPoint (*tracks[index - 1], true));
            view.rebuildHeaders();
        }
        else if (result == 3 && index >= 0 && index < tracks.size() - 1)
        {
            view.beginUndoStep();
            ed->moveTrack (t, te::TrackInsertPoint (*tracks[index + 1], false));
            view.rebuildHeaders();
        }
        else if (result == 4)
        {
            juce::Component::SafePointer<TimelineView> safeOwner (&view);
            Dialogs::confirm (&view, "Delete track",
                              "Delete the track \"" + t->getName() + "\" and everything on it?\n"
                              "You can bring it back with Undo.",
                              "Delete", "Cancel",
                              [safeOwner, t]
                              {
                                  if (safeOwner == nullptr)
                                      return;
                                  safeOwner->beginUndoStep();
                                  safeOwner->selectedTrack = nullptr;
                                  safeOwner->selectedClip = nullptr;
                                  safeOwner->getEngine().deleteTrack (*t);
                                  safeOwner->rebuildHeaders();
                              },
                              juce::MessageBoxIconType::WarningIcon);
        }
    });
}

//==============================================================================
//  Lanes
//==============================================================================
juce::Rectangle<float> TimelineLanes::clipBounds (te::Clip& clip, int laneIndex) const
{
    const auto pos = clip.getPosition();
    const auto x1 = owner.beatToX (owner.timeToBeat (pos.getStart()));
    const auto x2 = owner.beatToX (owner.timeToBeat (pos.getEnd()));
    const auto y = (float) (owner.getLaneY (laneIndex) - owner.scrollY);
    const auto h = laneIndex >= owner.firstTrackLane ? owner.trackHeight : owner.getLaneHeight (laneIndex);
    return { x1, y + 3.0f, juce::jmax (3.0f, x2 - x1), (float) h - 6.0f };
}

te::Clip* TimelineLanes::clipAt (juce::Point<int> p, int& laneIndex) const
{
    laneIndex = owner.laneAtY (p.y + owner.scrollY);
    if (auto* ct = dynamic_cast<te::ClipTrack*> (owner.getLane (laneIndex)))
    {
        const auto clips = ct->getClips();
        for (int i = clips.size(); --i >= 0;)
            if (clipBounds (*clips[i], laneIndex).contains (p.toFloat()))
                return clips[i];
    }
    return nullptr;
}

void TimelineLanes::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    auto* ed = owner.getEdit();
    g.fillAll (background);
    if (ed == nullptr)
        return;

    const auto bpb = owner.getBeatsPerBar();
    int totalClips = 0;

    // Lane backgrounds
    for (int i = 0; i < owner.getNumLanes(); ++i)
    {
        const auto y = owner.getLaneY (i) - owner.scrollY;
        const auto h = owner.getLaneHeight (i);
        if (y + h < 0 || y > getHeight())
            continue;

        auto* lane = owner.getLane (i);
        g.setColour (i < owner.firstTrackLane ? header.withAlpha (0.7f)
                            : (lane == owner.selectedTrack ? card.brighter (0.03f) : (i % 2 ? background : background.brighter (0.02f))));
        g.fillRect (0, y, getWidth(), h);

        if (i == dragTargetLane && drag == Drag::move && i >= owner.firstTrackLane)
        {
            g.setColour (gold.withAlpha (0.08f));
            g.fillRect (0, y, getWidth(), h);
        }

        g.setColour (cardBorder.withAlpha (0.7f));
        g.drawHorizontalLine (y + h - 1, 0.0f, (float) getWidth());
    }

    // Grid
    const auto firstBar = (int) std::floor (owner.scrollBeats / bpb);
    for (int bar = juce::jmax (0, firstBar); ; ++bar)
    {
        const auto x = owner.beatToX (bar * bpb);
        if (x > getWidth())
            break;

        g.setColour (cardBorder.withAlpha (0.9f));
        g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());

        if (owner.pxPerBeat >= 14)
        {
            g.setColour (cardBorder.withAlpha (0.35f));
            for (int b = 1; b < (int) bpb; ++b)
                g.drawVerticalLine ((int) owner.beatToX (bar * bpb + b), 0.0f, (float) getHeight());
        }
    }

    // Loop region shading
    if (ed->getTransport().looping.get())
    {
        const auto loop = ed->getTransport().getLoopRange();
        const auto x1 = owner.beatToX (owner.timeToBeat (loop.getStart()));
        const auto x2 = owner.beatToX (owner.timeToBeat (loop.getEnd()));
        g.setColour (gold.withAlpha (0.05f));
        g.fillRect (x1, 0.0f, x2 - x1, (float) getHeight());
    }

    // Clips
    for (int i = 0; i < owner.getNumLanes(); ++i)
    {
        auto* ct = dynamic_cast<te::ClipTrack*> (owner.getLane (i));
        if (ct == nullptr)
            continue;

        for (auto* clip : ct->getClips())
        {
            if (i >= owner.firstTrackLane)
                ++totalClips;
            auto r = clipBounds (*clip, i);
            if (r.getRight() < 0 || r.getX() > getWidth() || r.getBottom() < 0 || r.getY() > getHeight())
                continue;

            const auto colour = i == 0 ? clip->getColour() : ct->getColour();
            const auto selected = owner.selectedClip.get() == clip;

            g.setColour (colour.withAlpha (i == 0 ? 0.55f : 0.30f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (selected ? text : colour);
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, selected ? 2.0f : 1.0f);

            // Waveform inside audio clips
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (clip))
            {
                // Draw from the original recording (pitch/speed changes play from a prepared copy)
                const te::AudioFile file (owner.getEdit()->engine, wc->getSourceFileReference().getFile());
                auto& thumb = thumbnails[clip->itemID];
                if (thumb == nullptr)
                    thumb = std::make_unique<te::SmartThumbnail> (owner.getEdit()->engine, file, *this, nullptr);   // no Edit*: safe when songs change
                else if (thumbnailFiles[clip->itemID] != file.getFile())
                    thumb->setNewFile (file);
                thumbnailFiles[clip->itemID] = file.getFile();

                auto area = r.reduced (2.0f, 3.0f).withTrimmedTop (13.0f).toNearestInt();
                const auto pos = clip->getPosition();
                g.saveState();
                g.reduceClipRegion (r.toNearestInt());
                g.setColour (colour.brighter (0.5f));
                const auto speed = juce::jmax (0.01, wc->getSpeedRatio());
                const auto loopLen = wc->isLooping() && ! wc->getAutoTempo() ? wc->getLoopRange().getLength().inSeconds() : 0.0;
                if (loopLen > 0.01)
                {
                    // A loop: draw each repeat, with a thin line where it starts again
                    const auto loopStart = wc->getLoopRange().getStart();
                    for (double t = 0.0; t < pos.getLength().inSeconds() - 1.0e-4; t += loopLen)
                    {
                        const auto x0 = owner.beatToX (owner.timeToBeat (pos.getStart() + te::TimeDuration::fromSeconds (t)));
                        const auto len = juce::jmin (loopLen, pos.getLength().inSeconds() - t);
                        const auto x1 = owner.beatToX (owner.timeToBeat (pos.getStart() + te::TimeDuration::fromSeconds (t + len)));
                        auto sub = area.withX ((int) x0).withRight ((int) x1);
                        g.setColour (colour.brighter (0.5f));
                        thumb->drawChannels (g, sub, { loopStart, loopStart + te::TimeDuration::fromSeconds (len) },
                                             juce::Decibels::decibelsToGain (wc->getGainDB()));
                        if (t > 0.0)
                        {
                            g.setColour (colour.brighter (0.8f).withAlpha (0.7f));
                            g.drawVerticalLine ((int) x0, r.getY() + 13.0f, r.getBottom() - 2.0f);
                        }
                    }
                }
                else
                {
                    const auto srcStart = te::TimePosition::fromSeconds (pos.getOffset().inSeconds() * speed);
                    thumb->drawChannels (g, area, { srcStart, srcStart + te::TimeDuration::fromSeconds (pos.getLength().inSeconds() * speed) },
                                         juce::Decibels::decibelsToGain (wc->getGainDB()));
                }
                g.restoreState();

                // Fades: shaded corners with a handle on each
                const auto clipStart = clip->getPosition().getStart(), clipEnd = clip->getPosition().getEnd();
                const auto fi = owner.beatToX (owner.timeToBeat (clipStart + wc->getFadeIn()));
                const auto fo = owner.beatToX (owner.timeToBeat (clipEnd - wc->getFadeOut()));
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                if (fi > r.getX() + 1.0f)
                {
                    juce::Path p;
                    p.startNewSubPath (r.getX(), r.getY());
                    p.lineTo (fi, r.getY());
                    p.lineTo (r.getX(), r.getBottom());
                    p.closeSubPath();
                    g.fillPath (p);
                }
                if (fo < r.getRight() - 1.0f)
                {
                    juce::Path p;
                    p.startNewSubPath (r.getRight(), r.getY());
                    p.lineTo (fo, r.getY());
                    p.lineTo (r.getRight(), r.getBottom());
                    p.closeSubPath();
                    g.fillPath (p);
                }
                if (selected || r.getWidth() > 40)
                {
                    g.setColour (selected ? text : colour.brighter (0.6f));
                    g.fillRect (juce::Rectangle<float> (juce::jmax (r.getX() + 2.0f, fi - 3.0f), r.getY() + 2.0f, 6.0f, 6.0f));
                    g.fillRect (juce::Rectangle<float> (juce::jmin (r.getRight() - 8.0f, fo - 3.0f), r.getY() + 2.0f, 6.0f, 6.0f));
                }

                // Gain, pitch and speed at a glance
                juce::StringArray badges;
                if (Loops::isFollowing (*wc))
                    badges.add (juce::String::fromUTF8 ("\xe2\x86\xbb ") + juce::String (juce::roundToInt (Loops::getLoopTempo (*wc))) + " BPM"
                                + (Loops::isFollowingKey (*wc) ? " key" : ""));
                if (std::abs (wc->getGainDB()) > 0.05f)        badges.add (juce::String (wc->getGainDB(), 1) + " dB");
                if (std::abs (wc->getPitchChange()) > 0.005f)  badges.add ((wc->getPitchChange() > 0 ? "+" : "") + juce::String (wc->getPitchChange(), 1) + " st");
                if (std::abs (wc->getSpeedRatio() - 1.0) > 0.001) badges.add (juce::String (juce::roundToInt (wc->getSpeedRatio() * 100.0)) + "%");
                if (! badges.isEmpty() && r.getWidth() > 30)
                {
                    const bool narrow = r.getWidth() < 110;
                    auto label = badges.joinIntoString (narrow ? " " : "  ");
                    if (narrow) label = label.replace (" st", "").replace (" dB", "dB");
                    g.setColour (text.withAlpha (0.9f));
                    g.setFont (studioFont (narrow ? 9.5f : 10.5f, true));
                    g.drawText (label, r.reduced (4.0f, 3.0f).toNearestInt(), juce::Justification::bottomRight, true);
                }

                const auto takes = Takes::count (*wc);
                if (takes > 1 && r.getWidth() > 90)
                {
                    g.setColour (gold);
                    g.setFont (studioFont (11.0f, true));
                    g.drawText ("Take " + juce::String (Takes::current (*wc) + 1) + "/" + juce::String (takes),
                                r.reduced (6.0f, 2.0f).toNearestInt(), juce::Justification::topRight, true);
                }
            }

            // Notes inside MIDI clips
            if (auto* mc = dynamic_cast<te::MidiClip*> (clip))
            {
                const auto& notes = mc->getSequence().getNotes();
                if (! notes.isEmpty())
                {
                    int lo = 127, hi = 0;
                    for (auto* n : notes) { lo = juce::jmin (lo, n->getNoteNumber()); hi = juce::jmax (hi, n->getNoteNumber()); }
                    lo -= 1; hi += 1;

                    auto area = r.reduced (3.0f, 4.0f).withTrimmedTop (14.0f);
                    const auto rowH = area.getHeight() / (float) juce::jmax (1, hi - lo + 1);

                    g.setColour (colour.brighter (0.4f));
                    g.saveState();
                    g.reduceClipRegion (r.toNearestInt());
                    for (auto* n : notes)
                    {
                        const auto range = n->getEditTimeRange (*mc);
                        const auto nx1 = owner.beatToX (owner.timeToBeat (range.getStart()));
                        const auto nx2 = owner.beatToX (owner.timeToBeat (range.getEnd()));
                        const auto ny = area.getBottom() - (float) (n->getNoteNumber() - lo + 1) * rowH;
                        g.fillRect (nx1, ny, juce::jmax (1.5f, nx2 - nx1), juce::jmax (1.5f, rowH * 0.8f));
                    }
                    g.restoreState();
                }
            }

            if (r.getWidth() > 24)
            {
                g.setColour (i == 0 ? header : text);
                g.setFont (studioFont (i == 0 ? 12.5f : 11.5f, true));
                g.drawText (clip->getName(), r.reduced (6.0f, 2.0f).toNearestInt(),
                            i == 0 ? juce::Justification::centredLeft : juce::Justification::topLeft, true);
            }
        }
    }

    if (totalClips == 0 && owner.getNumLanes() > 1)
    {
        g.setColour (textDim);
        g.setFont (studioFont (13.5f));
        g.drawText ("Press Record (R) and play your keyboard, or double-click an instrument lane to add an empty clip.",
                    getLocalBounds().withTrimmedTop (owner.getLaneY (owner.firstTrackLane) - owner.scrollY).removeFromTop (owner.trackHeight),
                    juce::Justification::centred, true);
    }

    // While recording: a red band grows on each armed track
    if (owner.getEngine().isRecording())
    {
        const auto x1 = owner.beatToX (owner.timeToBeat (te::TimePosition::fromSeconds (owner.getEngine().getRecordStartSeconds())));
        const auto x2 = owner.beatToX (owner.getPlayheadBeat());
        for (int i = owner.firstTrackLane; i < owner.getNumLanes(); ++i)
            if (auto* at = dynamic_cast<te::AudioTrack*> (owner.getLane (i)))
            {
                const auto inst = StudioEngine::isInstrumentTrack (*at);
                if ((inst && owner.getEngine().getArmedTrack() == at) || (! inst && owner.getEngine().isAudioArmed (*at)))
                {
                    const auto y = owner.getLaneY (i) - owner.scrollY;
                    g.setColour (bad.withAlpha (0.25f));
                    g.fillRect (juce::Rectangle<float> (x1, (float) y + 4.0f, juce::jmax (0.0f, x2 - x1), (float) owner.trackHeight - 8.0f));
                }
            }
    }

    paintChords (g);

    // Playhead
    const auto px = owner.beatToX (owner.getPlayheadBeat());
    g.setColour (gold);
    g.drawVerticalLine ((int) px, 0.0f, (float) getHeight());
}

void TimelineLanes::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    // ---- Chord lane ----------------------------------------------------------
    if (owner.laneAtY (e.y + owner.scrollY) == owner.chordLane && owner.getEdit() != nullptr)
    {
        owner.selectClip (nullptr);
        const auto events = ChordTrack::getEvents (*owner.getEdit());
        const auto index = chordAt (e.getPosition(), events);
        const auto beat = owner.xToBeat ((float) e.x);

        if (index < 0)
        {
            owner.clearChordSelection();
            if (e.mods.isPopupMenu())
                showChordQuickMenu (owner.snap (beat));
            else
                owner.getEngine().setPositionSeconds (owner.beatToTime (owner.snap (beat)).inSeconds());
            return;
        }

        const auto& ev = events[(size_t) index];
        const auto alreadySelected = std::any_of (owner.selectedChords.begin(), owner.selectedChords.end(),
                                                  [&ev] (const juce::ValueTree& v) { return v == ev.node; });
        if (! (e.mods.isPopupMenu() && alreadySelected))
            owner.selectChord (ev.node, e.mods.isShiftDown());

        if (e.mods.isPopupMenu())
        {
            showChordMenu (ev);
            return;
        }

        if (! e.mods.isShiftDown())
            owner.auditionChord (ev.chord);

        const auto r = chordBounds (ev);
        drag = (e.x - r.getX() < 7 && r.getWidth() > 20) ? Drag::trimStart
             : (r.getRight() - e.x < 7 && r.getWidth() > 20) ? Drag::trimEnd
             : Drag::move;

        owner.beginUndoStep();
        dragChord = ev.node;
        dragStartBeat = beat;
        chordOrigStart = ev.start;
        chordOrigEnd = ev.start + ev.length;
        return;
    }

    dragChord = {};
    owner.clearChordSelection();

    int lane = -1;
    auto* clip = clipAt (e.getPosition(), lane);
    const auto beat = owner.xToBeat ((float) e.x);

    if (clip != nullptr)
    {
        owner.selectClip (clip);
        if (lane >= owner.firstTrackLane)
            owner.selectTrack (owner.getLane (lane));

        if (e.mods.isPopupMenu())
        {
            showClipMenu (*clip);
            return;
        }

        const auto r = clipBounds (*clip, lane);
        drag = (e.x - r.getX() < 7 && r.getWidth() > 20) ? Drag::trimStart
             : (r.getRight() - e.x < 7 && r.getWidth() > 20) ? Drag::trimEnd
             : Drag::move;

        // Fade handles: the small squares at the top corners of audio clips
        if (auto* ac = dynamic_cast<te::AudioClipBase*> (clip))
        {
            const auto fi = owner.beatToX (owner.timeToBeat (clip->getPosition().getStart() + ac->getFadeIn()));
            const auto fo = owner.beatToX (owner.timeToBeat (clip->getPosition().getEnd() - ac->getFadeOut()));
            if (e.y - r.getY() < 12 && std::abs (e.x - juce::jmax (r.getX() + 5.0f, fi)) < 7) drag = Drag::fadeIn;
            else if (e.y - r.getY() < 12 && std::abs (e.x - juce::jmin (r.getRight() - 5.0f, fo)) < 7) drag = Drag::fadeOut;
        }

        owner.beginUndoStep();
        dragStartBeat = beat;
        origStart = owner.timeToBeat (clip->getPosition().getStart());
        origEnd   = owner.timeToBeat (clip->getPosition().getEnd());
        dragTargetLane = lane;
        return;
    }

    owner.selectClip (nullptr);
    if (lane >= owner.firstTrackLane)
        owner.selectTrack (owner.getLane (lane));

    if (e.mods.isPopupMenu())
    {
        showLaneMenu (lane, beat);
        return;
    }

    owner.getEngine().setPositionSeconds (owner.beatToTime (owner.snap (beat)).inSeconds());
    drag = Drag::none;
}

void TimelineLanes::mouseDrag (const juce::MouseEvent& e)
{
    if (dragChord.isValid() && drag != Drag::none && owner.getEdit() != nullptr)
    {
        const auto delta = owner.xToBeat ((float) e.x) - dragStartBeat;
        const auto minLen = owner.snapMode == 0 ? 0.5 : juce::jmax (0.5, owner.snap (1.0));
        auto& ed = *owner.getEdit();

        if (drag == Drag::move)
            ChordTrack::setTiming (ed, dragChord, juce::jmax (0.0, owner.snap (chordOrigStart + delta)), chordOrigEnd - chordOrigStart);
        else if (drag == Drag::trimEnd)
            ChordTrack::setTiming (ed, dragChord, chordOrigStart,
                                   juce::jmax (minLen, owner.snap (chordOrigEnd + delta) - chordOrigStart));
        else
        {
            const auto newStart = juce::jlimit (0.0, chordOrigEnd - minLen, owner.snap (chordOrigStart + delta));
            ChordTrack::setTiming (ed, dragChord, newStart, chordOrigEnd - newStart);
        }

        repaint();
        return;
    }

    auto clip = owner.selectedClip;
    if (drag == Drag::none || clip == nullptr)
        return;

    const auto delta = owner.xToBeat ((float) e.x) - dragStartBeat;
    const auto minLen = owner.snapMode == 0 ? 0.25 : juce::jmax (0.5, owner.snap (1.0));

    switch (drag)
    {
        case Drag::move:
        {
            const auto newStart = juce::jmax (0.0, owner.snap (origStart + delta));
            clip->setStart (owner.beatToTime (newStart), false, true);

            if (! isArranger (clip->getTrack()))
            {
                const auto lane = owner.laneAtY (e.y + owner.scrollY);
                if (lane >= owner.firstTrackLane)
                    dragTargetLane = lane;
            }
            break;
        }
        case Drag::trimEnd:
            clip->setEnd (owner.beatToTime (juce::jmax (origStart + minLen, owner.snap (origEnd + delta))), true);
            break;
        case Drag::trimStart:
            clip->setStart (owner.beatToTime (juce::jlimit (0.0, origEnd - minLen, owner.snap (origStart + delta))), true, false);
            break;
        case Drag::fadeIn:
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (clip.get()))
            {
                const auto t = owner.beatToTime (juce::jlimit (origStart, origEnd, owner.xToBeat ((float) e.x)));
                ac->setFadeIn (te::TimeDuration::fromSeconds (juce::jmax (0.0, (t - clip->getPosition().getStart()).inSeconds())));
            }
            break;
        case Drag::fadeOut:
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (clip.get()))
            {
                const auto t = owner.beatToTime (juce::jlimit (origStart, origEnd, owner.xToBeat ((float) e.x)));
                ac->setFadeOut (te::TimeDuration::fromSeconds (juce::jmax (0.0, (clip->getPosition().getEnd() - t).inSeconds())));
            }
            break;
        case Drag::none:
            break;
    }

    repaint();
}

void TimelineLanes::mouseUp (const juce::MouseEvent&)
{
    if (dragChord.isValid())
    {
        dragChord = {};
        drag = Drag::none;
        if (owner.onChordsChanged)
            owner.onChordsChanged();
        owner.refreshAll();
        return;
    }

    if (drag == Drag::move && owner.selectedClip != nullptr && ! isArranger (owner.selectedClip->getTrack()))
    {
        if (auto* target = dynamic_cast<te::AudioTrack*> (owner.getLane (dragTargetLane)))
            if (target != owner.selectedClip->getTrack())
            {
                owner.selectedClip->moveTo (*target);
                owner.selectTrack (target);
            }
    }

    if (auto* ac = dynamic_cast<te::AudioClipBase*> (owner.selectedClip.get()))
        AudioEdit::prepare (*ac);    // overlapping audio clips crossfade automatically

    drag = Drag::none;
    dragTargetLane = -1;
    owner.refreshAll();
}

void TimelineLanes::mouseMove (const juce::MouseEvent& e)
{
    int lane = -1;
    if (auto* clip = clipAt (e.getPosition(), lane))
    {
        const auto r = clipBounds (*clip, lane);
        const auto edge = r.getWidth() > 20 && (e.x - r.getX() < 7 || r.getRight() - e.x < 7);
        bool fadeHandle = false;
        if (auto* ac = dynamic_cast<te::AudioClipBase*> (clip))
        {
            const auto fi = juce::jmax (r.getX() + 5.0f, owner.beatToX (owner.timeToBeat (clip->getPosition().getStart() + ac->getFadeIn())));
            const auto fo = juce::jmin (r.getRight() - 5.0f, owner.beatToX (owner.timeToBeat (clip->getPosition().getEnd() - ac->getFadeOut())));
            fadeHandle = e.y - r.getY() < 12 && (std::abs (e.x - fi) < 7 || std::abs (e.x - fo) < 7);
        }
        setMouseCursor (fadeHandle ? juce::MouseCursor::UpDownLeftRightResizeCursor
                        : edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
    }
    else
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }
}

void TimelineLanes::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (owner.laneAtY (e.y + owner.scrollY) == owner.chordLane && owner.getEdit() != nullptr)
    {
        const auto events = ChordTrack::getEvents (*owner.getEdit());
        const auto index = chordAt (e.getPosition(), events);
        if (index >= 0)
            owner.auditionChord (events[(size_t) index].chord, true);
        else
            showChordQuickMenu (owner.snap (owner.xToBeat ((float) e.x)));
        return;
    }

    int lane = -1;
    if (auto* c = clipAt (e.getPosition(), lane))
    {
        owner.openClip (c);
        return;
    }

    const auto beat = owner.xToBeat ((float) e.x);

    if (lane == 0)
    {
        owner.addSectionAt (beat);
        return;
    }

    if (auto* track = dynamic_cast<te::AudioTrack*> (owner.getLane (lane)))
    {
        if (! StudioEngine::isInstrumentTrack (*track))
            return;

        const auto bpb = owner.getBeatsPerBar();
        const auto start = std::floor (beat / bpb) * bpb;

        owner.beginUndoStep();
        if (auto clip = track->insertMIDIClip ("Clip", te::TimeRange (owner.beatToTime (start), owner.beatToTime (start + bpb)), nullptr))
        {
            owner.selectClip (clip.get());
            owner.openClip (clip.get());
        }
        owner.refreshAll();
    }
}

void TimelineLanes::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        owner.zoomAround (w.deltaY > 0 ? 1.15 : 1.0 / 1.15, (float) e.x);
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const auto amount = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        owner.scrollBy (-amount * 300.0 / owner.pxPerBeat);
    }
    else
    {
        owner.scrollY = juce::roundToInt (owner.scrollY - w.deltaY * 200.0f);
        owner.refreshAll();
    }
}

void TimelineLanes::showClipMenu (te::Clip& clip)
{
    juce::PopupMenu menu, takes;
    if (dynamic_cast<te::MidiClip*> (&clip) != nullptr)
    {
        menu.addItem (6, "Open in piano roll");
        menu.addItem (26, "Find the chords (put them on the Chord Track)...");
        menu.addSeparator();
    }
    juce::PopupMenu fadesMenu;
    if (auto* ac = dynamic_cast<te::AudioClipBase*> (&clip))
    {
        menu.addItem (20, "Split at playhead  (S)");
        menu.addItem (21, "Clip gain, pitch and speed...");
        menu.addItem (22, "Normalise (loudest point to -1 dB)");
        const double lengths[] = { 0.0, 0.01, 0.1, 0.5, 2.0 };
        const char* names[] = { "None", "Click guard (10 ms)", "Short (0.1 s)", "Medium (0.5 s)", "Long (2 s)" };
        fadesMenu.addSectionHeader ("Fade in");
        for (int i = 0; i < 5; ++i)
            fadesMenu.addItem (600 + i, names[i], true, std::abs (ac->getFadeIn().inSeconds() - lengths[i]) < 0.001);
        fadesMenu.addSectionHeader ("Fade out");
        for (int i = 0; i < 5; ++i)
            fadesMenu.addItem (610 + i, names[i], true, std::abs (ac->getFadeOut().inSeconds() - lengths[i]) < 0.001);
        menu.addSubMenu ("Fades (or drag the squares on the clip)", fadesMenu);
        menu.addSeparator();
    }
    juce::PopupMenu loopMenu, loopKeys;
    if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&clip))
    {
        if (Loops::isFollowing (*wc))
        {
            const auto bpm = Loops::getLoopTempo (*wc);
            const auto txt = [] (double v) { return juce::String (v, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."); };
            loopMenu.addItem (700, "Follows the song's tempo (click to stop)", true, true);
            loopMenu.addSeparator();
            loopMenu.addSectionHeader ("Its own tempo: " + txt (bpm) + " BPM");
            loopMenu.addItem (701, "It is twice as fast (" + txt (bpm * 2.0) + " BPM)");
            loopMenu.addItem (702, "It is half as fast (" + txt (bpm / 2.0) + " BPM)");
            loopMenu.addItem (703, "Type its tempo...");
            loopMenu.addSeparator();
            static const char* keyNames[] = { "C", "C# / Db", "D", "Eb", "E", "F", "F# / Gb", "G", "Ab", "A", "Bb", "B" };
            const auto root = Loops::getRoot (*wc);
            loopKeys.addItem (720, "Not known", true, root < 0);
            for (int k = 0; k < 12; ++k)
                loopKeys.addItem (721 + k, keyNames[k], true, root == k);
            loopMenu.addSubMenu ("Its key" + (root >= 0 ? juce::String (": ") + keyNames[root] : juce::String()), loopKeys);
            loopMenu.addItem (704, "Follow the song's key", root >= 0, Loops::isFollowingKey (*wc));
            loopMenu.addSeparator();
            loopMenu.addItem (710, "Repeat: 1 time");
            loopMenu.addItem (711, "Repeat: 2 times");
            loopMenu.addItem (712, "Repeat: 4 times");
            loopMenu.addItem (713, "Repeat: 8 times");
            loopMenu.addItem (714, "Repeat to the end of the song");
            menu.addSubMenu ("Loop", loopMenu);
        }
        else
        {
            menu.addItem (705, "Make it a loop (follow the song's tempo)");
        }
        menu.addSeparator();

        menu.addItem (24, juce::String ("Timing correction (line up to the beat)...") + (Timing::isCorrected (*wc) ? "   [on]" : ""));
        {
            const auto made = Harmony::findHarmonyTracks (*wc).size();
            const auto* tr = dynamic_cast<te::AudioTrack*> (wc->getTrack());
            menu.addItem (26, "Find the chords (put them on the Chord Track)...");
            menu.addItem (27, juce::String ("Split into stems (voice, drums, bass, other)...")
                              + (Stems::findStemTracks (*wc).isEmpty() ? juce::String() : juce::String ("   [done]")),
                          tr == nullptr || ! Harmony::isHarmonyTrack (*tr));
            menu.addItem (25, juce::String ("Harmony and choir stacks...") + (made > 0 ? "   [" + juce::String (made) + " parts]" : juce::String()),
                          tr == nullptr || ! Harmony::isHarmonyTrack (*tr));
        }
        if (Comp::recordedTakes (*wc).size() > 1)
            menu.addItem (23, "Comp takes (choose the best of each part)...");
        const auto n = Takes::count (*wc);
        if (n > 1)
        {
            for (int i = 0; i < n; ++i)
                takes.addItem (500 + i, "Take " + juce::String (i + 1), true, Takes::current (*wc) == i);
            menu.addSubMenu ("Takes (" + juce::String (n) + ")", takes);
            menu.addItem (7, "Keep this take, delete the others...");
            menu.addSeparator();
        }
    }
    menu.addItem (1, "Rename...");
    menu.addItem (2, "Duplicate", true, false);
    menu.addItem (3, "Copy");
    if (isArranger (clip.getTrack()))
        addColourMenu (menu, 100);
    menu.addSeparator();
    menu.addItem (4, "Delete");

    juce::Component::SafePointer<TimelineView> safe (&owner);
    te::Clip::Ptr clipPtr (&clip);

    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, clipPtr] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        safe->selectClip (clipPtr.get());

        if (result == 1)
        {
            askForName (safe.getComponent(), "Rename clip", clipPtr->getName(), [safe, clipPtr] (const juce::String& n)
            {
                if (safe == nullptr) return;
                safe->beginUndoStep();
                clipPtr->setName (n);
                safe->refreshAll();
            });
        }
        else if (result == 2) safe->duplicateSelectedClip();
        else if (result == 3) safe->copySelectedClip();
        else if (result == 4) safe->deleteSelectedClip();
        else if (result == 6) safe->openClip (clipPtr.get());
        else if (result == 20)
        {
            safe->selectClip (clipPtr.get());
            safe->splitAtPlayhead();
        }
        else if (result == 21) safe->showClipSettings (*clipPtr);
        else if (result == 22)
        {
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (clipPtr.get()))
            {
                safe->beginUndoStep();
                AudioEdit::normalise (*ac);
                safe->refreshAll();
            }
        }
        else if (result == 24)
        {
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (clipPtr.get()))
            {
                juce::Component::SafePointer<TimelineView> view (safe.getComponent());
                auto content = Timing::createPanel (*wc, [view] { if (view != nullptr) view->refreshAll(); });
                const auto r = safe->lanesView.getScreenBounds().withSizeKeepingCentre (10, 10);
                juce::CallOutBox::launchAsynchronously (std::move (content), r, nullptr);
            }
        }
        else if (result == 26 || result == 27)
        {
            juce::Component::SafePointer<TimelineView> view (safe.getComponent());
            auto refresh = [view]
            {
                if (view == nullptr) return;
                view->refreshAll();
                if (view->onTracksChanged) view->onTracksChanged();
                if (view->onChordsChanged) view->onChordsChanged();
            };
            std::unique_ptr<juce::Component> content;
            if (result == 26)
                content = ChordDetect::createPanel (*clipPtr, [view] { return view != nullptr ? view->currentKey() : Theory::Key(); },
                                                    [view] { return view != nullptr ? view->getBeatsPerBar() : 4.0; }, refresh);
            else if (auto* wc = dynamic_cast<te::WaveAudioClip*> (clipPtr.get()))
                content = Stems::createPanel (*wc, refresh);
            if (content != nullptr)
                juce::CallOutBox::launchAsynchronously (std::move (content), safe->lanesView.getScreenBounds().withSizeKeepingCentre (10, 10), nullptr);
        }
        else if (result == 25)
        {
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (clipPtr.get()))
            {
                juce::Component::SafePointer<TimelineView> view (safe.getComponent());
                auto content = Harmony::createPanel (*wc, [view] { return view != nullptr ? view->currentKey() : Theory::Key(); },
                                                     [view]
                                                     {
                                                         if (view == nullptr) return;
                                                         view->refreshAll();
                                                         if (view->onTracksChanged) view->onTracksChanged();
                                                     });
                const auto r = safe->lanesView.getScreenBounds().withSizeKeepingCentre (10, 10);
                juce::CallOutBox::launchAsynchronously (std::move (content), r, nullptr);
            }
        }
        else if (result == 23)
        {
            if (safe->onOpenComp)
                safe->onOpenComp (dynamic_cast<te::WaveAudioClip*> (clipPtr.get()));
        }
        else if (result >= 700 && result < 740)
        {
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (clipPtr.get()))
                safe->loopMenuChosen (*wc, result);
        }
        else if (result >= 600 && result < 620)
        {
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (clipPtr.get()))
            {
                const double lengths[] = { 0.0, 0.01, 0.1, 0.5, 2.0 };
                safe->beginUndoStep();
                if (result < 610) ac->setFadeIn  (te::TimeDuration::fromSeconds (lengths[result - 600]));
                else              ac->setFadeOut (te::TimeDuration::fromSeconds (lengths[result - 610]));
                safe->refreshAll();
            }
        }
        else if (result >= 500)
        {
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (clipPtr.get()))
            {
                safe->beginUndoStep();
                Takes::select (*wc, result - 500);
                safe->refreshAll();
            }
        }
        else if (result == 7)
        {
            Dialogs::confirm (safe.getComponent(), "Keep this take",
                              "Keep the take you hear now and delete the other takes of this clip (and their recordings)?",
                              "Delete others", "Cancel", [safe, clipPtr]
                              {
                                  if (safe == nullptr) return;
                                  if (auto* wc = dynamic_cast<te::WaveAudioClip*> (clipPtr.get()))
                                  {
                                      safe->beginUndoStep();
                                      Takes::keepOnly (*wc, juce::jmax (0, Takes::current (*wc)), true);
                                  }
                                  safe->refreshAll();
                              }, juce::MessageBoxIconType::WarningIcon);
        }
        else if (result >= 100)
        {
            safe->beginUndoStep();
            clipPtr->setColour (palette[result - 100]);
            safe->refreshAll();
        }
    });
}

void TimelineLanes::showLaneMenu (int laneIndex, double beat)
{
    auto* track = owner.getLane (laneIndex);
    if (track == nullptr)
        return;

    if (laneIndex == 0)
    {
        owner.addSectionAt (beat);
        return;
    }

    if (laneIndex == owner.chordLane)
    {
        showChordQuickMenu (owner.snap (beat));
        return;
    }

    juce::PopupMenu menu;
    auto* audioTrack = dynamic_cast<te::AudioTrack*> (track);
    const auto instrument = audioTrack != nullptr && StudioEngine::isInstrumentTrack (*audioTrack);
    menu.addItem (1, "New empty clip here", instrument);
    menu.addItem (2, "Paste here", owner.clipboard != nullptr);

    // Starter grooves (best on the Drums track)
    juce::PopupMenu grooves;
    int num = 4, den = 4;
    if (auto* sig = owner.getEdit()->tempoSequence.getTimeSig (0))
    {
        num = sig->numerator.get();
        den = sig->denominator.get();
    }
    juce::String category;
    for (size_t i = 0; i < Grooves::all().size(); ++i)
    {
        const auto& g = Grooves::all()[i];
        if (category != g.category) { grooves.addSectionHeader (g.category); category = g.category; }
        const auto fits = Grooves::fits (g, num, den);
        grooves.addItem (300 + (int) i, juce::String (g.name) + (fits ? juce::String() : "  (for " + juce::String (g.category) + ")"), fits && instrument);
    }
    menu.addSeparator();
    menu.addSubMenu ("Insert groove (fills to the end of the section)", grooves, instrument);

    juce::Component::SafePointer<TimelineView> safe (&owner);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, audioTrack, beat] (int result)
    {
        if (safe == nullptr || result == 0 || audioTrack == nullptr)
            return;

        if (result >= 300)
        {
            safe->insertGroove (*audioTrack, Grooves::all()[(size_t) (result - 300)], beat);
            return;
        }

        if (result == 1)
        {
            const auto bpb = safe->getBeatsPerBar();
            const auto start = std::floor (beat / bpb) * bpb;
            safe->beginUndoStep();
            if (auto clip = audioTrack->insertMIDIClip ("Clip", te::TimeRange (safe->beatToTime (start), safe->beatToTime (start + bpb)), nullptr))
            {
                safe->selectClip (clip.get());
                safe->openClip (clip.get());
            }
            safe->refreshAll();
        }
        else if (result == 2)
        {
            safe->getEngine().setPositionSeconds (safe->beatToTime (safe->snap (beat)).inSeconds());
            safe->selectTrack (audioTrack);
            safe->pasteClip();
        }
    });
}

//==============================================================================
//  Chord lane
//==============================================================================
juce::Rectangle<float> TimelineLanes::chordBounds (const ChordTrack::Event& e) const
{
    const auto x1 = owner.beatToX (e.start);
    const auto x2 = owner.beatToX (e.start + e.length);
    const auto y = (float) (owner.getLaneY (owner.chordLane) - owner.scrollY);
    return { x1 + 1.0f, y + 4.0f, juce::jmax (3.0f, x2 - x1 - 2.0f), (float) owner.chordLaneHeight - 8.0f };
}

int TimelineLanes::chordAt (juce::Point<int> p, const std::vector<ChordTrack::Event>& events) const
{
    for (int i = (int) events.size(); --i >= 0;)
        if (chordBounds (events[(size_t) i]).contains (p.toFloat()))
            return i;
    return -1;
}

static juce::Colour chordColour (const Theory::Chord& c)
{
    switch (c.root)
    {
        case 0:            return juce::Colour (0xffd4a843);     // home
        case 5: case 2:    return juce::Colour (0xff5b8def);     // pre-dominant
        case 7: case 11:   return juce::Colour (0xffe07a4f);     // dominant
        case 9: case 4:    return juce::Colour (0xff4fb3c8);     // relative chords
        default:           return juce::Colour (0xffb07ee0);     // colours from outside the key
    }
}

void TimelineLanes::paintChords (juce::Graphics& g)
{
    auto* ed = owner.getEdit();
    if (ed == nullptr)
        return;

    const auto key = owner.currentKey();
    const auto events = ChordTrack::getEvents (*ed);

    for (const auto& e : events)
    {
        const auto r = chordBounds (e);
        if (r.getRight() < 0 || r.getX() > getWidth())
            continue;

        const auto selected = std::any_of (owner.selectedChords.begin(), owner.selectedChords.end(),
                                           [&e] (const juce::ValueTree& v) { return v == e.node; });
        const auto colour = chordColour (e.chord);

        g.setColour (colour.withAlpha (selected ? 0.55f : 0.32f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (selected ? StudioColours::text : colour);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, selected ? 2.0f : 1.0f);

        auto text = Theory::label (e.chord, key, owner.chordDisplay);
        g.setFont (studioFont (r.getWidth() < 46.0f ? 10.5f : 13.0f, true));
        if (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) > r.getWidth() - 10.0f)
            text = owner.chordDisplay == Theory::Display::names ? Theory::nameLabel (e.chord, key) : Theory::numberLabel (e.chord);

        g.setColour (StudioColours::text);
        g.drawFittedText (text, r.reduced (r.getWidth() < 46.0f ? 2.0f : 4.0f, 0.0f).toNearestInt(),
                          r.getWidth() < 46.0f ? juce::Justification::centred : juce::Justification::centredLeft, 1, 0.5f);
    }

    if (events.empty())
    {
        const auto y = owner.getLaneY (owner.chordLane) - owner.scrollY;
        g.setColour (StudioColours::textDim);
        g.setFont (studioFont (12.5f));
        g.drawText ("Double-click here or open the Palette to add chords", 10, y, getWidth() - 20,
                    owner.chordLaneHeight, juce::Justification::centredLeft, true);
    }
}

void TimelineLanes::showChordQuickMenu (double beat)
{
    const auto key = owner.currentKey();
    const auto groups = Theory::paletteFor (key);

    juce::PopupMenu menu;
    std::vector<Theory::Chord> items;

    for (size_t gi = 0; gi < juce::jmin ((size_t) 3, groups.size()); ++gi)
    {
        menu.addSectionHeader (groups[gi].name);
        for (const auto& pc : groups[gi].chords)
        {
            items.push_back (pc.chord);
            menu.addItem ((int) items.size(), Theory::label (pc.chord, key, Theory::Display::both));
        }
    }
    menu.addSeparator();
    menu.addItem (1000, "More chords in the Palette...");

    juce::Component::SafePointer<TimelineView> safe (&owner);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, items, beat] (int result)
    {
        if (safe == nullptr || result == 0 || safe->getEdit() == nullptr)
            return;

        if (result == 1000)
        {
            if (safe->onTogglePalette) safe->onTogglePalette();
            return;
        }

        const auto& chord = items[(size_t) result - 1];
        safe->beginUndoStep();
        auto bpb = safe->getBeatsPerBar();
        ChordTrack::clearRange (*safe->getEdit(), beat, beat + bpb);
        auto node = ChordTrack::add (*safe->getEdit(), chord, beat, bpb);
        safe->selectChord (node, false);
        safe->auditionChord (chord);
        if (safe->onChordsChanged) safe->onChordsChanged();
        safe->refreshAll();
    });
}

void TimelineLanes::showChordMenu (const ChordTrack::Event& ev)
{
    const auto key = owner.currentKey();
    juce::PopupMenu menu, qualities, basses;

    juce::String lastGroup;
    juce::PopupMenu* sub = nullptr;
    std::vector<std::unique_ptr<juce::PopupMenu>> groups;
    juce::StringArray groupNames;
    const auto& all = Theory::qualities();

    for (size_t i = 0; i < all.size(); ++i)
    {
        if (lastGroup != all[i].group)
        {
            groups.push_back (std::make_unique<juce::PopupMenu>());
            groupNames.add (all[i].group);
            sub = groups.back().get();
            lastGroup = all[i].group;
        }
        auto c = ev.chord;
        c.quality = all[i].id;
        sub->addItem (100 + (int) i, Theory::label (c, key, Theory::Display::both), true, ev.chord.quality == all[i].id);
    }
    for (size_t i = 0; i < groups.size(); ++i)
        qualities.addSubMenu (groupNames[(int) i], *groups[i]);

    basses.addItem (300, "No slash bass", true, ev.chord.bass < 0);
    for (int i = 0; i < 12; ++i)
        basses.addItem (301 + i, "Bass on " + Theory::degreeLabel (i, false) + "  (" + Theory::noteName (key.tonic + i, key.flats) + ")",
                        true, ev.chord.bass == i);

    menu.addItem (1, "Hear (arpeggio)");
    menu.addSubMenu ("Change chord type", qualities);
    menu.addSubMenu ("Slash bass", basses);
    menu.addSeparator();
    menu.addItem (2, "Reharmonise selection: Rich");
    menu.addItem (3, "Reharmonise selection: Advanced gospel");
    menu.addItem (4, "Save selection to My Progressions...");
    menu.addSeparator();
    menu.addItem (5, "Delete");

    juce::Component::SafePointer<TimelineView> safe (&owner);
    const auto node = ev.node;
    const auto chord = ev.chord;

    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, node, chord] (int result)
    {
        if (safe == nullptr || result == 0 || safe->getEdit() == nullptr)
            return;

        auto& ed = *safe->getEdit();
        if (result == 1)       safe->auditionChord (chord, true);
        else if (result == 2)  safe->reharmoniseSelection (Theory::Level::rich);
        else if (result == 3)  safe->reharmoniseSelection (Theory::Level::advanced);
        else if (result == 4)  { if (safe->onSaveSelection) safe->onSaveSelection(); }
        else if (result == 5)  safe->deleteSelectedChords();
        else if (result >= 100 && result < 300)
        {
            auto c = chord;
            c.quality = Theory::qualities()[(size_t) (result - 100)].id;
            safe->beginUndoStep();
            ChordTrack::setChord (ed, node, c);
            safe->auditionChord (c);
        }
        else if (result >= 300)
        {
            auto c = chord;
            c.bass = result == 300 ? -1 : result - 301;
            safe->beginUndoStep();
            ChordTrack::setChord (ed, node, c);
            safe->auditionChord (c);
        }

        if (safe->onChordsChanged) safe->onChordsChanged();
        safe->refreshAll();
    });
}

//==============================================================================
void TimelineView::selectChord (const juce::ValueTree& node, bool extend)
{
    auto* ed = getEdit();
    if (ed == nullptr)
        return;

    if (extend && ! selectedChords.empty())
    {
        const auto a = ChordTrack::fromNode (selectedChords.front()).start;
        const auto b = ChordTrack::fromNode (node).start;
        const auto lo = juce::jmin (a, b), hi = juce::jmax (a, b);

        selectedChords.clear();
        for (const auto& e : ChordTrack::getEvents (*ed))
            if (e.start >= lo - 1.0e-6 && e.start <= hi + 1.0e-6)
                selectedChords.push_back (e.node);
    }
    else
    {
        selectedChords = { node };
    }

    if (onChordSelected)
    {
        const auto ev = ChordTrack::fromNode (node);
        onChordSelected (&ev.chord);
    }
    lanesView.repaint();
}

void TimelineView::clearChordSelection()
{
    if (selectedChords.empty())
        return;
    selectedChords.clear();
    lanesView.repaint();
}

std::vector<ChordTrack::Event> TimelineView::getSelectedChordEvents() const
{
    std::vector<ChordTrack::Event> out;
    for (const auto& v : selectedChords)
        if (v.getParent().isValid())
            out.push_back (ChordTrack::fromNode (v));
    std::sort (out.begin(), out.end(), [] (const auto& a, const auto& b) { return a.start < b.start; });
    return out;
}

void TimelineView::deleteSelectedChords()
{
    auto* ed = getEdit();
    if (ed == nullptr || selectedChords.empty())
        return;

    beginUndoStep();
    for (auto& v : selectedChords)
        if (v.getParent().isValid())
            ChordTrack::remove (*ed, v);
    selectedChords.clear();
    if (onChordsChanged) onChordsChanged();
    refreshAll();
}

void TimelineView::reharmoniseSelection (Theory::Level level)
{
    auto* ed = getEdit();
    const auto events = getSelectedChordEvents();
    if (ed == nullptr || events.empty())
        return;

    const auto start = events.front().start;
    const auto end = events.back().start + events.back().length;
    const auto reharm = Theory::applyLevel (ChordTrack::toTimed (events), level, currentKey().minor);

    beginUndoStep();
    ChordTrack::clearRange (*ed, start, end);
    ChordTrack::insert (*ed, reharm, start);

    selectedChords.clear();
    for (const auto& e : ChordTrack::getEvents (*ed))
        if (e.start >= start - 1.0e-6 && e.start < end - 1.0e-6)
            selectedChords.push_back (e.node);

    if (onChordsChanged) onChordsChanged();
    refreshAll();
}

void TimelineView::auditionChord (const Theory::Chord& chord, bool arpeggio)
{
    const auto voicing = (Theory::Voicing) StudioEngine::getSetting ("chordVoicing", 2);
    const auto lh = (Theory::LeftHand) StudioEngine::getSetting ("chordLeftHand", 0);
    engine.audition (Theory::voice (chord, currentKey(), voicing, lh), arpeggio);
}

//==============================================================================
void TimelineView::insertGroove (te::AudioTrack& track, const Grooves::Groove& groove, double beat)
{
    auto* ed = getEdit();
    if (ed == nullptr)
        return;

    const auto bpb = getBeatsPerBar();
    const auto start = std::floor (beat / bpb + 1.0e-6) * bpb;

    // Fill to the end of the song section this bar is in, or 4 bars
    auto end = start + 4.0 * bpb;
    if (auto* arranger = ed->getArrangerTrack())
        for (auto* c : arranger->getClips())
        {
            const auto cs = timeToBeat (c->getPosition().getStart()), ce = timeToBeat (c->getPosition().getEnd());
            if (start >= cs - 1.0e-6 && start < ce - 1.0e-6)
                end = juce::jmax (start + bpb, ce);
        }

    beginUndoStep();
    if (auto clip = track.insertMIDIClip (groove.name, te::TimeRange (beatToTime (start), beatToTime (end)), nullptr))
    {
        Grooves::writeTo (clip->getSequence(), groove, clip->getContentBeatAtTime (clip->getPosition().getStart()).inBeats(),
                          end - start, bpb, &ed->getUndoManager());
        selectClip (clip.get());
    }
    refreshAll();
}

//==============================================================================
void TimelineView::loopMenuChosen (te::WaveAudioClip& wc, int result)
{
    auto* ed = getEdit();
    if (ed == nullptr)
        return;
    const auto bpb = juce::jmax (1, (int) std::round (getBeatsPerBar()));
    te::Clip::Ptr keep (&wc);

    if (result == 700)
    {
        beginUndoStep();
        Loops::stopFollowing (wc);
    }
    else if (result == 701 || result == 702)
    {
        beginUndoStep();
        Loops::setLoopTempo (wc, Loops::getLoopTempo (wc) * (result == 701 ? 2.0 : 0.5));
    }
    else if (result == 703 || result == 705)
    {
        if (result == 705)
        {
            const auto songBpm = ed->tempoSequence.getTempoAt (wc.getPosition().getStart()).getBpm();
            const auto g = Loops::guess (ed->engine, wc.getOriginalFile(), songBpm, bpb, true);
            if (g.valid())
            {
                beginUndoStep();
                Loops::follow (wc, g, 1);
                refreshAll();
                return;
            }
        }
        juce::Component::SafePointer<TimelineView> safe (this);
        const auto current = Loops::getLoopTempo (wc);
        askForName (this, "Tempo of this loop (BPM)", current > 0.0 ? juce::String (current, 1) : juce::String ("90"),
                    [safe, keep, bpb] (const juce::String& text)
        {
            auto* w = dynamic_cast<te::WaveAudioClip*> (keep.get());
            const auto bpm = text.retainCharacters ("0123456789.").getDoubleValue();
            if (safe == nullptr || w == nullptr || bpm < 30.0 || bpm > 300.0)
                return;
            safe->beginUndoStep();
            if (Loops::isFollowing (*w))
                Loops::setLoopTempo (*w, bpm);
            else
            {
                Loops::Guess g;
                g.bpm = bpm;
                g.beats = bpm * te::AudioFile (w->edit.engine, w->getOriginalFile()).getLength() / 60.0;
                g.root = Loops::rootFromName (w->getOriginalFile().getFileNameWithoutExtension());
                Loops::follow (*w, g, 1);
            }
            juce::ignoreUnused (bpb);
            safe->refreshAll();
        });
        return;
    }
    else if (result == 704)
    {
        beginUndoStep();
        Loops::setFollowKey (wc, ! Loops::isFollowingKey (wc));
    }
    else if (result >= 720 && result <= 732)
    {
        beginUndoStep();
        Loops::setRoot (wc, result == 720 ? -1 : result - 721);
    }
    else if (result >= 710 && result <= 714)
    {
        const double loopBeats = Loops::getLoopTempo (wc) > 0.0
                                   ? wc.getLoopRange().getLength().inSeconds() * ed->tempoSequence.getTempoAt (wc.getPosition().getStart()).getBpm() / 60.0
                                   : 0.0;
        if (loopBeats <= 0.0)
            return;
        double beats = loopBeats;
        if (result == 711) beats *= 2.0;
        if (result == 712) beats *= 4.0;
        if (result == 713) beats *= 8.0;
        if (result == 714)
        {
            // To the end of the last section (or of the last clip)
            double endBeat = timeToBeat (wc.getPosition().getEnd());
            if (auto* at = ed->getArrangerTrack())
                for (auto* c : at->getClips())
                    endBeat = juce::jmax (endBeat, timeToBeat (c->getPosition().getEnd()));
            const auto start = timeToBeat (wc.getPosition().getStart());
            beats = juce::jmax (loopBeats, std::floor ((endBeat - start) / loopBeats + 1.0e-6) * loopBeats);
        }
        beginUndoStep();
        const auto start = timeToBeat (wc.getPosition().getStart());
        wc.setEnd (beatToTime (start + beats), false);
    }
    refreshAll();
}

void TimelineView::importAudioTo (te::AudioTrack& track, bool asLoop)
{
    chooser = std::make_unique<juce::FileChooser> (asLoop ? "Import a loop" : "Import an audio file",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<TimelineView> safe (this);
    te::AudioTrack::Ptr t (&track);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, t, asLoop] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        safe->importFileAt (*t, fc.getResult(), safe->snap (safe->getPlayheadBeat()), asLoop);
    });
}

te::Clip* TimelineView::importFileAt (te::AudioTrack& track, const juce::File& file, double beat, bool asLoop)
{
    auto* ed = getEdit();
    if (ed == nullptr)
        return nullptr;

    beginUndoStep();
    auto* c = engine.importAudio (track, file, beatToTime (juce::jmax (0.0, beat)).inSeconds());
    if (c == nullptr)
    {
        Dialogs::message (this, "Import audio", file.getFileName() + " could not be read as audio.");
        return nullptr;
    }
    selectClip (c);

    // A loop follows the song's tempo. Without being told, only when the file or its name says its tempo.
    if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c))
    {
        const auto bpb = juce::jmax (1, (int) std::round (getBeatsPerBar()));
        const auto songBpm = ed->tempoSequence.getTempoAt (c->getPosition().getStart()).getBpm();
        const auto g = Loops::guess (ed->engine, file, songBpm, bpb, asLoop);
        if (g.valid())
        {
            // Fill at least 4 bars when asked for a loop; otherwise play it once
            const auto repeats = asLoop ? juce::jmax (1, (int) std::ceil (4.0 * bpb / g.beats - 1.0e-6)) : 1;
            if (Loops::follow (*wc, g, repeats) && ! asLoop)
                lastLoopMessage = file.getFileNameWithoutExtension() + ": a loop at " + juce::String (g.bpm, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".")
                                  + " BPM (" + g.how + "). It now follows the song's tempo. Right-click it > Loop to change.";
        }
        else if (asLoop)
        {
            Dialogs::message (this, "Import a loop",
                              "The tempo of this audio could not be worked out, so it plays at its own speed. "
                              "Right-click it > Loop > \"Make it a loop\" and type its tempo.");
        }
    }
    refreshAll();
    return c;
}

bool TimelineView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (MySounds::isAudioFile (juce::File (f)))
            return getEdit() != nullptr;
    return false;
}

void TimelineView::fileDragMove (const juce::StringArray&, int x, int y)
{
    dropPoint = lanesView.getLocalPoint (this, juce::Point<int> (x, y));
    lanesView.repaint();
}

void TimelineView::fileDragExit (const juce::StringArray&)
{
    dropPoint = { -1, -1 };
    lanesView.repaint();
}

void TimelineView::filesDropped (const juce::StringArray& files, int x, int y)
{
    dropPoint = { -1, -1 };
    auto* ed = getEdit();
    if (ed == nullptr)
        return;

    const auto p = lanesView.getLocalPoint (this, juce::Point<int> (x, y));
    const auto lane = laneAtY (p.y + scrollY);
    auto* track = dynamic_cast<te::AudioTrack*> (getLane (lane));
    auto beat = snap (juce::jmax (0.0, xToBeat ((float) p.x)));

    // Onto a My Sounds track: the files become its sounds
    if (track != nullptr)
        if (auto* s = dynamic_cast<SamplerPlugin*> (StudioEngine::getInstrumentPlugin (*track)); s != nullptr && s->isMySounds())
        {
            s->getUndoManager()->beginNewTransaction ("Add sounds");
            for (auto& f : files)
                if (MySounds::isAudioFile (juce::File (f)) && MySounds::lengthSeconds (juce::File (f)) <= 60.0)
                    MySounds::add (s->state, engine.copyIntoSong (juce::File (f), "Sounds"), s->getUndoManager());
            InstrumentPanel::open (engine, *track);
            return;
        }

    // Onto an instrument track or empty space: a new audio track
    if (track == nullptr || StudioEngine::isInstrumentTrack (*track))
    {
        const auto count = (int) Mixing::getSongTracks (*ed).size();
        beginUndoStep();
        track = dynamic_cast<te::AudioTrack*> (engine.addTrack (false, juce::File (files[0]).getFileNameWithoutExtension(),
                                                                palette[count % (int) std::size (palette)],
                                                                count > 0 ? Mixing::getSongTracks (*ed).getLast() : nullptr, "grand"));
        if (track == nullptr)
            return;
    }

    // Several files: one after the other
    for (auto& f : files)
    {
        const juce::File file (f);
        if (! MySounds::isAudioFile (file))
            continue;
        if (auto* c = importFileAt (*track, file, beat, false))
            beat = timeToBeat (c->getPosition().getEnd());
    }
    rebuildHeaders();
    if (lastLoopMessage.isNotEmpty())
    {
        Dialogs::message (this, "Loop", lastLoopMessage);
        lastLoopMessage = {};
    }
}

//==============================================================================
//  Split and clip settings (Step 10)
//==============================================================================
void TimelineView::splitAtPlayhead()
{
    auto* ed = getEdit();
    if (ed == nullptr)
        return;

    const auto time = ed->getTransport().getPosition();
    juce::Array<te::Clip*> toSplit;

    // The selected clip, or every clip under the playhead on the selected track
    if (selectedClip != nullptr && ! isArranger (selectedClip->getTrack()))
        toSplit.add (selectedClip.get());
    else if (auto* ct = dynamic_cast<te::ClipTrack*> (selectedTrack))
        for (auto* c : ct->getClips())
            toSplit.add (c);

    beginUndoStep();
    te::Clip* last = nullptr;
    for (auto* c : toSplit)
        if (c->getPosition().time.contains (time))
            if (auto* right = AudioEdit::splitAt (*c, time))
                last = right;

    if (last != nullptr)
        selectClip (last);
    refreshAll();
}

namespace
{
    // Small window for clip gain, pitch and speed
    class ClipSettings final : public juce::Component
    {
    public:
        ClipSettings (te::AudioClipBase& c, TimelineView& t) : clip (&c), timeline (t)
        {
            auto setup = [this] (juce::Slider& s, double lo, double hi, double step, double value, const juce::String& suffix, double reset)
            {
                s.setSliderStyle (juce::Slider::LinearHorizontal);
                s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 74, 22);
                s.setRange (lo, hi, step);
                s.setValue (value, juce::dontSendNotification);
                s.setTextValueSuffix (suffix);
                s.setDoubleClickReturnValue (true, reset);
                s.onDragStart = [this] { clip->edit.getUndoManager().beginNewTransaction(); };
                addAndMakeVisible (s);
            };
            setup (gain, -24.0, 12.0, 0.1, c.getGainDB(), " dB", 0.0);
            setup (pitch, -12.0, 12.0, 0.01, c.getPitchChange(), " st", 0.0);
            setup (speed, 50.0, 200.0, 1.0, c.getSpeedRatio() * 100.0, " %", 100.0);
            gain.onValueChange  = [this] { clip->setGainDB ((float) gain.getValue()); timeline.repaint(); };
            pitch.onValueChange = [this] { AudioEdit::setPitch (*clip, (float) pitch.getValue()); timeline.repaint(); };
            speed.onValueChange = [this] { AudioEdit::setSpeed (*clip, speed.getValue() / 100.0, keepPitch.getToggleState()); timeline.refreshAll(); };
            pitch.setTooltip ("Semitones up or down (1 = one key). Fine values tune a voice slightly.");
            speed.setTooltip ("Play the clip faster or slower");

            keepPitch.setToggleState (c.getTimeStretchMode() != te::TimeStretcher::disabled || std::abs (c.getSpeedRatio() - 1.0) < 0.001,
                                      juce::dontSendNotification);
            keepPitch.setTooltip ("On: the key stays the same when the speed changes. Off: like a tape, faster is higher.");
            keepPitch.onClick = [this] { AudioEdit::setSpeed (*clip, speed.getValue() / 100.0, keepPitch.getToggleState()); };
            addAndMakeVisible (keepPitch);
            setSize (380, 190);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (StudioColours::textDim);
            g.setFont (studioFont (12.0f));
            for (auto [y, text] : { std::pair<int, const char*> { 10, "Clip gain" }, { 62, "Pitch (semitones)" }, { 114, "Speed" } })
                g.drawText (text, 12, y, 200, 16, juce::Justification::centredLeft);
        }

        void resized() override
        {
            gain.setBounds (10, 28, getWidth() - 20, 26);
            pitch.setBounds (10, 80, getWidth() - 20, 26);
            speed.setBounds (10, 132, getWidth() - 130, 26);
            keepPitch.setBounds (getWidth() - 116, 132, 110, 26);
        }

    private:
        te::AudioClipBase* clip;
        te::Clip::Ptr clipRef { clip };     // keeps the clip alive while this window is open
        TimelineView& timeline;
        juce::Slider gain, pitch, speed;
        juce::ToggleButton keepPitch { "Keep pitch" };
    };
}

void TimelineView::showClipSettings (te::Clip& c)
{
    auto* ac = dynamic_cast<te::AudioClipBase*> (&c);
    if (ac == nullptr)
        return;
    auto content = std::make_unique<ClipSettings> (*ac, *this);
    const auto r = lanesView.getScreenBounds().withSizeKeepingCentre (10, 10);
    juce::CallOutBox::launchAsynchronously (std::move (content), r, nullptr);
}
