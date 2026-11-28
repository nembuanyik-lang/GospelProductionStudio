#include "SoundLibrary.h"
#include "StudioLookAndFeel.h"
#include "Sfz.h"
#include <juce_cryptography/juce_cryptography.h>

#ifndef GPS_UPDATE_REPO
 #define GPS_UPDATE_REPO "nembuanyik-lang/GospelProductionStudio"
#endif

SoundLibrary* SoundLibrary::instance = nullptr;

//==============================================================================
SoundLibrary::SoundLibrary()
{
    instance = this;
}

SoundLibrary::~SoundLibrary()
{
    alive->store (false);
    window = nullptr;
    tasks.clear();
    worker.removeAllJobs (true, 20000);
    instance = nullptr;
}

juce::String SoundLibrary::getBaseUrl() const
{
    // GPS_SOUNDS_URL lets a developer test with a local server.
    const auto custom = juce::SystemStats::getEnvironmentVariable ("GPS_SOUNDS_URL", {});
    if (custom.isNotEmpty())
        return custom.endsWithChar ('/') ? custom : custom + "/";
    return "https://github.com/" + juce::String (GPS_UPDATE_REPO) + "/releases/download/sounds-v1/";
}

juce::String SoundLibrary::formatSize (juce::int64 bytes)
{
    if (bytes <= 0)
        return "nothing yet";
    if (bytes >= 1024 * 1024)
        return juce::String (bytes / (1024.0 * 1024.0), bytes >= 100 * 1024 * 1024 ? 0 : 1) + " MB";
    return juce::String (juce::jmax ((juce::int64) 1, bytes / 1024)) + " KB";
}

void SoundLibrary::refresh()
{
    if (loading)
        return;

    loading = true;
    catalogueError = {};
    sendChangeMessage();

    auto flag = alive;
    const auto url = getBaseUrl() + "packs.json";

    worker.addJob ([this, flag, url]
    {
        juce::String text;
        int statusCode = 0;
        if (auto stream = juce::URL (url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                 .withConnectionTimeoutMs (15000)
                                                                 .withNumRedirectsToFollow (8)
                                                                 .withStatusCode (&statusCode)))
            text = stream->readEntireStreamAsString();

        juce::MessageManager::callAsync ([this, flag, text, statusCode]
        {
            if (! flag->load())
                return;

            loading = false;
            const auto json = juce::JSON::parse (text);
            auto* list = json["packs"].getArray();

            if (list == nullptr)
            {
                catalogueError = statusCode == 404
                    ? juce::String ("The sound packs have not been published yet. On GitHub, run the workflow \"Build sound packs\" once.")
                    : juce::String ("Could not reach the Sound Library on GitHub. Check the internet connection and try again. "
                                    "(The GitHub repository must be public.)");
                sendChangeMessage();
                return;
            }

            packs.clear();
            for (auto& item : *list)
            {
                Pack p;
                p.id = item["id"].toString();
                p.name = item["name"].toString();
                p.category = item["category"].toString();
                p.version = item["version"].toString();
                p.description = item["description"].toString();
                p.licence = item["licence"].toString();
                p.credit = item["credit"].toString();
                p.file = item["file"].toString();
                p.sha256 = item["sha256"].toString();
                p.size = (juce::int64) item["size"];
                p.installedSize = (juce::int64) item["installedSize"];
                if (p.id.isNotEmpty() && p.file.isNotEmpty())
                    packs.push_back (p);
            }
            sendChangeMessage();
        });
    });
}

SoundLibrary::Status SoundLibrary::getStatus (const Pack& p) const
{
    auto it = status.find (p.id);
    if (it != status.end() && (it->second.state == State::downloading || it->second.state == State::installing
                                || it->second.state == State::failed))
        return it->second;

    if (SoundPacks::isInstalled (p.id))
    {
        Status s;
        s.state = SoundPacks::installedVersion (p.id) == p.version ? State::installed : State::updateAvailable;
        return s;
    }
    return {};
}

void SoundLibrary::setStatus (const juce::String& id, State state, float progress, const juce::String& message)
{
    status[id] = { state, progress, message };

    // Listed as busy while downloading or installing, so closing the app asks first (Step 24)
    if (state == State::downloading || state == State::installing)
    {
        if (busyTokens.find (id) == busyTokens.end())
        {
            auto flag = alive;
            busyTokens[id] = BackgroundWork::busy ("Downloading a sound pack", [this, flag, id]
            {
                if (flag->load()) cancel (id);
            });
        }
    }
    else
        busyTokens.erase (id);

    sendChangeMessage();
}

void SoundLibrary::download (const Pack& p)
{
    if (tasks.count (p.id) > 0)
        return;

    auto root = SoundPacks::getRootFolder();
    root.createDirectory();
    const auto target = root.getChildFile (p.id + ".zip.part");
    target.deleteFile();

    setStatus (p.id, State::downloading, 0.0f);
    auto task = juce::URL (getBaseUrl() + p.file)
                    .downloadToFile (target, juce::URL::DownloadTaskOptions().withListener (this));
    if (task == nullptr)
    {
        setStatus (p.id, State::failed, 0.0f, "Could not start the download");
        return;
    }
    tasks[p.id] = std::move (task);
}

void SoundLibrary::cancel (const juce::String& id)
{
    tasks.erase (id);
    SoundPacks::getRootFolder().getChildFile (id + ".zip.part").deleteFile();
    status.erase (id);
    sendChangeMessage();
}

void SoundLibrary::remove (const juce::String& id)
{
    const auto folder = SoundPacks::getPackFolder (id);
    for (const auto& sfz : folder.findChildFiles (juce::File::findFiles, false, "*.sfz"))
        Sfz::Library::getInstance().forget (sfz);

    folder.deleteRecursively();
    status.erase (id);
    SoundPacks::changes().sendChangeMessage();
    sendChangeMessage();
}

void SoundLibrary::progress (juce::URL::DownloadTask* task, juce::int64 downloaded, juce::int64 total)
{
    auto flag = alive;
    juce::MessageManager::callAsync ([this, flag, task, downloaded, total]
    {
        if (! flag->load())
            return;
        for (auto& [id, t] : tasks)
            if (t.get() == task)
            {
                juce::int64 expected = total;
                for (auto& p : packs)
                    if (p.id == id && p.size > 0) expected = p.size;
                status[id] = { State::downloading, expected > 0 ? (float) downloaded / (float) expected : 0.0f, {} };
                sendChangeMessage();
            }
    });
}

void SoundLibrary::finished (juce::URL::DownloadTask* task, bool success)
{
    auto flag = alive;
    const auto statusCode = task->statusCode();
    juce::MessageManager::callAsync ([this, flag, task, success, statusCode]
    {
        if (! flag->load())
            return;

        for (auto it = tasks.begin(); it != tasks.end(); ++it)
        {
            if (it->second.get() != task)
                continue;

            const auto id = it->first;
            const auto zip = it->second->getTargetLocation();
            tasks.erase (it);

            const Pack* pack = nullptr;
            for (auto& p : packs)
                if (p.id == id) pack = &p;

            if (! success || pack == nullptr || (statusCode != 0 && statusCode >= 400))
            {
                zip.deleteFile();
                setStatus (id, State::failed, 0.0f, "Download failed. Check the connection and try again.");
                return;
            }
            install (*pack, zip);
            return;
        }
    });
}

void SoundLibrary::install (const Pack& pack, const juce::File& zip)
{
    setStatus (pack.id, State::installing, 1.0f);
    auto flag = alive;

    worker.addJob ([this, flag, pack, zip]
    {
        juce::String error;

        if (pack.size > 0 && zip.getSize() != pack.size)
            error = "The download was incomplete (" + formatSize (zip.getSize()) + " of " + formatSize (pack.size) + "). Please try again.";

        if (error.isEmpty() && pack.sha256.isNotEmpty())
        {
            juce::FileInputStream in (zip);
            if (juce::SHA256 (in).toHexString() != pack.sha256)
                error = "The download was damaged. Please try again.";
        }

        const auto folder = SoundPacks::getPackFolder (pack.id);
        const auto temp = folder.getSiblingFile (pack.id + ".installing");

        if (error.isEmpty())
        {
            temp.deleteRecursively();
            juce::ZipFile z (zip);
            const auto result = z.uncompressTo (temp, true);
            if (result.failed() || ! temp.getChildFile ("pack.json").existsAsFile())
                error = "Could not unpack the sound pack: " + result.getErrorMessage();
        }

        if (error.isEmpty())
        {
            folder.deleteRecursively();
            if (! temp.moveFileTo (folder))
                error = "Could not install into " + folder.getFullPathName();
        }

        zip.deleteFile();
        temp.deleteRecursively();

        juce::MessageManager::callAsync ([this, flag, id = pack.id, error]
        {
            if (! flag->load())
                return;
            if (error.isNotEmpty())
            {
                setStatus (id, State::failed, 0.0f, error);
                return;
            }
            for (const auto& sfz : SoundPacks::getPackFolder (id).findChildFiles (juce::File::findFiles, false, "*.sfz"))
                Sfz::Library::getInstance().forget (sfz);
            status.erase (id);
            SoundPacks::changes().sendChangeMessage();     // instruments using this pack load it now
            sendChangeMessage();
        });
    });
}

//==============================================================================
//  The window
//==============================================================================
class SoundLibraryComponent final : public juce::Component,
                                    private juce::ChangeListener
{
public:
    explicit SoundLibraryComponent (SoundLibrary& l) : library (l)
    {
        refreshButton.onClick = [this] { library.refresh(); };
        refreshButton.setWantsKeyboardFocus (false);
        addAndMakeVisible (refreshButton);

        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        library.addChangeListener (this);
        rebuild();
        if (library.getPacks().empty() && ! library.isLoading())
            library.refresh();
        setSize (720, 560);
    }

    ~SoundLibraryComponent() override
    {
        library.removeChangeListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace StudioColours;
        g.fillAll (background);

        auto area = getLocalBounds().reduced (20, 16);
        g.setColour (text);
        g.setFont (studioFont (20.0f, true));
        g.drawText ("Sound Library", area.removeFromTop (28), juce::Justification::centredLeft);

        g.setColour (textDim);
        g.setFont (studioFont (13.5f));
        g.drawFittedText ("Download each pack once. It stays on this computer and app updates never download it again. "
                          "The organ is built in and needs no download. Wi-Fi is best for the larger packs.",
                          area.removeFromTop (40), juce::Justification::topLeft, 2, 1.0f);

        juce::int64 installedBytes = 0;
        for (auto& p : library.getPacks())
            if (SoundPacks::isInstalled (p.id))
                installedBytes += p.installedSize;

        g.setColour (gold);
        g.setFont (studioFont (12.5f));
        g.drawText ("Installed on this computer: " + SoundLibrary::formatSize (installedBytes)
                        + juce::String::fromUTF8 ("   ·   Folder: ") + SoundPacks::getRootFolder().getFullPathName(),
                    getLocalBounds().reduced (20, 10).removeFromBottom (20), juce::Justification::centredLeft, true);

        if (library.isLoading() || library.getCatalogueError().isNotEmpty())
        {
            g.setColour (library.isLoading() ? textDim : warn);
            g.setFont (studioFont (14.0f));
            g.drawFittedText (library.isLoading() ? juce::String ("Getting the list of sound packs from GitHub...")
                                                  : library.getCatalogueError(),
                              viewport.getBounds().reduced (10), juce::Justification::centredTop, 4);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (20, 16);
        area.removeFromTop (28);
        refreshButton.setBounds (area.getRight() - 100, area.getY() - 30, 100, 28);
        area.removeFromTop (44);
        area.removeFromBottom (28);
        viewport.setBounds (area);
        layoutList();
    }

private:
    struct Row final : public juce::Component
    {
        Row (SoundLibraryComponent& o, const SoundLibrary::Pack& p) : owner (o), pack (p)
        {
            action.setWantsKeyboardFocus (false);
            second.setWantsKeyboardFocus (false);
            action.onClick = [this] { onAction(); };
            second.onClick = [this]
            {
                auto& lib = owner.library;
                const auto st = lib.getStatus (pack);
                if (st.state == SoundLibrary::State::updateAvailable || st.state == SoundLibrary::State::installed)
                    lib.remove (pack.id);
            };
            addAndMakeVisible (action);
            addChildComponent (second);
            refresh();
        }

        void onAction()
        {
            auto& lib = owner.library;
            const auto st = lib.getStatus (pack);
            switch (st.state)
            {
                case SoundLibrary::State::notInstalled:
                case SoundLibrary::State::failed:
                case SoundLibrary::State::updateAvailable: lib.download (pack); break;
                case SoundLibrary::State::downloading:     lib.cancel (pack.id); break;
                case SoundLibrary::State::installed:       lib.remove (pack.id); break;
                case SoundLibrary::State::installing:      break;
            }
        }

        void refresh()
        {
            const auto st = owner.library.getStatus (pack);
            const auto size = SoundLibrary::formatSize (pack.size);
            switch (st.state)
            {
                case SoundLibrary::State::notInstalled:    action.setButtonText ("Download " + size); break;
                case SoundLibrary::State::failed:          action.setButtonText ("Try again"); break;
                case SoundLibrary::State::updateAvailable: action.setButtonText ("Update " + size); break;
                case SoundLibrary::State::downloading:     action.setButtonText ("Cancel"); break;
                case SoundLibrary::State::installing:      action.setButtonText ("Installing..."); break;
                case SoundLibrary::State::installed:       action.setButtonText ("Remove"); break;
            }
            const auto primary = st.state == SoundLibrary::State::notInstalled || st.state == SoundLibrary::State::updateAvailable
                                 || st.state == SoundLibrary::State::failed;
            action.setColour (juce::TextButton::buttonColourId, primary ? StudioColours::gold : StudioColours::card.brighter (0.1f));
            action.setColour (juce::TextButton::textColourOffId, primary ? StudioColours::header : StudioColours::text);
            action.setEnabled (st.state != SoundLibrary::State::installing);
            second.setButtonText ("Remove");
            second.setVisible (st.state == SoundLibrary::State::updateAvailable);
            state = st;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            using namespace StudioColours;
            auto b = getLocalBounds().toFloat().reduced (0.0f, 4.0f);
            g.setColour (card);
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (state.state == SoundLibrary::State::installed ? good.withAlpha (0.6f) : cardBorder);
            g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

            auto area = getLocalBounds().reduced (14, 10).withTrimmedRight (170);
            g.setColour (text);
            g.setFont (studioFont (15.5f, true));
            auto top = area.removeFromTop (22);
            g.drawText (pack.name, top, juce::Justification::centredLeft, true);

            g.setColour (gold);
            g.setFont (studioFont (12.0f, true));
            juce::String tag = pack.category.toUpperCase();
            if (state.state == SoundLibrary::State::installed) tag << juce::String::fromUTF8 ("   ·   INSTALLED");
            if (state.state == SoundLibrary::State::updateAvailable) tag << juce::String::fromUTF8 ("   ·   NEW VERSION");
            g.drawText (tag, top.withTrimmedLeft (juce::GlyphArrangement::getStringWidthInt (studioFont (15.5f, true), pack.name) + 14),
                        juce::Justification::centredLeft, true);

            g.setColour (textDim);
            g.setFont (studioFont (13.0f));
            g.drawFittedText (pack.description, area.removeFromTop (36), juce::Justification::topLeft, 2);
            g.setFont (studioFont (11.5f));
            g.drawText (pack.credit + juce::String::fromUTF8 ("  ·  ") + pack.licence + juce::String::fromUTF8 ("  ·  ") + SoundLibrary::formatSize (pack.installedSize) + " on disk",
                        area.removeFromTop (16), juce::Justification::centredLeft, true);

            if (state.state == SoundLibrary::State::downloading || state.state == SoundLibrary::State::installing)
            {
                auto bar = getLocalBounds().reduced (14, 0).removeFromBottom (14).withTrimmedBottom (7).toFloat();
                g.setColour (header);
                g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (gold);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, state.progress)), 3.0f);
            }
            if (state.state == SoundLibrary::State::failed && state.message.isNotEmpty())
            {
                g.setColour (bad);
                g.setFont (studioFont (12.0f));
                g.drawText (state.message, getLocalBounds().reduced (14, 0).removeFromBottom (20), juce::Justification::centredLeft, true);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14, 0);
            action.setBounds (r.removeFromRight (150).withSizeKeepingCentre (150, 32));
            second.setBounds (action.getBounds().translated (0, 36).withHeight (26));
        }

        SoundLibraryComponent& owner;
        SoundLibrary::Pack pack;
        SoundLibrary::Status state;
        juce::TextButton action, second;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (rows.size() != (int) library.getPacks().size())
            rebuild();
        for (auto* r : rows)
            r->refresh();
        repaint();
    }

    void rebuild()
    {
        rows.clear();
        for (const auto& p : library.getPacks())
            list.addAndMakeVisible (rows.add (new Row (*this, p)));
        layoutList();
    }

    void layoutList()
    {
        const auto w = viewport.getMaximumVisibleWidth();
        int y = 0;
        for (auto* r : rows)
        {
            r->setBounds (0, y, w, 108);
            y += 112;
        }
        list.setSize (w, y);
    }

    SoundLibrary& library;
    juce::TextButton refreshButton { "Refresh" };
    juce::Viewport viewport;
    juce::Component list;
    juce::OwnedArray<Row> rows;
};

//==============================================================================
std::unique_ptr<juce::Component> SoundLibrary::createComponent()
{
    if (auto* lib = getInstance())
        return std::make_unique<SoundLibraryComponent> (*lib);
    return {};
}

void SoundLibrary::openWindow()
{
    auto* lib = getInstance();
    if (lib == nullptr)
        return;

    if (onOpenRequest != nullptr)
    {
        onOpenRequest();
        return;
    }

    if (lib->window != nullptr)
    {
        lib->window->toFront (true);
        return;
    }

    struct Window final : public juce::DocumentWindow
    {
        explicit Window (SoundLibrary& l)
            : juce::DocumentWindow ("Sound Library", StudioColours::background, juce::DocumentWindow::closeButton), library (l)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new SoundLibraryComponent (l), true);
            setResizable (true, false);
            setResizeLimits (600, 420, 1400, 1200);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            auto* lib = &library;
            juce::MessageManager::callAsync ([lib] { if (SoundLibrary::getInstance() == lib) lib->window = nullptr; });
        }

        SoundLibrary& library;
    };

    lib->window = std::make_unique<Window> (*lib);
}
