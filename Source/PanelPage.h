#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "StudioLookAndFeel.h"

//==============================================================================
//  A page shown inside the main window (Sound Library, Plugins, Audio Setup),
//  with a "Back" button. Inside the window, so it can never hide behind it.
//==============================================================================
class PanelPage final : public juce::Component
{
public:
    PanelPage (const juce::String& titleText, std::unique_ptr<juce::Component> contentToShow,
               std::function<void()> onBackPressed)
        : content (std::move (contentToShow)), onBack (std::move (onBackPressed))
    {
        backButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\x90  Back"));
        backButton.setTooltip ("Back to where you were (Esc)");
        backButton.setWantsKeyboardFocus (false);
        backButton.onClick = [this] { if (onBack) onBack(); };
        addAndMakeVisible (backButton);

        title.setText (titleText, juce::dontSendNotification);
        title.setFont (studioFont (20.0f, true));
        title.setColour (juce::Label::textColourId, StudioColours::text);
        addAndMakeVisible (title);

        if (content != nullptr)
            addAndMakeVisible (*content);
        setWantsKeyboardFocus (true);
    }

    juce::Component* getContent() const   { return content.get(); }
    juce::String getTitle() const         { return title.getText(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (StudioColours::background);
        g.setColour (StudioColours::cardBorder);
        g.drawHorizontalLine (topBarHeight - 1, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto bar = r.removeFromTop (topBarHeight).reduced (16, 8);
        backButton.setBounds (bar.removeFromLeft (110));
        bar.removeFromLeft (16);
        title.setBounds (bar);

        if (content != nullptr)
        {
            // Keep the page readable on wide screens
            auto body = r.reduced (16, 10);
            if (body.getWidth() > 1000)
                body = body.withSizeKeepingCentre (1000, body.getHeight());
            content->setBounds (body);
        }
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey && onBack != nullptr)
        {
            onBack();
            return true;
        }
        return false;
    }

    static constexpr int topBarHeight = 52;

private:
    std::unique_ptr<juce::Component> content;
    std::function<void()> onBack;
    juce::TextButton backButton;
    juce::Label title;
};
