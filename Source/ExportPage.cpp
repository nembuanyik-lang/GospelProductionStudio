#include "ExportPage.h"
#include "ReleasePage.h"
#include "Mp3Encoder.h"
#include "MasterPlugin.h"
#include "StudioEngine.h"
#include "StudioLookAndFeel.h"

using namespace StudioColours;

namespace
{
    void styleLabel (juce::Label& l, const juce::String& text, bool heading = true)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (studioFont (heading ? 13.5f : 12.5f, heading));
        l.setColour (juce::Label::textColourId, heading ? StudioColours::text : StudioColours::textDim);
    }
}

ExportPage::ExportPage (te::Edit& e, const juce::String& title) : edit (e), songTitle (title)
{
    styleLabel (whatLabel, "What to export");
    styleLabel (formatLabel, "Format");
    styleLabel (rateLabel, "Sample rate");
    styleLabel (titleLabel, "Title");
    styleLabel (artistLabel, "Artist");
    styleLabel (folderLabel, "Save in");
    styleLabel (folderPath, {}, false);
    styleLabel (note, {}, false);
    styleLabel (status, {}, false);
    note.setJustificationType (juce::Justification::topLeft);
    status.setColour (juce::Label::textColourId, gold);
    for (auto* l : { &whatLabel, &formatLabel, &rateLabel, &titleLabel, &artistLabel, &folderLabel, &folderPath, &note, &status })
        addAndMakeVisible (l);

    for (auto* b : { &mixButton, &stemsButton, &loopButton })
    {
        b->setRadioGroupId (1401);
        b->onClick = [this] { updateEnabled(); };
        addAndMakeVisible (b);
    }
    const auto what = StudioEngine::getSetting ("exportWhat", 0);
    (what == 1 ? stemsButton : what == 2 ? loopButton : mixButton).setToggleState (true, juce::dontSendNotification);

    for (int f = 0; f < 4; ++f)
    {
        formatBox.addItem (Exporter::formatName ((Exporter::Format) f), f + 1);
        if (Exporter::isMp3 ((Exporter::Format) f) && ! Mp3Encoder::isAvailable())
            formatBox.setItemEnabled (f + 1, false);
    }
    auto format = StudioEngine::getSetting ("exportFormat", Mp3Encoder::isAvailable() ? 0 : 2);
    if (! Mp3Encoder::isAvailable() && format < 2) format = 2;
    formatBox.setSelectedId (format + 1, juce::dontSendNotification);
    formatBox.onChange = [this] { updateEnabled(); };
    addAndMakeVisible (formatBox);

    rateBox.addItem ("44.1 kHz (music, streaming, CD)", 1);
    rateBox.addItem ("48 kHz (video)", 2);
    rateBox.setSelectedId (StudioEngine::getSetting ("exportRate", 1), juce::dontSendNotification);
    addAndMakeVisible (rateBox);

    loudButton.setToggleState (StudioEngine::getSetting ("exportLoud", 1) != 0, juce::dontSendNotification);
    fxButton.setToggleState (StudioEngine::getSetting ("exportStemFx", 1) != 0, juce::dontSendNotification);
    addAndMakeVisible (loudButton);
    addAndMakeVisible (fxButton);

    for (auto* ed : { &titleEditor, &artistEditor })
    {
        ed->setFont (studioFont (14.0f));
        ed->setColour (juce::TextEditor::backgroundColourId, card);
        ed->setColour (juce::TextEditor::outlineColourId, cardBorder);
        ed->setColour (juce::TextEditor::textColourId, StudioColours::text);
        addAndMakeVisible (ed);
    }
    titleEditor.setText (songTitle, false);
    artistEditor.setText (StudioEngine::getTextSetting ("exportArtist", {}), false);
    artistEditor.setTextToShowWhenEmpty ("Your name or your choir's", textDim);

    folderButton.onClick = [this] { chooseFolder(); };
    openButton.onClick = [this]
    {
        auto f = folder();
        f.createDirectory();
        f.revealToUser();
    };
    exportButton.onClick = [this] { startExport(); };
    stopButton.onClick = [this] { if (job != nullptr) job->cancel(); };
    exportButton.setColour (juce::TextButton::buttonColourId, gold);
    exportButton.setColour (juce::TextButton::textColourOffId, header);
    releaseButton.setTooltip ("Everything DistroKid, TuneCore, Boomplay, Audiomack... ask for: details, cover, lyrics, master, checked");
    releaseButton.onClick = [] { if (ReleasePage::onOpenRequest) juce::MessageManager::callAsync ([] { if (ReleasePage::onOpenRequest) ReleasePage::onOpenRequest(); }); };
    for (auto* b : { &folderButton, &openButton, &exportButton, &stopButton, &releaseButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    progressBar.setColour (juce::ProgressBar::foregroundColourId, gold);
    progressBar.setColour (juce::ProgressBar::backgroundColourId, card);
    addChildComponent (progressBar);

    const auto saved = StudioEngine::getTextSetting ("exportFolderRoot", {});
    if (saved.isNotEmpty())
        customFolder = juce::File (saved).getChildFile (juce::File::createLegalFileName (songTitle));

    updateEnabled();
}

ExportPage::~ExportPage()
{
    job.reset();
}

juce::File ExportPage::folder() const
{
    return customFolder != juce::File() ? customFolder : Exporter::defaultFolder (songTitle);
}

void ExportPage::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose where to save the exports", folder().getParentDirectory());
    juce::Component::SafePointer<ExportPage> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || ! fc.getResult().isDirectory())
            return;
        StudioEngine::setTextSetting ("exportFolderRoot", fc.getResult().getFullPathName());
        safe->customFolder = fc.getResult().getChildFile (juce::File::createLegalFileName (safe->songTitle));
        safe->updateEnabled();
    });
}

void ExportPage::updateEnabled()
{
    const auto busy = job != nullptr && job->isRunning();
    const auto stems = stemsButton.getToggleState();
    const auto mix = mixButton.getToggleState();

    const auto mastered = MasterPlugin::isActive (edit);
    loudButton.setEnabled (! busy && ! stems && ! mastered);
    loudButton.setButtonText (mastered ? "Loudness is set by Mastering (it is on for this song)"
                                       : "Ready to share: even loudness like streaming services (-14 LUFS), peaks under -1 dB");
    fxButton.setEnabled (! busy && stems);
    for (juce::Component* c : { (juce::Component*) &mixButton, (juce::Component*) &stemsButton, (juce::Component*) &loopButton,
                                (juce::Component*) &formatBox, (juce::Component*) &rateBox, (juce::Component*) &titleEditor,
                                (juce::Component*) &artistEditor, (juce::Component*) &folderButton, (juce::Component*) &exportButton })
        c->setEnabled (! busy);
    stopButton.setVisible (busy);
    progressBar.setVisible (busy);

    folderPath.setText (folder().getFullPathName(), juce::dontSendNotification);

    juce::String n;
    const auto format = (Exporter::Format) (formatBox.getSelectedId() - 1);
    if (stems)
    {
        const auto count = Exporter::stemTracks (edit).size();
        n << count << (count == 1 ? " track" : " tracks") << " will be exported, each starting at the same moment so they line up. "
          << "Muted and empty tracks are left out. ";
        if (Exporter::isMp3 (format))
            n << "For another studio, WAV 24-bit is better than MP3.";
    }
    else if (mix)
    {
        const auto len = Exporter::songRange (edit).getLength().inSeconds();
        n << "From the start of the song to the end of the last clip (" << (int) len / 60 << ":"
          << juce::String ((int) len % 60).paddedLeft ('0', 2) << "), plus the reverb's tail. Master effects are included.";
        if (! mastered)
            n << " Tip: Mastering (in the Mixer, on the Master) gives the song a final polish before you export.";
    }
    else
    {
        n << "Only the part between the loop markers. Good for sending one verse or chorus.";
    }
    if (! Mp3Encoder::isAvailable())
        n << "\nMP3 is not in this build yet, so WAV is used.";
    note.setText (n, juce::dontSendNotification);
}

Exporter::Options ExportPage::gatherOptions() const
{
    Exporter::Options o;
    o.what = stemsButton.getToggleState() ? Exporter::What::stems
           : loopButton.getToggleState()  ? Exporter::What::loop : Exporter::What::mix;
    o.format = (Exporter::Format) (formatBox.getSelectedId() - 1);
    o.sampleRate = rateBox.getSelectedId() == 2 ? 48000.0 : 44100.0;
    o.loudForSharing = loudButton.getToggleState();
    o.stemsWithSharedFx = fxButton.getToggleState();
    o.title = titleEditor.getText().trim();
    o.artist = artistEditor.getText().trim();
    o.folder = folder();
    return o;
}

void ExportPage::startExport()
{
    const auto o = gatherOptions();
    StudioEngine::setSetting ("exportWhat", (int) o.what);
    StudioEngine::setSetting ("exportFormat", (int) o.format);
    StudioEngine::setSetting ("exportRate", rateBox.getSelectedId());
    StudioEngine::setSetting ("exportLoud", o.loudForSharing ? 1 : 0);
    StudioEngine::setSetting ("exportStemFx", o.stemsWithSharedFx ? 1 : 0);
    StudioEngine::setTextSetting ("exportArtist", o.artist);

    juce::Component::SafePointer<ExportPage> safe (this);
    Exporter::Job::Callbacks cb;
    cb.progress = [safe] (const juce::String& text, float p)
    {
        if (safe == nullptr) return;
        safe->progress = p;
        safe->status.setText (text + "...", juce::dontSendNotification);
    };
    cb.finished = [safe] (juce::Result r, const juce::Array<juce::File>& files, const juce::StringArray& warnings)
    {
        if (safe == nullptr) return;
        safe->progress = 1.0;
        if (r.wasOk())
        {
            juce::String text = "Done! ";
            if (files.size() == 1) text << files[0].getFileName() << "  (" << juce::File::descriptionOfSizeInBytes (files[0].getSize()) << ")";
            else                   text << files.size() << " files saved.";
            safe->status.setText (text + "  Press \"Open folder\" to see it.", juce::dontSendNotification);
            safe->status.setColour (juce::Label::textColourId, good);
            if (! warnings.isEmpty())
            {
                safe->status.setColour (juce::Label::textColourId, warn);
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Exported, but check the levels",
                                                        warnings.joinIntoString ("\n\n"));
            }
        }
        else
        {
            safe->status.setText (r.getErrorMessage(), juce::dontSendNotification);
            safe->status.setColour (juce::Label::textColourId, warn);
        }
        // The job is finished; release it after this callback returns
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->job.reset(); safe->updateEnabled(); } });
    };

    job = std::make_unique<Exporter::Job> (edit, o, cb);
    status.setColour (juce::Label::textColourId, gold);
    status.setText ("Starting...", juce::dontSendNotification);
    progress = 0.0;
    job->start();
    updateEnabled();
}

void ExportPage::paint (juce::Graphics& g)
{
    g.setColour (card);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
}

void ExportPage::resized()
{
    auto r = getLocalBounds().reduced (20, 14);
    const auto compact = getHeight() < 470;
    const auto rowH = compact ? 24 : 28;

    auto twoCols = [&] (juce::Label& label, juce::Component& comp, int h)
    {
        auto row = r.removeFromTop (h);
        label.setBounds (row.removeFromLeft (110));
        comp.setBounds (row.removeFromLeft (juce::jmin (row.getWidth(), 520)));
        r.removeFromTop (compact ? 4 : 8);
    };

    whatLabel.setBounds (r.removeFromTop (20));
    for (auto* b : { &mixButton, &stemsButton, &loopButton })
        b->setBounds (r.removeFromTop (rowH).withTrimmedLeft (10));
    r.removeFromTop (compact ? 4 : 10);

    twoCols (formatLabel, formatBox, rowH);
    twoCols (rateLabel, rateBox, rowH);
    loudButton.setBounds (r.removeFromTop (rowH).withTrimmedLeft (10));
    fxButton.setBounds (r.removeFromTop (rowH).withTrimmedLeft (10));
    r.removeFromTop (compact ? 4 : 8);

    {
        auto row = r.removeFromTop (rowH);
        titleLabel.setBounds (row.removeFromLeft (110));
        titleEditor.setBounds (row.removeFromLeft (juce::jmin (260, row.getWidth() / 2 - 10)));
        row.removeFromLeft (20);
        artistLabel.setBounds (row.removeFromLeft (60));
        artistEditor.setBounds (row.removeFromLeft (juce::jmin (220, row.getWidth())));
        r.removeFromTop (compact ? 4 : 8);
    }
    {
        auto row = r.removeFromTop (rowH);
        folderLabel.setBounds (row.removeFromLeft (110));
        openButton.setBounds (row.removeFromRight (110));
        row.removeFromRight (8);
        folderButton.setBounds (row.removeFromRight (130));
        row.removeFromRight (8);
        folderPath.setBounds (row);
    }
    r.removeFromTop (compact ? 4 : 8);

    auto bottom = r.removeFromBottom (compact ? 34 : 40);
    exportButton.setBounds (bottom.removeFromLeft (140));
    bottom.removeFromLeft (10);
    stopButton.setBounds (bottom.removeFromLeft (80));
    releaseButton.setBounds (bottom.removeFromRight (juce::jmin (270, juce::jmax (0, bottom.getWidth() - 10))));
    bottom.removeFromLeft (10);
    progressBar.setBounds (bottom.removeFromLeft (juce::jmin (300, bottom.getWidth() / 2)).reduced (0, 6));
    bottom.removeFromLeft (10);
    status.setBounds (bottom);

    note.setBounds (r);
}
