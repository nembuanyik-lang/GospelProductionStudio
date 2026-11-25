#pragma once

#include "Timeline.h"
#include "Automation.h"

//==============================================================================
//  The automation lane under a track (Step 13).
//  Click to add a point, drag points, double-click or right-click a point to
//  remove it. Hold Shift while dragging to move freely (no snapping).
//==============================================================================
class AutomationLane final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    AutomationLane (TimelineView&, te::AudioTrack&);

    te::AudioTrack& getTrack() const      { return track; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    static constexpr int height = 64;

private:
    te::AutomatableParameter* param() const;
    float valueToY (float) const;
    float yToValue (float) const;
    float timeToX (te::TimePosition) const;
    te::TimePosition xToTime (float x, bool snapToGrid) const;
    int pointAt (juce::Point<float>) const;
    void showMenu();

    TimelineView& owner;
    te::AudioTrack& track;
    int hoverPoint = -1, dragPoint = -1;
};
