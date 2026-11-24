#pragma once

#include "Timeline.h"

class ChordPanel;

//==============================================================================
//  One clickable chord in the palette.
//==============================================================================
class ChordChip final : public juce::Button
{
public:
    ChordChip (ChordPanel& p, const Theory::Chord& c, const juce::String& h, bool suggestion)
        : juce::Button ({}), panel (p), chord (c), hint (h), isSuggestion (suggestion) {}

    void paintButton (juce::Graphics&, bool over, bool down) override;
    void clicked (const juce::ModifierKeys&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    ChordPanel& panel;
    Theory::Chord chord;
    juce::String hint;
    bool isSuggestion;
};

//==============================================================================
//  The Chord palette: chords in the key, gospel colours, progressions.
//==============================================================================
class ChordPanel final : public juce::Component,
                         private juce::Timer
{
public:
    ChordPanel (StudioEngine&, TimelineView&);
    ~ChordPanel() override;

    void refresh();                                      // key or display changed
    void showSuggestionsFor (const Theory::Chord*);
    void saveSelectionAsProgression();

    // Called by chips
    void chipClicked (const Theory::Chord&, bool arpeggio);
    void addChordAtPlayhead (const Theory::Chord&);
    Theory::Key getKey() const                          { return timeline.currentKey(); }
    Theory::Display getDisplay() const                  { return timeline.chordDisplay; }
    bool isCurrent (const Theory::Chord& c) const       { return hasCurrent && c == current; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct ProgressionList;
    struct MyList;
    struct Saved { juce::String name, text; };

    void timerCallback() override;
    void showTab (int);
    void rebuildPalette();
    void schedulePaletteRebuild();
    void layoutPalette();
    void updateBuilderChord();
    double chordLengthBeats() const;
    void insertProgression (const std::vector<Theory::TimedChord>&);
    void hearProgression (std::vector<Theory::TimedChord>);
    void chordsToNotes();
    void loadSaved();
    void storeSaved();
    std::vector<Theory::TimedChord> selectedLibraryChords() const;

    StudioEngine& engine;
    TimelineView& timeline;

    Theory::Chord current;
    bool hasCurrent = false;
    std::vector<Theory::Chord> suggestions;
    int lastPlayheadIndex = -2;
    int tab = 0;

    // Header
    juce::ComboBox displayBox;
    juce::TextButton paletteTab { "Palette" }, progressionsTab { "Progressions" }, mineTab { "My Progressions" };

    // Palette
    juce::Viewport paletteViewport;
    juce::Component paletteContent;
    juce::OwnedArray<ChordChip> chips;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> groupTitles;
    juce::ComboBox degreeBox, qualityBox, bassBox;
    juce::TextButton hearBuiltButton { "Hear" }, addBuiltButton { "Add" };

    // Progressions
    juce::ComboBox categoryBox, levelBox;
    std::unique_ptr<ProgressionList> progressionModel;
    juce::ListBox progressionList;
    juce::TextButton hearProgressionButton { "Hear" }, insertProgressionButton { "Insert at playhead" };
    std::vector<int> visibleProgressions;

    // My Progressions
    std::unique_ptr<MyList> myModel;
    juce::ListBox myList;
    juce::TextButton saveMineButton { "Save selected chords" }, insertMineButton { "Insert" }, deleteMineButton { "Delete" };
    std::vector<Saved> saved;

    // Shared controls
    juce::ComboBox lengthBox, voicingBox, leftHandBox;
    juce::ToggleButton anticipateToggle { "Anticipate (push early)" };
    juce::TextButton notesButton { "Chords to notes..." };

    juce::Rectangle<int> titleArea, builderLabelArea, sharedLabelArea;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> labels;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    friend struct ProgressionList;
    friend struct MyList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordPanel)
};
