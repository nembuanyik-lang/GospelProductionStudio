#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongLibrary.h"
#include "StudioLookAndFeel.h"

//==============================================================================
//  Left-hand side of the Song Library: search, filters and the song list.
//==============================================================================
class SongListPanel final : public juce::Component,
                            private juce::ListBoxModel,
                            private juce::ChangeListener
{
public:
    explicit SongListPanel (SongLibrary&);
    ~SongListPanel() override;

    std::function<void (const juce::String& id)> onSongSelected;
    std::function<void (const juce::String& id)> onSongCreated;

    void selectSong (const juce::String& id);
    void selectFirst();
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    static juce::Colour statusColour (const juce::String& status);

    SongLibrary& library;

    juce::TextButton newButton    { "+ New Song" };
    juce::TextEditor searchBox;
    juce::ComboBox   statusFilter, themeFilter;
    juce::ListBox    list         { "Songs", this };

    std::vector<juce::String> visibleIds;
    juce::String selectedId;
    bool updatingSelection = false;
    juce::Rectangle<int> countArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongListPanel)
};
