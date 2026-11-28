#include "ReleasePage.h"
#include "BackgroundWork.h"
#include "Mp3Encoder.h"
#include "MasterPlugin.h"
#include "StudioEngine.h"
#include "StudioLookAndFeel.h"

using namespace StudioColours;

//==============================================================================
//  The details form
//==============================================================================
class ReleasePage::Form final : public juce::Component
{
public:
    explicit Form (Release::Info& i, std::function<void()> changed) : info (i), onChanged (std::move (changed))
    {
        text ("Title", info.title, "The song's name, normal capitals, nothing else in it");
        text ("Version", info.version, "Live, Acoustic, Radio edit... or leave empty");
        text ("Primary artist", info.artist, "Your artist name, exactly as on your other releases");
        text ("Featuring", info.featured, "Other singers or artists on the song (comma between names)");
        text ("Songwriters", info.songwriters, "Real full names of who wrote the words and music (comma between names)");
        text ("Producer", info.producer, "Who produced the song");
        text ("Mixed and mastered by", info.engineer, "Who mixed and mastered it");
        text ("Label", info.label, "Your label, or leave empty (your artist name is used)");
        choice ("Sub-genre", info.subGenre, Release::subGenres(), false);
        choice ("Language of the words", info.language, Release::languages(), true);
        text ("Release date", info.releaseDate, "YYYY-MM-DD, best 3 to 4 weeks from now");
        text ("ISRC", info.isrc, "Leave empty: your distributor gives one. Or type yours, like CM-ABC-26-00001");
        text ("UPC", info.upc, "Leave empty: your distributor gives one");
        {
            auto* box = new juce::ComboBox();
            box->addItem ("Clean (no explicit words)", 1);
            box->addItem ("Explicit", 2);
            box->setSelectedId (info.explicitLyrics ? 2 : 1, juce::dontSendNotification);
            box->onChange = [this, box] { info.explicitLyrics = box->getSelectedId() == 2; onChanged(); };
            add ("Explicit", box);
        }
        text (juce::String::fromUTF8 ("\xe2\x84\x97 line (recording)"), info.pLine, juce::String::fromUTF8 ("Who owns the recording: \xe2\x84\x97 2026 Your Name"));
        text (juce::String::fromUTF8 ("\xc2\xa9 line (song)"), info.cLine, juce::String::fromUTF8 ("Who owns the song: \xc2\xa9 2026 Your Name"));
        text ("Preview start", info.clipStart, "Where the 30-second preview and TikTok clip begin (m:ss): the best part");
        {
            auto* e = new juce::TextEditor();
            e->setMultiLine (true, true);
            e->setReturnKeyStartsNewLine (true);
            e->setScrollbarsShown (true);
            e->setText (info.lyrics, false);
            e->setTooltip ("Taken from the song's lyrics. Plain words only, no chord names, a blank line between parts.");
            e->onTextChange = [this, e] { info.lyrics = e->getText(); onChanged(); };
            add ("Lyrics", e, 220);
        }
    }

    int idealHeight() const
    {
        int h = 0;
        for (auto& r : rows) h += r.height + 8;
        return h + 10;
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (textDim);
        g.setFont (studioFont (12.5f));
        for (auto& r : rows)
            g.drawText (r.label, r.labelArea, juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        int y = 4;
        for (auto& r : rows)
        {
            r.labelArea = { 0, y, 150, juce::jmin (r.height, 26) };
            r.comp->setBounds (156, y, getWidth() - 160, r.height);
            y += r.height + 8;
        }
    }

private:
    struct Row { juce::String label; std::unique_ptr<juce::Component> comp; int height; juce::Rectangle<int> labelArea; };

    void add (const juce::String& label, juce::Component* c, int height = 26)
    {
        addAndMakeVisible (c);
        rows.push_back ({ label, std::unique_ptr<juce::Component> (c), height, {} });
    }

    void text (const juce::String& label, juce::String& value, const juce::String& hint)
    {
        auto* e = new juce::TextEditor();
        e->setText (value, false);
        e->setTextToShowWhenEmpty (hint, textDim.withAlpha (0.6f));
        e->setTooltip (hint);
        e->onTextChange = [this, e, &value] { value = e->getText(); onChanged(); };
        add (label, e);
    }

    void choice (const juce::String& label, juce::String& value, const juce::StringArray& items, bool editable)
    {
        auto* box = new juce::ComboBox();
        box->addItemList (items, 1);
        box->setEditableText (editable);
        box->setText (value, juce::dontSendNotification);
        box->onChange = [this, box, &value] { value = box->getText(); onChanged(); };
        add (label, box);
    }

    Release::Info& info;
    std::function<void()> onChanged;
    std::vector<Row> rows;
};

//==============================================================================
//  The checks, one per line with a coloured mark
//==============================================================================
class ReleasePage::ChecksView final : public juce::Component
{
public:
    void set (std::vector<Release::Check> c)
    {
        // What needs fixing first: red, then orange, then the rest
        auto rank = [] (Release::Level l) { return l == Release::Level::problem ? 0 : l == Release::Level::warning ? 1 : l == Release::Level::info ? 3 : 2; };
        std::stable_sort (c.begin(), c.end(), [&] (const Release::Check& a, const Release::Check& b) { return rank (a.level) < rank (b.level); });
        list = std::move (c);
        setSize (getWidth(), juce::jmax (40, (int) list.size() * rowHeight + 6));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        int y = 2;
        for (auto& c : list)
        {
            const auto colour = c.level == Release::Level::ok ? good : c.level == Release::Level::info ? juce::Colour (0xff5fb0d8)
                              : c.level == Release::Level::warning ? warn : bad;
            const auto mark = c.level == Release::Level::ok ? juce::String::fromUTF8 ("\xe2\x9c\x93") : c.level == Release::Level::info ? juce::String ("i")
                            : c.level == Release::Level::warning ? juce::String ("!") : juce::String::fromUTF8 ("\xe2\x9c\x97");
            auto r = juce::Rectangle<int> (0, y, getWidth(), rowHeight);
            auto dot = r.removeFromLeft (22).withSizeKeepingCentre (16, 16).toFloat();
            g.setColour (colour);
            g.fillEllipse (dot);
            g.setColour (header);
            g.setFont (studioFont (11.0f, true));
            g.drawText (mark, dot, juce::Justification::centred);
            g.setColour (c.level == Release::Level::problem ? text : text.withAlpha (0.85f));
            g.setFont (studioFont (12.0f));
            g.drawFittedText (c.text, r.withTrimmedLeft (4), juce::Justification::centredLeft, 3, 1.0f);
            y += rowHeight;
        }
    }

    static constexpr int rowHeight = 40;

private:
    std::vector<Release::Check> list;
};

//==============================================================================
ReleasePage::ReleasePage (te::Edit& e, const Song& s, std::function<void (const juce::var&)> save)
    : edit (e), song (s), saveDetails (std::move (save))
{
    info = Release::Info::fromSong (song, StudioEngine::getTextSetting ("exportArtist", {}));

    form = std::make_unique<Form> (info, [this] { detailsChanged(); });
    formView.setViewedComponent (form.get(), false);
    formView.setScrollBarsShown (true, false);
    addAndMakeVisible (formView);

    checks = std::make_unique<ChecksView>();
    checksView.setViewedComponent (checks.get(), false);
    checksView.setScrollBarsShown (true, false);
    addAndMakeVisible (checksView);

    checksTitle.setText ("Ready for the stores?", juce::dontSendNotification);
    checksTitle.setFont (studioFont (15.0f, true));
    checksTitle.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (checksTitle);

    for (auto* b : { &coverButton, &makeButton, &openButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    coverButton.onClick = [this] { chooseCover(); };
    makeButton.onClick = [this] { makePackage(); };
    openButton.onClick = [this] { if (lastFolder.isDirectory()) lastFolder.revealToUser(); };
    openButton.setVisible (false);
    makeButton.setColour (juce::TextButton::buttonColourId, gold);
    makeButton.setColour (juce::TextButton::textColourOffId, header);

    status.setColour (juce::Label::textColourId, textDim);
    status.setFont (studioFont (12.5f));
    status.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (status);
    addChildComponent (progressBar);

    if (juce::File (info.coverPath).existsAsFile())
        coverImage = juce::ImageFileFormat::loadFrom (juce::File (info.coverPath));

    checks->set (Release::check (info));
    startTimer (800);
}

ReleasePage::~ReleasePage()
{
    alive->store (false);
    stopTimer();
    if (dirty && saveDetails)
        saveDetails (info.toVar());
}

juce::File ReleasePage::releaseFolder() const
{
    auto root = StudioEngine::getTextSetting ("exportFolderRoot", {});
    auto base = root.isNotEmpty() ? juce::File (root) : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Gospel Production Studio");
    return base.getChildFile ("Releases").getChildFile (juce::File::createLegalFileName (info.artist.trim() + " - " + info.displayTitle()));
}

void ReleasePage::detailsChanged()
{
    dirty = true;
}

void ReleasePage::timerCallback()
{
    if (! dirty)
        return;
    dirty = false;
    if (saveDetails)
        saveDetails (info.toVar());
    if (info.artist.trim().isNotEmpty())
        StudioEngine::setTextSetting ("exportArtist", info.artist.trim());     // the next song starts with it
    if (! working)
        checks->set (Release::check (info));
}

void ReleasePage::chooseCover()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the cover art (square, 3000 x 3000)",
                                                   juce::File::getSpecialLocation (juce::File::userPicturesDirectory), "*.jpg;*.jpeg;*.png");
    juce::Component::SafePointer<ReleasePage> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        safe->info.coverPath = fc.getResult().getFullPathName();
        safe->coverImage = juce::ImageFileFormat::loadFrom (fc.getResult());
        safe->dirty = true;
        safe->timerCallback();
        safe->repaint();
    });
}

void ReleasePage::makePackage()
{
    if (working)
        return;
    // Copyright lines from the artist and year when left empty
    if (info.pLine.trim().isEmpty() && info.artist.trim().isNotEmpty())
        info.pLine = juce::String::fromUTF8 ("\xe2\x84\x97 ") + info.year() + " " + info.artist.trim();
    if (info.cLine.trim().isEmpty() && info.artist.trim().isNotEmpty())
        info.cLine = juce::String::fromUTF8 ("\xc2\xa9 ") + info.year() + " " + info.artist.trim();
    dirty = true;
    timerCallback();

    // Only what would make the stores refuse it stops us here (the audio is checked after)
    std::vector<Release::Check> blocking;
    for (auto& c : Release::check (info))
        if (c.level == Release::Level::problem)
            blocking.push_back (c);
    if (! blocking.empty())
    {
        juce::StringArray lines;
        for (auto& c : blocking) lines.add (c.text);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Please fix these first",
                                                lines.joinIntoString ("\n\n"));
        return;
    }

    auto folder = releaseFolder();
    for (int n = 2; folder.exists(); ++n)
        folder = folder.getSiblingFile (juce::File::createLegalFileName (info.artist.trim() + " - " + info.displayTitle()) + " (" + juce::String (n) + ")");
    folder.createDirectory();

    Exporter::Options o;
    o.what = Exporter::What::mix;
    o.format = Exporter::Format::wav24;
    o.sampleRate = 44100.0;
    o.loudForSharing = ! MasterPlugin::isActive (edit);       // Mastering already sets the loudness
    o.title = info.displayTitle();
    o.artist = info.artist;
    o.folder = folder;

    juce::Component::SafePointer<ReleasePage> safe (this);
    Exporter::Job::Callbacks cb;
    cb.progress = [safe] (const juce::String& t, float p)
    {
        if (safe == nullptr) return;
        safe->progress = p * 0.7;
        safe->status.setText ("Master WAV: " + t + "...", juce::dontSendNotification);
    };
    cb.finished = [safe, folder] (juce::Result r, const juce::Array<juce::File>& files, const juce::StringArray&)
    {
        if (safe == nullptr) return;
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->job.reset(); });
        if (r.failed() || files.isEmpty())
        {
            safe->working = false;
            safe->makeButton.setEnabled (true);
            safe->progressBar.setVisible (false);
            safe->status.setColour (juce::Label::textColourId, warn);
            safe->status.setText (r.failed() ? r.getErrorMessage() : juce::String ("Nothing was written."), juce::dontSendNotification);
            return;
        }
        safe->finishPackage (folder, files[0]);
    };

    working = true;
    makeButton.setEnabled (false);
    openButton.setVisible (false);
    progress = 0.0;
    progressBar.setVisible (true);
    status.setColour (juce::Label::textColourId, gold);
    status.setText ("Starting...", juce::dontSendNotification);
    job = std::make_unique<Exporter::Job> (edit, o, cb);
    job->start();
}

void ReleasePage::finishPackage (const juce::File& folder, const juce::File& wav)
{
    status.setText ("Cover, lyrics, details and the MP3...", juce::dontSendNotification);
    progress = 0.72;

    const auto i = info;
    const auto s = song;
    auto flag = alive;
    juce::Component::SafePointer<ReleasePage> safe (this);
    BackgroundWork::run ("Making the release package", [i, s, folder, wav, flag, safe] (const std::atomic<bool>&)
    {
        juce::StringArray made { wav.getFileName() };
        juce::StringArray trouble;

        const auto master = Analysis::analyseFile (wav);
        const auto silence = Release::leadingSilenceSeconds (wav);

        // Cover
        const auto coverFile = folder.getChildFile ("Cover (3000x3000).jpg");
        juce::String problem;
        juce::MemoryBlock coverBytes;
        if (Release::writeCover (juce::File (i.coverPath), coverFile, problem))
        {
            made.add (coverFile.getFileName());
            coverFile.loadFileAsData (coverBytes);
        }
        else
            trouble.add ("Cover: " + problem);

        // Lyrics and details
        if (i.lyrics.trim().isNotEmpty())
        {
            folder.getChildFile ("Lyrics.txt").replaceWithText (i.lyrics.trim() + "\n", false, false, "\r\n");
            made.add ("Lyrics.txt");
        }
        folder.getChildFile ("Song details.txt").replaceWithText (Release::detailsText (i, master, s), false, false, "\r\n");
        folder.getChildFile ("Song details.json").replaceWithText (juce::JSON::toString (Release::detailsJson (i, master, s)));
        made.add ("Song details.txt");
        made.add ("Song details.json");

        // MP3 with everything inside
        if (Mp3Encoder::isAvailable())
        {
            Mp3Encoder::Tags tags;
            tags.title = i.displayTitle();
            tags.artist = i.featured.trim().isEmpty() ? i.artist : i.artist + " feat. " + i.featured;
            tags.album = i.displayTitle() + " - Single";
            tags.year = i.year();
            tags.genre = "Gospel";
            tags.composer = i.songwriters;
            tags.copyright = i.cLine;
            juce::String isrc;
            if (Release::checkIsrc (i.isrc, isrc)) tags.isrc = isrc;
            tags.publisher = i.label.isNotEmpty() ? i.label : i.artist;
            tags.track = "1/1";
            tags.coverArt = coverBytes;
            const auto mp3 = wav.withFileExtension (".mp3");
            const auto r = Mp3Encoder::encode (wav, mp3, 320, tags, [safe] (float p)
            {
                BackgroundWork::onMessageThread ([safe, p] { if (safe != nullptr) safe->progress = 0.75 + 0.25 * p; });
            });
            if (r.wasOk()) made.add (mp3.getFileName());
            else           trouble.add ("MP3: " + r.getErrorMessage());
        }

        BackgroundWork::onMessageThread ([flag, safe, made, trouble, folder, master, silence]
        {
            if (! flag->load() || safe == nullptr)
                return;
            auto& page = *safe;
            page.working = false;
            page.progress = 1.0;
            page.progressBar.setVisible (false);
            page.makeButton.setEnabled (true);
            page.lastFolder = folder;
            page.openButton.setVisible (true);
            const auto results = Release::check (page.info, &master, silence);
            page.checks->set (results);
            const bool fine = trouble.isEmpty() && ! Release::hasProblems (results);
            page.status.setColour (juce::Label::textColourId, fine ? good : warn);
            juce::String t;
            t << (fine ? "Ready to upload! " : "Made, but look at the red and orange lines. ") << "In " << folder.getFileName() << ":\n"
              << made.joinIntoString (",  ");
            if (! trouble.isEmpty())
                t << "\n" << trouble.joinIntoString ("\n");
            page.status.setText (t, juce::dontSendNotification);
            page.resized();
        });
    });
}

//==============================================================================
void ReleasePage::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.setColour (card);
    g.fillRoundedRectangle (formView.getBounds().expanded (8).toFloat(), 8.0f);
    auto right = coverArea.getUnion (checksView.getBounds()).getUnion (makeButton.getBounds()).getUnion (status.getBounds()).expanded (10);
    g.fillRoundedRectangle (right.toFloat(), 8.0f);

    // Cover preview
    g.setColour (header);
    g.fillRect (coverArea);
    if (coverImage.isValid())
        g.drawImageWithin (coverImage, coverArea.getX(), coverArea.getY(), coverArea.getWidth(), coverArea.getHeight(),
                           juce::RectanglePlacement::centred);
    else
    {
        g.setColour (textDim);
        g.setFont (studioFont (12.0f));
        g.drawFittedText ("No cover art yet\n3000 x 3000, square", coverArea.reduced (8), juce::Justification::centred, 3);
    }
}

void ReleasePage::resized()
{
    auto r = getLocalBounds().reduced (18, 14);
    auto right = r.removeFromRight (juce::jmin (460, r.getWidth() / 2));
    r.removeFromRight (24);

    formView.setBounds (r.reduced (4));
    form->setSize (formView.getWidth() - formView.getScrollBarThickness() - 4, form->idealHeight());

    right.reduce (4, 4);
    auto top = right.removeFromTop (130);
    coverArea = top.removeFromLeft (130);
    top.removeFromLeft (14);
    coverButton.setBounds (top.removeFromTop (30).withWidth (juce::jmin (200, top.getWidth())));
    top.removeFromTop (8);
    makeButton.setBounds (top.removeFromTop (34).withWidth (juce::jmin (240, top.getWidth())));
    top.removeFromTop (6);
    openButton.setBounds (top.removeFromTop (28).withWidth (juce::jmin (160, top.getWidth())));
    right.removeFromTop (10);

    auto bottom = right.removeFromBottom (status.getText().isEmpty() ? 40 : 86);
    progressBar.setBounds (bottom.removeFromTop (14));
    bottom.removeFromTop (4);
    status.setBounds (bottom);
    right.removeFromBottom (6);

    checksTitle.setBounds (right.removeFromTop (24));
    checksView.setBounds (right);
    checks->setSize (checksView.getWidth() - checksView.getScrollBarThickness() - 2, checks->getHeight());
}
