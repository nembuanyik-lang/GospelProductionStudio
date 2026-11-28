#pragma once

#include "Exporter.h"

//==============================================================================
//  The Export page (Step 14): what to export, the format, the loudness, the
//  folder; then a progress bar, Stop, and "Open folder" when done.
//==============================================================================
class ExportPage final : public juce::Component
{
public:
    ExportPage (te::Edit&, const juce::String& songTitle);
    ~ExportPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void updateEnabled();
    void startExport();
    void chooseFolder();
    juce::File folder() const;
    Exporter::Options gatherOptions() const;

    te::Edit& edit;
    juce::String songTitle;

    juce::Label whatLabel, formatLabel, rateLabel, titleLabel, artistLabel, folderLabel, folderPath, note, status;
    juce::ToggleButton mixButton { "The whole song (mix)" },
                       stemsButton { "Each track on its own (stems, for another studio or a mix engineer)" },
                       loopButton { "Only the loop range" };
    juce::ComboBox formatBox, rateBox;
    juce::ToggleButton loudButton { "Ready to share: even loudness like streaming services (-14 LUFS), peaks under -1 dB" },
                       fxButton { "Stems include each track's share of the shared reverb and delay" };
    juce::TextEditor titleEditor, artistEditor;
    juce::TextButton folderButton { "Change folder..." }, openButton { "Open folder" },
                     exportButton { "Export" }, stopButton { "Stop" },
                     releaseButton { "Release package for distributors..." };
    double progress = 0.0;
    juce::ProgressBar progressBar { progress };

    std::unique_ptr<Exporter::Job> job;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File customFolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportPage)
};
