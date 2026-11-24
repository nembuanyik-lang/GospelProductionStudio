#include "PianoRoll.h"

namespace
{
    // Grid and length choices, in quarter notes (converted to song beats with the time signature)
    struct Choice { const char* name; double quarters; };

    const Choice gridChoices[] = {
        { "Grid: bar", -1.0 }, { "Grid: 1/4", 1.0 }, { "Grid: 1/8", 0.5 }, { "Grid: 1/16", 0.25 },
        { "Grid: 1/32", 0.125 }, { "Grid: 1/8T", 1.0 / 3.0 }, { "Grid: 1/16T", 1.0 / 6.0 }, { "Grid: off", 0.0 }
    };

    const Choice lengthChoices[] = {
        { "Length: last used", -2.0 }, { "Length: grid", 0.0 }, { "Length: 1/16", 0.25 }, { "Length: 1/8", 0.5 },
        { "Length: 1/4", 1.0 }, { "Length: 1/2", 2.0 }, { "Length: whole", 4.0 }
    };

    bool isBlack (int note)
    {
        const auto pc = note % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    double quartersPerBeat (te::Edit* ed)
    {
        if (ed != nullptr)
            if (auto* sig = ed->tempoSequence.getTimeSig (0))
                return 4.0 / juce::jmax (1, sig->denominator.get());
        return 1.0;
    }

    const juce::Colour noteColour { 0xffd4a843 };
}

//==============================================================================
//  Ruler with bar numbers and the chord strip
//==============================================================================
void RollRuler::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    g.fillAll (header);

    const auto bpb = owner.timeline.getBeatsPerBar();
    const auto first = std::floor (owner.xToBeat (0.0f) / bpb) * bpb;
    const auto last = owner.xToBeat ((float) getWidth());

    g.setFont (studioFont (11.5f, true));
    for (double b = juce::jmax (0.0, first); b <= last; b += 1.0)
    {
        const auto x = owner.beatToX (b);
        const auto isBar = std::fmod (b, bpb) < 1.0e-6;
        g.setColour (isBar ? textDim : cardBorder.brighter (0.2f));
        g.drawVerticalLine ((int) x, isBar ? 3.0f : (float) rulerHeight - 7.0f, (float) rulerHeight);
        if (isBar && owner.pxPerBeat * bpb > 26.0)
        {
            g.setColour (text);
            g.drawText (juce::String ((int) (b / bpb) + 1), (int) x + 4, 2, 40, rulerHeight - 4, juce::Justification::centredLeft);
        }
    }

    // Clip range on the ruler
    g.setColour (gold.withAlpha (0.25f));
    const auto cx1 = owner.beatToX (owner.clipStartBeat()), cx2 = owner.beatToX (owner.clipEndBeat());
    g.fillRect (juce::Rectangle<float> (cx1, (float) rulerHeight - 4.0f, cx2 - cx1, 4.0f));

    // Chord strip
    auto strip = getLocalBounds().withTrimmedTop (rulerHeight).toFloat();
    g.setColour (card);
    g.fillRect (strip);
    const auto key = owner.timeline.currentKey();
    for (const auto& e : owner.chordEvents)
    {
        const auto x1 = owner.beatToX (e.start), x2 = owner.beatToX (e.start + e.length);
        if (x2 < 0 || x1 > getWidth())
            continue;
        auto r = juce::Rectangle<float> (x1 + 1.0f, strip.getY() + 2.0f, juce::jmax (2.0f, x2 - x1 - 2.0f), strip.getHeight() - 4.0f);
        g.setColour (gold.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (gold.withAlpha (0.5f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (text);
        g.setFont (studioFont (r.getWidth() < 46.0f ? 10.5f : 12.5f, true));
        g.drawFittedText (Theory::label (e.chord, key, owner.timeline.chordDisplay), r.reduced (4.0f, 0.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.5f);
    }
    if (owner.chordEvents.empty())
    {
        g.setColour (textDim);
        g.setFont (studioFont (11.5f));
        g.drawText ("No chords yet: add them on the Chords lane or from the Palette", strip.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
    }

    const auto px = owner.beatToX (owner.timeline.getPlayheadBeat());
    g.setColour (gold);
    g.drawVerticalLine ((int) px, 0.0f, (float) getHeight());
    g.setColour (cardBorder);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void RollRuler::mouseDown (const juce::MouseEvent& e)
{
    const auto beat = juce::jmax (0.0, owner.xToBeat ((float) e.x));

    if (e.y >= rulerHeight)
        if (const auto* ev = owner.chordAtBeat (beat))
            owner.timeline.auditionChord (ev->chord);

    owner.engine.setPositionSeconds (owner.timeline.beatToTime (owner.snapBeat (beat)).inSeconds());
    owner.repaint();
}

void RollRuler::mouseDrag (const juce::MouseEvent& e)
{
    if (e.y < rulerHeight || e.getDistanceFromDragStartY() < -4)
    {
        const auto beat = juce::jmax (0.0, owner.xToBeat ((float) e.x));
        owner.engine.setPositionSeconds (owner.timeline.beatToTime (owner.snapBeat (beat)).inSeconds());
        owner.repaint();
    }
}

void RollRuler::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    owner.zoomX (w.deltaY > 0 ? 1.15 : 1.0 / 1.15, (float) e.x);
}

//==============================================================================
//  Vertical keyboard
//==============================================================================
void RollKeys::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    const auto key = owner.timeline.currentKey();
    const auto h = owner.noteHeight;

    g.fillAll (header);
    for (int n = 0; n < 128; ++n)
    {
        const auto y = owner.noteToY (n);
        if (y > getHeight() || y + h < 0)
            continue;

        const auto black = isBlack (n);
        auto r = juce::Rectangle<float> (0.0f, y, (float) getWidth(), h);
        g.setColour (black ? juce::Colour (0xff23262d) : juce::Colour (0xffe9e6de));
        g.fillRect (black ? r.withWidth (getWidth() * 0.62f) : r);
        if (black)
        {
            g.setColour (juce::Colour (0xffe9e6de));
            g.fillRect (r.withTrimmedLeft (getWidth() * 0.62f));
        }

        if (n == lastNote)
        {
            g.setColour (gold.withAlpha (0.8f));
            g.fillRect (r);
        }

        if (n % 12 == key.tonic)
        {
            g.setColour (gold);
            g.fillRect (r.removeFromRight (4.0f));
        }

        g.setColour (juce::Colour (0xff9a968c));
        g.drawHorizontalLine ((int) (y + h), 0.0f, (float) getWidth());

        if (n % 12 == 0 && h >= 9.0f)
        {
            g.setColour (juce::Colour (0xff3a3a3a));
            g.setFont (studioFont (juce::jmin (11.5f, h - 1.0f), true));
            g.drawText ("C" + juce::String (n / 12 - 1), 0, (int) y, getWidth() - 7, (int) h, juce::Justification::centredRight);
        }
    }
    g.setColour (cardBorder);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void RollKeys::mouseDown (const juce::MouseEvent& e)
{
    lastNote = owner.yToNote ((float) e.y);
    owner.hear (lastNote);
    repaint();
}

void RollKeys::mouseDrag (const juce::MouseEvent& e)
{
    const auto n = owner.yToNote ((float) e.y);
    if (n != lastNote)
    {
        lastNote = n;
        owner.hear (n);
        repaint();
    }
}

void RollKeys::mouseUp (const juce::MouseEvent&)
{
    lastNote = -1;
    repaint();
}

void RollKeys::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isAltDown())
        owner.zoomY (w.deltaY > 0 ? 1.12f : 1.0f / 1.12f, (float) e.y);
    else
        owner.scrollYBy (-juce::roundToInt (w.deltaY * 220.0f));
}

//==============================================================================
//  Note grid
//==============================================================================
void RollGrid::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    const auto w = (float) getWidth(), h = owner.noteHeight;
    const auto key = owner.timeline.currentKey();
    const auto firstNote = owner.yToNote ((float) getHeight()), lastNoteVisible = owner.yToNote (0.0f);

    g.fillAll (juce::Colour (0xff15171c));

    // Rows: notes in the key are lighter; black keys darker
    for (int n = firstNote; n <= lastNoteVisible; ++n)
    {
        const auto y = owner.noteToY (n);
        const auto inKey = Theory::isInKey (n % 12, key);
        g.setColour (inKey ? juce::Colour (0xff1e2128) : (isBlack (n) ? juce::Colour (0xff121418) : juce::Colour (0xff17191e)));
        g.fillRect (0.0f, y, w, h);
        g.setColour (n % 12 == key.tonic ? goldDim.withAlpha (0.35f) : (n % 12 == 0 ? cardBorder.brighter (0.15f) : juce::Colour (0xff1a1c22)));
        g.drawHorizontalLine ((int) (y + h), 0.0f, w);
    }

    // Chord Track behind the notes: chord tones of each chord are tinted gold
    for (const auto& e : owner.chordEvents)
    {
        const auto x1 = owner.beatToX (e.start), x2 = owner.beatToX (e.start + e.length);
        if (x2 < 0 || x1 > w)
            continue;

        const auto tones = Theory::chordPitchClasses (e.chord, key);
        const auto rootPc = (key.tonic + e.chord.root) % 12;
        for (int n = firstNote; n <= lastNoteVisible; ++n)
        {
            const auto pc = n % 12;
            if (std::find (tones.begin(), tones.end(), pc) == tones.end())
                continue;
            g.setColour (gold.withAlpha (pc == rootPc ? 0.16f : 0.08f));
            g.fillRect (x1, owner.noteToY (n), x2 - x1, h);
        }
        g.setColour (gold.withAlpha (0.35f));
        g.drawVerticalLine ((int) x1, 0.0f, (float) getHeight());
    }

    // Vertical lines: bars, beats and grid
    const auto bpb = owner.timeline.getBeatsPerBar();
    const auto gridStep = owner.gridBeats();
    const auto start = juce::jmax (0.0, owner.xToBeat (0.0f)), end = owner.xToBeat (w);
    if (gridStep > 0.0 && gridStep * owner.pxPerBeat >= 6.0 && gridStep < 1.0)
    {
        g.setColour (juce::Colour (0xff1d2027));
        for (double b = std::floor (start / gridStep) * gridStep; b <= end; b += gridStep)
            g.drawVerticalLine ((int) owner.beatToX (b), 0.0f, (float) getHeight());
    }
    for (double b = std::floor (start); b <= end; b += 1.0)
    {
        const auto isBar = std::fmod (b, bpb) < 1.0e-6;
        g.setColour (isBar ? juce::Colour (0xff3a404c) : juce::Colour (0xff262a33));
        g.drawVerticalLine ((int) owner.beatToX (b), 0.0f, (float) getHeight());
    }

    // Outside the clip is shaded
    const auto cx1 = owner.beatToX (owner.clipStartBeat()), cx2 = owner.beatToX (owner.clipEndBeat());
    g.setColour (juce::Colours::black.withAlpha (0.38f));
    if (cx1 > 0) g.fillRect (0.0f, 0.0f, cx1, (float) getHeight());
    if (cx2 < w) g.fillRect (cx2, 0.0f, w - cx2, (float) getHeight());

    // Notes
    const auto check = owner.checkNotes();
    for (auto* n : owner.getNotes())
    {
        const auto x1 = owner.beatToX (owner.contentToEdit (n->getStartBeat().inBeats()));
        const auto x2 = owner.beatToX (owner.contentToEdit (n->getEndBeat().inBeats()));
        if (x2 < 0 || x1 > w)
            continue;

        auto r = juce::Rectangle<float> (x1, owner.noteToY (n->getNoteNumber()), juce::jmax (3.0f, x2 - x1), h).reduced (0.5f, 1.0f);
        const auto selected = owner.isSelected (*n);
        const auto fit = check ? owner.fitOf (*n) : PianoRoll::Fit::noChord;
        const auto vel = n->getVelocity() / 127.0f;

        auto base = fit == PianoRoll::Fit::clash ? bad : noteColour;
        g.setColour (base.withMultipliedBrightness (0.55f + 0.45f * vel).withAlpha (selected ? 1.0f : 0.92f));
        g.fillRoundedRectangle (r, 3.0f);

        g.setColour (selected ? juce::Colours::white : base.darker (0.6f));
        g.drawRoundedRectangle (r, 3.0f, selected ? 1.6f : 1.0f);

        if (fit == PianoRoll::Fit::chordTone && r.getWidth() > 8.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillEllipse (r.getX() + 3.0f, r.getCentreY() - 2.0f, 4.0f, 4.0f);
        }

        if (r.getWidth() > 34.0f && h >= 12.0f)
        {
            g.setColour (juce::Colour (0xff1a1408));
            g.setFont (studioFont (juce::jmin (11.0f, h - 2.0f), true));
            g.drawText (Theory::midiNoteName (n->getNoteNumber(), key), r.withTrimmedLeft (fit == PianoRoll::Fit::chordTone ? 9.0f : 4.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    if (drag == Drag::rubber)
    {
        g.setColour (gold.withAlpha (0.12f));
        g.fillRect (rubber);
        g.setColour (gold);
        g.drawRect (rubber, 1);
    }

    if (owner.getNotes().empty())
    {
        g.setColour (textDim);
        g.setFont (studioFont (13.0f));
        g.drawText (owner.drawMode ? "Click to draw notes. Drag while drawing to set the length."
                                   : "Double-click to add a note, or press B for the Draw tool. Gold rows are the tones of each chord.",
                    getLocalBounds().reduced (20), juce::Justification::centredTop);
    }

    const auto px = owner.beatToX (owner.timeline.getPlayheadBeat());
    g.setColour (gold);
    g.drawVerticalLine ((int) px, 0.0f, (float) getHeight());
}

te::MidiNote* RollGrid::noteAt (juce::Point<int> p, Drag& edge) const
{
    const auto notes = owner.getNotes();
    for (int i = (int) notes.size(); --i >= 0;)
    {
        auto* n = notes[(size_t) i];
        const auto x1 = owner.beatToX (owner.contentToEdit (n->getStartBeat().inBeats()));
        const auto x2 = juce::jmax (x1 + 3.0f, owner.beatToX (owner.contentToEdit (n->getEndBeat().inBeats())));
        const auto y = owner.noteToY (n->getNoteNumber());
        if (p.y < y || p.y >= y + owner.noteHeight || p.x < x1 - 1 || p.x > x2 + 1)
            continue;

        const auto handle = juce::jmin (7.0f, (x2 - x1) / 3.0f);
        edge = p.x >= x2 - handle ? Drag::resizeEnd : (p.x <= x1 + handle && x2 - x1 > 14.0f ? Drag::resizeStart : Drag::move);
        return n;
    }
    edge = Drag::none;
    return nullptr;
}

void RollGrid::addNoteAt (juce::Point<int> p)
{
    auto clip = owner.getClip();
    if (clip == nullptr)
        return;

    const auto startEdit = juce::jmax (owner.clipStartBeat(), owner.floorToGrid (owner.xToBeat ((float) p.x)));
    const auto pitch = owner.yToNote ((float) p.y);
    const auto length = owner.newNoteLength();

    owner.timeline.beginUndoStep();
    auto* n = clip->getSequence().addNote (pitch, te::BeatPosition::fromBeats (owner.editToContent (startEdit)),
                                           te::BeatDuration::fromBeats (length), 100, 0, owner.undo());
    if (n == nullptr)
        return;

    owner.clearSelection();
    owner.select (n, false);
    owner.hear (pitch);
    beginNoteDrag (*n, Drag::resizeEnd, p);
    owner.notesChanged();
}

void RollGrid::beginNoteDrag (te::MidiNote& n, Drag mode, juce::Point<int> p)
{
    drag = mode;
    anchorNote = n.state;
    anchorStart = owner.contentToEdit (n.getStartBeat().inBeats());
    anchorEnd = owner.contentToEdit (n.getEndBeat().inBeats());
    anchorPitch = n.getNoteNumber();
    lastHeardPitch = anchorPitch;
    mouseStartBeat = owner.xToBeat ((float) p.x);

    originals.clear();
    for (auto* s : owner.getSelectedNotes())
        originals.push_back ({ s->state, owner.contentToEdit (s->getStartBeat().inBeats()), s->getLengthBeats().inBeats(), s->getNoteNumber() });
}

void RollGrid::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (owner.getClip() == nullptr)
        return;

    Drag edge;
    auto* hit = noteAt (e.getPosition(), edge);

    if (e.mods.isPopupMenu())
    {
        if (hit != nullptr)
        {
            // Right-click deletes the note (and the rest of the selection, if it was part of it)
            if (! owner.isSelected (*hit))
            {
                owner.clearSelection();
                owner.select (hit, false);
            }
            owner.deleteSelected();
        }
        else
        {
            owner.showEditMenu();
        }
        return;
    }

    if (hit != nullptr)
    {
        if (e.mods.isShiftDown())
        {
            if (owner.isSelected (*hit))
            {
                owner.selection.erase (std::remove (owner.selection.begin(), owner.selection.end(), hit->state), owner.selection.end());
                owner.notesChanged();
                return;
            }
            owner.select (hit, true);
        }
        else if (! owner.isSelected (*hit))
        {
            owner.clearSelection();
            owner.select (hit, false);
        }

        owner.hear (hit->getNoteNumber());
        owner.timeline.beginUndoStep();

        // Alt-drag copies the selected notes, the copies are dragged
        if (e.mods.isAltDown() && edge == Drag::move)
        {
            auto& seq = owner.getClip()->getSequence();
            std::vector<juce::ValueTree> copies;
            juce::ValueTree anchorCopy;
            for (auto* s : owner.getSelectedNotes())
                if (auto* c = seq.addNote (s->getNoteNumber(), s->getStartBeat(), s->getLengthBeats(), s->getVelocity(), s->getColour(), owner.undo()))
                {
                    copies.push_back (c->state);
                    if (s == hit) anchorCopy = c->state;
                }
            owner.selection = copies;
            if (auto* a = owner.findNote (anchorCopy))
                hit = a;
        }

        beginNoteDrag (*hit, edge, e.getPosition());
        owner.updateStatus();
        repaint();
        return;
    }

    if (owner.drawMode)
    {
        addNoteAt (e.getPosition());
        return;
    }

    // Rubber-band selection
    selectionBefore = e.mods.isShiftDown() ? owner.selection : std::vector<juce::ValueTree>();
    if (! e.mods.isShiftDown())
        owner.clearSelection();
    drag = Drag::rubber;
    rubber = { e.x, e.y, 0, 0 };
    repaint();
}

void RollGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::none || owner.getClip() == nullptr)
        return;

    if (drag == Drag::rubber)
    {
        rubber = juce::Rectangle<int> (e.getMouseDownPosition(), e.getPosition());
        owner.selection = selectionBefore;
        for (auto* n : owner.getNotes())
        {
            const auto x1 = owner.beatToX (owner.contentToEdit (n->getStartBeat().inBeats()));
            const auto x2 = owner.beatToX (owner.contentToEdit (n->getEndBeat().inBeats()));
            const auto r = juce::Rectangle<float> (x1, owner.noteToY (n->getNoteNumber()), juce::jmax (3.0f, x2 - x1), owner.noteHeight);
            if (r.intersects (rubber.toFloat()) && ! owner.isSelected (*n))
                owner.selection.push_back (n->state);
        }
        owner.updateStatus();
        repaint();
        owner.repaint();
        return;
    }

    auto* um = owner.undo();
    const auto raw = owner.xToBeat ((float) e.x) - mouseStartBeat;
    const auto minLength = owner.gridBeats() > 0.0 ? juce::jmin (owner.gridBeats(), 0.25) : 0.0625;

    if (drag == Drag::move)
    {
        auto newAnchor = owner.snapBeat (anchorStart + raw);
        auto deltaBeats = newAnchor - anchorStart;
        auto deltaPitch = owner.yToNote ((float) e.y) - anchorPitch;

        // Keep every note inside the clip start and the MIDI range
        for (const auto& o : originals)
        {
            deltaBeats = juce::jmax (deltaBeats, owner.clipStartBeat() - o.start);
            deltaPitch = juce::jlimit (-o.pitch, 127 - o.pitch, deltaPitch);
        }

        for (const auto& o : originals)
            if (auto* n = owner.findNote (o.note))
            {
                n->setStartAndLength (te::BeatPosition::fromBeats (owner.editToContent (o.start + deltaBeats)),
                                      te::BeatDuration::fromBeats (o.length), um);
                n->setNoteNumber (o.pitch + deltaPitch, um);
            }

        if (anchorPitch + deltaPitch != lastHeardPitch)
        {
            lastHeardPitch = anchorPitch + deltaPitch;
            owner.hear (lastHeardPitch);
        }
    }
    else if (drag == Drag::resizeEnd)
    {
        const auto newEnd = owner.snapBeat (anchorEnd + raw);
        const auto delta = newEnd - anchorEnd;
        for (const auto& o : originals)
            if (auto* n = owner.findNote (o.note))
                n->setStartAndLength (n->getStartBeat(), te::BeatDuration::fromBeats (juce::jmax (minLength, o.length + delta)), um);
    }
    else if (drag == Drag::resizeStart)
    {
        const auto newStart = owner.snapBeat (anchorStart + raw);
        auto delta = newStart - anchorStart;
        for (const auto& o : originals)
            delta = juce::jlimit (owner.clipStartBeat() - o.start, o.length - minLength, delta);
        for (const auto& o : originals)
            if (auto* n = owner.findNote (o.note))
                n->setStartAndLength (te::BeatPosition::fromBeats (owner.editToContent (o.start + delta)),
                                      te::BeatDuration::fromBeats (o.length - delta), um);
    }

    owner.notesChanged();
}

void RollGrid::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::rubber)
    {
        drag = Drag::none;
        repaint();
        return;
    }

    if (drag != Drag::none && owner.getClip() != nullptr)
    {
        if (auto* n = owner.findNote (anchorNote))
            owner.lastLength = n->getLengthBeats().inBeats();

        // Grow the clip to the next bar line if notes now run past its end
        double maxEnd = 0.0;
        for (auto* n : owner.getNotes())
            maxEnd = juce::jmax (maxEnd, owner.contentToEdit (n->getEndBeat().inBeats()));
        if (maxEnd > owner.clipEndBeat() + 1.0e-6)
        {
            const auto bpb = owner.timeline.getBeatsPerBar();
            owner.getClip()->setEnd (owner.timeline.beatToTime (std::ceil (maxEnd / bpb - 1.0e-6) * bpb), false);
        }
    }

    drag = Drag::none;
    originals.clear();
    owner.notesChanged();
}

void RollGrid::mouseMove (const juce::MouseEvent& e)
{
    Drag edge;
    auto* n = noteAt (e.getPosition(), edge);
    if (n != nullptr)
        setMouseCursor (edge == Drag::move ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (owner.drawMode ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);

    const auto key = owner.timeline.currentKey();
    const auto pitch = owner.yToNote ((float) e.y);
    owner.status = n != nullptr ? Theory::midiNoteName (n->getNoteNumber(), key) + "  velocity " + juce::String (n->getVelocity())
                                : Theory::midiNoteName (pitch, key);
    owner.repaint (0, owner.getHeight() - PianoRoll::footerHeight, owner.getWidth(), PianoRoll::footerHeight);
}

void RollGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (owner.drawMode || e.mods.isPopupMenu())
        return;

    Drag edge;
    if (noteAt (e.getPosition(), edge) == nullptr)
    {
        addNoteAt (e.getPosition());
        drag = Drag::none;
        originals.clear();
    }
}

void RollGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
        owner.zoomX (w.deltaY > 0 ? 1.15 : 1.0 / 1.15, (float) e.x);
    else if (e.mods.isAltDown())
        owner.zoomY (w.deltaY > 0 ? 1.12f : 1.0f / 1.12f, (float) e.y);
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const auto amount = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        owner.scrollXBy (-amount * 300.0 / owner.pxPerBeat);
    }
    else
        owner.scrollYBy (-juce::roundToInt (w.deltaY * 220.0f));
}

bool RollGrid::keyPressed (const juce::KeyPress& key)
{
    return owner.keyPressed (key);
}

//==============================================================================
//  Velocity lane
//==============================================================================
void RollVelocity::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    g.fillAll (juce::Colour (0xff111317));
    g.setColour (cardBorder);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());

    const auto usable = (float) getHeight() - 8.0f;
    for (auto* n : owner.getNotes())
    {
        const auto x = owner.beatToX (owner.contentToEdit (n->getStartBeat().inBeats()));
        if (x < -6 || x > getWidth())
            continue;
        const auto v = n->getVelocity() / 127.0f;
        const auto top = 4.0f + usable * (1.0f - v);
        const auto selected = owner.isSelected (*n);
        g.setColour ((selected ? juce::Colours::white : noteColour).withAlpha (0.35f + 0.5f * v));
        g.fillRect (x, top, 4.0f, (float) getHeight() - top);
        g.setColour (selected ? juce::Colours::white : noteColour);
        g.fillEllipse (x - 2.0f, top - 3.0f, 8.0f, 6.0f);
    }

    const auto px = owner.beatToX (owner.timeline.getPlayheadBeat());
    g.setColour (gold.withAlpha (0.6f));
    g.drawVerticalLine ((int) px, 0.0f, (float) getHeight());
}

void RollVelocity::mouseDown (const juce::MouseEvent& e)
{
    if (owner.getClip() == nullptr)
        return;

    owner.timeline.beginUndoStep();
    relative = false;
    startY = e.y;
    startVelocities.clear();

    // Starting on a selected note's bar adjusts every selected note together
    for (auto* n : owner.getNotes())
    {
        const auto x = owner.beatToX (owner.contentToEdit (n->getStartBeat().inBeats()));
        if (std::abs (e.x - (x + 2.0f)) <= 5.0f && owner.isSelected (*n) && owner.getSelectedNotes().size() > 1)
        {
            relative = true;
            for (auto* s : owner.getSelectedNotes())
                startVelocities.push_back ({ s->state, s->getVelocity() });
            break;
        }
    }

    apply (e);
}

void RollVelocity::mouseDrag (const juce::MouseEvent& e)
{
    apply (e);
}

void RollVelocity::apply (const juce::MouseEvent& e)
{
    auto* um = owner.undo();
    const auto usable = (float) getHeight() - 8.0f;

    if (relative)
    {
        const auto delta = juce::roundToInt ((startY - e.y) / usable * 127.0f);
        for (const auto& [state, v] : startVelocities)
            if (auto* n = owner.findNote (state))
                n->setVelocity (juce::jlimit (1, 127, v + delta), um);
        owner.status = "Velocity " + juce::String (delta >= 0 ? "+" : "") + juce::String (delta) + " on " + juce::String ((int) startVelocities.size()) + " notes";
    }
    else
    {
        const auto v = juce::jlimit (1, 127, juce::roundToInt ((1.0f - (e.y - 4.0f) / usable) * 127.0f));
        for (auto* n : owner.getNotes())
        {
            const auto x = owner.beatToX (owner.contentToEdit (n->getStartBeat().inBeats()));
            if (std::abs (e.x - (x + 2.0f)) <= 5.0f)
                n->setVelocity (v, um);
        }
        owner.status = "Velocity " + juce::String (v);
    }

    owner.notesChanged();
}

//==============================================================================
//  PianoRoll
//==============================================================================
PianoRoll::PianoRoll (StudioEngine& e, TimelineView& t)
    : engine (e), timeline (t)
{
    auto noFocus = [] (juce::Component& c) { c.setWantsKeyboardFocus (false); };

    for (auto* b : { &selectButton, &drawButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (3001);
        noFocus (*b);
        addAndMakeVisible (b);
    }
    selectButton.setTooltip ("Select tool (V): click, drag and box-select notes. Double-click adds a note.");
    drawButton.setTooltip ("Draw tool (B): click to draw notes, drag to set their length.");
    selectButton.onClick = [this] { setDrawMode (false); };
    drawButton.onClick   = [this] { setDrawMode (true); };
    selectButton.setToggleState (true, juce::dontSendNotification);

    for (int i = 0; i < (int) std::size (gridChoices); ++i)
        gridBox.addItem (gridChoices[i].name, i + 1);
    gridBox.setSelectedItemIndex (StudioEngine::getSetting ("rollGrid", 3), juce::dontSendNotification);
    gridBox.onChange = [this] { StudioEngine::setSetting ("rollGrid", gridBox.getSelectedItemIndex()); repaint(); };
    gridBox.setTooltip ("Grid for drawing, moving and quantising (T = triplet)");

    for (int i = 0; i < (int) std::size (lengthChoices); ++i)
        lengthBox.addItem (lengthChoices[i].name, i + 1);
    lengthBox.setSelectedItemIndex (StudioEngine::getSetting ("rollLength", 0), juce::dontSendNotification);
    lengthBox.onChange = [this] { StudioEngine::setSetting ("rollLength", lengthBox.getSelectedItemIndex()); };
    lengthBox.setTooltip ("Length of new notes");

    editButton.setTooltip ("Quantise, transpose, legato, humanise, velocity, copy and paste");
    editButton.onClick = [this] { showEditMenu(); };

    fitButton.setTooltip ("Move notes that clash with the chord onto the nearest chord tone (selected notes, or all)");
    fitButton.onClick = [this] { fitToChords(); };

    checkToggle.setToggleState (StudioEngine::getSetting ("rollCheck", 1) != 0, juce::dontSendNotification);
    checkToggle.setTooltip ("White dot = chord tone. Red = clashes with the chord (outside the chord and the key).");
    checkToggle.onClick = [this] { StudioEngine::setSetting ("rollCheck", checkToggle.getToggleState() ? 1 : 0); repaint(); };

    zoomButton.setTooltip ("Show the whole clip");
    zoomButton.onClick = [this] { zoomToClip(); };

    splitButton.setClickingTogglesState (true);
    splitButton.setToggleState (StudioEngine::getSetting ("rollSplit", 1) != 0, juce::dontSendNotification);
    splitButton.setTooltip ("Show the timeline above the piano roll");
    splitButton.onClick = [this]
    {
        StudioEngine::setSetting ("rollSplit", splitButton.getToggleState() ? 1 : 0);
        if (onSplitChanged) onSplitChanged();
    };

    closeButton.setTooltip ("Close the piano roll (Esc)");
    closeButton.onClick = [this] { if (onClose) onClose(); };

    for (juce::Component* c : { (juce::Component*) &gridBox, (juce::Component*) &lengthBox, (juce::Component*) &editButton,
                                (juce::Component*) &fitButton, (juce::Component*) &checkToggle, (juce::Component*) &zoomButton,
                                (juce::Component*) &splitButton, (juce::Component*) &closeButton })
    {
        noFocus (*c);
        addAndMakeVisible (c);
    }
    gridBox.setWantsKeyboardFocus (false);

    for (juce::Component* c : { (juce::Component*) &ruler, (juce::Component*) &keys, (juce::Component*) &grid,
                                (juce::Component*) &velocity, (juce::Component*) &hScroll, (juce::Component*) &vScroll })
        addAndMakeVisible (c);

    hScroll.addListener (this);
    vScroll.addListener (this);
    hScroll.setAutoHide (false);
    vScroll.setAutoHide (false);

    setWantsKeyboardFocus (true);
}

PianoRoll::~PianoRoll()
{
    hScroll.removeListener (this);
    vScroll.removeListener (this);
}

//==============================================================================
void PianoRoll::setClip (te::MidiClip* c)
{
    clip = c;
    selection.clear();
    status = {};

    if (clip == nullptr)
    {
        stopTimer();
        return;
    }

    refreshGeometry();
    zoomToClip();

    // Centre the view on the notes (or around middle C)
    int lo = 127, hi = 0;
    for (auto* n : getNotes())
    {
        lo = juce::jmin (lo, n->getNoteNumber());
        hi = juce::jmax (hi, n->getNoteNumber());
    }
    const auto centre = lo <= hi ? (lo + hi) / 2 : 62;
    scrollY = juce::roundToInt ((127 - centre) * noteHeight - grid.getHeight() / 2.0f);
    scrollYBy (0);

    startTimerHz (25);
    updateStatus();
    repaint();
    grid.grabKeyboardFocus();
}

void PianoRoll::refreshGeometry()
{
    if (clip == nullptr)
        return;

    const auto start = clip->getPosition().getStart();
    contentOffset = timeline.timeToBeat (start) - clip->getContentBeatAtTime (start).inBeats();

    if (auto* ed = getEdit())
        chordEvents = ChordTrack::getEvents (*ed);
}

double PianoRoll::clipStartBeat() const
{
    return clip != nullptr ? timeline.timeToBeat (clip->getPosition().getStart()) : 0.0;
}

double PianoRoll::clipEndBeat() const
{
    return clip != nullptr ? timeline.timeToBeat (clip->getPosition().getEnd()) : 0.0;
}

juce::UndoManager* PianoRoll::undo() const
{
    auto* ed = getEdit();
    return ed != nullptr ? &ed->getUndoManager() : nullptr;
}

double PianoRoll::gridBeats() const
{
    const auto q = gridChoices[juce::jlimit (0, (int) std::size (gridChoices) - 1, gridBox.getSelectedItemIndex())].quarters;
    if (q < 0.0) return timeline.getBeatsPerBar();
    return q / quartersPerBeat (getEdit());
}

double PianoRoll::snapBeat (double beat) const
{
    const auto g = gridBeats();
    return g > 0.0 ? std::round (beat / g) * g : beat;
}

double PianoRoll::floorToGrid (double beat) const
{
    const auto g = gridBeats();
    return g > 0.0 ? std::floor (beat / g + 1.0e-6) * g : beat;
}

double PianoRoll::newNoteLength() const
{
    const auto q = lengthChoices[juce::jlimit (0, (int) std::size (lengthChoices) - 1, lengthBox.getSelectedItemIndex())].quarters;
    const auto grid = gridBeats() > 0.0 ? gridBeats() : 1.0;

    if (q < -1.0) return lastLength > 0.0 ? lastLength : grid;
    if (q == 0.0) return grid;
    return q / quartersPerBeat (getEdit());
}

//==============================================================================
std::vector<te::MidiNote*> PianoRoll::getNotes() const
{
    std::vector<te::MidiNote*> out;
    if (clip != nullptr)
        for (auto* n : clip->getSequence().getNotes())
            out.push_back (n);
    return out;
}

te::MidiNote* PianoRoll::findNote (const juce::ValueTree& state) const
{
    if (clip == nullptr || ! state.isValid())
        return nullptr;
    for (auto* n : clip->getSequence().getNotes())
        if (n->state == state)
            return n;
    return nullptr;
}

bool PianoRoll::isSelected (const te::MidiNote& n) const
{
    return std::find (selection.begin(), selection.end(), n.state) != selection.end();
}

void PianoRoll::select (te::MidiNote* n, bool add)
{
    if (n == nullptr)
        return;
    if (! add)
        selection.clear();
    if (! isSelected (*n))
        selection.push_back (n->state);
    updateStatus();
    repaint();
}

void PianoRoll::clearSelection()
{
    selection.clear();
    updateStatus();
    repaint();
}

std::vector<te::MidiNote*> PianoRoll::getSelectedNotes() const
{
    std::vector<te::MidiNote*> out;
    for (const auto& s : selection)
        if (auto* n = findNote (s))
            out.push_back (n);
    return out;
}

std::vector<te::MidiNote*> PianoRoll::getTargetNotes() const
{
    auto sel = getSelectedNotes();
    return sel.empty() ? getNotes() : sel;
}

void PianoRoll::hear (int note)
{
    engine.audition ({ note }, false, 320);
}

void PianoRoll::notesChanged()
{
    updateStatus();
    repaint();
    timeline.repaint();
}

void PianoRoll::updateStatus()
{
    const auto sel = getSelectedNotes();
    if (! sel.empty())
    {
        const auto key = timeline.currentKey();
        status = juce::String ((int) sel.size()) + (sel.size() == 1 ? " note selected: " + Theory::midiNoteName (sel.front()->getNoteNumber(), key)
                                                                         + "  velocity " + juce::String (sel.front()->getVelocity())
                                                                   : juce::String (" notes selected"));
    }
    else if (clip != nullptr)
    {
        status = juce::String ((int) getNotes().size()) + " notes in this clip";
    }
}

//==============================================================================
const ChordTrack::Event* PianoRoll::chordAtBeat (double editBeat) const
{
    const auto i = ChordTrack::indexAt (chordEvents, editBeat + 1.0e-6);
    return i >= 0 ? &chordEvents[(size_t) i] : nullptr;
}

const ChordTrack::Event* PianoRoll::chordForNote (const te::MidiNote& n) const
{
    // Notes played a little early (pushed, humanised) still belong to the chord they mostly sound over.
    const auto s = contentToEdit (n.getStartBeat().inBeats());
    const auto e = s + juce::jmax (1.0e-3, n.getLengthBeats().inBeats());
    const ChordTrack::Event* best = nullptr;
    double bestOverlap = 0.0;

    for (const auto& ev : chordEvents)
    {
        const auto overlap = juce::jmin (e, ev.start + ev.length) - juce::jmax (s, ev.start);
        if (overlap > bestOverlap + 1.0e-9)
        {
            bestOverlap = overlap;
            best = &ev;
        }
    }
    return best;
}

PianoRoll::Fit PianoRoll::fitOf (const te::MidiNote& n) const
{
    const auto* ev = chordForNote (n);
    if (ev == nullptr)
        return Fit::noChord;

    const auto key = timeline.currentKey();
    const auto pc = n.getNoteNumber() % 12;
    const auto tones = Theory::chordPitchClasses (ev->chord, key);
    if (std::find (tones.begin(), tones.end(), pc) != tones.end())
        return Fit::chordTone;
    return Theory::isInKey (pc, key) ? Fit::inKey : Fit::clash;
}

//==============================================================================
//  Editing commands
//==============================================================================
void PianoRoll::deleteSelected()
{
    if (clip == nullptr)
        return;
    const auto sel = getSelectedNotes();
    if (sel.empty())
        return;

    timeline.beginUndoStep();
    for (auto* n : sel)
        clip->getSequence().removeNote (*n, undo());
    selection.clear();
    notesChanged();
}

void PianoRoll::selectAll()
{
    selection.clear();
    for (auto* n : getNotes())
        selection.push_back (n->state);
    notesChanged();
}

void PianoRoll::transpose (int semitones)
{
    const auto notes = getTargetNotes();
    if (notes.empty())
        return;

    timeline.beginUndoStep();
    for (auto* n : notes)
        n->setNoteNumber (juce::jlimit (0, 127, n->getNoteNumber() + semitones), undo());
    if (notes.size() == 1)
        hear (notes.front()->getNoteNumber());
    notesChanged();
}

void PianoRoll::nudge (int steps)
{
    const auto notes = getSelectedNotes();
    const auto step = gridBeats() > 0.0 ? gridBeats() : 0.25;
    if (notes.empty())
        return;

    auto delta = steps * step;
    for (auto* n : notes)
        delta = juce::jmax (delta, clipStartBeat() - contentToEdit (n->getStartBeat().inBeats()));

    timeline.beginUndoStep();
    for (auto* n : notes)
        n->setStartAndLength (n->getStartBeat() + te::BeatDuration::fromBeats (delta), n->getLengthBeats(), undo());
    notesChanged();
}

void PianoRoll::quantise (double strength, bool ends)
{
    const auto g = gridBeats();
    const auto notes = getTargetNotes();
    if (g <= 0.0 || notes.empty())
        return;

    timeline.beginUndoStep();
    for (auto* n : notes)
    {
        const auto start = contentToEdit (n->getStartBeat().inBeats());
        auto newStart = start + (snapBeat (start) - start) * strength;
        newStart = juce::jmax (clipStartBeat(), newStart);
        auto length = n->getLengthBeats().inBeats();

        if (ends)
        {
            const auto end = start + length;
            const auto newEnd = end + (snapBeat (end) - end) * strength;
            length = juce::jmax (g * strength + (1.0 - strength) * length * 0.5, newEnd - newStart);
            length = juce::jmax (length, g * 0.5);
        }

        n->setStartAndLength (te::BeatPosition::fromBeats (editToContent (newStart)), te::BeatDuration::fromBeats (length), undo());
    }
    notesChanged();
}

void PianoRoll::legato()
{
    auto notes = getTargetNotes();
    if (notes.size() < 2)
        return;

    std::sort (notes.begin(), notes.end(), [] (auto* a, auto* b) { return a->getStartBeat() < b->getStartBeat(); });

    timeline.beginUndoStep();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        // Next note that starts later (notes of a chord share the same next start)
        const auto start = notes[i]->getStartBeat().inBeats();
        for (size_t j = i + 1; j < notes.size(); ++j)
        {
            const auto next = notes[j]->getStartBeat().inBeats();
            if (next > start + 1.0e-4)
            {
                notes[i]->setStartAndLength (notes[i]->getStartBeat(), te::BeatDuration::fromBeats (next - start), undo());
                break;
            }
        }
    }
    notesChanged();
}

void PianoRoll::humanise()
{
    const auto notes = getTargetNotes();
    if (notes.empty())
        return;

    auto& random = juce::Random::getSystemRandom();
    const auto range = 0.03 / quartersPerBeat (getEdit());       // about a 1/128 note either way

    timeline.beginUndoStep();
    for (auto* n : notes)
    {
        const auto shift = (random.nextDouble() * 2.0 - 1.0) * range;
        const auto start = juce::jmax (clipStartBeat(), contentToEdit (n->getStartBeat().inBeats()) + shift);
        n->setStartAndLength (te::BeatPosition::fromBeats (editToContent (start)), n->getLengthBeats(), undo());
        n->setVelocity (juce::jlimit (1, 127, n->getVelocity() + random.nextInt (17) - 8), undo());
    }
    notesChanged();
}

void PianoRoll::scaleVelocity (double factor)
{
    const auto notes = getTargetNotes();
    if (notes.empty())
        return;

    timeline.beginUndoStep();
    for (auto* n : notes)
        n->setVelocity (juce::jlimit (1, 127, juce::roundToInt (n->getVelocity() * factor)), undo());
    notesChanged();
}

void PianoRoll::fitToChords()
{
    const auto notes = getTargetNotes();
    const auto key = timeline.currentKey();
    int moved = 0;

    timeline.beginUndoStep();
    for (auto* n : notes)
    {
        if (fitOf (*n) != Fit::clash)
            continue;

        const auto* ev = chordForNote (*n);
        if (ev == nullptr)
            continue;
        const auto tones = Theory::chordPitchClasses (ev->chord, key);
        const auto pitch = n->getNoteNumber();

        // Nearest chord tone, preferring the one below on a tie
        for (int d = 1; d <= 6; ++d)
        {
            const auto down = pitch - d, up = pitch + d;
            if (down >= 0 && std::find (tones.begin(), tones.end(), down % 12) != tones.end()) { n->setNoteNumber (down, undo()); ++moved; break; }
            if (up <= 127 && std::find (tones.begin(), tones.end(), up % 12) != tones.end())   { n->setNoteNumber (up, undo());   ++moved; break; }
        }
    }

    notesChanged();
    status = moved == 0 ? juce::String ("No clashing notes: everything fits the chords")
                        : juce::String (moved) + (moved == 1 ? " note moved onto the chord" : " notes moved onto the chords");
    repaint();
}

void PianoRoll::selectClashes()
{
    selection.clear();
    for (auto* n : getNotes())
        if (fitOf (*n) == Fit::clash)
            selection.push_back (n->state);
    notesChanged();
    if (selection.empty())
        status = "No clashing notes";
}

void PianoRoll::copySelection()
{
    const auto sel = getSelectedNotes();
    if (sel.empty())
        return;

    double first = 1.0e9;
    for (auto* n : sel)
        first = juce::jmin (first, n->getStartBeat().inBeats());

    clipboard.clear();
    for (auto* n : sel)
        clipboard.push_back ({ n->getNoteNumber(), n->getStartBeat().inBeats() - first, n->getLengthBeats().inBeats(), n->getVelocity() });
    status = juce::String ((int) clipboard.size()) + " notes copied";
    repaint();
}

void PianoRoll::pasteAtPlayhead()
{
    if (clip == nullptr || clipboard.empty())
        return;

    const auto at = editToContent (juce::jmax (clipStartBeat(), floorToGrid (timeline.getPlayheadBeat())));
    timeline.beginUndoStep();
    selection.clear();
    double end = 0.0;
    for (const auto& c : clipboard)
    {
        if (auto* n = clip->getSequence().addNote (c.pitch, te::BeatPosition::fromBeats (at + c.start), te::BeatDuration::fromBeats (c.length),
                                                   c.velocity, 0, undo()))
            selection.push_back (n->state);
        end = juce::jmax (end, at + c.start + c.length);
    }

    if (contentToEdit (end) > clipEndBeat())
    {
        const auto bpb = timeline.getBeatsPerBar();
        clip->setEnd (timeline.beatToTime (std::ceil (contentToEdit (end) / bpb - 1.0e-6) * bpb), false);
    }
    notesChanged();
}

void PianoRoll::duplicateSelection()
{
    const auto sel = getSelectedNotes();
    if (clip == nullptr || sel.empty())
        return;

    double first = 1.0e9, last = 0.0;
    for (auto* n : sel)
    {
        first = juce::jmin (first, n->getStartBeat().inBeats());
        last = juce::jmax (last, n->getEndBeat().inBeats());
    }

    // Place the copy right after the selection, rounded up to the grid
    const auto g = gridBeats() > 0.0 ? gridBeats() : 0.25;
    const auto shift = std::ceil ((last - first) / g - 1.0e-6) * g;

    timeline.beginUndoStep();
    std::vector<juce::ValueTree> copies;
    double end = 0.0;
    for (auto* n : sel)
    {
        if (auto* c = clip->getSequence().addNote (n->getNoteNumber(), n->getStartBeat() + te::BeatDuration::fromBeats (shift),
                                                   n->getLengthBeats(), n->getVelocity(), n->getColour(), undo()))
            copies.push_back (c->state);
        end = juce::jmax (end, n->getEndBeat().inBeats() + shift);
    }
    selection = copies;

    if (contentToEdit (end) > clipEndBeat())
    {
        const auto bpb = timeline.getBeatsPerBar();
        clip->setEnd (timeline.beatToTime (std::ceil (contentToEdit (end) / bpb - 1.0e-6) * bpb), false);
    }
    notesChanged();
}

void PianoRoll::showEditMenu()
{
    const auto hasSelection = ! getSelectedNotes().empty();
    const juce::String target = hasSelection ? " (selected)" : " (all notes)";

    juce::PopupMenu quant, transposeMenu, vel, menu;
    quant.addItem (10, "Starts, 100%");
    quant.addItem (11, "Starts, 75% (tight but human)");
    quant.addItem (12, "Starts, 50% (keep the feel)");
    quant.addItem (13, "Starts and ends, 100%");

    transposeMenu.addItem (20, "Up a semitone  (Up arrow)");
    transposeMenu.addItem (21, "Down a semitone  (Down arrow)");
    transposeMenu.addItem (22, "Up an octave  (Shift + Up)");
    transposeMenu.addItem (23, "Down an octave  (Shift + Down)");

    vel.addItem (30, "Louder (+10%)");
    vel.addItem (31, "Softer (-10%)");
    vel.addItem (32, "Set all to 100");

    menu.addSubMenu ("Quantise to the grid" + target, quant);
    menu.addSubMenu ("Transpose" + target, transposeMenu);
    menu.addSubMenu ("Velocity" + target, vel);
    menu.addItem (40, "Legato (each note lasts to the next)" + target);
    menu.addItem (41, "Humanise (small timing and velocity changes)" + target);
    menu.addItem (42, "Fit to chords" + target);
    menu.addSeparator();
    menu.addItem (50, "Select all  (Ctrl+A)");
    menu.addItem (51, "Select clashing notes");
    menu.addItem (52, "Copy  (Ctrl+C)", hasSelection);
    menu.addItem (53, "Paste at playhead  (Ctrl+V)", ! clipboard.empty());
    menu.addItem (54, "Duplicate  (Ctrl+D)", hasSelection);
    menu.addItem (55, "Delete  (Delete)", hasSelection);

    juce::Component::SafePointer<PianoRoll> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&editButton), [safe] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        switch (r)
        {
            case 10: safe->quantise (1.0, false); break;
            case 11: safe->quantise (0.75, false); break;
            case 12: safe->quantise (0.5, false); break;
            case 13: safe->quantise (1.0, true); break;
            case 20: safe->transpose (1); break;
            case 21: safe->transpose (-1); break;
            case 22: safe->transpose (12); break;
            case 23: safe->transpose (-12); break;
            case 30: safe->scaleVelocity (1.1); break;
            case 31: safe->scaleVelocity (0.9); break;
            case 32:
            {
                safe->timeline.beginUndoStep();
                for (auto* n : safe->getTargetNotes())
                    n->setVelocity (100, safe->undo());
                safe->notesChanged();
                break;
            }
            case 40: safe->legato(); break;
            case 41: safe->humanise(); break;
            case 42: safe->fitToChords(); break;
            case 50: safe->selectAll(); break;
            case 51: safe->selectClashes(); break;
            case 52: safe->copySelection(); break;
            case 53: safe->pasteAtPlayhead(); break;
            case 54: safe->duplicateSelection(); break;
            case 55: safe->deleteSelected(); break;
            default: break;
        }
    });
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    if (clip == nullptr)
        return false;

    const auto code = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) key.getKeyCode());
    const auto cmd = key.getModifiers().isCommandDown();
    const auto shift = key.getModifiers().isShiftDown();

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
    if (key == juce::KeyPress::escapeKey)
    {
        if (! selection.empty()) clearSelection();
        else if (onClose) onClose();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::upKey)    { transpose (shift ? 12 : 1); return true; }
    if (key.getKeyCode() == juce::KeyPress::downKey)  { transpose (shift ? -12 : -1); return true; }
    if (key.getKeyCode() == juce::KeyPress::leftKey)  { nudge (-1); return true; }
    if (key.getKeyCode() == juce::KeyPress::rightKey) { nudge (1); return true; }

    if (cmd && code == 'a') { selectAll(); return true; }
    if (cmd && code == 'c') { copySelection(); return true; }
    if (cmd && code == 'v') { pasteAtPlayhead(); return true; }
    if (cmd && code == 'd') { duplicateSelection(); return true; }
    if (cmd && (code == 'z' || code == 'y'))
    {
        if (auto* ed = getEdit())
        {
            if (code == 'z' && ! shift) ed->undo();
            else                        ed->redo();
        }
        notesChanged();
        return true;
    }

    if (! cmd && code == 'q') { quantise (1.0, false); return true; }
    if (! cmd && code == 'b') { drawButton.setToggleState (true, juce::sendNotification); return true; }
    if (! cmd && code == 'v') { selectButton.setToggleState (true, juce::sendNotification); return true; }

    return false;   // Space, Home, R and other Studio shortcuts go to the page
}

void PianoRoll::setDrawMode (bool shouldDraw)
{
    drawMode = shouldDraw;
    grid.setMouseCursor (drawMode ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

//==============================================================================
//  View: zoom and scroll
//==============================================================================
void PianoRoll::zoomToClip()
{
    if (clip == nullptr || grid.getWidth() <= 0)
        return;

    const auto start = clipStartBeat(), end = juce::jmax (start + 1.0, clipEndBeat());
    pxPerBeat = juce::jlimit (8.0, 600.0, (grid.getWidth() - 24.0) / (end - start));
    scrollBeats = juce::jmax (0.0, start - 8.0 / pxPerBeat);
    updateScrollbars();
    repaint();
}

void PianoRoll::zoomX (double factor, float aroundX)
{
    const auto beat = xToBeat (aroundX);
    pxPerBeat = juce::jlimit (8.0, 600.0, pxPerBeat * factor);
    scrollBeats = juce::jmax (0.0, beat - aroundX / pxPerBeat);
    updateScrollbars();
    repaint();
}

void PianoRoll::zoomY (float factor, float aroundY)
{
    const auto note = 127.0f - (aroundY + (float) scrollY) / noteHeight;
    noteHeight = juce::jlimit (7.0f, 30.0f, noteHeight * factor);
    scrollY = juce::roundToInt ((127.0f - note) * noteHeight - aroundY);
    scrollYBy (0);
}

void PianoRoll::scrollXBy (double beats)
{
    scrollBeats = juce::jmax (0.0, scrollBeats + beats);
    updateScrollbars();
    repaint();
}

void PianoRoll::scrollYBy (int px)
{
    const auto maxScroll = juce::jmax (0, juce::roundToInt (128 * noteHeight) - grid.getHeight());
    scrollY = juce::jlimit (0, maxScroll, scrollY + px);
    updateScrollbars();
    repaint();
}

void PianoRoll::updateScrollbars()
{
    const auto visibleBeats = grid.getWidth() / pxPerBeat;
    const auto total = juce::jmax (clipEndBeat() + 16.0, scrollBeats + visibleBeats);
    hScroll.setRangeLimits (0.0, total, juce::dontSendNotification);
    hScroll.setCurrentRange (scrollBeats, visibleBeats, juce::dontSendNotification);

    vScroll.setRangeLimits (0.0, 128.0 * noteHeight, juce::dontSendNotification);
    vScroll.setCurrentRange (scrollY, grid.getHeight(), juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* bar, double start)
{
    if (bar == &hScroll) scrollBeats = juce::jmax (0.0, start);
    else                 scrollY = juce::roundToInt (start);
    repaint();
}

void PianoRoll::timerCallback()
{
    // The clip may have been deleted from the timeline, or undone away.
    if (clip == nullptr || clip->getTrack() == nullptr || ! clip->state.getParent().isValid())
    {
        clip = nullptr;
        if (onClose) onClose();
        return;
    }

    refreshGeometry();

    // Follow the playhead while playing
    const auto beat = timeline.getPlayheadBeat();
    if (engine.isPlaying())
    {
        const auto x = beatToX (beat);
        if (x > grid.getWidth() - 20 || x < 0)
        {
            scrollBeats = juce::jmax (0.0, beat - 20.0 / pxPerBeat);
            updateScrollbars();
        }
    }
    if (beat != lastFollowBeat || engine.isPlaying())
    {
        lastFollowBeat = beat;
        ruler.repaint();
        grid.repaint();
        velocity.repaint();
    }
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    g.setColour (card);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (cardBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);

    if (clip != nullptr && titleArea.getWidth() > 60)
    {
        auto* track = clip->getTrack();
        g.setColour (gold);
        g.setFont (studioFont (13.5f, true));
        g.drawFittedText ((track != nullptr ? track->getName() + juce::String::fromUTF8 ("  ·  ") : juce::String()) + clip->getName(),
                          titleArea, juce::Justification::centredLeft, 1, 0.8f);
    }

    // Footer: status and legend
    g.setColour (textDim);
    g.setFont (studioFont (12.0f));
    auto f = footerArea;
    g.drawText (status, f.removeFromLeft (f.getWidth() / 2), juce::Justification::centredLeft, true);
    g.drawText (juce::String::fromUTF8 ("Gold rows = chord tones  ·  B draw  ·  V select  ·  Q quantise  ·  Alt-drag copies  ·  Right-click deletes"),
                f, juce::Justification::centredRight, true);
}

void PianoRoll::resized()
{
    auto area = getLocalBounds().reduced (8, 6);

    // Toolbar
    toolbarArea = area.removeFromTop (toolbarHeight - 6);
    auto t = toolbarArea.withSizeKeepingCentre (toolbarArea.getWidth(), 30);
    closeButton.setBounds (t.removeFromRight (58));
    t.removeFromRight (6);
    splitButton.setBounds (t.removeFromRight (56));
    t.removeFromRight (12);

    selectButton.setBounds (t.removeFromLeft (64));
    drawButton.setBounds (t.removeFromLeft (58));
    t.removeFromLeft (8);
    gridBox.setBounds (t.removeFromLeft (118));
    t.removeFromLeft (6);
    lengthBox.setBounds (t.removeFromLeft (136));
    t.removeFromLeft (6);
    editButton.setBounds (t.removeFromLeft (60));
    t.removeFromLeft (6);
    fitButton.setBounds (t.removeFromLeft (104));
    t.removeFromLeft (6);
    zoomButton.setBounds (t.removeFromLeft (40));
    t.removeFromLeft (6);
    checkToggle.setBounds (t.removeFromLeft (112));
    t.removeFromLeft (10);
    titleArea = t;

    area.removeFromTop (4);
    footerArea = area.removeFromBottom (footerHeight);
    area.removeFromBottom (2);

    auto velArea = area.removeFromBottom (velocityHeight);
    hScroll.setBounds (area.removeFromBottom (10).withTrimmedLeft (keysWidth).withTrimmedRight (10));
    vScroll.setBounds (area.removeFromRight (10).withTrimmedTop (RollRuler::rulerHeight + RollRuler::chordHeight));

    auto top = area.removeFromTop (RollRuler::rulerHeight + RollRuler::chordHeight);
    ruler.setBounds (top.withTrimmedLeft (keysWidth));
    keys.setBounds (area.removeFromLeft (keysWidth));
    grid.setBounds (area);
    velocity.setBounds (velArea.withTrimmedLeft (keysWidth).withTrimmedRight (10));

    scrollYBy (0);
    updateScrollbars();
}
