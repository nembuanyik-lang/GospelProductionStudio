#pragma once

#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>

//==============================================================================
//  Background work, in one place (plan Step 24).
//
//  Everything the app does while you keep working (harmonies, stems, chord
//  finding, stretching recordings, exports, the release package, comps, sound
//  pack downloads) is listed here while it runs. Closing the app asks first
//  when something important is still working, can stop it all cleanly, and waits
//  for it before the app's windows and engine are taken down, so nothing is left
//  half-written and nothing calls back into a closed app.
//==============================================================================
namespace BackgroundWork
{
    // While a Token is alive, its work is listed as busy. 'stop' (optional) asks the work to stop early.
    struct Token
    {
        juce::String what;
        bool askBeforeClosing = true;
        std::function<void()> stop;
    };
    std::shared_ptr<Token> busy (const juce::String& what, std::function<void()> stop = {}, bool askBeforeClosing = true);

    // Runs 'work' on the app's background threads, listed as busy until it returns.
    // 'stopRequested' turns true when the user stops it or the app closes: check it often.
    using Work = std::function<void (const std::atomic<bool>& stopRequested)>;
    void run (const juce::String& what, Work work, bool askBeforeClosing = true);

    // Runs 'fn' on the message thread later, unless the app is closing by then.
    void onMessageThread (std::function<void()> fn);

    juce::StringArray whatIsBusy (bool onlyImportant = true);
    bool isIdle();
    void stopAll();

    // At the very end of the app: stop everything and wait (up to 'timeoutMs') for it to finish.
    void shutdown (int timeoutMs = 15000);
    bool isShuttingDown();
}
