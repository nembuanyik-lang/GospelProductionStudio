#pragma once

#include "Timeline.h"

class PianoRoll;

//==============================================================================
//  Parts of the piano roll (each draws one area and handles its mouse).
//==============================================================================
class RollRuler final : public juce::Component
{
public:
    explicit RollRuler (PianoRoll& o) : owner (o) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    static constexpr int rulerHeight = 18, chordHeight = 22;
private:
    PianoRoll& owner;
};

class RollKeys final : public juce::Component
{
public:
    explicit RollKeys (PianoRoll& o) : owner (o) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    PianoRoll& owner;
    int lastNote = -1;
};

class RollGrid final : public juce::Component
{
public:
    explicit RollGrid (PianoRoll& o) : owner (o) { setWantsKeyboardFocus (true); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Drag { none, move, resizeEnd, resizeStart, rubber };

    struct Original { juce::ValueTree note; double start, length; int pitch; };

    te::MidiNote* noteAt (juce::Point<int>, Drag& edge) const;
    void addNoteAt (juce::Point<int>);
    void beginNoteDrag (te::MidiNote&, Drag, juce::Point<int>);

    PianoRoll& owner;
    Drag drag = Drag::none;
    std::vector<Original> originals;
    juce::ValueTree anchorNote;
    double anchorStart = 0.0, anchorEnd = 0.0, mouseStartBeat = 0.0;
    int anchorPitch = 60, lastHeardPitch = -1;
    juce::Rectangle<int> rubber;
    std::vector<juce::ValueTree> selectionBefore;

    friend class PianoRoll;
};

class RollVelocity final : public juce::Component
{
public:
    explicit RollVelocity (PianoRoll& o) : owner (o) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    void apply (const juce::MouseEvent&);
    PianoRoll& owner;
    bool relative = false;
    int startY = 0;
    std::vector<std::pair<juce::ValueTree, int>> startVelocities;
};

//==============================================================================
//  The piano roll: opens one MIDI clip for note editing, with the song's
//  Chord Track drawn behind the notes.
//==============================================================================
class PianoRoll final : public juce::Component,
                        private juce::Timer,
                        private juce::ScrollBar::Listener
{
public:
    PianoRoll (StudioEngine&, TimelineView&);
    ~PianoRoll() override;

    void setClip (te::MidiClip*);
    te::MidiClip* getClip() const                   { return clip.get(); }
    bool isOpen() const                             { return clip != nullptr; }

    std::function<void()> onClose, onSplitChanged;
    bool isSplit() const                            { return splitButton.getToggleState(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // ---- Geometry (x in song beats, y in MIDI notes) ------------------------
    float  beatToX (double editBeat) const          { return (float) ((editBeat - scrollBeats) * pxPerBeat); }
    double xToBeat (float x) const                  { return scrollBeats + x / pxPerBeat; }
    float  noteToY (int note) const                 { return (127 - note) * noteHeight - (float) scrollY; }
    int    yToNote (float y) const                  { return juce::jlimit (0, 127, 127 - (int) std::floor ((y + (float) scrollY) / noteHeight)); }
    double contentToEdit (double contentBeat) const { return contentBeat + contentOffset; }
    double editToContent (double editBeat) const    { return editBeat - contentOffset; }

    double gridBeats() const;                       // 0 = grid off
    double snapBeat (double editBeat) const;
    double floorToGrid (double editBeat) const;
    double newNoteLength() const;
    double clipStartBeat() const, clipEndBeat() const;

    // ---- Notes ---------------------------------------------------------------
    te::Edit* getEdit() const                       { return engine.getEdit(); }
    juce::UndoManager* undo() const;
    std::vector<te::MidiNote*> getNotes() const;
    te::MidiNote* findNote (const juce::ValueTree&) const;
    bool isSelected (const te::MidiNote&) const;
    void select (te::MidiNote*, bool add);
    void clearSelection();
    std::vector<te::MidiNote*> getSelectedNotes() const;
    std::vector<te::MidiNote*> getTargetNotes() const;   // selection, or every note when nothing is selected
    void hear (int note);
    void notesChanged();                                 // repaint here and on the timeline

    // Note checks against the Chord Track
    enum class Fit { chordTone, inKey, clash, noChord };
    Fit fitOf (const te::MidiNote&) const;
    const ChordTrack::Event* chordAtBeat (double editBeat) const;
    const ChordTrack::Event* chordForNote (const te::MidiNote&) const;   // the chord the note overlaps most

    // ---- Editing commands ----------------------------------------------------
    void deleteSelected();
    void selectAll();
    void transpose (int semitones);
    void nudge (int steps);
    void quantise (double strength, bool ends);
    void legato();
    void humanise();
    void scaleVelocity (double factor);
    void fitToChords();
    void selectClashes();
    void copySelection();
    void pasteAtPlayhead();
    void duplicateSelection();
    void showEditMenu();
    void zoomToClip();
    void zoomX (double factor, float aroundX);
    void zoomY (float factor, float aroundY);
    void scrollXBy (double beats);
    void scrollYBy (int px);

    StudioEngine& engine;
    TimelineView& timeline;

    double scrollBeats = 0.0, pxPerBeat = 80.0, contentOffset = 0.0;
    float  noteHeight = 14.0f;
    int    scrollY = 0;
    bool   drawMode = false;
    juce::String status;

    std::vector<juce::ValueTree> selection;
    std::vector<ChordTrack::Event> chordEvents;      // refreshed while open

    struct ClipNote { int pitch; double start, length; int velocity; };
    std::vector<ClipNote> clipboard;
    double lastLength = 0.0;

    void updateStatus();

private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void updateScrollbars();
    void refreshGeometry();
    void setDrawMode (bool);

    te::MidiClip::Ptr clip;

    juce::TextButton selectButton { "Select" }, drawButton { "Draw" };
    juce::ComboBox gridBox, lengthBox;
    juce::TextButton editButton { "Edit" }, fitButton { "Fit to chords" }, zoomButton { "Fit" };
    juce::ToggleButton checkToggle { "Check notes" };
    juce::TextButton splitButton { "Split" }, closeButton { "Close" };

    RollRuler ruler { *this };
    RollKeys keys { *this };
    RollGrid grid { *this };
    RollVelocity velocity { *this };
    juce::ScrollBar hScroll { false }, vScroll { true };

    juce::Rectangle<int> toolbarArea, titleArea, footerArea;
    double lastFollowBeat = -1.0;

public:
    static constexpr int keysWidth = 56, velocityHeight = 48, toolbarHeight = 38, footerHeight = 18;
    bool checkNotes() const                         { return checkToggle.getToggleState(); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};
