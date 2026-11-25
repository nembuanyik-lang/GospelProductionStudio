#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Automation (Step 13): volume, pan, sends and effect settings that change
//  during the song. Each track can show one automation lane under it.
//
//  A parameter is named by a short key that is saved on the track:
//      "vol", "pan", "send0" (reverb), "send1" (delay),
//      "fx:<effect id>:<parameter id>" for an effect or plugin setting.
//==============================================================================
namespace Automation
{
    struct Choice
    {
        juce::String key, name, group;      // group: "" for the mixer, else the effect name
        bool hasPoints = false;
    };

    std::vector<Choice> choicesFor (te::AudioTrack&);

    // create: make a send (switched off) if the track has none yet
    te::AutomatableParameter* resolve (te::AudioTrack&, const juce::String& key, bool create = false);
    juce::String describe (te::AudioTrack&, const juce::String& key);   // "Volume", "EQ: Mid gain 1"

    // Which lane a track shows
    bool isShown (const te::AudioTrack&);
    juce::String getShownKey (const te::AudioTrack&);                  // "vol" by default
    void show (te::AudioTrack&, const juce::String& key);
    void hide (te::AudioTrack&);

    int countAutomated (te::AudioTrack&);                              // parameters with points

    // Recording: move faders and knobs while the song plays
    bool isWriting (te::Edit&);
    void setWriting (te::Edit&, bool);

    // Gestures (start and end of a fader move) so a recording knows when to stop
    void beginGesture (te::AutomatableParameter*);
    void endGesture (te::AutomatableParameter*);
}
