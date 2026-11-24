#pragma once

#include "SongListPanel.h"
#include "SongEditor.h"

//==============================================================================
//  Song Library page: list on the left, the selected song on the right.
//==============================================================================
class LibraryPage final : public juce::Component
{
public:
    LibraryPage (SongLibrary& lib, MemoRecorder& memo)
        : listPanel (lib), editor (lib, memo)
    {
        listPanel.onSongSelected = [this] (const juce::String& id)
        {
            if (id.isEmpty())
                editor.clear();
            else
                editor.loadSong (id);
        };

        listPanel.onSongCreated = [this] (const juce::String&)
        {
            editor.focusTitle();
        };

        editor.onSongDeleted = [this] (const juce::String& id)
        {
            if (onSongDeleted)
                onSongDeleted (id);
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<LibraryPage> (this)]
                                             { if (safe != nullptr) safe->listPanel.selectFirst(); });
        };

        editor.onOpenInStudio = [this] (const juce::String& id)
        {
            if (onOpenInStudio)
                onOpenInStudio (id);
        };

        addAndMakeVisible (listPanel);
        addAndMakeVisible (editor);

        listPanel.selectFirst();
    }

    void flush()   { editor.flush(); }

    std::function<void (const juce::String& id)> onOpenInStudio, onSongDeleted;

    void resized() override
    {
        auto area = getLocalBounds();
        listPanel.setBounds (area.removeFromLeft (juce::jlimit (300, 380, getWidth() / 3)));
        editor.setBounds (area);
    }

private:
    SongListPanel listPanel;
    SongEditor    editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryPage)
};
