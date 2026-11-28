#pragma once

#include "Timeline.h"
#include "Grooves.h"
#include "RealFeel.h"

//==============================================================================
//  The drum grid (Step 17): program drums by clicking boxes.
//
//  One row per drum (kick, snare, hi-hats, toms, cymbals...), one column per
//  step (16ths, or triplets for a shuffle). Click to add or remove a hit,
//  Shift-click for an accent, right-click to choose how hard. Start from a
//  gospel groove, add fills, swing and humanise.
//==============================================================================
class DrumGrid final : public juce::Component,
                       private juce::Timer,
                       private juce::ScrollBar::Listener
{
public:
    DrumGrid (StudioEngine&, TimelineView&);
    ~DrumGrid() override;

    void setClip (te::MidiClip*);
    te::MidiClip* getClip() const          { return clip.get(); }
    bool isOpen() const                    { return clip != nullptr; }

    std::function<void()> onClose;
    std::function<void (te::MidiClip*)> onPianoRoll;     // switch this clip to the piano roll

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // The drums shown, top to bottom
    struct Row { int note; juce::String name; };
    static const std::vector<Row>& standardRows();

    // ---- Used by the tests too ----------------------------------------------
    double stepBeats() const               { return 1.0 / stepsPerBeat; }
    int numSteps() const;
    double stepToContentBeat (int step) const;            // with swing
    int contentBeatToStep (double beat) const;            // nearest step, with swing
    te::MidiNote* hitAt (int note, int step) const;
    void setHit (int note, int step, int velocity);       // 0 removes
    void applyGroove (const Grooves::Groove&);
    void addFill (int which, int bar);
    void setSwing (float amount);                         // 0 .. 0.5 of a step (0.33 = triplet shuffle)
    float getSwing() const;
    void humanise();
    // Step 32: real drummers
    void applyFeel (const RealFeel::Style&);              // timing and accents of real drummers in that style
    void backToGrid();                                    // timing exactly on the grid again
    void applyRealGroove (const RealFeel::Groove&);       // a groove played by a real drummer
    void showHumaniseMenu();
    void repeatBar (int bar);
    void clearAll();
    void setBars (int bars);                              // makes the clip this many bars long

    static juce::StringArray fillNames();

private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void refreshRows();
    void updateScroll();
    void showHint();
    juce::UndoManager* undo() const;
    int barCount() const;
    int stepsPerBar() const;

    juce::Rectangle<int> gridArea() const;
    int rowAtY (int y) const;
    int stepAtX (int x) const;
    float stepToX (int step) const;

    StudioEngine& engine;
    TimelineView& timeline;
    te::MidiClip::Ptr clip;

    std::vector<Row> rows;
    int stepsPerBeat = 4;
    double contentStart = 0.0, lengthBeats = 4.0, beatsPerBar = 4.0, clipStartEditBeat = 0.0;
    float cellWidth = 26.0f;
    int rowHeight = 22, scrollStep = 0, scrollRow = 0, selectedBar = -1;
    int dragNote = -1, dragVelocity = 0, lastDragStep = -1;
    bool dragAdding = true;
    bool showPercussion = false;           // the African percussion rows (also shown when the clip uses them)
    int playStep = -1;

    juce::TextButton grooveButton { "Start from a groove" }, fillButton { "Add a fill" }, humaniseButton { "Humanise" },
                     repeatButton { "Repeat bar" }, clearButton { "Clear" }, pianoButton { "Piano roll" }, closeButton { "Close" };
    juce::ComboBox stepBox, barsBox;
    juce::Slider swingSlider;
    juce::Label swingLabel, status;
    juce::ScrollBar hScroll { false }, vScroll { true };

    static constexpr int headerWidth = 112, rulerHeight = 22, toolbarHeight = 40, footerHeight = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumGrid)
};
