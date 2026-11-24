#pragma once

#include "StudioEngine.h"
#include "StudioLookAndFeel.h"
#include "ChordTrack.h"
#include "Grooves.h"

class TimelineView;

//==============================================================================
//  Bar ruler with playhead and loop band.
//==============================================================================
class TimelineRuler final : public juce::Component
{
public:
    explicit TimelineRuler (TimelineView& o) : owner (o) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class Drag { none, position, loopStart, loopEnd, loopMove, loopNew };
    Drag drag = Drag::none;
    double dragStartBeat = 0.0, origLoopStart = 0.0, origLoopEnd = 0.0;
    bool isInLoopStrip (int y) const   { return y >= getHeight() / 2; }

    TimelineView& owner;
};

//==============================================================================
//  The name, colour and buttons at the left of one track.
//==============================================================================
class TrackHeader final : public juce::Component
{
public:
    TrackHeader (TimelineView&, te::Track&);

    void refresh();
    te::Track& getTrack() const   { return track; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void showMenu();
    float meterLevel = 0.0f;
    friend class TimelineView;

    TimelineView& owner;
    te::Track& track;
    te::AudioTrack* audioTrack = nullptr;
    bool isInstrument = false;

    juce::Label name;
    juce::TextButton muteButton { "M" }, soloButton { "S" }, armButton { "R" };
    juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton addSectionButton { "+ Section" };
};

//==============================================================================
//  Track lanes with clips: select, move, trim, copy, delete.
//==============================================================================
class TimelineLanes final : public juce::Component
{
public:
    explicit TimelineLanes (TimelineView& o) : owner (o) { setWantsKeyboardFocus (true); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class Drag { none, move, trimStart, trimEnd };

    // Chord lane
    int  chordAt (juce::Point<int>, const std::vector<ChordTrack::Event>&) const;
    juce::Rectangle<float> chordBounds (const ChordTrack::Event&) const;
    void showChordMenu (const ChordTrack::Event&);
    void showChordQuickMenu (double beat);
    void paintChords (juce::Graphics&);
    juce::ValueTree dragChord;
    double chordOrigStart = 0.0, chordOrigEnd = 0.0;

    std::map<te::EditItemID, std::unique_ptr<te::SmartThumbnail>> thumbnails;
    std::map<te::EditItemID, juce::File> thumbnailFiles;

    te::Clip* clipAt (juce::Point<int>, int& laneIndex) const;
    juce::Rectangle<float> clipBounds (te::Clip&, int laneIndex) const;
    void showClipMenu (te::Clip&);
    void showLaneMenu (int laneIndex, double beat);

    TimelineView& owner;
    Drag drag = Drag::none;
    double dragStartBeat = 0.0, origStart = 0.0, origEnd = 0.0;
    int dragTargetLane = -1;
};

//==============================================================================
//  The whole arrangement view.
//==============================================================================
class TimelineView final : public juce::Component,
                           private juce::Timer,
                           private juce::ScrollBar::Listener
{
public:
    explicit TimelineView (StudioEngine&);
    ~TimelineView() override;

    void editChanged();                     // call after a song is opened or closed

    // Geometry ---------------------------------------------------------------
    static constexpr int headerWidth = 230, rulerHeight = 40, sectionLaneHeight = 34, chordLaneHeight = 38, trackHeight = 66;
    static constexpr int chordLane = 1, firstTrackLane = 2;

    te::Edit* getEdit() const               { return engine.getEdit(); }
    StudioEngine& getEngine()               { return engine; }

    double timeToBeat (te::TimePosition) const;
    te::TimePosition beatToTime (double) const;
    float  beatToX (double beat) const      { return (float) ((beat - scrollBeats) * pxPerBeat); }
    double xToBeat (float x) const          { return scrollBeats + x / pxPerBeat; }
    double snap (double beat) const;
    double getBeatsPerBar() const;
    double getPlayheadBeat() const;

    int  getNumLanes() const                { return (int) lanes.size(); }
    te::Track* getLane (int i) const        { return juce::isPositiveAndBelow (i, (int) lanes.size()) ? lanes[(size_t) i] : nullptr; }
    int  getLaneY (int i) const;
    int  getLaneHeight (int i) const        { return i == 0 ? sectionLaneHeight : i == chordLane ? chordLaneHeight : trackHeight; }
    int  laneAtY (int y) const;
    int  getContentHeight() const;
    int  scrollY = 0;

    // Selection -------------------------------------------------------------
    te::Clip::Ptr selectedClip;
    te::Track* selectedTrack = nullptr;
    void selectClip (te::Clip*);
    void selectTrack (te::Track*);

    // Actions -----------------------------------------------------------------
    void beginUndoStep();
    void deleteSelectedClip();
    void duplicateSelectedClip();
    void copySelectedClip();
    void pasteClip();
    void addTrackMenu();
    void addSectionAt (double beat);
    void zoomAround (double factor, float x);
    void scrollBy (double beats);
    void zoomToFit();
    void rebuildHeaders();
    void refreshAll();

    std::function<void()> onTracksChanged;
    std::function<void (te::MidiClip*)> onOpenClip;          // piano roll (Step 7)
    void openClip (te::Clip*);
    void insertGroove (te::AudioTrack&, const Grooves::Groove&, double beat);
    void importAudioTo (te::AudioTrack&);
    std::function<juce::File()> getMemoFile;
    std::unique_ptr<juce::FileChooser> chooser;

    // Chords (Step 6) --------------------------------------------------------
    std::function<Theory::Key()> getKey;
    Theory::Display chordDisplay = Theory::Display::both;
    std::vector<juce::ValueTree> selectedChords;
    std::function<void (const Theory::Chord*)> onChordSelected;
    std::function<void()> onTogglePalette, onSaveSelection, onChordsChanged;

    Theory::Key currentKey() const          { return getKey ? getKey() : Theory::Key(); }
    void selectChord (const juce::ValueTree&, bool extend);
    void clearChordSelection();
    void deleteSelectedChords();
    void reharmoniseSelection (Theory::Level);
    void auditionChord (const Theory::Chord&, bool arpeggio = false);
    std::vector<ChordTrack::Event> getSelectedChordEvents() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void updateScrollbars();
    juce::String trackSignature() const;

    StudioEngine& engine;
    std::vector<te::Track*> lanes;

    double pxPerBeat = 18.0, scrollBeats = 0.0;
    int snapMode = 2;    // 0 off, 1 half beat, 2 beat, 3 bar
    te::Clip::Ptr clipboard;
    juce::String lastSignature;
    double lastPlayhead = -1.0;

    juce::TextButton addTrackButton { "+ Track" }, undoButton { "Undo" }, redoButton { "Redo" }, fitButton { "Fit" };
    juce::ComboBox snapBox;
    juce::Slider zoomSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    TimelineRuler ruler { *this };
    TimelineLanes lanesView { *this };
    juce::Component headerHolder;
    juce::OwnedArray<juce::Component> headers;
    juce::ScrollBar hScroll { false }, vScroll { true };
    juce::Rectangle<int> toolbarArea, cornerArea;

    friend class TimelineRuler;
    friend class TimelineLanes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineView)
};
