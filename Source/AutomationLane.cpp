#include "AutomationLane.h"
#include "StudioLookAndFeel.h"

using namespace StudioColours;

AutomationLane::AutomationLane (TimelineView& o, te::AudioTrack& t) : owner (o), track (t)
{
    // (the hint is drawn in the lane while it is empty)
}

te::AutomatableParameter* AutomationLane::param() const
{
    return Automation::resolve (track, Automation::getShownKey (track));
}

float AutomationLane::valueToY (float v) const
{
    auto* p = param();
    const auto range = p != nullptr ? p->getValueRange() : juce::Range<float> (0.0f, 1.0f);
    const auto norm = range.getLength() > 0.0f ? (v - range.getStart()) / range.getLength() : 0.5f;
    const auto top = 6.0f, bottom = (float) getHeight() - 6.0f;
    return bottom - juce::jlimit (0.0f, 1.0f, norm) * (bottom - top);
}

float AutomationLane::yToValue (float y) const
{
    auto* p = param();
    const auto range = p != nullptr ? p->getValueRange() : juce::Range<float> (0.0f, 1.0f);
    const auto top = 6.0f, bottom = (float) getHeight() - 6.0f;
    const auto norm = juce::jlimit (0.0f, 1.0f, (bottom - y) / (bottom - top));
    return range.getStart() + norm * range.getLength();
}

float AutomationLane::timeToX (te::TimePosition t) const
{
    return owner.beatToX (owner.timeToBeat (t));
}

te::TimePosition AutomationLane::xToTime (float x, bool snapToGrid) const
{
    auto beat = juce::jmax (0.0, owner.xToBeat (x));
    if (snapToGrid)
        beat = owner.snap (beat);
    return owner.beatToTime (beat);
}

int AutomationLane::pointAt (juce::Point<float> pos) const
{
    auto* p = param();
    if (p == nullptr)
        return -1;
    auto& curve = p->getCurve();
    for (int i = curve.getNumPoints(); --i >= 0;)
    {
        const juce::Point<float> pt (timeToX (curve.getPointTime (i)), valueToY (curve.getPointValue (i)));
        if (pt.getDistanceFrom (pos) < 7.0f)
            return i;
    }
    return -1;
}

void AutomationLane::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (header.withAlpha (0.55f));
    g.fillRect (r);
    g.setColour (cardBorder.withAlpha (0.6f));
    g.drawHorizontalLine (0, 0.0f, r.getRight());

    // Gentle guide lines at the top, middle and bottom
    g.setColour (cardBorder.withAlpha (0.35f));
    for (auto f : { 0.0f, 0.5f, 1.0f })
        g.drawHorizontalLine ((int) (6.0f + f * (r.getHeight() - 12.0f)), 0.0f, r.getRight());

    auto* p = param();
    const auto colour = track.getColour().brighter (0.2f);
    g.setFont (studioFont (11.0f));

    if (p == nullptr)
    {
        g.setColour (textDim);
        g.drawText ("This effect was removed. Choose another setting in the track header.", r.reduced (10.0f, 0.0f), juce::Justification::centredLeft);
        return;
    }

    auto& curve = p->getCurve();
    const auto n = curve.getNumPoints();

    if (n == 0)
    {
        const auto y = valueToY (p->getCurrentBaseValue());
        juce::Path line;
        line.startNewSubPath (0.0f, y);
        line.lineTo (r.getRight(), y);
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, line, dashes, 2);
        g.setColour (colour.withAlpha (0.5f));
        g.fillPath (dashed);

        g.setColour (textDim);
        g.drawText ("Click to draw how " + Automation::describe (track, Automation::getShownKey (track)).toLowerCase()
                        + " changes during the song  (now " + p->getCurrentValueAsString() + ")",
                    r.reduced (10.0f, 4.0f), juce::Justification::topLeft);
        return;
    }

    // The line, as the song will play it
    juce::Path path;
    juce::Path fill;
    const auto bottom = r.getBottom();
    for (float x = 0.0f; x <= r.getRight() + 2.0f; x += 2.0f)
    {
        const auto v = curve.getValueAt (xToTime (x, false), p->getCurrentBaseValue());
        const auto y = valueToY (v);
        if (juce::exactlyEqual (x, 0.0f)) { path.startNewSubPath (x, y); fill.startNewSubPath (x, bottom); }
        path.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (r.getRight() + 2.0f, bottom);
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (2.0f));

    for (int i = 0; i < n; ++i)
    {
        const auto x = timeToX (curve.getPointTime (i));
        if (x < -10.0f || x > r.getRight() + 10.0f)
            continue;
        const auto y = valueToY (curve.getPointValue (i));
        const auto big = (i == hoverPoint || i == dragPoint);
        g.setColour (big ? gold : text);
        g.fillEllipse (x - (big ? 5.0f : 4.0f), y - (big ? 5.0f : 4.0f), big ? 10.0f : 8.0f, big ? 10.0f : 8.0f);
        g.setColour (header);
        g.drawEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f, 1.0f);

        if (i == dragPoint || i == hoverPoint)
        {
            const auto label = p->valueToString (curve.getPointValue (i));
            const auto w = juce::jmax (50, juce::GlyphArrangement::getStringWidthInt (studioFont (11.0f, true), label) + 12);
            auto box = juce::Rectangle<float> (x + 8.0f, juce::jlimit (2.0f, r.getHeight() - 20.0f, y - 10.0f), (float) w, 18.0f);
            if (box.getRight() > r.getRight()) box.setX (x - 8.0f - box.getWidth());
            g.setColour (card.withAlpha (0.95f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (gold);
            g.setFont (studioFont (11.0f, true));
            g.drawText (label, box, juce::Justification::centred);
        }
    }

    g.setColour (textDim.withAlpha (0.8f));
    g.setFont (studioFont (10.5f));
    g.drawText (Automation::describe (track, Automation::getShownKey (track)), r.reduced (8.0f, 3.0f), juce::Justification::topRight);
}

void AutomationLane::mouseMove (const juce::MouseEvent& e)
{
    const auto i = pointAt (e.position);
    if (i != hoverPoint) { hoverPoint = i; repaint(); }
    setMouseCursor (i >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
}

void AutomationLane::mouseExit (const juce::MouseEvent&)
{
    if (hoverPoint >= 0) { hoverPoint = -1; repaint(); }
}

void AutomationLane::mouseDown (const juce::MouseEvent& e)
{
    auto* p = param();
    if (p == nullptr)
    {
        if (e.mods.isPopupMenu()) showMenu();
        return;
    }

    auto& um = track.edit.getUndoManager();
    auto& curve = p->getCurve();
    const auto i = pointAt (e.position);

    if (e.mods.isPopupMenu())
    {
        if (i >= 0)
        {
            um.beginNewTransaction();
            curve.removePoint (i, &um);
            hoverPoint = -1;
            repaint();
        }
        else
        {
            showMenu();
        }
        return;
    }

    um.beginNewTransaction();
    if (i >= 0)
    {
        dragPoint = i;
    }
    else
    {
        const auto t = xToTime (e.position.x, ! e.mods.isShiftDown());
        dragPoint = curve.addPoint (t, yToValue (e.position.y), 0.0f, &um);
    }
    repaint();
}

void AutomationLane::mouseDrag (const juce::MouseEvent& e)
{
    auto* p = param();
    if (p == nullptr || dragPoint < 0)
        return;

    auto& curve = p->getCurve();
    if (dragPoint >= curve.getNumPoints())
        return;

    // Keep the point between its neighbours
    auto t = xToTime (e.position.x, ! e.mods.isShiftDown());
    const auto eps = te::TimeDuration::fromSeconds (0.001);
    if (dragPoint > 0)
        t = juce::jmax (t, curve.getPointTime (dragPoint - 1) + eps);
    if (dragPoint + 1 < curve.getNumPoints())
        t = juce::jmin (t, curve.getPointTime (dragPoint + 1) - eps);

    dragPoint = curve.movePoint (dragPoint, te::EditPosition (t), yToValue (e.position.y),
                                 p->getValueRange(), false, &track.edit.getUndoManager());
    hoverPoint = dragPoint;
    repaint();
}

void AutomationLane::mouseUp (const juce::MouseEvent&)
{
    dragPoint = -1;
    repaint();
}

void AutomationLane::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto* p = param();
    if (p == nullptr)
        return;
    const auto i = pointAt (e.position);
    if (i >= 0)
    {
        auto& um = track.edit.getUndoManager();
        um.beginNewTransaction();
        p->getCurve().removePoint (i, &um);
        hoverPoint = dragPoint = -1;
        repaint();
    }
}

void AutomationLane::showMenu()
{
    juce::PopupMenu m;
    auto* p = param();
    m.addItem (1, "Remove all points on this line", p != nullptr && p->getCurve().getNumPoints() > 0);
    m.addItem (2, "Hide this lane");

    juce::Component::SafePointer<AutomationLane> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        if (r == 1)
        {
            if (auto* lineParam = safe->param())
            {
                auto& um = safe->track.edit.getUndoManager();
                um.beginNewTransaction();
                lineParam->getCurve().clear (&um);
                safe->repaint();
            }
        }
        else if (r == 2)
        {
            Automation::hide (safe->track);
            safe->owner.refreshAll();      // the lane goes away on the next update
        }
    });
}
