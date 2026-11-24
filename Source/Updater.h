#pragma once

#include <juce_events/juce_events.h>

//==============================================================================
//  Automatic updates from GitHub Releases.
//
//  The GitHub build bakes in the repository name and the build number.
//  The app asks GitHub for the latest release; if it is newer, it downloads
//  the Setup .exe, runs it silently and the new version opens by itself.
//==============================================================================
class Updater final : private juce::URL::DownloadTaskListener
{
public:
    struct Release
    {
        juce::String tag, version, notes, downloadUrl;
        int build = 0;
        juce::int64 size = 0;
    };

    enum class Result { updateAvailable, upToDate, failed };

    Updater();
    ~Updater() override;

    static juce::String getCurrentVersion();
    static int getCurrentBuild();
    static juce::String getRepository();
    static juce::String describeCurrent();

    /** Checks GitHub in the background; the callback runs on the message thread. */
    void check (std::function<void (Result, const Release&, const juce::String& message)> callback);

    /** Downloads and installs; progress is 0..1. The app closes when the installer starts. */
    void downloadAndInstall (const Release&,
                             std::function<void (float progress)> onProgress,
                             std::function<void (bool ok, const juce::String& message)> onDone);

    bool isBusy() const     { return busy; }

private:
    void progress (juce::URL::DownloadTask*, juce::int64 downloaded, juce::int64 total) override;
    void finished (juce::URL::DownloadTask*, bool success) override;

    static bool isNewer (const juce::String& version, int build);

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::unique_ptr<juce::URL::DownloadTask> download;
    std::function<void (float)> progressCallback;
    std::function<void (bool, const juce::String&)> doneCallback;
    juce::File installerFile;
    bool busy = false;
};
