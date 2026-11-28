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

    // Song tracks only (without the hidden return tracks and the group buses)
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

    // Presets of the built-in effects, by name ("Lead vocal: gentle")
    juce::StringArray presetNames (const juce::String& pluginType);
    bool applyPreset (te::Plugin&, const juce::String& presetName);

    // Vocal chains (Step 12): EQ, compressor, de-esser and the shared reverb and
    // delay, set up in one click for a kind of voice.
    struct VocalChain
    {
        const char* name;
        const char* hint;
        const char* eq;
        const char* compressor;
        const char* deEsser;
        float reverbDb, delayDb;        // sends to the shared reverb and delay (sendOffDb = off)
    };
    const std::vector<VocalChain>& vocalChains();
    bool applyVocalChain (te::AudioTrack&, int index);     // replaces an earlier chain on this track
    void removeVocalChain (te::AudioTrack&);
    juce::String currentVocalChain (te::AudioTrack&);        // its name, or empty

    // Group buses (plan Step 14): several tracks play into one bus (a "Choir bus"),
    // which has its own effects and fader, then goes to the master.
    struct BusKind { const char* name; juce::uint32 colour; const char* eq; const char* compressor; const char* hint; };
    const std::vector<BusKind>& busKinds();
    bool isGroupBus (const te::AudioTrack&);
    juce::Array<te::AudioTrack*> getGroupBuses (te::Edit&);
    te::AudioTrack* createGroupBus (te::Edit&, int kindIndex);
    void deleteGroupBus (te::AudioTrack& bus);                 // its tracks go back to the master
    void routeToBus (te::AudioTrack& track, te::AudioTrack* bus);   // nullptr = straight to the master
    te::AudioTrack* getBusOf (te::AudioTrack&);
    juce::Array<te::AudioTrack*> getBusMembers (te::AudioTrack& bus);

    // Sidechain ducking (plan Step 14): this track gets quieter whenever another one plays
    // (the kick pushes the bass down; the music dips under the preacher's voice).
    struct DuckKind { const char* name; const char* hint; float thresholdGain, ratio, attackMs, releaseMs; };
    const std::vector<DuckKind>& duckKinds();
    void setDucking (te::AudioTrack& track, te::AudioTrack* source, int kindIndex);   // source nullptr: off
    te::AudioTrack* getDuckingSource (te::AudioTrack&);
    int getDuckingKind (te::AudioTrack&);
    te::Plugin* findDucker (te::AudioTrack&);

    // A simple control panel for built-in effects (they have no window of their own)
    std::unique_ptr<te::Plugin::EditorComponent> createGenericEditor (te::Plugin&);
}
