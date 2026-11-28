#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  Plugin hosting (Step 11): VST3 instruments and effects from other companies.
//
//  - Scanning: each plugin file is checked in a separate copy of the app, so a
//    broken plugin can never crash the studio. A plugin that crashes or takes
//    longer than a minute is put aside as "failed" and skipped next time.
//  - The list of plugins found is kept by the engine between sessions.
//  - Plugins on tracks save their settings inside the song.
//==============================================================================
namespace PluginHost
{
    // Called first thing at start-up: in the helper copy of the app this scans
    // one plugin file, writes what it found and returns true (the app then quits).
    bool runScanChildIfRequested (const juce::String& commandLine);
    bool isScanChild (const juce::String& commandLine);

    // Plugins found by the last scan, sorted by name
    juce::Array<juce::PluginDescription> getInstruments (te::Engine&);
    juce::Array<juce::PluginDescription> getEffects (te::Engine&);

    // Folders searched (the standard VST3 folder plus any the user adds)
    juce::StringArray getDefaultFolders();
    juce::StringArray getExtraFolders();
    void setExtraFolders (const juce::StringArray&);

    // Plugins on tracks
    bool isPlugin (const te::Plugin*);                         // a VST3 plugin (not one of ours)
    juce::String describe (te::Plugin&);                       // "Keyscape", "Keyscape (missing)"
    void showWindow (te::Plugin&);                             // opens the plugin's own window
    void closeAllWindows (te::Edit&);

    std::unique_ptr<te::UIBehaviour> createUIBehaviour();

    // The Plugins window (scan, folders, list)
    void openManager (te::Engine&);
    void closeManager();
    std::unique_ptr<juce::Component> createManagerComponent (te::Engine&);
    inline std::function<void()> onOpenManagerRequest;     // set by the main window: show as a page

    //==========================================================================
    class Scanner final : private juce::Thread
    {
    public:
        explicit Scanner (te::Engine&);
        ~Scanner() override;

        // rescanAll: check every file again; retryFailed: try plugins that failed before
        void start (bool rescanAll, bool retryFailed);
        void stop();
        bool isScanning() const                           { return isThreadRunning(); }

        struct Progress { int done = 0, total = 0; juce::String current; juce::uint32 currentStarted = 0; };
        Progress getProgress() const;
        juce::StringArray getLastFailures() const;
        juce::String getLastSummary() const;

        std::function<void()> onFinished;                 // called on the message thread

        // For the developer test: scan one folder and wait (blocking)
        int scanFolderNow (const juce::File& folder);

        static constexpr int timeoutMs = 60000;

    private:
        void run() override;
        bool scanFile (const juce::String& file, juce::String& error);

        te::Engine& engine;
        bool rescanAll = false, retryFailed = false;
        mutable juce::CriticalSection lock;
        Progress progress;
        juce::StringArray failures;
        juce::String summary;
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    };

    Scanner& getScanner (te::Engine&);
    void releaseScanner();
}
