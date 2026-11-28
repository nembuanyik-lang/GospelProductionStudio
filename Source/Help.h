#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
//  The help assistant (plan Step 22): type a question in your own words
//  ("how do I record my voice?", "comment enregistrer?") and get the steps.
//  Everything is inside the app, so it works without internet.
//==============================================================================
namespace Help
{
    struct Topic
    {
        juce::String question;          // shown in the list
        juce::String keywords;          // extra words people use (English and French)
        juce::String answer;            // the steps
        juce::String area;              // Songs, Recording, Chords, Band, Mixing, Release, Setup
    };

    const std::vector<Topic>& topics();

    // Best matches first (all topics, in a sensible order, when the question is empty)
    std::vector<int> search (const juce::String& question, int maxResults = 12);

    std::unique_ptr<juce::Component> createComponent();
}
