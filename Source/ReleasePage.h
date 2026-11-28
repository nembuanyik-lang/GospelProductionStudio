#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Release.h"
#include "Exporter.h"

namespace te = tracktion;

//==============================================================================
//  The Release page (plan Step 18): song details for distributors, checked
//  the way the stores check them, then one folder with everything to upload.
//==============================================================================
class ReleasePage final : public juce::Component,
                          private juce::Timer
{
public:
    ReleasePage (te::Edit&, const Song&, std::function<void (const juce::var&)> saveDetails);
    ~ReleasePage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static inline std::function<void()> onOpenRequest;      // opens this page for the open song

private:
    class Form;
    class ChecksView;

    void timerCallback() override;
    void detailsChanged();
    void chooseCover();
    void makePackage();
    void finishPackage (const juce::File& folder, const juce::File& wav);
    juce::File releaseFolder() const;

    te::Edit& edit;
    Song song;
    std::function<void (const juce::var&)> saveDetails;
    Release::Info info;
    bool dirty = false;

    std::unique_ptr<Form> form;
    juce::Viewport formView;
    std::unique_ptr<ChecksView> checks;
    juce::Viewport checksView;

    juce::Image coverImage;
    juce::Rectangle<int> coverArea;
    juce::TextButton coverButton { "Choose cover art..." }, makeButton { "Make the release package" },
                     openButton { "Open the folder" };
    juce::Label checksTitle, status;
    double progress = 0.0;
    juce::ProgressBar progressBar { progress };

    std::unique_ptr<Exporter::Job> job;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastFolder;
    bool working = false;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReleasePage)
};
