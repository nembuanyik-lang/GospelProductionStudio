#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"
#include "StudioLookAndFeel.h"

//==============================================================================
//  One lyrics section card: type, number, lyrics, move up/down, remove.
//==============================================================================
class SectionEditor final : public juce::Component
{
public:
    SectionEditor()
    {
        typeBox.addItemList (Song::sectionTypes(), 1);
        typeBox.onChange = [this] { if (onChange) onChange(); };
        addAndMakeVisible (typeBox);

        lyrics.setMultiLine (true, true);
        lyrics.setReturnKeyStartsNewLine (true);
        lyrics.setScrollbarsShown (true);
        lyrics.setFont (studioFont (16.0f));
        lyrics.setTextToShowWhenEmpty ("Type the lyrics here...", StudioColours::textDim);
        lyrics.onTextChange = [this] { if (onChange) onChange(); };
        addAndMakeVisible (lyrics);

        upButton.onClick     = [this] { if (onMoveUp)   onMoveUp(); };
        downButton.onClick   = [this] { if (onMoveDown) onMoveDown(); };
        removeButton.onClick = [this] { if (onRemove)   onRemove(); };
        upButton.setTooltip ("Move this section up");
        downButton.setTooltip ("Move this section down");

        for (auto* b : { &upButton, &downButton, &removeButton })
            addAndMakeVisible (b);
    }

    std::function<void()> onChange, onMoveUp, onMoveDown, onRemove;

    void setSection (const SongSection& section)
    {
        if (Song::sectionTypes().contains (section.type))
            typeBox.setText (section.type, juce::dontSendNotification);
        else
            typeBox.setSelectedItemIndex (1, juce::dontSendNotification);

        lyrics.setText (section.lyrics, false);
    }

    SongSection getSection() const   { return { typeBox.getText(), lyrics.getText() }; }

    void setLabel (const juce::String& newLabel, bool isFirst, bool isLast)
    {
        label = newLabel;
        upButton.setEnabled (! isFirst);
        downButton.setEnabled (! isLast);
        repaint();
    }

    void focusLyrics()               { lyrics.grabKeyboardFocus(); }

    void paint (juce::Graphics& g) override
    {
        using namespace StudioColours;
        auto b = getLocalBounds().toFloat();
        g.setColour (card);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (cardBorder);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (gold);
        g.setFont (studioFont (14.0f, true));
        g.drawText (label.toUpperCase(), 16, 10, 160, 28, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (16, 10);
        auto top = area.removeFromTop (28);
        top.removeFromLeft (150);
        typeBox.setBounds (top.removeFromLeft (150));

        removeButton.setBounds (top.removeFromRight (84));
        top.removeFromRight (6);
        downButton.setBounds (top.removeFromRight (64));
        top.removeFromRight (6);
        upButton.setBounds (top.removeFromRight (52));

        area.removeFromTop (8);
        lyrics.setBounds (area);
    }

private:
    juce::String label;
    juce::ComboBox typeBox;
    juce::TextEditor lyrics;
    juce::TextButton upButton { "Up" }, downButton { "Down" }, removeButton { "Remove" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionEditor)
};
