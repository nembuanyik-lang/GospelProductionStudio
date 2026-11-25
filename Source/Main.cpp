#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"
#include "StudioLookAndFeel.h"
#include "RenderTest.h"
#include "PluginHost.h"

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
            quit();
            return;
        }

        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override                { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

    //==========================================================================
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, StudioColours::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);

            // Fit the screen the app opens on (small laptops: 1366 x 768 and smaller)
            auto screen = juce::Rectangle<int> (0, 0, 1366, 860);
            if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                screen = display->userArea;

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
};

START_JUCE_APPLICATION (GospelProductionStudioApplication)
