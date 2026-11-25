#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Mixing (Step 12): shared reverb and delay, sends, built-in effects.
//
//  The shared reverb and delay live on two hidden "return" tracks at the end
//  of the song. Each track sends some of its sound there (after its fader), so
//  the whole band sits in one room, which sounds more natural and costs much
//  less computer power than a reverb on every track.
//==============================================================================
namespace Mixing
{
    enum Bus { reverbBus = 0, delayBus = 1, numBuses = 2 };
    juce::String busName (int bus);                      // "Reverb", "Delay"

    // Song tracks only (without the hidden return tracks)
    juce::Array<te::AudioTrack*> getSongTracks (te::Edit&);
    bool isReturnTrack (const te::AudioTrack&);
    int  getReturnBus (const te::AudioTrack&);             // -1 when not a return
    te::AudioTrack* findReturn (te::Edit&, int bus);
    te::AudioTrack* ensureReturn (te::Edit&, int bus);    // creates it the first time

    // When a song opens: the shared reverb and delay and the master meter are ready
    void prepareSong (te::Edit&);

    // Sends: -60 dB means off
    static constexpr float sendOffDb = -60.0f;
    float getSendDb (te::AudioTrack&, int bus);
    void  setSendDb (te::AudioTrack&, int bus, float db);

    // Which plugins are "the mixer" and not effects
    bool isMixerPlugin (te::Plugin&);

    // Built-in effects (no download needed)
    struct BuiltIn { const char* type; const char* name; const char* hint; };
    const std::vector<BuiltIn>& builtInEffects();
    te::Plugin::Ptr createBuiltIn (te::Edit&, const juce::String& type);

    // Effects on the master (the whole song)
    std::vector<te::Plugin*> getMasterEffects (te::Edit&);
    te::Plugin* addMasterEffect (te::Edit&, te::Plugin::Ptr);

    // A simple control panel for built-in effects (they have no window of their own)
    std::unique_ptr<te::Plugin::EditorComponent> createGenericEditor (te::Plugin&);
}
