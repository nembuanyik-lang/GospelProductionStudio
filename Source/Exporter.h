#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "BackgroundWork.h"

namespace te = tracktion;

//==============================================================================
//  Bounce and export (Step 14).
//
//  The whole song (mix), each track on its own (stems) or the loop range,
//  as MP3 or WAV. Rendering runs in the background with progress and Cancel.
//==============================================================================
namespace Exporter
{
    enum class What   { mix = 0, stems, loop };
    enum class Format { mp3High = 0, mp3Small, wav16, wav24 };

    struct Options
    {
        What what = What::mix;
        Format format = Format::mp3High;
        double sampleRate = 44100.0;
        bool loudForSharing = true;        // mix: -14 LUFS (streaming level), peaks kept under -1 dB
        bool stemsWithSharedFx = true;     // stems: each track's share of the shared reverb and delay
        juce::String title, artist;
        juce::File folder;
    };

    juce::String formatName (Format);
    juce::String extension (Format);
    bool isMp3 (Format);

    te::TimeRange songRange (te::Edit&);                 // start of the song to the end of its last clip
    juce::File defaultFolder (const juce::String& songTitle);

    // Tracks that would become stems (not muted, with something on them)
    juce::Array<te::AudioTrack*> stemTracks (te::Edit&);

    //==========================================================================
    class Job final : private juce::Timer
    {
    public:
        struct Callbacks
        {
            std::function<void (const juce::String& status, float progress)> progress;
            std::function<void (juce::Result, const juce::Array<juce::File>& files, const juce::StringArray& warnings)> finished;
        };

        Job (te::Edit&, Options, Callbacks);
        ~Job() override;

        void start();
        void cancel();
        bool isRunning() const      { return running; }

    private:
        struct Step
        {
            juce::String label;
            juce::Array<te::Track*> tracks;      // empty: everything
            juce::File finalFile;
            bool isMix = true;
        };

        void startStep();
        void renderFinished (bool ok, const juce::String& error, juce::File rendered);
        void finishStep (juce::Result);
        void finishAll (juce::Result);
        void timerCallback() override;

        te::Edit& edit;
        Options options;
        Callbacks callbacks;
        std::vector<Step> steps;
        size_t current = 0;
        bool running = false;
        std::atomic<bool> cancelled { false };
        std::shared_ptr<te::EditRenderer::Handle> renderHandle;
        std::unique_ptr<std::thread> encoder;
        std::atomic<float> encodeProgress { 0.0f };
        juce::File tempFile;
        juce::Array<juce::File> written;
        juce::StringArray warnings;
        void checkForClipping (const juce::File&, const juce::String& label);
        te::TimeRange range;
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
        enum class Phase { idle, rendering, encoding } phase = Phase::idle;
        std::shared_ptr<BackgroundWork::Token> busyToken;       // listed as busy while it runs (Step 24)
    };
}
