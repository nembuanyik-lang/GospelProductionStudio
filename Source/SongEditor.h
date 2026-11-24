#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "SongLibrary.h"
#include "MemoRecorder.h"
#include "SectionEditor.h"

//==============================================================================
//  Right-hand side of the Song Library: everything about one song.
//  Changes save automatically less than a second after you stop typing.
//==============================================================================
class SongEditor final : public juce::Component,
                         private juce::Timer
{
public:
    SongEditor (SongLibrary&, MemoRecorder&);
    ~SongEditor() override;

    void loadSong (const juce::String& id);
    void clear();
    void flush();                     // save now if anything changed
    void focusTitle();

    std::function<void()> onSongDeleted;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void markDirty();
    void readFieldsIntoSong();
    void rebuildSections (int focusIndex = -1);
    void rebuildLabelsOnly();
    void layoutSections();
    void addSection (const juce::String& type);
    void moveSection (int index, int delta);
    void removeSection (int index);
    void deleteSong();

    void toggleRecord();
    void togglePlay();
    void deleteMemo();
    void updateMemoControls();

    static juce::String formatSeconds (double seconds);

    SongLibrary&  library;
    MemoRecorder& memo;

    Song song;
    bool hasSong = false, dirty = false;
    juce::uint32 lastEditMs = 0;

    // Details
    juce::TextEditor titleEdit, dateEdit, scriptureEdit, notesEdit;
    juce::TextButton todayButton      { "Today" };
    juce::ComboBox   keyBox, timeSigBox, themeBox, statusBox;
    juce::Slider     tempoSlider      { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::TextButton deleteSongButton { "Delete Song" };

    // Voice memo
    juce::TextButton recordButton     { "Record Memo" };
    juce::TextButton playButton       { "Play" };
    juce::TextButton deleteMemoButton { "Delete Memo" };
    juce::String     memoText;
    double           memoSeconds = 0.0;
    bool             memoSecondsStale = true, wasPlaying = false, wasRecording = false;
    juce::Colour     memoColour;

    // Lyrics
    juce::TextButton addSectionButton { "+ Add Section" };
    juce::Viewport   sectionsViewport;
    juce::Component  sectionsHolder;
    juce::OwnedArray<SectionEditor> sectionEditors;

    // Painted labels
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> fieldLabels;
    juce::Rectangle<int> memoArea, memoTextArea, lyricsHeaderArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongEditor)
};
