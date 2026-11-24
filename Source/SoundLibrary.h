#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Instruments.h"

//==============================================================================
//  Sound Library: downloads each sound pack once (piano, bass, drums) from the
//  project's GitHub release "sounds-v1" and installs it on this computer.
//  App updates never download the packs again.
//==============================================================================
class SoundLibrary final : public juce::ChangeBroadcaster,
                           private juce::URL::DownloadTaskListener
{
public:
    SoundLibrary();
    ~SoundLibrary() override;

    static SoundLibrary* getInstance()              { return instance; }
    static void openWindow();                       // shows the Sound Library window

    struct Pack
    {
        juce::String id, name, category, version, description, licence, credit, file, sha256;
        juce::int64 size = 0, installedSize = 0;
    };

    enum class State { notInstalled, downloading, installing, installed, updateAvailable, failed };

    struct Status
    {
        State state = State::notInstalled;
        float progress = 0.0f;
        juce::String message;
    };

    // Catalogue from GitHub
    void refresh();
    bool isLoading() const                          { return loading; }
    juce::String getCatalogueError() const          { return catalogueError; }
    const std::vector<Pack>& getPacks() const       { return packs; }
    juce::String getBaseUrl() const;

    Status getStatus (const Pack&) const;
    void download (const Pack&);
    void cancel (const juce::String& id);
    void remove (const juce::String& id);

    static juce::String formatSize (juce::int64 bytes);

private:
    void progress (juce::URL::DownloadTask*, juce::int64 downloaded, juce::int64 total) override;
    void finished (juce::URL::DownloadTask*, bool success) override;
    void install (const Pack&, const juce::File& zip);
    void setStatus (const juce::String& id, State, float progress = 0.0f, const juce::String& message = {});

    std::vector<Pack> packs;
    std::map<juce::String, Status> status;
    std::map<juce::String, std::unique_ptr<juce::URL::DownloadTask>> tasks;
    bool loading = false;
    juce::String catalogueError;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    juce::ThreadPool worker { juce::ThreadPoolOptions{}.withNumberOfThreads (1).withThreadName ("Sound packs") };
    std::unique_ptr<juce::DocumentWindow> window;

    static SoundLibrary* instance;
    friend class SoundLibraryComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundLibrary)
};
