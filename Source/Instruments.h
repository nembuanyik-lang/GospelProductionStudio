#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <vector>

//==============================================================================
//  The built-in instruments a track can use, and where sound packs live.
//==============================================================================
namespace Instruments
{
    enum class Kind { sampler, organ, studioKeys };

    struct Choice
    {
        const char* id;          // stored in the project
        const char* name;        // shown to the user
        const char* group;       // menu grouping
        Kind kind;
        const char* pack;        // sound pack id (sampler only)
        const char* patch;       // SFZ file inside the pack ("" = the pack's first instrument)
    };

    const std::vector<Choice>& all();
    const Choice* find (const juce::String& id);
}

namespace SoundPacks
{
    juce::File getRootFolder();                                 // where packs are installed
    juce::File getPackFolder (const juce::String& packId);
    bool isInstalled (const juce::String& packId);
    juce::String installedVersion (const juce::String& packId);

    // The SFZ file for a pack and patch, or an invalid File when not installed.
    juce::File findInstrument (const juce::String& packId, const juce::String& patch);

    // Broadcasts when packs are installed or removed.
    juce::ChangeBroadcaster& changes();
}
