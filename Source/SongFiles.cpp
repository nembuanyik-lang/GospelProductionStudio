#include "SongFiles.h"
#include "SongSafety.h"
#include "Takes.h"

namespace SongFiles
{
    namespace
    {
        const char* generatedFolders[] = { "Harmony", "Stems", "Tempo", "Timed" };

        bool isGeneratedFolder (const juce::File& dir)
        {
            for (auto* n : generatedFolders)
                if (dir.getFileName() == n) return true;
            return false;
        }

        void addPathsIn (const juce::ValueTree& v, juce::Array<juce::File>& out)
        {
            for (int i = 0; i < v.getNumProperties(); ++i)
            {
                const auto s = v[v.getPropertyName (i)].toString();
                if (juce::File::isAbsolutePath (s)) out.addIfNotAlreadyThere (juce::File (s));
            }
        }
    }

    juce::Array<juce::File> filesInUse (te::Edit& edit)
    {
        juce::Array<juce::File> used;
        for (auto* t : te::getAudioTracks (edit))
            for (auto* c : t->getClips())
                if (auto* w = dynamic_cast<te::WaveAudioClip*> (c))
                {
                    used.addIfNotAlreadyThere (w->getOriginalFile());
                    for (int i = 0; i < Takes::count (*w); ++i)
                        used.addIfNotAlreadyThere (Takes::fileFor (*w, i));
                    // Originals the clip remembers: before timing correction, before a tempo stretch, a loop's source...
                    addPathsIn (w->state, used);
                }
        return used;
    }

    Tidied tidy (te::Edit& edit)
    {
        Tidied done;
        if (! edit.editFileRetriever)
            return done;
        const auto songFolder = edit.editFileRetriever().getParentDirectory();
        const auto audio = songFolder.getChildFile ("Audio");
        if (songFolder == juce::File() || ! audio.isDirectory())
            return done;

        const auto used = filesInUse (edit);
        // Files an older version of the song still uses are kept, so going back to it works (Step 44)
        const auto versions = SongSafety::allVersionsText (songFolder);

        // The generated folders anywhere under the song's Audio folder (older versions nested them, e.g. Timed/Harmony)
        juce::Array<juce::File> folders;
        for (auto& d : audio.findChildFiles (juce::File::findDirectories, true))
            if (isGeneratedFolder (d))
                folders.add (d);

        for (auto& folder : folders)
            for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav"))
                if (! used.contains (f) && ! versions.contains (f.getFileName()))
                {
                    const auto size = f.getSize();
                    if (f.deleteFile()) { ++done.files; done.bytes += size; }
                }

        // Folders left empty go too (deepest first)
        std::sort (folders.begin(), folders.end(), [] (auto& a, auto& b) { return a.getFullPathName().length() > b.getFullPathName().length(); });
        for (auto& folder : folders)
            if (folder.isDirectory() && folder.findChildFiles (juce::File::findFilesAndDirectories, false).isEmpty())
                folder.deleteFile();
        return done;
    }

    juce::String describeSize (juce::int64 bytes)
    {
        if (bytes >= (juce::int64) 1024 * 1024 * 1024) return juce::String (bytes / (1024.0 * 1024.0 * 1024.0), 1) + " GB";
        if (bytes >= 1024 * 1024)                       return juce::String (juce::roundToInt (bytes / (1024.0 * 1024.0))) + " MB";
        return juce::String (juce::jmax ((juce::int64) 1, bytes / 1024)) + " KB";
    }
}
