#include "Takes.h"
#include "InstrumentPanel.h"
#include "SoundLibrary.h"
#include "Grooves.h"
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

    snapBox.addItemList ({ "Snap off", "Snap 1/2 beat", "Snap beat", "Snap bar" }, 1);
    snapBox.setSelectedItemIndex (snapMode, juce::dontSendNotification);
    snapBox.onChange = [this] { snapMode = snapBox.getSelectedItemIndex(); };

    zoomSlider.setRange (2.0, 200.0);
    zoomSlider.setSkewFactorFromMidPoint (24.0);
    zoomSlider.setValue (pxPerBeat, juce::dontSendNotification);
    zoomSlider.setTooltip ("Zoom (Ctrl + mouse wheel)");
    zoomSlider.onValueChange = [this] { zoomAround (zoomSlider.getValue() / pxPerBeat, 0.0f); };

    for (juce::Component* c : { (juce::Component*) &addTrackButton, (juce::Component*) &undoButton,
                                (juce::Component*) &redoButton, (juce::Component*) &fitButton,
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

void TimelineView::editChanged()
{
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
        for (auto* t : te::getAudioTracks (*ed))
            s << t->itemID.toString() << ",";
    }
    return s;
}

void TimelineView::rebuildHeaders()
{
    headers.clear();
    lanes.clear();

    if (auto* ed = getEdit())
    {
        lanes.push_back (ed->getArrangerTrack());
        lanes.push_back (nullptr);                  // the Chord Track lane

        for (auto* t : te::getAudioTracks (*ed))
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
        menu.addItem (100 + (int) i, c.name + (c.kind == Instruments::Kind::sampler && ! SoundPacks::isInstalled (c.pack)
                                                   ? juce::String ("  (download in Sound Library)") : juce::String()));
    }
    menu.addSeparator();
    menu.addItem (2, "Audio track (vocals, guitar)");

    juce::Component::SafePointer<TimelineView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addTrackButton), [safe] (int result)
    {
        if (safe == nullptr || result == 0 || safe->getEdit() == nullptr)
            return;

        const auto instrument = result >= 100;
        const auto* choice = instrument ? &Instruments::all()[(size_t) (result - 100)] : nullptr;
        const auto count = (int) te::getAudioTracks (*safe->getEdit()).size();
        const auto name = choice != nullptr ? (juce::String (choice->group) == "Pianos" ? juce::String ("Piano") : juce::String (choice->group))
                                            : "Audio " + juce::String (count + 1);

        te::Track* after = safe->selectedTrack;
        if (after == nullptr && count > 0)
            after = te::getAudioTracks (*safe->getEdit()).getLast();

        safe->beginUndoStep();
        if (auto* track = safe->engine.addTrack (instrument, name, palette[count % (int) std::size (palette)], after,
                                                 choice != nullptr ? choice->id : "grand"))
        {
            if (instrument)
                safe->engine.setArmedTrack (track);
            safe->selectedTrack = track;
        }
        safe->rebuildHeaders();
    });
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
        headers[i]->setBounds (0, getLaneY (i) - scrollY, headerHolder.getWidth(), getLaneHeight (i));
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
                        for (auto* t : te::getAudioTracks (*owner.getEdit()))
                            used = used || (t != audioTrack && eng.getTrackInput (*t) == in);
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
        if (description.contains ("not downloaded")) g.setColour (warn);

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
    for (auto* btn : { &muteButton, &soloButton, &armButton })
    {
        btn->setBounds (row.removeFromLeft (26));
        row.removeFromLeft (4);
    }
    row.removeFromLeft (4);
    volume.setBounds (row);
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
        menu.addItem (5, "Instrument settings...");
        menu.addSubMenu ("Change instrument", instruments);
        menu.addItem (6, "Sound Library...");
        menu.addSeparator();
    }
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

        auto& owner = safe->owner;
        auto* ed = owner.getEdit();
        auto* t = safe->audioTrack;
        if (ed == nullptr)
            return;

        const auto tracks = te::getAudioTracks (*ed);
        const auto index = tracks.indexOf (t);

        if (result == 1)
        {
            askForName (&owner, "Rename track", t->getName(), [&owner, t] (const juce::String& n)
            {
                owner.beginUndoStep();
                t->setName (n);
                owner.refreshAll();
            });
        }
        else if (result == 5)
        {
            InstrumentPanel::open (owner.getEngine(), *t);
        }
        else if (result >= 400 && result < 450)
        {
            const auto list = owner.getEngine().getAudioInputs();
            owner.getEngine().setTrackInput (*t, result == 400 ? nullptr : list[(size_t) (result - 401)]);
            owner.refreshAll();
        }
        else if (result == 450 || result == 451)
        {
            owner.getEngine().setInputsMono (result == 450);
            owner.refreshAll();
        }
        else if (result >= 460 && result <= 462)
        {
            owner.getEngine().setMonitorMode (*t, result == 460 ? te::InputDevice::MonitorMode::automatic
                                                 : result == 461 ? te::InputDevice::MonitorMode::on
                                                                 : te::InputDevice::MonitorMode::off);
        }
        else if (result == 7)
        {
            owner.importAudioTo (*t);
        }
        else if (result == 8)
        {
            owner.beginUndoStep();
            if (auto* c = owner.getEngine().importAudio (*t, owner.getMemoFile(), owner.getEngine().getPosition().seconds))
            {
                c->setName ("Voice memo");
                owner.selectClip (c);
            }
            owner.refreshAll();
        }
        else if (result == 6)
        {
            SoundLibrary::openWindow();
        }
        else if (result >= 200)
        {
            owner.beginUndoStep();
            owner.getEngine().setInstrument (*t, Instruments::all()[(size_t) (result - 200)].id);
            owner.refreshAll();
        }
        else if (result >= 100)
        {
            owner.beginUndoStep();
            t->setColour (palette[result - 100]);
            owner.rebuildHeaders();
        }
        else if (result == 2 && index > 0)
        {
            owner.beginUndoStep();
            ed->moveTrack (t, te::TrackInsertPoint (*tracks[index - 1], true));
            owner.rebuildHeaders();
        }
        else if (result == 3 && index >= 0 && index < tracks.size() - 1)
        {
            owner.beginUndoStep();
            ed->moveTrack (t, te::TrackInsertPoint (*tracks[index + 1], false));
            owner.rebuildHeaders();
        }
        else if (result == 4)
        {
            juce::Component::SafePointer<TimelineView> safeOwner (&owner);
            Dialogs::confirm (&owner, "Delete track",
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
    return { x1, y + 3.0f, juce::jmax (3.0f, x2 - x1), (float) owner.getLaneHeight (laneIndex) - 6.0f };
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
                const auto file = wc->getPlaybackFile();
                auto& thumb = thumbnails[clip->itemID];
                if (thumb == nullptr)
                    thumb = std::make_unique<te::SmartThumbnail> (owner.getEdit()->engine, file, *this, owner.getEdit());
                else if (thumbnailFiles[clip->itemID] != file.getFile())
                    thumb->setNewFile (file);
                thumbnailFiles[clip->itemID] = file.getFile();

                auto area = r.reduced (2.0f, 3.0f).withTrimmedTop (13.0f).toNearestInt();
                const auto pos = clip->getPosition();
                g.saveState();
                g.reduceClipRegion (r.toNearestInt());
                g.setColour (colour.brighter (0.5f));
                thumb->drawChannels (g, area, { te::TimePosition() + pos.getOffset(), pos.getOffset() + pos.getLength() },
                                     juce::Decibels::decibelsToGain (wc->getGainDB()));
                g.restoreState();

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
        setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
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
        menu.addSeparator();
    }
    if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&clip))
    {
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
void TimelineView::importAudioTo (te::AudioTrack& track)
{
    chooser = std::make_unique<juce::FileChooser> ("Import an audio file", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<TimelineView> safe (this);
    te::AudioTrack* t = &track;
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, t] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        safe->beginUndoStep();
        if (auto* c = safe->engine.importAudio (*t, fc.getResult(), safe->beatToTime (safe->snap (safe->getPlayheadBeat())).inSeconds()))
            safe->selectClip (c);
        else
            Dialogs::message (safe.getComponent(), "Import audio", "This file could not be read as audio.");
        safe->refreshAll();
    });
}
