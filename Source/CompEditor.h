#pragma once

#include "Timeline.h"
#include "AudioEdit.h"

//==============================================================================
//  Comp editor: the takes of one recording stacked; drag across a take to use
//  it for that part. "Make comp" renders one smooth performance, added as a
//  new take. The original takes are never changed.
//==============================================================================
class CompEditor final : public juce::Component,
                         private juce::Timer
{
public:
    CompEditor (StudioEngine&, TimelineView&);
    ~CompEditor() override;

    void setClip (te::WaveAudioClip*);
    bool isOpen() const                         { return clip != nullptr; }
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float timeToX (double fileTime) const;
    double xToTime (float x) const;
    double snapTime (double fileTime) const;
    int rowAt (int y) const;
    juce::Rectangle<int> rowBounds (int row) const;
    void listen (int row);
    void makeComp();

    StudioEngine& engine;
    TimelineView& timeline;
    te::Clip::Ptr clipRef;
    te::WaveAudioClip* clip = nullptr;

    std::vector<int> takes;                             // take indexes shown as rows
    std::vector<std::unique_ptr<te::SmartThumbnail>> thumbs;
    juce::OwnedArray<juce::TextButton> listenButtons;
    std::vector<Comp::Segment> segments;
    double fileStart = 0.0, fileEnd = 1.0;

    juce::ComboBox snapBox, fadeBox;
    juce::TextButton makeButton { "Make comp" }, closeButton { "Close" };
    juce::Rectangle<int> toolbar, lanes, compRow;
    int dragRow = -1;
    double dragStart = 0.0, dragNow = 0.0;
    bool rendering = false;
    juce::String status;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    static constexpr int labelWidth = 130, compRowHeight = 28;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompEditor)
};
