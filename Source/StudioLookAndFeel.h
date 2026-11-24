#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
//  Studio colour palette: deep charcoal with a gold accent.
//==============================================================================
namespace StudioColours
{
    inline const juce::Colour background  { 0xff121419 };
    inline const juce::Colour header      { 0xff0c0e12 };
    inline const juce::Colour card        { 0xff1b1e25 };
    inline const juce::Colour cardBorder  { 0xff2a2f39 };
    inline const juce::Colour text        { 0xffeceae4 };
    inline const juce::Colour textDim     { 0xff9097a3 };
    inline const juce::Colour gold        { 0xffd4a843 };
    inline const juce::Colour goldDim     { 0xff8a6d2c };
    inline const juce::Colour good        { 0xff4caf7d };
    inline const juce::Colour warn        { 0xffe0a33a };
    inline const juce::Colour bad         { 0xffe05a4f };
}

//==============================================================================
//  Font helper (JUCE 8 FontOptions).
//==============================================================================
inline juce::Font studioFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
//  Look and feel applied to the whole application.
//==============================================================================
class StudioLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    StudioLookAndFeel()
        : juce::LookAndFeel_V4 (makeScheme())
    {
        using namespace StudioColours;

        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::DocumentWindow::backgroundColourId,  background);

        setColour (juce::TextButton::buttonColourId,   card.brighter (0.08f));
        setColour (juce::TextButton::buttonOnColourId, gold);
        setColour (juce::TextButton::textColourOffId,  text);
        setColour (juce::TextButton::textColourOnId,   header);

        setColour (juce::ComboBox::backgroundColourId, card.brighter (0.05f));
        setColour (juce::ComboBox::outlineColourId,    cardBorder);
        setColour (juce::ComboBox::arrowColourId,      gold);

        setColour (juce::PopupMenu::backgroundColourId,            card);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, gold);
        setColour (juce::PopupMenu::highlightedTextColourId,       header);

        setColour (juce::ToggleButton::tickColourId, gold);
        setColour (juce::Slider::thumbColourId,      gold);
        setColour (juce::Slider::trackColourId,      goldDim);

        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,   gold.withAlpha (0.85f));
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, gold.withAlpha (0.30f));
        setColour (juce::MidiKeyboardComponent::whiteNoteColourId,        juce::Colour (0xfff4f1ea));
        setColour (juce::MidiKeyboardComponent::blackNoteColourId,        juce::Colour (0xff15161a));
        setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff3a3d45));
        setColour (juce::MidiKeyboardComponent::shadowColourId,           juce::Colours::black.withAlpha (0.35f));
        setColour (juce::MidiKeyboardComponent::textLabelColourId,        juce::Colour (0xff6b6f78));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return studioFont (juce::jmin (15.0f, (float) buttonHeight * 0.45f), true);
    }

private:
    static juce::LookAndFeel_V4::ColourScheme makeScheme()
    {
        using namespace StudioColours;
        return { background,          // windowBackground
                 card,                // widgetBackground
                 card,                // menuBackground
                 cardBorder,          // outline
                 text,                // defaultText
                 card.brighter (0.1f),// defaultFill
                 header,              // highlightedText
                 gold,                // highlightedFill
                 text };              // menuText
    }
};
