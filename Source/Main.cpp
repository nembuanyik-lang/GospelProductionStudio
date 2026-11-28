#include "BinaryData.h"
#include "SongSafety.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"
#include "StudioLookAndFeel.h"
#include "RenderTest.h"
#include "PluginHost.h"
#include "BackgroundWork.h"

//==============================================================================
class GospelProductionStudioApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    // The plugin checker runs as a second copy of the app, one plugin at a time
    bool moreThanOneInstanceAllowed() override          { return PluginHost::isScanChild (getCommandLineParameters()); }

    void initialise (const juce::String& commandLine) override
    {
        if (PluginHost::runScanChildIfRequested (commandLine))
        {
            quit();
            return;
        }

        if (RenderTest::runIfRequested (commandLine))
        {
            setApplicationReturnValue (RenderTest::failuresInLastRun() > 0 ? 1 : 0);
            quit();
            return;
        }

        // Did the app close properly last time? (Step 43: the window offers to recover)
        SongSafety::Session::appStarted (juce::SystemStats::getEnvironmentVariable ("GPS_TEST_SESSION_FILE", {}).isNotEmpty()
                                             ? juce::File (juce::SystemStats::getEnvironmentVariable ("GPS_TEST_SESSION_FILE", {}))
                                             : juce::File());

        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        closeTimer.tick = [this] { checkClose(); };
        mainWindow = std::make_unique<MainWindow> (getApplicationName());

        // For testing recovery: GPS_TEST_CRASH_AFTER_MS=20000 stops the app the hard way, like a power cut
        if (const auto ms = juce::SystemStats::getEnvironmentVariable ("GPS_TEST_CRASH_AFTER_MS", {}).getIntValue(); ms > 0)
            juce::Timer::callAfterDelay (ms, [] { std::_Exit (3); });

        // For testing closing on a computer without a window manager: GPS_TEST_CLOSE_AFTER_MS=25000
        if (const auto ms = juce::SystemStats::getEnvironmentVariable ("GPS_TEST_CLOSE_AFTER_MS", {}).getIntValue(); ms > 0)
            juce::Timer::callAfterDelay (ms, [this] { systemRequestedQuit(); });
    }

    void shutdown() override
    {
        closeTimer.stopTimer();
        BackgroundWork::shutdown();      // stop and wait for background work before the windows and engine go
        mainWindow = nullptr;
        SongSafety::Session::appClosedNormally();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    //==========================================================================
    //  Closing safely (Step 24): ask first when something important is still working
    void systemRequestedQuit() override
    {
        if (askingToClose)
            return;
        if (mainWindow == nullptr || closing != Closing::no)
        {
            if (closing == Closing::no) quit();
            return;
        }

        const auto busy = BackgroundWork::whatIsBusy (true);
        if (busy.isEmpty())
        {
            finishAndQuit();
            return;
        }

        juce::String list;
        for (auto& b : busy) list << "\n   - " << b;
        askingToClose = true;
        juce::AlertWindow::showAsync (juce::MessageBoxOptions{}
            .withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withTitle ("The studio is still working")
            .withMessage ("Still working on:" + list + "\n\n"
                          "Close when finished: the studio closes by itself as soon as this is done.\n"
                          "Stop and close: it stops now (nothing half-made is kept) and the studio closes.")
            .withButton ("Close when finished", 1)
            .withButton ("Stop and close", 2)
            .withButton ("Keep working", 0)
            .withAssociatedComponent (mainWindow.get()),
            [this] (int r)
            {
                askingToClose = false;
                if (r == 0 || mainWindow == nullptr) return;
                closing = r == 1 ? Closing::whenFinished : Closing::stopping;
                if (closing == Closing::stopping)
                    BackgroundWork::stopAll();
                mainWindow->setName (juce::String (getApplicationName()) + (closing == Closing::stopping ? "  -  stopping, then closing..." : "  -  closing when the work is finished..."));
                closeStarted = juce::Time::getMillisecondCounter();
                closeTimer.startTimer (300);
            });
    }

    void checkClose()
    {
        // Something may have started meanwhile (a stop finishes): wait for all of it, but never hang
        const bool done = closing == Closing::whenFinished ? BackgroundWork::whatIsBusy (true).isEmpty() : BackgroundWork::isIdle();
        const bool tooLong = closing == Closing::stopping && juce::Time::getMillisecondCounter() - closeStarted > 15000;
        if (done || tooLong)
        {
            closeTimer.stopTimer();
            finishAndQuit();
        }
    }

    void finishAndQuit()
    {
        closing = Closing::quitting;
        if (mainWindow != nullptr)
            if (auto* mc = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
                mc->prepareToClose();
        quit();
    }
    void anotherInstanceStarted (const juce::String&) override {}

    //==========================================================================
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, StudioColours::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setIcon (juce::ImageCache::getFromMemory (BinaryData::icon256_png, BinaryData::icon256_pngSize));   // the logo
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);

            // Fit the screen the app opens on (small laptops: 1366 x 768 and smaller)
            auto screen = juce::Rectangle<int> (0, 0, 1366, 860);
            if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                screen = display->userBounds.toNearestIntEdges();

            setResizeLimits (juce::jmin (960, screen.getWidth()), juce::jmin (560, screen.getHeight()), 10000, 10000);
            centreWithSize (juce::jmin (1366, screen.getWidth() - 20), juce::jmin (860, screen.getHeight() - 40));
            setVisible (true);

            if (screen.getWidth() < 1400 || screen.getHeight() < 900)
                setFullScreen (true);       // maximised, so nothing is hidden below the taskbar
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    StudioLookAndFeel lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;

    enum class Closing { no, whenFinished, stopping, quitting };
    Closing closing = Closing::no;
    bool askingToClose = false;
    juce::uint32 closeStarted = 0;
    struct CloseTimer final : juce::Timer
    {
        std::function<void()> tick;
        void timerCallback() override   { if (tick) tick(); }
    } closeTimer;
};

START_JUCE_APPLICATION (GospelProductionStudioApplication)
