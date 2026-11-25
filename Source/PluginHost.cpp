#include "PluginHost.h"
#include "StudioEngine.h"
#include "StudioLookAndFeel.h"
#include "Mixing.h"

namespace PluginHost
{
namespace
{
    const char* scanFlag = "--gps-scan-plugin";

    juce::StringArray splitCommandLine (const juce::String& commandLine)
    {
        juce::StringArray args;
        args.addTokens (commandLine, " ", "\"");
        args.trim();
        args.removeEmptyStrings();
        for (auto& a : args)
            a = a.unquoted();
        return args;
    }

    juce::AudioPluginFormat* findVst3 (te::Engine& engine)
    {
        for (auto* f : engine.getPluginManager().pluginFormatManager.getFormats())
            if (f->getName() == "VST3")
                return f;
        return nullptr;
    }

    juce::Array<juce::PluginDescription> collect (te::Engine& engine, bool instruments)
    {
        juce::Array<juce::PluginDescription> result;
        for (const auto& d : engine.getPluginManager().knownPluginList.getTypes())
            if (d.pluginFormatName == "VST3" && d.isInstrument == instruments)
                result.add (d);

        std::sort (result.begin(), result.end(), [] (const auto& a, const auto& b)
                   { return a.name.compareIgnoreCase (b.name) < 0; });
        return result;
    }
}

//==============================================================================
bool isScanChild (const juce::String& commandLine)
{
    return commandLine.contains (scanFlag);
}

bool runScanChildIfRequested (const juce::String& commandLine)
{
    const auto args = splitCommandLine (commandLine);
    const auto i = args.indexOf (scanFlag);
    if (i < 0)
        return false;

    if (i + 2 >= args.size())
        return true;

    const auto file = args[i + 1];
    const juce::File out (args[i + 2]);

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* f : formats.getFormats())
        if (f->getName() == "VST3" && f->fileMightContainThisPluginType (file))
            f->findAllTypesForFile (found, file);

    juce::XmlElement root ("PLUGINS");
    for (auto* d : found)
        root.addChildElement (d->createXml().release());
    root.setAttribute ("complete", 1);
    out.replaceWithText (root.toString());
    return true;
}

//==============================================================================
juce::Array<juce::PluginDescription> getInstruments (te::Engine& e)  { return collect (e, true); }
juce::Array<juce::PluginDescription> getEffects (te::Engine& e)      { return collect (e, false); }

juce::StringArray getDefaultFolders()
{
    juce::StringArray folders;
   #if JUCE_WINDOWS
    folders.add (juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory)
                    .getChildFile ("Common Files").getChildFile ("VST3").getFullPathName());
   #elif JUCE_MAC
    folders.add ("/Library/Audio/Plug-Ins/VST3");
    folders.add ("~/Library/Audio/Plug-Ins/VST3");
   #else
    folders.add (juce::File ("~/.vst3").getFullPathName());
    folders.add ("/usr/lib/vst3");
    folders.add ("/usr/local/lib/vst3");
   #endif
    return folders;
}

juce::StringArray getExtraFolders()
{
    juce::StringArray folders;
    folders.addTokens (StudioEngine::getTextSetting ("pluginFolders", {}), "\n", {});
    folders.removeEmptyStrings();
    return folders;
}

void setExtraFolders (const juce::StringArray& folders)
{
    StudioEngine::setTextSetting ("pluginFolders", folders.joinIntoString ("\n"));
}

//==============================================================================
bool isPlugin (const te::Plugin* p)
{
    return dynamic_cast<const te::ExternalPlugin*> (p) != nullptr;
}

juce::String describe (te::Plugin& p)
{
    auto* ep = dynamic_cast<te::ExternalPlugin*> (&p);
    if (ep == nullptr)
        return p.getName() + (p.isEnabled() ? juce::String() : juce::String (" (off)"));

    auto name = ep->getName();
    if (name.isEmpty())
        name = ep->state[te::IDs::name].toString();

    if (ep->isInitialisingAsync())
        return name + " (loading)";
    if (ep->getAudioPluginInstance() == nullptr)
        return name + " (not installed)";
    if (! ep->isEnabled())
        return name + " (off)";
    return name;
}

void showWindow (te::Plugin& p)
{
    if (auto* ep = dynamic_cast<te::ExternalPlugin*> (&p))
    {
        if (ep->isInitialisingAsync())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, ep->getName(),
                                                    "The plugin is still loading. Try again in a moment.");
            return;
        }
        if (ep->getAudioPluginInstance() == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, describe (p),
                "This plugin is not installed on this computer, or it could not start.\n\n"
                "Install it with its own installer, then open Plugins and press \"Scan for plugins\". "
                "Its settings are kept safely in the song until then.");
            return;
        }
    }

    if (p.windowState != nullptr)
        p.windowState->showWindowExplicitly();
}

void closeAllWindows (te::Edit& edit)
{
    for (auto* w : te::PluginWindowState::getAllWindows (edit))
        w->hideWindowForShutdown();
}

//==============================================================================
//  The plugin's own window
//==============================================================================
namespace
{
   #if JUCE_LINUX
    constexpr bool addWindowToDesktop = false;
   #else
    constexpr bool addWindowToDesktop = true;
   #endif

    class PluginWindow final : public juce::DocumentWindow
    {
    public:
        explicit PluginWindow (te::Plugin& p)
            : DocumentWindow (titleFor (p), juce::Colour (0xff15171c), DocumentWindow::closeButton, addWindowToDesktop),
              plugin (p)
        {
            getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
            setResizeLimits (100, 50, 4000, 4000);
            setUsingNativeTitleBar (true);
            recreateEditor();
            setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());

           #if JUCE_LINUX
            setAlwaysOnTop (true);
            addToDesktop();
           #endif
            updateStoredBounds = true;
        }

        ~PluginWindow() override
        {
            updateStoredBounds = false;
            plugin.edit.flushPluginStateIfNeeded (plugin);
            setEditor (nullptr);
        }

        static juce::String titleFor (te::Plugin& p)
        {
            auto title = p.getName();
            if (auto* t = p.getOwnerTrack())
                title = t->getName() + juce::String::fromUTF8 (" \xc2\xb7 ") + title;
            return title;
        }

        void recreateEditor()
        {
            setEditor (nullptr);
            auto ed = plugin.createEditor();
            if (ed == nullptr && ! isPlugin (&plugin))
                ed = Mixing::createGenericEditor (plugin);      // built-in effects
            setEditor (std::move (ed));
        }

        void recreateEditorAsync()
        {
            setEditor (nullptr);
            juce::Component::SafePointer<juce::Component> safe (this);
            juce::Timer::callAfterDelay (50, [this, safe] { if (safe != nullptr) recreateEditor(); });
        }

        te::Plugin::EditorComponent* getEditor() const   { return editor.get(); }

        void show()
        {
            setVisible (true);
            toFront (false);
            setBoundsConstrained (getBounds());
        }

    private:
        void setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor)
        {
            setConstrainer (nullptr);
            editor.reset();

            if (newEditor != nullptr)
            {
                editor = std::move (newEditor);
                setContentNonOwned (editor.get(), true);
            }

            setResizable (editor == nullptr || editor->allowWindowResizing(), false);
            if (editor != nullptr && editor->allowWindowResizing())
                setConstrainer (editor->getBoundsConstrainer());
        }

        void moved() override
        {
            if (updateStoredBounds)
            {
                plugin.windowState->lastWindowBounds = getBounds();
                plugin.edit.pluginChanged (plugin);
            }
        }

        void closeButtonPressed() override              { plugin.windowState->closeWindowExplicitly(); }
        float getDesktopScaleFactor() const override    { return 1.0f; }

        te::Plugin& plugin;
        std::unique_ptr<te::Plugin::EditorComponent> editor;
        bool updateStoredBounds = false;
    };

    class StudioUIBehaviour final : public te::UIBehaviour
    {
    public:
        std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState& state) override
        {
            auto* ws = dynamic_cast<te::Plugin::WindowState*> (&state);
            if (ws == nullptr)
                return {};

            auto& plugin = ws->plugin;
            if (auto* ep = dynamic_cast<te::ExternalPlugin*> (&plugin))
                if (ep->getAudioPluginInstance() == nullptr)
                    return {};

            auto w = std::make_unique<PluginWindow> (plugin);
            if (w->getEditor() == nullptr)
                return {};

            w->show();
            return w;
        }

        void recreatePluginWindowContentAsync (te::Plugin& p) override
        {
            if (auto* w = dynamic_cast<PluginWindow*> (p.windowState->pluginWindow.get()))
                return w->recreateEditorAsync();

            UIBehaviour::recreatePluginWindowContentAsync (p);
        }
    };
}

std::unique_ptr<te::UIBehaviour> createUIBehaviour()
{
    return std::make_unique<StudioUIBehaviour>();
}

//==============================================================================
//  Scanner
//==============================================================================
Scanner::Scanner (te::Engine& e) : juce::Thread ("Plugin scan"), engine (e) {}

Scanner::~Scanner()
{
    alive->store (false);
    stopThread (Scanner::timeoutMs + 5000);
}

void Scanner::start (bool all, bool retry)
{
    if (isThreadRunning())
        return;

    rescanAll = all;
    retryFailed = retry;
    {
        const juce::ScopedLock sl (lock);
        progress = {};
        failures.clear();
        summary = {};
    }
    startThread();
}

void Scanner::stop()
{
    signalThreadShouldExit();
}

Scanner::Progress Scanner::getProgress() const
{
    const juce::ScopedLock sl (lock);
    return progress;
}

juce::StringArray Scanner::getLastFailures() const
{
    const juce::ScopedLock sl (lock);
    return failures;
}

juce::String Scanner::getLastSummary() const
{
    const juce::ScopedLock sl (lock);
    return summary;
}

bool Scanner::scanFile (const juce::String& file, juce::String& error)
{
    auto& list = engine.getPluginManager().knownPluginList;
    auto out = juce::File::createTempFile (".xml");
    const juce::StringArray args { juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName(),
                                   scanFlag, file, out.getFullPathName() };

    juce::ChildProcess child;
    if (! child.start (args, 0))
    {
        error = "the checker could not start (an antivirus may be blocking it)";
        return false;
    }

    const auto startTime = juce::Time::getMillisecondCounter();
    bool timedOut = false;
    while (child.isRunning())
    {
        if (threadShouldExit())
        {
            child.kill();
            out.deleteFile();
            error = "stopped";
            return false;
        }
        if (juce::Time::getMillisecondCounter() - startTime > (juce::uint32) timeoutMs)
        {
            timedOut = true;
            child.kill();
            break;
        }
        juce::Thread::sleep (30);
    }

    auto xml = juce::parseXML (out);
    out.deleteFile();

    if (xml == nullptr || ! xml->hasTagName ("PLUGINS") || ! xml->getBoolAttribute ("complete"))
    {
        error = timedOut ? "took longer than a minute to start" : "crashed while starting";
        return false;
    }

    for (auto* e : xml->getChildIterator())
    {
        juce::PluginDescription d;
        if (d.loadFromXml (*e))
        {
            d.lastInfoUpdateTime = juce::Time::getCurrentTime();
            list.addType (d);
        }
    }
    return true;
}

void Scanner::run()
{
    auto* format = findVst3 (engine);
    if (format == nullptr)
    {
        const juce::ScopedLock sl (lock);
        summary = "VST3 support is missing from this build.";
    }
    else
    {
        auto& list = engine.getPluginManager().knownPluginList;

        juce::FileSearchPath path;
        for (auto& f : getDefaultFolders())  path.add (juce::File (f));
        for (auto& f : getExtraFolders())    path.add (juce::File (f));

        // Forget plugins that were uninstalled
        for (const auto& t : list.getTypes())
            if (t.pluginFormatName == "VST3" && ! format->doesPluginStillExist (t))
                list.removeType (t);

        const auto files = format->searchPathsForPlugins (path, true, false);
        juce::StringArray toScan;
        int skippedFailed = 0;

        for (auto& f : files)
        {
            if (list.getBlacklistedFiles().contains (f))
            {
                if (! retryFailed) { ++skippedFailed; continue; }
                list.removeFromBlacklist (f);
            }
            if (rescanAll || ! list.isListingUpToDate (f, *format))
                toScan.add (f);
        }

        {
            const juce::ScopedLock sl (lock);
            progress.total = toScan.size();
        }

        for (auto& f : toScan)
        {
            if (threadShouldExit())
                break;

            {
                const juce::ScopedLock sl (lock);
                progress.current = juce::File (f).getFileNameWithoutExtension();
                progress.currentStarted = juce::Time::getMillisecondCounter();
            }

            juce::String error;
            if (! scanFile (f, error) && error != "stopped")
            {
                list.addToBlacklist (f);
                const juce::ScopedLock sl (lock);
                failures.add (juce::File (f).getFileNameWithoutExtension() + ": " + error);
            }

            const juce::ScopedLock sl (lock);
            ++progress.done;
        }

        const auto numInstruments = getInstruments (engine).size();
        const auto numEffects     = getEffects (engine).size();
        const juce::ScopedLock sl (lock);
        summary = threadShouldExit() ? juce::String ("Scan stopped. ")
                                     : juce::String ("Scan finished. ");
        summary << numInstruments << (numInstruments == 1 ? " instrument" : " instruments") << " and "
                << numEffects << (numEffects == 1 ? " effect" : " effects") << " ready.";
        if (! failures.isEmpty())
            summary << " " << failures.size() << " could not be used.";
        if (skippedFailed > 0)
            summary << " " << skippedFailed << " skipped because they failed before.";
        progress.current = {};
    }

    juce::MessageManager::callAsync ([this, a = alive]
    {
        if (a->load() && onFinished != nullptr)
            onFinished();
    });
}

int Scanner::scanFolderNow (const juce::File& folder)
{
    auto* format = findVst3 (engine);
    if (format == nullptr)
        return 0;

    juce::FileSearchPath path;
    path.add (folder);
    int count = 0;
    for (auto& f : format->searchPathsForPlugins (path, true, false))
    {
        juce::String error;
        if (scanFile (f, error))
            ++count;
        else
        {
            engine.getPluginManager().knownPluginList.addToBlacklist (f);
            failures.add (juce::File (f).getFileNameWithoutExtension() + ": " + error);
        }
    }
    return count;
}

namespace
{
    std::unique_ptr<Scanner> scanner;
}

Scanner& getScanner (te::Engine& engine)
{
    if (scanner == nullptr)
        scanner = std::make_unique<Scanner> (engine);
    return *scanner;
}

void releaseScanner()
{
    scanner.reset();
}

//==============================================================================
//  The Plugins window
//==============================================================================
namespace
{
    class PluginsComponent final : public juce::Component,
                                   private juce::TableListBoxModel,
                                   private juce::ChangeListener,
                                   private juce::Timer
    {
    public:
        explicit PluginsComponent (te::Engine& e) : engine (e)
        {
            title.setText ("Plugins (VST3)", juce::dontSendNotification);
            title.setFont (juce::FontOptions (20.0f, juce::Font::bold));
            addAndMakeVisible (title);

            help.setText ("Install a plugin with its own installer first, then press \"Scan for plugins\". "
                          "Each plugin is checked safely on its own, so a faulty one cannot crash the studio. "
                          "Then right-click a track: \"Change instrument\" or \"Effects\".",
                          juce::dontSendNotification);
            help.setFont (juce::FontOptions (13.0f));
            help.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
            help.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (help);

            folders.setFont (juce::FontOptions (12.5f));
            folders.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
            folders.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (folders);

            status.setFont (juce::FontOptions (13.5f, juce::Font::bold));
            status.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (status);

            scanButton.onClick   = [this] { startScan (false, false); };
            rescanButton.onClick = [this] { startScan (true, false); };
            retryButton.onClick  = [this] { startScan (false, true); };
            stopButton.onClick   = [this] { getScanner (engine).stop(); };
            addFolderButton.onClick = [this] { addFolder(); };
            removeFolderButton.onClick = [this] { removeFolder(); };
            scanButton.setTooltip ("Look for plugins installed since the last scan");
            rescanButton.setTooltip ("Check every plugin again (after updating plugins)");
            retryButton.setTooltip ("Try again with plugins that failed before");
            for (auto* b : { &scanButton, &rescanButton, &retryButton, &stopButton, &addFolderButton, &removeFolderButton })
            {
                b->setWantsKeyboardFocus (false);
                addAndMakeVisible (*b);
            }

            auto& h = table.getHeader();
            h.addColumn ("Name", 1, 200);
            h.addColumn ("Type", 2, 90);
            h.addColumn ("Maker", 3, 140);
            h.addColumn ("Status", 4, 290);
            table.setModel (this);
            table.setRowHeight (24);
            table.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1b1e24));
            table.getHeader().setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour (0xff262b33));
            table.getHeader().setColour (juce::TableHeaderComponent::textColourId, juce::Colours::white.withAlpha (0.8f));
            table.getHeader().setColour (juce::TableHeaderComponent::outlineColourId, juce::Colour (0xff15171c));
            addAndMakeVisible (table);

            engine.getPluginManager().knownPluginList.addChangeListener (this);
            getScanner (engine).onFinished = [this] { refresh(); };
            refresh();
            startTimer (200);
        }

        ~PluginsComponent() override
        {
            engine.getPluginManager().knownPluginList.removeChangeListener (this);
            if (scanner != nullptr)
                scanner->onFinished = nullptr;
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff15171c));
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (16);
            title.setBounds (r.removeFromTop (28));
            help.setBounds (r.removeFromTop (48));
            folders.setBounds (r.removeFromTop (34));

            auto row = r.removeFromTop (30);
            for (auto* b : { &scanButton, &rescanButton, &retryButton, &stopButton })
            {
                b->setBounds (row.removeFromLeft (b == &stopButton ? 70 : 150));
                row.removeFromLeft (6);
            }
            r.removeFromTop (6);
            auto row2 = r.removeFromTop (28);
            addFolderButton.setBounds (row2.removeFromLeft (150));
            row2.removeFromLeft (6);
            removeFolderButton.setBounds (row2.removeFromLeft (170));
            r.removeFromTop (6);
            status.setBounds (r.removeFromTop (24));
            r.removeFromTop (4);
            table.setBounds (r);
        }

    private:
        struct Row { juce::String name, type, maker, status; bool failed = false; };

        void startScan (bool all, bool retry)
        {
            getScanner (engine).start (all, retry);
            updateStatus();
        }

        void refresh()
        {
            rows.clear();
            auto& list = engine.getPluginManager().knownPluginList;
            auto add = [this] (const juce::Array<juce::PluginDescription>& ds, const char* type)
            {
                for (auto& d : ds)
                    rows.push_back ({ d.name, type, d.manufacturerName, "Ready" + (d.version.isNotEmpty() ? "  (v" + d.version + ")" : juce::String()) });
            };
            add (getInstruments (engine), "Instrument");
            add (getEffects (engine), "Effect");
            const auto reasons = getScanner (engine).getLastFailures();
            for (auto& f : list.getBlacklistedFiles())
            {
                const auto name = juce::File (f).getFileNameWithoutExtension();
                juce::String why = "Could not be used";
                for (auto& r : reasons)
                    if (r.startsWith (name + ": "))
                        why = "Failed: " + r.fromFirstOccurrenceOf (": ", false, false);
                rows.push_back ({ name, "-", "", why, true });
            }

            juce::StringArray all (getDefaultFolders());
            all.addArray (getExtraFolders());
            folders.setText ("Looks in:  " + all.joinIntoString ("   |   "), juce::dontSendNotification);
            removeFolderButton.setEnabled (! getExtraFolders().isEmpty());

            table.updateContent();
            table.repaint();
            updateStatus();
        }

        void updateStatus()
        {
            auto& s = getScanner (engine);
            const auto scanning = s.isScanning();
            for (auto* b : { &scanButton, &rescanButton, &retryButton, &addFolderButton })
                b->setEnabled (! scanning);
            stopButton.setEnabled (scanning);

            juce::String text;
            if (scanning)
            {
                const auto p = s.getProgress();
                text = p.total == 0 ? juce::String ("Looking for plugins...")
                                    : "Checking " + juce::String (juce::jmin (p.done + 1, p.total)) + " of " + juce::String (p.total)
                                        + (p.current.isNotEmpty() ? ":  " + p.current : juce::String());
                if (p.current.isNotEmpty() && juce::Time::getMillisecondCounter() - p.currentStarted > 5000)
                    text << "   (slow to start: waiting up to one minute)";
            }
            else if (s.getLastSummary().isNotEmpty())
            {
                text = s.getLastSummary();
            }
            else
            {
                text = rows.empty() ? juce::String ("No plugins yet. Press \"Scan for plugins\".")
                                    : juce::String ((int) rows.size()) + " plugins in the list.";
            }
            status.setText (text, juce::dontSendNotification);
            status.setColour (juce::Label::textColourId, scanning ? juce::Colour (0xffd4a843) : juce::Colours::white);
        }

        void addFolder()
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a folder with VST3 plugins");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc)
            {
                const auto dir = fc.getResult();
                if (! dir.isDirectory())
                    return;
                auto extra = getExtraFolders();
                extra.addIfNotAlreadyThere (dir.getFullPathName());
                setExtraFolders (extra);
                refresh();
                startScan (false, false);
            });
        }

        void removeFolder()
        {
            const auto extra = getExtraFolders();
            juce::PopupMenu m;
            for (int i = 0; i < extra.size(); ++i)
                m.addItem (i + 1, extra[i]);
            juce::Component::SafePointer<PluginsComponent> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&removeFolderButton), [safe, extra] (int r)
            {
                if (safe == nullptr || r <= 0)
                    return;
                auto copy = extra;
                copy.remove (r - 1);
                setExtraFolders (copy);
                safe->refresh();
            });
        }

        int getNumRows() override   { return (int) rows.size(); }

        void paintRowBackground (juce::Graphics& g, int row, int, int, bool selected) override
        {
            g.fillAll (selected ? juce::Colour (0xff2c3340) : (row % 2 ? juce::Colour (0xff1b1e24) : juce::Colour (0xff20242b)));
        }

        void paintCell (juce::Graphics& g, int row, int column, int w, int h, bool) override
        {
            if (row < 0 || row >= (int) rows.size())
                return;
            const auto& r = rows[(size_t) row];
            const auto text = column == 1 ? r.name : column == 2 ? r.type : column == 3 ? r.maker : r.status;
            g.setColour (column == 4 ? (r.failed ? juce::Colour (0xffe07a4f) : juce::Colour (0xff4caf7d))
                                     : juce::Colours::white.withAlpha (column == 1 ? 0.95f : 0.7f));
            g.setFont (juce::FontOptions (13.0f, column == 1 ? juce::Font::bold : juce::Font::plain));
            g.drawText (text, 6, 0, w - 8, h, juce::Justification::centredLeft, true);
        }

        void changeListenerCallback (juce::ChangeBroadcaster*) override   { refresh(); }

        void timerCallback() override
        {
            auto& s = getScanner (engine);
            if (s.isScanning() || wasScanning)
                updateStatus();
            wasScanning = s.isScanning();
        }

        te::Engine& engine;
        juce::Label title, help, folders, status;
        juce::TextButton scanButton { "Scan for plugins" }, rescanButton { "Check all again" },
                         retryButton { "Try failed again" }, stopButton { "Stop" },
                         addFolderButton { "Add a folder..." }, removeFolderButton { "Remove an added folder" };
        juce::TableListBox table;
        std::vector<Row> rows;
        std::unique_ptr<juce::FileChooser> chooser;
        bool wasScanning = false;
    };

    class PluginsWindow final : public juce::DocumentWindow
    {
    public:
        explicit PluginsWindow (te::Engine& e)
            : DocumentWindow ("Plugins", juce::Colour (0xff15171c), DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PluginsComponent (e), false);

            auto screen = juce::Rectangle<int> (0, 0, 1366, 768);
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                screen = d->userArea;
            setResizable (true, false);
            setResizeLimits (560, 360, 3000, 2000);
            centreWithSize (juce::jmin (780, screen.getWidth() - 40), juce::jmin (560, screen.getHeight() - 60));
            setVisible (true);
        }

        void closeButtonPressed() override   { closeManager(); }
    };

    std::unique_ptr<PluginsWindow> managerWindow;
}

void openManager (te::Engine& engine)
{
    if (managerWindow == nullptr)
        managerWindow = std::make_unique<PluginsWindow> (engine);
    managerWindow->toFront (true);
}

void closeManager()
{
    managerWindow.reset();
}

} // namespace PluginHost
