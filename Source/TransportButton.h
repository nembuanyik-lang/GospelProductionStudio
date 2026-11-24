#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "StudioLookAndFeel.h"

//==============================================================================
//  Round transport button with a drawn icon (no font glyphs needed).
//==============================================================================
class TransportButton final : public juce::Button
{
public:
    enum class Icon { toStart, play, stop, record, loop, metronome };

    TransportButton (const juce::String& name, Icon i, juce::Colour activeColour)
        : juce::Button (name), icon (i), active (activeColour)
    {
        setTooltip (name);
    }

    void setLit (bool shouldBeLit)
    {
        if (lit != shouldBeLit)
        {
            lit = shouldBeLit;
            repaint();
        }
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        namespace sc = StudioColours;
        const auto card = sc::card, cardBorder = sc::cardBorder, ink0 = sc::text, dim = sc::textDim, bad = sc::bad;
        auto b = getLocalBounds().toFloat().reduced (1.5f);

        auto fill = lit ? active.withAlpha (0.22f) : card.brighter (0.06f);
        if (over) fill = fill.brighter (0.08f);
        if (down) fill = fill.darker (0.15f);

        g.setColour (fill);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (lit ? active : cardBorder.brighter (0.15f));
        g.drawRoundedRectangle (b, 8.0f, lit ? 2.0f : 1.0f);

        const auto ink = isEnabled() ? (lit ? active : ink0) : dim.withAlpha (0.5f);
        g.setColour (icon == Icon::record ? (isEnabled() ? bad : dim) : ink);

        const auto s = juce::jmin (b.getWidth(), b.getHeight()) * 0.36f;
        const auto c = b.getCentre();
        juce::Path p;

        switch (icon)
        {
            case Icon::play:
                p.addTriangle (c.x - s * 0.55f, c.y - s * 0.7f, c.x - s * 0.55f, c.y + s * 0.7f, c.x + s * 0.75f, c.y);
                g.fillPath (p);
                break;

            case Icon::stop:
                g.fillRoundedRectangle (juce::Rectangle<float> (s * 1.25f, s * 1.25f).withCentre (c), 2.0f);
                break;

            case Icon::record:
                g.fillEllipse (juce::Rectangle<float> (s * 1.4f, s * 1.4f).withCentre (c));
                if (lit)
                {
                    g.setColour (bad.withAlpha (0.35f));
                    g.drawEllipse (juce::Rectangle<float> (s * 2.0f, s * 2.0f).withCentre (c), 2.0f);
                }
                break;

            case Icon::toStart:
                g.fillRect (juce::Rectangle<float> (c.x - s * 0.8f, c.y - s * 0.65f, s * 0.22f, s * 1.3f));
                p.addTriangle (c.x + s * 0.7f, c.y - s * 0.65f, c.x + s * 0.7f, c.y + s * 0.65f, c.x - s * 0.45f, c.y);
                g.fillPath (p);
                break;

            case Icon::loop:
            {
                const auto r = juce::Rectangle<float> (s * 1.9f, s * 1.1f).withCentre (c);
                g.drawRoundedRectangle (r, s * 0.5f, 2.2f);
                juce::Path arrow;
                arrow.addTriangle (r.getRight() - s * 0.55f, r.getY() - s * 0.32f,
                                   r.getRight() - s * 0.55f, r.getY() + s * 0.32f,
                                   r.getRight() - s * 0.05f, r.getY());
                g.fillPath (arrow);
                break;
            }

            case Icon::metronome:
            {
                p.startNewSubPath (c.x - s * 0.7f, c.y + s * 0.8f);
                p.lineTo (c.x - s * 0.3f, c.y - s * 0.8f);
                p.lineTo (c.x + s * 0.3f, c.y - s * 0.8f);
                p.lineTo (c.x + s * 0.7f, c.y + s * 0.8f);
                p.closeSubPath();
                g.strokePath (p, juce::PathStrokeType (2.0f));
                g.drawLine (c.x, c.y + s * 0.5f, c.x + s * 0.55f, c.y - s * 0.75f, 2.2f);
                break;
            }
        }
    }

private:
    Icon icon;
    juce::Colour active;
    bool lit = false;
};
