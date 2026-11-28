#include "CompEditor.h"
#include "BackgroundWork.h"
#include "Takes.h"

namespace
{
    const juce::Colour takeColours[] = { juce::Colour (0xffd4a843), juce::Colour (0xff5b8def), juce::Colour (0xff4caf7d),
                                         juce::Colour (0xffe07a4f), juce::Colour (0xffb07ee0), juce::Colour (0xff4fb3c8) };
    juce::Colour colourFor (int i)    { return takeColours[(size_t) (i % 6)]; }
}

CompEditor::CompEditor (StudioEngine& e, TimelineView& t) : engine (e), timeline (t)
{
    snapBox.addItemList ({ "Snap: beat", "Snap: half beat", "Snap: bar", "Snap: off" }, 1);
    snapBox.setSelectedItemIndex (StudioEngine::getSetting ("compSnap", 1), juce::dontSendNotification);
    snapBox.onChange = [this] { StudioEngine::setSetting ("compSnap", snapBox.getSelectedItemIndex()); };
    fadeBox.addItemList ({ "Crossfade 5 ms", "Crossfade 10 ms", "Crossfade 20 ms", "Crossfade 40 ms" }, 1);
    fadeBox.setSelectedItemIndex (StudioEngine::getSetting ("compFade", 2), juce::dontSendNotification);
    fadeBox.onChange = [this] { StudioEngine::setSetting ("compFade", fadeBox.getSelectedItemIndex()); };
    fadeBox.setTooltip ("How smoothly one take blends into the next at each join");

    makeButton.setColour (juce::TextButton::buttonColourId, StudioColours::gold);
    makeButton.setColour (juce::TextButton::textColourOffId, StudioColours::header);
    makeButton.setTooltip ("Join the chosen parts into one recording (added as a new take; your takes stay as they are)");
    makeButton.onClick = [this] { makeComp(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };

    for (juce::Component* c : { (juce::Component*) &snapBox, (juce::Component*) &fadeBox,
                                (juce::Component*) &makeButton, (juce::Component*) &closeButton })
    {
        c->setWantsKeyboardFocus (false);
        addAndMakeVisible (c);
    }
}

CompEditor::~CompEditor()
{
    alive->store (false);
}

void CompEditor::setClip (te::WaveAudioClip* c)
{
    clip = c;
    clipRef = c;
    takes.clear();
    thumbs.clear();
    listenButtons.clear();
    status = {};

    if (clip == nullptr)
    {
        stopTimer();
        return;
    }

    const auto pos = clip->getPosition();
    fileStart = pos.getOffset().inSeconds();
    fileEnd = fileStart + pos.getLength().inSeconds() * clip->getSpeedRatio();

    takes = Comp::recordedTakes (*clip);
    const auto current = Takes::current (*clip);
    const auto defaultTake = std::find (takes.begin(), takes.end(), current) != takes.end() ? current : (takes.empty() ? 0 : takes.front());
    segments = Comp::load (*clip, fileStart, fileEnd, defaultTake);

    for (size_t i = 0; i < takes.size(); ++i)
    {
        const te::AudioFile file (clip->edit.engine, Takes::fileFor (*clip, takes[i]));
        thumbs.push_back (std::make_unique<te::SmartThumbnail> (clip->edit.engine, file, *this, nullptr));

        auto* b = listenButtons.add (new juce::TextButton ("Listen"));
        b->setWantsKeyboardFocus (false);
        b->setTooltip ("Play this take from the start of the clip");
        b->onClick = [this, row = (int) i] { listen (row); };
        addAndMakeVisible (b);
    }

    startTimerHz (15);
    resized();
    repaint();
}

//==============================================================================
float CompEditor::timeToX (double t) const
{
    return (float) lanes.getX() + (float) ((t - fileStart) / juce::jmax (0.001, fileEnd - fileStart)) * (float) lanes.getWidth();
}

double CompEditor::xToTime (float x) const
{
    return fileStart + (double) (x - (float) lanes.getX()) / juce::jmax (1, lanes.getWidth()) * (fileEnd - fileStart);
}

double CompEditor::snapTime (double t) const
{
    if (clip == nullptr || snapBox.getSelectedItemIndex() == 3)
        return t;

    // Snap on the song's beat grid
    const auto songTime = clip->getPosition().getStart() + te::TimeDuration::fromSeconds ((t - fileStart) / clip->getSpeedRatio());
    auto beat = timeline.timeToBeat (songTime);
    const auto grid = snapBox.getSelectedItemIndex() == 0 ? 1.0 : snapBox.getSelectedItemIndex() == 1 ? 0.5 : timeline.getBeatsPerBar();
    beat = std::round (beat / grid) * grid;
    const auto snapped = fileStart + (timeline.beatToTime (beat) - clip->getPosition().getStart()).inSeconds() * clip->getSpeedRatio();
    return juce::jlimit (fileStart, fileEnd, snapped);
}

juce::Rectangle<int> CompEditor::rowBounds (int row) const
{
    const auto h = takes.empty() ? 0 : juce::jmin (80, lanes.getHeight() / (int) takes.size());
    return { 0, lanes.getY() + row * h, getWidth(), h };
}

int CompEditor::rowAt (int y) const
{
    for (int i = 0; i < (int) takes.size(); ++i)
        if (rowBounds (i).contains (rowBounds (i).getX() + 1, y))
            return i;
    return -1;
}

//==============================================================================
void CompEditor::mouseDown (const juce::MouseEvent& e)
{
    dragRow = rowAt (e.y);
    if (dragRow < 0 || e.x < lanes.getX() || rendering)
        return;
    dragStart = dragNow = xToTime ((float) e.x);
    repaint();
}

void CompEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0)
        return;
    dragNow = juce::jlimit (fileStart, fileEnd, xToTime ((float) e.x));
    repaint();
}

void CompEditor::mouseUp (const juce::MouseEvent& e)
{
    if (dragRow < 0 || clip == nullptr)
        return;

    const auto take = takes[(size_t) dragRow];
    double a = juce::jmin (dragStart, dragNow), b = juce::jmax (dragStart, dragNow);

    if (e.getDistanceFromDragStart() < 4)
    {
        // A click uses this take for the whole part under the mouse
        for (const auto& s : segments)
            if (dragStart >= s.start && dragStart < s.end) { a = s.start; b = s.end; }
    }
    else
    {
        a = snapTime (a);
        b = snapTime (b);
    }

    clip->edit.getUndoManager().beginNewTransaction();
    Comp::assign (segments, a, b, take);
    Comp::save (*clip, segments);
    dragRow = -1;
    status = "Take " + juce::String (take + 1) + " chosen for " + juce::String (b - a, 2) + " s. Press Make comp to hear the result.";
    repaint();
}

void CompEditor::listen (int row)
{
    if (clip == nullptr || ! juce::isPositiveAndBelow (row, (int) takes.size()))
        return;
    Takes::select (*clip, takes[(size_t) row]);
    engine.setPositionSeconds (clip->getPosition().getStart().inSeconds());
    if (! engine.isPlaying())
        engine.togglePlay();
    status = "Playing take " + juce::String (takes[(size_t) row] + 1) + " (the clip now plays this take)";
    timeline.refreshAll();
    repaint();
}

void CompEditor::makeComp()
{
    if (clip == nullptr || rendering)
        return;

    std::vector<juce::File> files;
    for (int i = 0; i < Takes::count (*clip); ++i)
        files.push_back (Takes::fileFor (*clip, i));

    const auto first = Takes::fileFor (*clip, takes.empty() ? 0 : takes.front());
    const auto out = first.getParentDirectory().getNonexistentChildFile (clip->getName().replaceCharacters (" /\\:", "____") + "_Comp", ".wav", false);
    const double fades[] = { 0.005, 0.01, 0.02, 0.04 };
    const auto xf = fades[juce::jlimit (0, 3, fadeBox.getSelectedItemIndex())];
    const auto segs = segments;

    rendering = true;
    status = "Making the comp...";
    repaint();

    auto flag = alive;
    BackgroundWork::run ("Making the comp", [this, flag, files, segs, xf, out] (const std::atomic<bool>&)
    {
        const auto result = Comp::render (files, segs, xf, out);
        BackgroundWork::onMessageThread ([this, flag, result, out]
        {
            if (! flag->load())
                return;
            rendering = false;
            if (result.failed() || clip == nullptr)
            {
                status = "Could not make the comp: " + result.getErrorMessage();
                repaint();
                return;
            }
            clip->edit.getUndoManager().beginNewTransaction();
            Comp::addCompTake (*clip, out);
            status = "Comp made and playing in the song (it is the newest take; your takes are unchanged).";
            timeline.refreshAll();
            repaint();
        });
    });
}

void CompEditor::timerCallback()
{
    if (clip == nullptr || clip->getTrack() == nullptr || ! clip->state.getParent().isValid())
    {
        clip = nullptr;
        clipRef = nullptr;
        if (onClose) onClose();
        return;
    }
    repaint (lanes.withTrimmedTop (-compRowHeight - 24));     // playhead and waveforms as they load
}

//==============================================================================
void CompEditor::paint (juce::Graphics& g)
{
    using namespace StudioColours;
    g.setColour (card);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (cardBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);

    if (clip == nullptr)
        return;

    g.setColour (gold);
    g.setFont (studioFont (14.0f, true));
    g.drawText ("Comp  -  " + clip->getName(), toolbar.withTrimmedRight (520), juce::Justification::centredLeft, true);

    // Comp row: which take plays where
    g.setColour (textDim);
    g.setFont (studioFont (12.0f, true));
    g.drawText ("Comp", compRow.withWidth (labelWidth).reduced (10, 0), juce::Justification::centredLeft);
    for (const auto& s : segments)
    {
        auto r = juce::Rectangle<float> (timeToX (s.start), (float) compRow.getY() + 3.0f, timeToX (s.end) - timeToX (s.start), (float) compRow.getHeight() - 6.0f);
        const auto row = (int) (std::find (takes.begin(), takes.end(), s.take) - takes.begin());
        g.setColour (colourFor (row).withAlpha (0.75f));
        g.fillRoundedRectangle (r.reduced (1.0f, 0.0f), 3.0f);
        if (r.getWidth() > 26)
        {
            g.setColour (header);
            g.setFont (studioFont (11.0f, true));
            g.drawText ("T" + juce::String (s.take + 1), r.toNearestInt(), juce::Justification::centred);
        }
    }

    // Take rows with waveforms; the chosen parts are lit
    for (int i = 0; i < (int) takes.size(); ++i)
    {
        const auto row = rowBounds (i);
        const auto colour = colourFor (i);
        const auto wave = row.withLeft (lanes.getX()).withRight (lanes.getRight());
        g.setColour (i % 2 ? background : background.brighter (0.03f));
        g.fillRect (wave);

        for (const auto& s : segments)
            if (s.take == takes[(size_t) i])
            {
                g.setColour (colour.withAlpha (0.22f));
                g.fillRect (juce::Rectangle<float> (timeToX (s.start), (float) row.getY(), timeToX (s.end) - timeToX (s.start), (float) row.getHeight()));
            }

        g.setColour (colour.withAlpha (0.9f));
        thumbs[(size_t) i]->drawChannels (g, wave.reduced (0, 4),
                                          { te::TimePosition::fromSeconds (fileStart), te::TimePosition::fromSeconds (fileEnd) }, 1.0f);

        g.setColour (Takes::current (*clip) == takes[(size_t) i] ? gold : text);
        g.setFont (studioFont (13.0f, true));
        g.drawText ("Take " + juce::String (takes[(size_t) i] + 1), row.withWidth (labelWidth).reduced (10, 0).removeFromTop (row.getHeight() / 2),
                    juce::Justification::bottomLeft);
        g.setColour (cardBorder);
        g.drawHorizontalLine (row.getBottom() - 1, 0.0f, (float) getWidth());
    }

    // Drag selection
    if (dragRow >= 0)
    {
        const auto row = rowBounds (dragRow);
        const auto x1 = timeToX (juce::jmin (dragStart, dragNow)), x2 = timeToX (juce::jmax (dragStart, dragNow));
        g.setColour (text.withAlpha (0.2f));
        g.fillRect (juce::Rectangle<float> (x1, (float) row.getY(), juce::jmax (1.0f, x2 - x1), (float) row.getHeight()));
    }

    // Playhead
    const auto t = (engine.getEdit()->getTransport().getPosition() - clip->getPosition().getStart()).inSeconds() * clip->getSpeedRatio() + fileStart;
    if (t >= fileStart && t <= fileEnd)
    {
        g.setColour (gold);
        g.drawVerticalLine ((int) timeToX (t), (float) compRow.getY(), (float) lanes.getBottom());
    }

    g.setColour (textDim);
    g.setFont (studioFont (12.0f));
    g.drawText (status.isNotEmpty() ? status
                                    : juce::String ("Drag across a take to use it for that part. Click a part to give it to that take. Listen to each take with its button."),
                getLocalBounds().reduced (12, 6).removeFromBottom (18), juce::Justification::centredLeft, true);
}

void CompEditor::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    toolbar = area.removeFromTop (34);
    auto t = toolbar.withSizeKeepingCentre (toolbar.getWidth(), 30);
    closeButton.setBounds (t.removeFromRight (70));
    t.removeFromRight (8);
    makeButton.setBounds (t.removeFromRight (120));
    t.removeFromRight (8);
    fadeBox.setBounds (t.removeFromRight (150));
    t.removeFromRight (6);
    snapBox.setBounds (t.removeFromRight (140));

    area.removeFromTop (6);
    area.removeFromBottom (22);
    compRow = area.removeFromTop (compRowHeight).withTrimmedLeft (0);
    area.removeFromTop (4);
    lanes = area.withTrimmedLeft (labelWidth);
    compRow = compRow.withLeft (0);

    for (int i = 0; i < listenButtons.size(); ++i)
    {
        const auto row = rowBounds (i);
        listenButtons[i]->setBounds (juce::Rectangle<int> (10, row.getCentreY() + 2, 72, juce::jmin (24, row.getHeight() / 2 - 4)));
    }
}
