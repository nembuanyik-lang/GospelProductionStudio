#include "DrumGrid.h"
#include "Percussion.h"
#include "StudioLookAndFeel.h"

using namespace StudioColours;

namespace
{
    constexpr int velAccent = 118, velNormal = 96, velSoft = 66, velGhost = 38;
    const juce::Identifier swingId ("gpsSwing"), stepsId ("gpsSteps");

    juce::Colour rowColour (int note)
    {
        switch (note)
        {
            case 36:                               return juce::Colour (0xffe07a4f);   // kick
            case 37: case 38: case 39: case 40:    return juce::Colour (0xffd4a843);   // snare, rim, clap
            case 42: case 44: case 46:             return juce::Colour (0xff5fb0d8);   // hats
            case 41: case 43: case 45: case 47: case 48: case 50:
                                                   return juce::Colour (0xff9c7ad8);   // toms
            case 49: case 51: case 53: case 57: case 59:
                                                   return juce::Colour (0xff4caf7d);   // cymbals
            default:                               return juce::Colour (0xffc98bb0);   // percussion
        }
    }
}

//==============================================================================
const std::vector<DrumGrid::Row>& DrumGrid::standardRows()
{
    static const std::vector<Row> list {
        { 49, "Crash" },      { 51, "Ride" },       { 53, "Ride bell" },
        { 46, "Open hat" },   { 42, "Closed hat" }, { 44, "Pedal hat" },
        { 50, "High tom" },   { 47, "Mid tom" },    { 45, "Low tom" },   { 43, "Floor tom" },
        { 38, "Snare" },      { 37, "Rim click" },  { 39, "Clap" },
        { 36, "Kick" },
    };
    return list;
}

juce::StringArray DrumGrid::fillNames()
{
    return { "Snare roll", "Toms down", "Triplet run (gospel)", "Kick and snare build", "Stop (break)", "Crash on the next 1" };
}

//==============================================================================
DrumGrid::DrumGrid (StudioEngine& e, TimelineView& t) : engine (e), timeline (t)
{
    setWantsKeyboardFocus (true);

    for (auto* b : { &grooveButton, &fillButton, &humaniseButton, &repeatButton, &clearButton, &pianoButton, &closeButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    grooveButton.setComponentID ("drums.groove");
    grooveButton.setTooltip ("Fill this clip with a ready gospel groove");
    fillButton.setTooltip ("Put a fill at the end of the chosen bar (click a bar number to choose)");
    humaniseButton.setTooltip ("Make it sound played, not programmed: a little at random, or the timing and accents of real drummers in a style");
    repeatButton.setTooltip ("Copy the chosen bar to every bar after it");
    pianoButton.setTooltip ("Edit this clip in the piano roll instead");

    stepBox.addItem ("16ths", 4);
    stepBox.addItem ("8ths", 2);
    stepBox.addItem ("Triplets", 3);
    stepBox.addItem ("16th triplets", 6);
    stepBox.setTooltip ("How fine the boxes are");
    stepBox.onChange = [this]
    {
        stepsPerBeat = juce::jmax (1, stepBox.getSelectedId());
        if (clip != nullptr)
            clip->state.setProperty (stepsId, stepsPerBeat, nullptr);
        swingSlider.setEnabled (stepsPerBeat == 2 || stepsPerBeat == 4);
        cellWidth = stepsPerBeat >= 6 ? 18.0f : stepsPerBeat == 3 ? 28.0f : stepsPerBeat == 2 ? 36.0f : 26.0f;
        updateScroll();
        repaint();
    };
    addAndMakeVisible (stepBox);

    for (int b : { 1, 2, 4, 8, 16 })
        barsBox.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);
    barsBox.setTooltip ("How long this drum clip is");
    barsBox.onChange = [this] { if (barsBox.getSelectedId() > 0) setBars (barsBox.getSelectedId()); };
    addAndMakeVisible (barsBox);

    swingLabel.setText ("Swing", juce::dontSendNotification);
    swingLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (swingLabel);
    swingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    swingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    swingSlider.setRange (0.0, 100.0, 1.0);
    swingSlider.setTextValueSuffix ("%");
    swingSlider.setTooltip ("Pushes the off-beats late. About 66% is a full shuffle.");
    swingSlider.onDragEnd = [this] { setSwing ((float) swingSlider.getValue() / 200.0f); };
    swingSlider.onValueChange = [this] { if (! swingSlider.isMouseButtonDown()) setSwing ((float) swingSlider.getValue() / 200.0f); };
    addAndMakeVisible (swingSlider);

    status.setColour (juce::Label::textColourId, textDim);
    status.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (status);

    hScroll.addListener (this);
    vScroll.addListener (this);
    hScroll.setAutoHide (false);
    addAndMakeVisible (hScroll);
    addChildComponent (vScroll);

    grooveButton.onClick = [this]
    {
        juce::PopupMenu m, other;
        const auto& list = Grooves::all();
        // Grooves for this time signature, grouped: church grooves first, then Cameroon, West Africa, Central Africa, fills
        juce::StringArray categories;
        for (const auto& g : list)
            categories.addIfNotAlreadyThere (juce::String (g.category).containsChar ('/') ? juce::String ("Church") : juce::String (g.category));
        for (const auto& cat : categories)
        {
            bool header = false;
            for (int i = 0; i < (int) list.size(); ++i)
            {
                const auto& g = list[(size_t) i];
                const auto gc = juce::String (g.category).containsChar ('/') ? juce::String ("Church") : juce::String (g.category);
                if (gc != cat) continue;
                const bool fits = std::abs (Grooves::numSteps (g) / (double) g.stepsPerBeat - beatsPerBar) < 1.0e-6;
                if (! fits) { other.addItem (i + 1, juce::String (g.name) + "   (" + juce::String (g.numerator) + "/" + juce::String (g.denominator) + ")"); continue; }
                if (! header) { m.addSectionHeader (cat); header = true; }
                m.addItem (i + 1, g.name);
            }
        }
        if (other.getNumItems() > 0)
            m.addSubMenu ("Other time signatures", other);

        // Step 32: two-bar grooves played by real drummers (4/4)
        if (! RealFeel::grooves().empty() && std::abs (beatsPerBar - 4.0) < 1.0e-6)
        {
            juce::PopupMenu real;
            juce::String lastStyle;
            for (int i = 0; i < (int) RealFeel::grooves().size(); ++i)
            {
                const auto& g = RealFeel::grooves()[(size_t) i];
                if (lastStyle != g.style) { real.addSectionHeader (g.style); lastStyle = g.style; }
                real.addItem (20000 + i, g.name);
            }
            m.addSubMenu ("Played by real drummers", real);
        }
        m.addSeparator();
        m.addItem (9000, "Show the African percussion rows (congas, djembe, talking drum, shakers, bells)", true, showPercussion);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (grooveButton),
                         [safe = juce::Component::SafePointer<DrumGrid> (this)] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (r == 9000)
            {
                safe->showPercussion = ! safe->showPercussion;
                safe->refreshRows();
                safe->resized();
                safe->repaint();
                return;
            }
            if (r >= 20000)
            {
                if (r - 20000 < (int) RealFeel::grooves().size())
                    safe->applyRealGroove (RealFeel::grooves()[(size_t) (r - 20000)]);
                return;
            }
            safe->applyGroove (Grooves::all()[(size_t) r - 1]);
        });
    };

    fillButton.onClick = [this]
    {
        juce::PopupMenu m;
        const auto names = fillNames();
        for (int i = 0; i < names.size(); ++i)
            m.addItem (i + 1, names[i]);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (fillButton),
                         [safe = juce::Component::SafePointer<DrumGrid> (this)] (int r)
        {
            if (safe != nullptr && r > 0)
                safe->addFill (r - 1, safe->selectedBar >= 0 ? safe->selectedBar : safe->barCount() - 1);
        });
    };

    humaniseButton.onClick = [this] { showHumaniseMenu(); };
    repeatButton.onClick   = [this] { repeatBar (selectedBar >= 0 ? selectedBar : 0); };
    clearButton.onClick    = [this] { clearAll(); };
    closeButton.onClick    = [this] { if (onClose) onClose(); };
    pianoButton.onClick    = [this]
    {
        if (clip != nullptr && onPianoRoll)
        {
            te::MidiClip::Ptr c = clip;
            c->state.setProperty ("gpsPianoRoll", true, nullptr);
            onPianoRoll (c.get());
        }
    };
}

DrumGrid::~DrumGrid()
{
    hScroll.removeListener (this);
    vScroll.removeListener (this);
}

juce::UndoManager* DrumGrid::undo() const
{
    auto* ed = engine.getEdit();
    return ed != nullptr ? &ed->getUndoManager() : nullptr;
}

//==============================================================================
void DrumGrid::setClip (te::MidiClip* c)
{
    clip = c;
    selectedBar = -1;
    playStep = -1;
    scrollStep = 0;

    if (clip == nullptr)
    {
        stopTimer();
        return;
    }

    clip->state.removeProperty ("gpsPianoRoll", nullptr);
    beatsPerBar = timeline.getBeatsPerBar();
    const int saved = clip->state.getProperty (stepsId, 0);
    const bool eighthBased = std::abs (beatsPerBar - std::round (beatsPerBar)) < 1.0e-6 && (int) std::round (beatsPerBar) % 3 == 0
                             && beatsPerBar >= 6.0;
    stepBox.setSelectedId (saved > 0 ? saved : (eighthBased ? 2 : 4), juce::sendNotificationSync);

    swingSlider.setValue (getSwing() * 200.0, juce::dontSendNotification);
    refreshRows();
    {
        const auto bars = lengthBeats / juce::jmax (1.0, beatsPerBar);
        const auto whole = (int) std::round (bars);
        barsBox.setSelectedId (std::abs (bars - whole) < 1.0e-3 && barsBox.indexOfItemId (whole) >= 0 ? whole : 0, juce::dontSendNotification);
        if (barsBox.getSelectedId() == 0)
            barsBox.setText (juce::String (bars, 1) + " bars", juce::dontSendNotification);
    }
    showHint();
    startTimerHz (30);
    resized();
    repaint();
}

void DrumGrid::refreshRows()
{
    rows = standardRows();
    if (clip == nullptr)
        return;

    // African percussion (Step 21): shown when asked for, or when the clip already plays any of it
    bool percussionUsed = showPercussion;
    for (auto* n : clip->getSequence().getNotes())
        percussionUsed = percussionUsed || Percussion::handles (n->getNoteNumber());
    if (percussionUsed)
        for (auto& p : Percussion::sounds())
            if (std::none_of (rows.begin(), rows.end(), [&p] (const Row& r) { return r.note == p.note; }))
                rows.push_back ({ p.note, p.name });

    // Any other drum the clip already plays gets its own row too
    for (auto* n : clip->getSequence().getNotes())
    {
        const auto note = n->getNoteNumber();
        if (std::none_of (rows.begin(), rows.end(), [note] (const Row& r) { return r.note == note; }))
            rows.push_back ({ note, "Note " + juce::String (note) });
    }

    clipStartEditBeat = timeline.timeToBeat (clip->getPosition().getStart());
    contentStart = clip->getContentBeatAtTime (clip->getPosition().getStart()).inBeats();
    lengthBeats = timeline.timeToBeat (clip->getPosition().getEnd()) - clipStartEditBeat;
}

int DrumGrid::stepsPerBar() const   { return juce::jmax (1, (int) std::round (beatsPerBar * stepsPerBeat)); }
int DrumGrid::numSteps() const      { return juce::jmax (1, (int) std::floor (lengthBeats * stepsPerBeat + 1.0e-6)); }
int DrumGrid::barCount() const      { return juce::jmax (1, (int) std::ceil (numSteps() / (double) stepsPerBar() - 1.0e-6)); }

float DrumGrid::getSwing() const
{
    return clip != nullptr ? (float) clip->state.getProperty (swingId, 0.0f) : 0.0f;
}

double DrumGrid::stepToContentBeat (int step) const
{
    auto beat = contentStart + step * stepBeats();
    const bool swingable = stepsPerBeat == 2 || stepsPerBeat == 4;
    if (swingable && (step % 2) == 1)
        beat += getSwing() * stepBeats();
    return beat;
}

int DrumGrid::contentBeatToStep (double beat) const
{
    const auto rel = (beat - contentStart) / stepBeats();
    const bool swingable = stepsPerBeat == 2 || stepsPerBeat == 4;
    const auto sw = (double) getSwing();
    if (! swingable || sw <= 0.0)
        return (int) std::floor (rel + 0.5);

    // Steps come in pairs: the first on time, the second late by the swing. Pick the nearest.
    const auto pair = std::floor (rel / 2.0);
    const auto within = rel - pair * 2.0;          // 0 .. 2
    const int base = (int) pair * 2;
    if (within < (1.0 + sw) * 0.5)  return base;
    if (within < (3.0 + sw) * 0.5)  return base + 1;
    return base + 2;
}

te::MidiNote* DrumGrid::hitAt (int note, int step) const
{
    if (clip == nullptr)
        return nullptr;
    for (auto* n : clip->getSequence().getNotes())
        if (n->getNoteNumber() == note && contentBeatToStep (n->getStartBeat().inBeats()) == step)
            return n;
    return nullptr;
}

void DrumGrid::setHit (int note, int step, int velocity)
{
    if (clip == nullptr || step < 0 || step >= numSteps())
        return;
    auto& seq = clip->getSequence();
    auto* um = undo();

    if (auto* existing = hitAt (note, step))
    {
        if (velocity <= 0)
            seq.removeNote (*existing, um);
        else
            existing->setVelocity (velocity, um);
    }
    else if (velocity > 0)
    {
        seq.addNote (note, te::BeatPosition::fromBeats (stepToContentBeat (step)),
                     te::BeatDuration::fromBeats (stepBeats() * 0.9), velocity, 0, um);
    }
    repaint();
    timeline.repaint();
}

//==============================================================================
void DrumGrid::applyGroove (const Grooves::Groove& g)
{
    if (clip == nullptr)
        return;
    if (auto* um = undo()) um->beginNewTransaction ("Drum groove");
    auto& seq = clip->getSequence();
    seq.clear (undo());
    Grooves::writeTo (seq, g, contentStart, lengthBeats, beatsPerBar, undo());

    // Show the groove at its own step size
    stepBox.setSelectedId (g.stepsPerBeat == 1 ? 2 : g.stepsPerBeat, juce::sendNotificationSync);
    if (getSwing() > 0.0f)
    {
        const auto s = getSwing();
        clip->state.setProperty (swingId, 0.0f, nullptr);
        if (stepsPerBeat == 2 || stepsPerBeat == 4)
            setSwing (s);   // put the swing back onto the new notes
        else
            swingSlider.setValue (0.0, juce::dontSendNotification);   // triplet grooves already swing
    }
    status.setText (juce::String ("Groove: ") + g.name, juce::dontSendNotification);
    refreshRows();
    resized();             // new rows (African percussion) may need the scroll bar
    repaint();
    timeline.repaint();
}

void DrumGrid::addFill (int which, int bar)
{
    if (clip == nullptr)
        return;
    bar = juce::jlimit (0, barCount() - 1, bar);
    const int spb = stepsPerBar();
    const int barStart = bar * spb;
    const int barEnd = juce::jmin (numSteps(), barStart + spb);
    const int half = barStart + spb / 2;
    if (auto* um = undo()) um->beginNewTransaction ("Drum fill");

    // Clear the second half of the bar (cymbals on the next downbeat are kept)
    if (which != 5)
        for (auto* n : clip->getSequence().getNotes())
        {
            const auto s = contentBeatToStep (n->getStartBeat().inBeats());
            if (s >= half && s < barEnd)
                clip->getSequence().removeNote (*n, undo());
        }

    const int count = barEnd - half;
    auto hit = [this] (int note, int step, int vel) { setHit (note, step, vel); };
    const int toms[] = { 50, 47, 45, 43 };
    const int beatSteps = stepsPerBeat;

    switch (which)
    {
        case 0:  // Snare roll, getting louder
            for (int i = 0; i < count; ++i)
                hit (38, half + i, juce::jmin (124, 60 + i * 64 / juce::jmax (1, count)));
            hit (36, half, velNormal);
            break;

        case 1:  // Toms down, two hits each
            for (int i = 0; i < count; ++i)
                hit (toms[juce::jlimit (0, 3, i * 4 / juce::jmax (1, count))], half + i, i % 2 == 0 ? velAccent : velNormal);
            for (int i = 0; i < count; i += beatSteps)
                hit (36, half + i, velNormal);
            break;

        case 2:  // Triplet run: snare, tom, tom in groups of three, ending on the floor tom
            for (int i = 0; i < count; ++i)
            {
                const int group = i % 3;
                hit (group == 0 ? 38 : toms[juce::jlimit (0, 3, (i * 4) / juce::jmax (1, count))], half + i, group == 0 ? velAccent : velNormal);
                if (group == 2) hit (36, half + i, velSoft);
            }
            break;

        case 3:  // Kick and snare together, building
            for (int i = 0; i < count; i += juce::jmax (1, beatSteps / 2))
            {
                hit (36, half + i, juce::jmin (124, 80 + i * 40 / juce::jmax (1, count)));
                hit (38, half + i, juce::jmin (124, 70 + i * 50 / juce::jmax (1, count)));
            }
            break;

        case 4:  // Stop: one hit with a crash, then silence
            hit (36, half, velAccent);
            hit (49, half, velAccent);
            break;

        case 5:  // Crash and kick on the 1 of the next bar
        {
            const int next = barEnd < numSteps() ? barEnd : barStart;
            hit (49, next, velAccent);
            hit (36, next, velAccent);
            break;
        }
        default: break;
    }

    status.setText ("Fill in bar " + juce::String (bar + 1) + ": " + fillNames()[which], juce::dontSendNotification);
    repaint();
    timeline.repaint();
}

void DrumGrid::setSwing (float amount)
{
    if (clip == nullptr)
        return;
    amount = juce::jlimit (0.0f, 0.5f, amount);
    const auto old = getSwing();
    if (std::abs (amount - old) < 1.0e-4f)
        return;

    // Work out every note's step with the old swing, then place it again with the new one
    struct Move { te::MidiNote* n; int step; double offset; };
    std::vector<Move> moves;
    for (auto* n : clip->getSequence().getNotes())
    {
        const auto beat = n->getStartBeat().inBeats();
        const auto step = contentBeatToStep (beat);
        moves.push_back ({ n, step, beat - stepToContentBeat (step) });   // keep any humanise offset
    }

    if (auto* um = undo()) um->beginNewTransaction ("Swing");
    clip->state.setProperty (swingId, amount, undo());
    for (auto& m : moves)
        m.n->setStartAndLength (te::BeatPosition::fromBeats (stepToContentBeat (m.step) + m.offset), m.n->getLengthBeats(), undo());

    swingSlider.setValue (amount * 200.0, juce::dontSendNotification);
    repaint();
    timeline.repaint();
}

void DrumGrid::humanise()
{
    if (clip == nullptr)
        return;
    if (auto* um = undo()) um->beginNewTransaction ("Humanise drums");
    juce::Random rnd;
    const auto maxShift = 0.012;   // beats: about 6 ms at 120 BPM
    for (auto* n : clip->getSequence().getNotes())
    {
        const auto step = contentBeatToStep (n->getStartBeat().inBeats());
        const auto t = stepToContentBeat (step) + (rnd.nextDouble() * 2.0 - 1.0) * maxShift;
        n->setStartAndLength (te::BeatPosition::fromBeats (juce::jmax (contentStart, t)), n->getLengthBeats(), undo());
        n->setVelocity (juce::jlimit (1, 127, n->getVelocity() + rnd.nextInt ({ -9, 10 })), undo());
    }
    status.setText ("Humanised: small changes in timing and strength", juce::dontSendNotification);
    repaint();
    timeline.repaint();
}

//==============================================================================
//  Step 32: the feel of real drummers
//==============================================================================
void DrumGrid::showHumaniseMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "A little, at random");
    m.addSeparator();
    m.addSectionHeader ("Feel of real drummers");
    if (RealFeel::styles().empty())
        m.addItem (-1, "(comes with the GitHub build)", false);
    for (int i = 0; i < (int) RealFeel::styles().size(); ++i)
        m.addItem (100 + i, juce::String (RealFeel::styles()[(size_t) i].name) + " feel");
    m.addSeparator();
    m.addItem (2, "Back to the grid (exact timing)");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (humaniseButton),
                     [safe = juce::Component::SafePointer<DrumGrid> (this)] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 1)        safe->humanise();
        else if (r == 2)   safe->backToGrid();
        else if (r >= 100 && r - 100 < (int) RealFeel::styles().size())
            safe->applyFeel (RealFeel::styles()[(size_t) (r - 100)]);
    });
}

void DrumGrid::applyFeel (const RealFeel::Style& style)
{
    if (clip == nullptr)
        return;
    if (auto* um = undo()) um->beginNewTransaction (juce::String ("Drum feel: ") + style.name);
    juce::Random random (0x5eed + (juce::int64) std::hash<std::string>() (style.key));   // the same each time
    for (auto* n : clip->getSequence().getNotes())
    {
        const auto step = contentBeatToStep (n->getStartBeat().inBeats());
        const auto gridBeat = stepToContentBeat (step);
        const auto inBar = std::fmod (juce::jmax (0.0, step * stepBeats()), beatsPerBar);
        const auto placed = RealFeel::place (style, n->getNoteNumber(), inBar, n->getVelocity(), random);
        n->setStartAndLength (te::BeatPosition::fromBeats (juce::jmax (contentStart, gridBeat + placed.offsetBeats)), n->getLengthBeats(), undo());
        n->setVelocity (placed.velocity, undo());
    }
    clip->state.setProperty ("gpsDrumFeel", style.name, undo());
    status.setText (juce::String ("Feel: ") + style.name + ", from " + juce::String (style.performances)
                    + " performances by real drummers", juce::dontSendNotification);
    repaint();
    timeline.repaint();
}

void DrumGrid::backToGrid()
{
    if (clip == nullptr)
        return;
    if (auto* um = undo()) um->beginNewTransaction ("Drums back to the grid");
    for (auto* n : clip->getSequence().getNotes())
        n->setStartAndLength (te::BeatPosition::fromBeats (stepToContentBeat (contentBeatToStep (n->getStartBeat().inBeats()))),
                              n->getLengthBeats(), undo());
    clip->state.removeProperty ("gpsDrumFeel", undo());
    status.setText ("Back to the grid: exact timing", juce::dontSendNotification);
    repaint();
    timeline.repaint();
}

void DrumGrid::applyRealGroove (const RealFeel::Groove& g)
{
    if (clip == nullptr)
        return;
    if (auto* um = undo()) um->beginNewTransaction ("Drum groove");
    auto& seq = clip->getSequence();
    seq.clear (undo());
    const auto end = contentStart + lengthBeats - 1.0e-6;
    for (double start = contentStart; start < end; start += g.lengthBeats)
        for (const auto& h : g.hits)
        {
            const auto beat = start + h.beat;
            if (beat < end)
                seq.addNote (h.note, te::BeatPosition::fromBeats (beat), te::BeatDuration::fromBeats (0.2), h.velocity, 0, undo());
        }
    clip->state.setProperty ("gpsDrumFeel", g.style, undo());
    stepBox.setSelectedId (4, juce::sendNotificationSync);
    status.setText (juce::String ("Groove: ") + g.name + ", played by a real drummer", juce::dontSendNotification);
    refreshRows();
    resized();
    repaint();
    timeline.repaint();
}

void DrumGrid::repeatBar (int bar)
{
    if (clip == nullptr)
        return;
    bar = juce::jlimit (0, barCount() - 1, bar);
    const int spb = stepsPerBar();
    struct Hit { int note, offset, velocity; };
    std::vector<Hit> hits;
    std::vector<te::MidiNote*> later;
    for (auto* n : clip->getSequence().getNotes())
    {
        const auto s = contentBeatToStep (n->getStartBeat().inBeats());
        if (s >= bar * spb && s < (bar + 1) * spb)
            hits.push_back ({ n->getNoteNumber(), s - bar * spb, n->getVelocity() });
        else if (s >= (bar + 1) * spb)
            later.push_back (n);
    }

    if (auto* um = undo()) um->beginNewTransaction ("Repeat bar");
    for (auto* n : later)
        clip->getSequence().removeNote (*n, undo());
    for (int b = bar + 1; b < barCount(); ++b)
        for (auto& h : hits)
            setHit (h.note, b * spb + h.offset, h.velocity);

    status.setText ("Bar " + juce::String (bar + 1) + " repeated to the end", juce::dontSendNotification);
    repaint();
    timeline.repaint();
}

void DrumGrid::setBars (int bars)
{
    if (clip == nullptr || bars <= 0)
        return;
    const auto newEnd = timeline.beatToTime (clipStartEditBeat + bars * beatsPerBar);
    if (std::abs ((newEnd - clip->getPosition().getEnd()).inSeconds()) < 1.0e-4)
        return;
    if (auto* um = undo()) um->beginNewTransaction ("Drum clip length");
    clip->setEnd (newEnd, false);
    refreshRows();
    updateScroll();
    status.setText ("The clip is now " + juce::String (bars) + (bars == 1 ? " bar" : " bars") + " long. Use Repeat bar to fill it.",
                    juce::dontSendNotification);
    repaint();
    timeline.refreshAll();
}

void DrumGrid::showHint()
{
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    status.setText ("Click a box to add or remove a hit" + dot + "Shift-click: accent" + dot + "Right-click: how hard" + dot
                    + "Click a drum name to hear it" + dot + "Click a bar number to choose that bar", juce::dontSendNotification);
}

void DrumGrid::clearAll()
{
    if (clip == nullptr)
        return;
    if (auto* um = undo()) um->beginNewTransaction ("Clear drums");
    clip->getSequence().clear (undo());
    repaint();
    timeline.repaint();
}

//==============================================================================
juce::Rectangle<int> DrumGrid::gridArea() const
{
    auto r = getLocalBounds();
    r.removeFromTop (toolbarHeight + rulerHeight);
    r.removeFromBottom (footerHeight + 12);
    r.removeFromLeft (headerWidth);
    if (vScroll.isVisible()) r.removeFromRight (12);
    return r;
}

float DrumGrid::stepToX (int step) const { return (float) gridArea().getX() + (step - scrollStep) * cellWidth; }

int DrumGrid::stepAtX (int x) const
{
    const auto s = scrollStep + (int) std::floor ((x - gridArea().getX()) / cellWidth);
    return s >= 0 && s < numSteps() ? s : -1;
}

int DrumGrid::rowAtY (int y) const
{
    const auto r = (y - gridArea().getY()) / rowHeight + scrollRow;
    return y >= gridArea().getY() && r >= 0 && r < (int) rows.size() ? r : -1;
}

void DrumGrid::updateScroll()
{
    const auto area = gridArea();
    const auto visibleSteps = juce::jmax (1, (int) (area.getWidth() / cellWidth));
    scrollStep = juce::jlimit (0, juce::jmax (0, numSteps() - visibleSteps), scrollStep);
    hScroll.setRangeLimits (0.0, (double) numSteps());
    hScroll.setCurrentRange ((double) scrollStep, (double) visibleSteps, juce::dontSendNotification);

    const auto visibleRows = juce::jmax (1, area.getHeight() / rowHeight);
    vScroll.setVisible ((int) rows.size() > visibleRows);
    scrollRow = juce::jlimit (0, juce::jmax (0, (int) rows.size() - visibleRows), scrollRow);
    vScroll.setRangeLimits (0.0, (double) rows.size());
    vScroll.setCurrentRange ((double) scrollRow, (double) visibleRows, juce::dontSendNotification);
}

void DrumGrid::scrollBarMoved (juce::ScrollBar* bar, double start)
{
    if (bar == &hScroll) scrollStep = (int) std::round (start);
    else                 scrollRow = (int) std::round (start);
    repaint();
}

void DrumGrid::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop (toolbarHeight).reduced (6, 6);
    const bool narrow = getWidth() < 1000;

    grooveButton.setButtonText (narrow ? "Groove" : "Start from a groove");
    grooveButton.setBounds (bar.removeFromLeft (narrow ? 70 : 140));   bar.removeFromLeft (6);
    fillButton.setBounds (bar.removeFromLeft (narrow ? 70 : 84));      bar.removeFromLeft (6);
    humaniseButton.setBounds (bar.removeFromLeft (78));                bar.removeFromLeft (6);
    repeatButton.setBounds (bar.removeFromLeft (84));                  bar.removeFromLeft (6);
    clearButton.setBounds (bar.removeFromLeft (52));                   bar.removeFromLeft (8);
    barsBox.setBounds (bar.removeFromLeft (84));                       bar.removeFromLeft (12);

    closeButton.setBounds (bar.removeFromRight (60));                  bar.removeFromRight (6);
    pianoButton.setBounds (bar.removeFromRight (80));                  bar.removeFromRight (12);
    stepBox.setBounds (bar.removeFromRight (narrow ? 96 : 116));       bar.removeFromRight (10);
    swingSlider.setBounds (bar.removeFromRight (juce::jlimit (100, 170, bar.getWidth() - 48)));
    swingLabel.setBounds (bar.removeFromRight (46));

    auto footer = r.removeFromBottom (footerHeight);
    status.setBounds (footer.withTrimmedLeft (6));
    auto scrollRow_ = r.removeFromBottom (12);
    hScroll.setBounds (scrollRow_.withTrimmedLeft (headerWidth));

    // Rows grow to fill the space (easier to click), within limits
    {
        const auto h = r.getHeight() - rulerHeight;
        rowHeight = juce::jlimit (20, 30, h / juce::jmax (1, (int) rows.size()));
    }
    updateScroll();
    if (vScroll.isVisible())
        vScroll.setBounds (r.removeFromRight (12).withTrimmedTop (rulerHeight));
    updateScroll();
}

//==============================================================================
void DrumGrid::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.setColour (header);
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));

    if (clip == nullptr)
        return;

    const auto area = gridArea();
    const int spb = stepsPerBar();
    const int total = numSteps();

    // ---- Ruler: bar numbers (click to choose a bar) ---------------------------
    auto ruler = juce::Rectangle<int> (area.getX(), toolbarHeight, area.getWidth(), rulerHeight);
    g.setColour (card);
    g.fillRect (ruler.withX (0).withWidth (getWidth()));
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (clip->getName(), 6, toolbarHeight, headerWidth - 10, rulerHeight, juce::Justification::centredLeft, true);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (ruler);
        for (int bar = 0; bar < barCount(); ++bar)
        {
            const auto x = stepToX (bar * spb);
            const auto w = spb * cellWidth;
            if (x + w < area.getX() || x > area.getRight())
                continue;
            if (bar == selectedBar)
            {
                g.setColour (goldDim.withAlpha (0.6f));
                g.fillRect (juce::Rectangle<float> (x, (float) ruler.getY(), w, (float) rulerHeight));
            }
            g.setColour (bar == selectedBar ? text : textDim);
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (juce::jmax (x, (float) area.getX()) + 4, (float) ruler.getY(), 40, (float) rulerHeight),
                        juce::Justification::centredLeft);
            for (int b = 1; b < (int) std::round (beatsPerBar); ++b)
            {
                g.setColour (cardBorder);
                const auto bx = x + b * stepsPerBeat * cellWidth;
                g.drawVerticalLine ((int) bx, (float) ruler.getBottom() - 5, (float) ruler.getBottom());
            }
        }
    }

    // ---- Row names --------------------------------------------------------------
    for (int i = scrollRow; i < (int) rows.size(); ++i)
    {
        const int y = area.getY() + (i - scrollRow) * rowHeight;
        if (y >= area.getBottom())
            break;
        auto r = juce::Rectangle<int> (0, y, headerWidth, rowHeight);
        g.setColour (i % 2 == 0 ? card : card.darker (0.15f));
        g.fillRect (r);
        g.setColour (rowColour (rows[(size_t) i].note));
        g.fillRect (r.removeFromLeft (4).reduced (0, 2));
        g.setColour (text);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (rows[(size_t) i].name, r.withTrimmedLeft (8), juce::Justification::centredLeft, true);
    }

    // ---- Cells --------------------------------------------------------------------
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (area);
    g.setColour (juce::Colour (0xff15171c));
    g.fillRect (area);

    const int firstStep = scrollStep, lastStep = juce::jmin (total, scrollStep + (int) (area.getWidth() / cellWidth) + 2);
    for (int i = scrollRow; i < (int) rows.size(); ++i)
    {
        const int y = area.getY() + (i - scrollRow) * rowHeight;
        if (y >= area.getBottom())
            break;
        for (int st = firstStep; st < lastStep; ++st)
        {
            const auto x = stepToX (st);
            const bool onBeat = st % stepsPerBeat == 0;
            const bool beatGroup = (st / stepsPerBeat) % 2 == 0;
            auto cell = juce::Rectangle<float> (x, (float) y, cellWidth, (float) rowHeight).reduced (1.5f, 2.0f);
            g.setColour (beatGroup ? juce::Colour (0xff22252d) : juce::Colour (0xff1c1f26));
            if (st == playStep)
                g.setColour (juce::Colour (0xff3a3d46));
            g.fillRoundedRectangle (cell, 3.0f);
            if (onBeat)
            {
                g.setColour (juce::Colour (0xff2f333d));
                g.drawRoundedRectangle (cell, 3.0f, 1.0f);
            }
        }
    }

    // Bar lines
    for (int bar = 0; bar <= barCount(); ++bar)
    {
        const auto x = stepToX (bar * spb) - 0.5f;
        g.setColour (goldDim.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
    }

    // Hits: brighter = harder
    for (auto* n : clip->getSequence().getNotes())
    {
        const auto row = std::find_if (rows.begin(), rows.end(), [n] (const Row& r) { return r.note == n->getNoteNumber(); });
        if (row == rows.end())
            continue;
        const int i = (int) (row - rows.begin());
        if (i < scrollRow)
            continue;
        const int y = area.getY() + (i - scrollRow) * rowHeight;
        const int st = contentBeatToStep (n->getStartBeat().inBeats());
        if (st < firstStep || st >= lastStep || y >= area.getBottom())
            continue;
        const auto v = n->getVelocity() / 127.0f;
        auto cell = juce::Rectangle<float> (stepToX (st), (float) y, cellWidth, (float) rowHeight).reduced (2.5f, 3.0f);
        if (v < 0.4f)
            cell = cell.withSizeKeepingCentre (cell.getWidth() * 0.6f, cell.getHeight() * 0.6f);   // ghost notes are small
        g.setColour (rowColour (row->note).withMultipliedBrightness (0.45f + 0.75f * v).withAlpha (0.55f + 0.45f * v));
        g.fillRoundedRectangle (cell, 3.0f);
        if (n->getVelocity() >= velAccent)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillEllipse (cell.getCentreX() - 2.0f, cell.getCentreY() - 2.0f, 4.0f, 4.0f);
        }
    }

    // Area past the end of the clip
    const auto endX = stepToX (total);
    if (endX < area.getRight())
    {
        g.setColour (background.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (endX, (float) area.getY(), (float) area.getRight() - endX, (float) area.getHeight()));
    }
}

//==============================================================================
void DrumGrid::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (clip == nullptr)
        return;
    const auto area = gridArea();
    const auto pos = e.getPosition();

    // Ruler: choose a bar for fills and repeats
    if (pos.y >= toolbarHeight && pos.y < toolbarHeight + rulerHeight && pos.x >= area.getX())
    {
        const auto st = stepAtX (pos.x);
        const auto bar = st >= 0 ? st / stepsPerBar() : -1;
        selectedBar = bar == selectedBar ? -1 : bar;
        status.setText (selectedBar >= 0 ? "Bar " + juce::String (selectedBar + 1) + " chosen: fills and Repeat bar use it"
                                         : juce::String(), juce::dontSendNotification);
        repaint();
        return;
    }

    const auto row = rowAtY (pos.y);
    if (row < 0)
        return;
    const auto note = rows[(size_t) row].note;

    // Row name: hear the drum
    if (pos.x < headerWidth)
    {
        engine.audition ({ note }, false, 300);
        return;
    }

    const auto st = stepAtX (pos.x);
    if (st < 0)
        return;

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        auto* existing = hitAt (note, st);
        m.addItem (1, "Accent (loud)");
        m.addItem (2, "Normal");
        m.addItem (3, "Soft");
        m.addItem (4, "Ghost note (very soft)");
        if (existing != nullptr)
        {
            m.addSeparator();
            m.addItem (5, "Remove");
        }
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                         [safe = juce::Component::SafePointer<DrumGrid> (this), note, st] (int r)
        {
            if (safe == nullptr || r <= 0)
                return;
            const int v[] = { 0, velAccent, velNormal, velSoft, velGhost, 0 };
            if (auto* um = safe->undo()) um->beginNewTransaction ("Drum hit");
            safe->setHit (note, st, v[r]);
            if (r < 5) safe->engine.audition ({ note }, false, 200);
        });
        return;
    }

    if (auto* um = undo()) um->beginNewTransaction ("Drum hits");
    const bool has = hitAt (note, st) != nullptr;
    dragNote = note;
    dragAdding = ! has || e.mods.isShiftDown();
    dragVelocity = e.mods.isShiftDown() ? velAccent : velNormal;
    lastDragStep = st;
    setHit (note, st, dragAdding ? dragVelocity : 0);
    if (dragAdding)
        engine.audition ({ note }, false, 200);
}

void DrumGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNote < 0 || clip == nullptr)
        return;
    const auto st = stepAtX (e.getPosition().x);
    if (st < 0 || st == lastDragStep)
        return;
    lastDragStep = st;
    const bool has = hitAt (dragNote, st) != nullptr;
    if (dragAdding != has || (dragAdding && e.mods.isShiftDown()))
        setHit (dragNote, st, dragAdding ? dragVelocity : 0);
}

void DrumGrid::mouseUp (const juce::MouseEvent&)
{
    dragNote = -1;
    lastDragStep = -1;
}

void DrumGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        cellWidth = juce::jlimit (12.0f, 60.0f, cellWidth * (w.deltaY > 0 ? 1.15f : 1.0f / 1.15f));
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const auto d = std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : -w.deltaY;
        scrollStep += d > 0 ? stepsPerBeat : -stepsPerBeat;
    }
    else if (vScroll.isVisible())
    {
        scrollRow += w.deltaY < 0 ? 2 : -2;
    }
    else
    {
        scrollStep += w.deltaY < 0 ? stepsPerBeat : -stepsPerBeat;
    }
    updateScroll();
    repaint();
}

bool DrumGrid::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }

    const auto code = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode());
    if (k.getModifiers().isCommandDown() && (code == 'z' || code == 'y'))
    {
        if (auto* ed = engine.getEdit())
        {
            if (code == 'z' && ! k.getModifiers().isShiftDown()) ed->undo();
            else                                                 ed->redo();
            swingSlider.setValue (getSwing() * 200.0, juce::dontSendNotification);
            repaint();
            timeline.repaint();
        }
        return true;
    }
    return false;
}

//==============================================================================
void DrumGrid::timerCallback()
{
    if (clip == nullptr || clip->getTrack() == nullptr || ! clip->state.getParent().isValid())
    {
        clip = nullptr;
        stopTimer();
        if (onClose) onClose();
        return;
    }

    // The clip may have been moved or stretched on the timeline
    const auto oldLength = lengthBeats, oldStart = contentStart;
    const auto oldRows = rows.size();
    refreshRows();
    beatsPerBar = timeline.getBeatsPerBar();
    if (! juce::exactlyEqual (oldLength, lengthBeats) || ! juce::exactlyEqual (oldStart, contentStart) || oldRows != rows.size())
    {
        updateScroll();
        repaint();
    }

    int step = -1;
    const auto beat = timeline.getPlayheadBeat() - clipStartEditBeat;
    if (beat >= 0.0 && beat < lengthBeats)
        step = (int) std::floor (beat * stepsPerBeat);

    if (step != playStep)
    {
        playStep = step;

        // Keep the playhead in view while playing
        if (engine.isPlaying() && step >= 0)
        {
            const auto visible = (int) (gridArea().getWidth() / cellWidth);
            if (step < scrollStep || step >= scrollStep + visible)
            {
                scrollStep = (step / stepsPerBar()) * stepsPerBar();
                updateScroll();
            }
        }
        repaint (gridArea().withTop (toolbarHeight));
    }
}
