#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
//  Small helpers for confirmation and message boxes.
//==============================================================================
namespace Dialogs
{
    inline void confirm (juce::Component* parent, const juce::String& title, const juce::String& message,
                         const juce::String& yesText, const juce::String& noText,
                         std::function<void()> onYes,
                         juce::MessageBoxIconType icon = juce::MessageBoxIconType::QuestionIcon)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions{}
                                          .withIconType (icon)
                                          .withTitle (title)
                                          .withMessage (message)
                                          .withButton (yesText, 1)
                                          .withButton (noText, 0)
                                          .withAssociatedComponent (parent),
                                      [onYes] (int result)
                                      {
                                          if (result == 1 && onYes)
                                              onYes();
                                      });
    }

    inline void message (juce::Component* parent, const juce::String& title, const juce::String& text,
                         juce::MessageBoxIconType icon = juce::MessageBoxIconType::WarningIcon)
    {
        // Centre over the whole window, so narrow panels don't push the box off-screen.
        if (parent != nullptr)
            parent = parent->getTopLevelComponent();

        juce::AlertWindow::showAsync (juce::MessageBoxOptions{}
                                          .withIconType (icon)
                                          .withTitle (title)
                                          .withMessage (text)
                                          .withButton ("OK", 0)
                                          .withAssociatedComponent (parent),
                                      nullptr);
    }
}
