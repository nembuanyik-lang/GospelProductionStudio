#include "Updater.h"

#ifndef GPS_BUILD_NUMBER
 #define GPS_BUILD_NUMBER 0
#endif

#ifndef GPS_UPDATE_REPO
 #define GPS_UPDATE_REPO "nembuanyik-lang/GospelProductionStudio"
#endif

namespace
{
    // "v0.4.0-build15" -> { "0.4.0", 15 }
    std::pair<juce::String, int> parseTag (const juce::String& tag)
    {
        auto t = tag.trim();
        if (t.startsWithIgnoreCase ("v"))
            t = t.substring (1);

        const auto version = t.upToFirstOccurrenceOf ("-", false, false);
        const auto build = t.fromFirstOccurrenceOf ("-build", false, true).getIntValue();
        return { version, build };
    }

    int compareVersions (const juce::String& a, const juce::String& b)
    {
        juce::StringArray pa, pb;
        pa.addTokens (a, ".", {});
        pb.addTokens (b, ".", {});

        for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
        {
            const auto x = pa[i].getIntValue(), y = pb[i].getIntValue();
            if (x != y)
                return x < y ? -1 : 1;
        }
        return 0;
    }
}

//==============================================================================
Updater::Updater() = default;

Updater::~Updater()
{
    alive->store (false);
    download.reset();
}

juce::String Updater::getCurrentVersion()   { return JUCE_APPLICATION_VERSION_STRING; }
int Updater::getCurrentBuild()              { return GPS_BUILD_NUMBER; }
juce::String Updater::getRepository()       { return GPS_UPDATE_REPO; }

juce::String Updater::describeCurrent()
{
    return "version " + getCurrentVersion()
         + (getCurrentBuild() > 0 ? " (build " + juce::String (getCurrentBuild()) + ")" : juce::String());
}

bool Updater::isNewer (const juce::String& version, int build)
{
    const auto c = compareVersions (version, getCurrentVersion());
    return c > 0 || (c == 0 && build > getCurrentBuild());
}

//==============================================================================
void Updater::check (std::function<void (Result, const Release&, const juce::String&)> callback)
{
    auto flag = alive;
    const auto repo = getRepository();

    juce::Thread::launch ([flag, repo, callback]
    {
        Release release;
        Result result = Result::failed;
        juce::String message;

        int status = 0;
        const juce::URL url ("https://api.github.com/repos/" + repo + "/releases/latest");
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                 .withExtraHeaders ("Accept: application/vnd.github+json\r\n"
                                                                    "User-Agent: GospelProductionStudio")
                                                 .withConnectionTimeoutMs (15000)
                                                 .withStatusCode (&status));

        if (stream == nullptr)
        {
            message = "Could not reach GitHub. Check the internet connection and try again.";
        }
        else if (status == 404)
        {
            message = "No published release was found for " + repo + ".\n\n"
                      "If the repository is private, the app cannot see its releases: make the "
                      "repository public (GitHub > Settings > General > Change visibility).";
        }
        else if (status != 200)
        {
            message = "GitHub answered with code " + juce::String (status) + ". Please try again later.";
        }
        else
        {
            const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
            release.tag   = json["tag_name"].toString();
            release.notes = json["body"].toString();

            const auto [version, build] = parseTag (release.tag);
            release.version = version;
            release.build   = build;

            if (auto* assets = json["assets"].getArray())
            {
                for (const auto& asset : *assets)
                {
                    if (asset["name"].toString().endsWithIgnoreCase (".exe"))
                    {
                        release.downloadUrl = asset["browser_download_url"].toString();
                        release.size        = (juce::int64) asset["size"];
                        break;
                    }
                }
            }

            if (release.version.isEmpty() || release.downloadUrl.isEmpty())
                message = "The latest release on GitHub has no Setup .exe attached.";
            else if (isNewer (release.version, release.build))
                result = Result::updateAvailable;
            else
            {
                result = Result::upToDate;
                message = "You have the latest version (" + describeCurrent() + ").";
            }
        }

        juce::MessageManager::callAsync ([flag, callback, result, release, message]
        {
            if (flag->load() && callback)
                callback (result, release, message);
        });
    });
}

//==============================================================================
void Updater::downloadAndInstall (const Release& release,
                                  std::function<void (float)> onProgress,
                                  std::function<void (bool, const juce::String&)> onDone)
{
    if (busy)
        return;

    busy = true;
    progressCallback = std::move (onProgress);
    doneCallback = std::move (onDone);

    installerFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("GospelProductionStudio-Update-" + release.version
                                       + "-" + juce::String (release.build) + ".exe");
    installerFile.deleteFile();

    download = juce::URL (release.downloadUrl)
                   .downloadToFile (installerFile,
                                    juce::URL::DownloadTaskOptions()
                                        .withExtraHeaders ("User-Agent: GospelProductionStudio")
                                        .withListener (this));

    if (download == nullptr)
    {
        busy = false;
        if (doneCallback)
            doneCallback (false, "The download could not start.");
    }
}

void Updater::progress (juce::URL::DownloadTask*, juce::int64 downloaded, juce::int64 total)
{
    auto flag = alive;
    const auto fraction = total > 0 ? (float) ((double) downloaded / (double) total) : 0.0f;
    juce::MessageManager::callAsync ([this, flag, fraction]
    {
        if (flag->load() && progressCallback)
            progressCallback (fraction);
    });
}

void Updater::finished (juce::URL::DownloadTask* task, bool success)
{
    auto flag = alive;
    const auto ok = success && ! task->hadError() && installerFile.getSize() > 100000;

    juce::MessageManager::callAsync ([this, flag, ok]
    {
        if (! flag->load())
            return;

        busy = false;

        if (! ok)
        {
            installerFile.deleteFile();
            if (doneCallback)
                doneCallback (false, "The download did not finish. Check the internet connection and try again.");
            return;
        }

       #if JUCE_WINDOWS
        // Inno Setup: install quietly, close this app, then open the new version.
        if (installerFile.startAsProcess ("/SILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /NORESTART"))
        {
            if (doneCallback)
                doneCallback (true, "Installing the update...");
            juce::JUCEApplicationBase::quit();
            return;
        }
       #endif

        installerFile.revealToUser();
        if (doneCallback)
            doneCallback (true, "The update was downloaded. Run it to finish installing.");
    });
}
