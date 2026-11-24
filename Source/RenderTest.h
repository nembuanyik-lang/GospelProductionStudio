#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
//  Developer check: "--render-test <folder>" renders a short demo of each
//  built-in instrument to WAV files, then quits. Not shown in the app.
//==============================================================================
namespace RenderTest
{
    bool runIfRequested (const juce::String& commandLine);
}
