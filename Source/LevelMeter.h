#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "StudioLookAndFeel.h"

//==============================================================================
//  Horizontal peak meter, -60 dB to 0 dB, with tick marks.
//==============================================================================
class LevelMeter final : public juce::Component
{
public:
    void setLevel (float newLevel)
    {
        if (std::abs (newLevel - level) > 0.0005f)
        {
            level = newLevel;
            repaint();
        }
    }

    float getLevelDb() const   { return juce::Decibels::gainToDecibels (level, minDb); }

    void paint (juce::Graphics& g) override
    {
        using namespace StudioColours;
        auto b = getLocalBounds().toFloat();

        g.setColour (header);
        g.fillRoundedRectangle (b, 4.0f);

        const auto proportion = juce::jlimit (0.0f, 1.0f,
                                              juce::jmap (getLevelDb(), minDb, 0.0f, 0.0f, 1.0f));

        juce::ColourGradient gradient (good, b.getX(), 0.0f, bad, b.getRight(), 0.0f, false);
        gradient.addColour (0.80, gold);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * proportion), 4.0f);

        g.setColour (background.withAlpha (0.7f));
        for (auto tickDb : { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f })
        {
            const auto x = b.getX() + b.getWidth() * juce::jmap (tickDb, minDb, 0.0f, 0.0f, 1.0f);
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
        }

        g.setColour (cardBorder);
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
    }

private:
    static constexpr float minDb = -60.0f;
    float level = 0.0f;
};
