#include "SongListPanel.h"

//==============================================================================
SongListPanel::SongListPanel (SongLibrary& lib)
    : library (lib)
{
    newButton.setColour (juce::TextButton::buttonColourId, StudioColours::gold);
    newButton.setColour (juce::TextButton::textColourOffId, StudioColours::header);
    newButton.onClick = [this]
    {
        const auto id = library.createSong();
        refresh();
        selectSong (id);
        if (onSongCreated)
            onSongCreated (id);
    };
    addAndMakeVisible (newButton);

    searchBox.setTextToShowWhenEmpty ("Search title, lyrics, scripture...", StudioColours::textDim);
    searchBox.onTextChange = [this] { refresh(); };
    addAndMakeVisible (searchBox);

    statusFilter.addItem ("All statuses", 1);
    statusFilter.addItemList (Song::statuses(), 2);
    statusFilter.setSelectedId (1, juce::dontSendNotification);
    statusFilter.onChange = [this] { refresh(); };
    addAndMakeVisible (statusFilter);

    themeFilter.addItem ("All themes", 1);
    themeFilter.addItemList (Song::themes(), 2);
    themeFilter.setSelectedId (1, juce::dontSendNotification);
    themeFilter.onChange = [this] { refresh(); };
    addAndMakeVisible (themeFilter);

    list.setRowHeight (66);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);

    library.addChangeListener (this);
    refresh();
}

SongListPanel::~SongListPanel()
{
    library.removeChangeListener (this);
}

//==============================================================================
void SongListPanel::refresh()
{
    const auto search = searchBox.getText();
    const auto status = statusFilter.getSelectedId() > 1 ? statusFilter.getText() : juce::String();
    const auto theme  = themeFilter.getSelectedId() > 1 ? themeFilter.getText() : juce::String();

    visibleIds.clear();
    for (const auto& s : library.getSongs())
    {
        if (status.isNotEmpty() && s.status != status) continue;
        if (theme.isNotEmpty() && s.theme != theme)    continue;
        if (! s.matchesSearch (search))                continue;
        visibleIds.push_back (s.id);
    }

    list.updateContent();

    updatingSelection = true;
    const auto it = std::find (visibleIds.begin(), visibleIds.end(), selectedId);
    if (it != visibleIds.end())
        list.selectRow ((int) std::distance (visibleIds.begin(), it), true, true);
    else
        list.deselectAllRows();
    updatingSelection = false;

    list.repaint();
    repaint (countArea);
}

void SongListPanel::selectSong (const juce::String& id)
{
    selectedId = id;
    refresh();

    const auto it = std::find (visibleIds.begin(), visibleIds.end(), id);
    if (it != visibleIds.end())
        list.scrollToEnsureRowIsOnscreen ((int) std::distance (visibleIds.begin(), it));

    if (onSongSelected)
        onSongSelected (id);
}

void SongListPanel::selectFirst()
{
    refresh();
    if (! visibleIds.empty())
        selectSong (visibleIds.front());
    else if (onSongSelected)
        onSongSelected ({});
}

void SongListPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

//==============================================================================
int SongListPanel::getNumRows()
{
    return (int) visibleIds.size();
}

void SongListPanel::selectedRowsChanged (int lastRowSelected)
{
    if (updatingSelection || ! juce::isPositiveAndBelow (lastRowSelected, (int) visibleIds.size()))
        return;

    const auto id = visibleIds[(size_t) lastRowSelected];
    if (id != selectedId)
    {
        selectedId = id;
        if (onSongSelected)
            onSongSelected (id);
    }
}

juce::Colour SongListPanel::statusColour (const juce::String& status)
{
    if (status == "Idea")     return StudioColours::textDim;
    if (status == "Written")  return juce::Colour (0xff6fa8dc);
    if (status == "Arranged") return juce::Colour (0xffb48ee0);
    if (status == "Recorded") return StudioColours::warn;
    if (status == "Mixed")    return StudioColours::gold;
    return StudioColours::good;   // Released
}

void SongListPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    using namespace StudioColours;

    if (! juce::isPositiveAndBelow (row, (int) visibleIds.size()))
        return;

    const auto* song = library.find (visibleIds[(size_t) row]);
    if (song == nullptr)
        return;

    auto b = juce::Rectangle<int> (0, 0, width, height).reduced (2, 3);

    if (selected)
    {
        g.setColour (gold.withAlpha (0.14f));
        g.fillRoundedRectangle (b.toFloat(), 6.0f);
        g.setColour (gold);
        g.fillRoundedRectangle (b.removeFromLeft (3).toFloat(), 1.5f);
    }
    else
    {
        b.removeFromLeft (3);
        g.setColour (cardBorder.withAlpha (0.5f));
        g.drawHorizontalLine (height - 1, 8.0f, (float) width - 8.0f);
    }

    auto inner = b.reduced (12, 8);

    // Status chip
    const auto chipText = song->status;
    g.setFont (studioFont (11.5f, true));
    const auto chipW = 74;
    auto chip = inner.removeFromRight (chipW).removeFromTop (20);
    const auto chipColour = statusColour (song->status);
    g.setColour (chipColour.withAlpha (0.18f));
    g.fillRoundedRectangle (chip.toFloat(), 10.0f);
    g.setColour (chipColour);
    g.drawText (chipText, chip, juce::Justification::centred);
    inner.removeFromRight (8);

    g.setColour (text);
    g.setFont (studioFont (15.0f, true));
    g.drawText (song->title, inner.removeFromTop (22), juce::Justification::centredLeft, true);

    g.setColour (textDim);
    g.setFont (studioFont (12.5f));
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    g.drawText (song->key + dot + juce::String (song->tempo) + " BPM" + dot + song->theme,
                inner.removeFromTop (18), juce::Justification::centredLeft, true);
    g.drawText ("Received " + song->dateReceived, inner, juce::Justification::centredLeft, true);
}

//==============================================================================
void SongListPanel::paint (juce::Graphics& g)
{
    using namespace StudioColours;

    g.setColour (header.withAlpha (0.6f));
    g.fillRect (getLocalBounds());
    g.setColour (cardBorder);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    const auto total = (int) library.getSongs().size();
    const auto shown = (int) visibleIds.size();
    g.setColour (textDim);
    g.setFont (studioFont (12.5f));
    g.drawText (shown == total ? juce::String (total) + (total == 1 ? " song" : " songs")
                               : juce::String (shown) + " of " + juce::String (total) + " songs",
                countArea, juce::Justification::centredLeft);
}

void SongListPanel::resized()
{
    auto area = getLocalBounds().reduced (16, 16);

    newButton.setBounds (area.removeFromTop (40));
    area.removeFromTop (12);
    searchBox.setBounds (area.removeFromTop (34));
    area.removeFromTop (8);

    auto filters = area.removeFromTop (32);
    statusFilter.setBounds (filters.removeFromLeft ((filters.getWidth() - 8) / 2));
    filters.removeFromLeft (8);
    themeFilter.setBounds (filters);

    area.removeFromTop (6);
    countArea = area.removeFromTop (24);
    list.setBounds (area);
}
