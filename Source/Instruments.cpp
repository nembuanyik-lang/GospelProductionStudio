#include "Instruments.h"

namespace Instruments
{
    const std::vector<Choice>& all()
    {
        static const std::vector<Choice> list {
            { "grand",      "Grand Piano",             "Pianos",  Kind::sampler,    "piano-grand",      "" },
            { "grandFull",  "Grand Piano (full detail)", "Pianos", Kind::sampler,   "piano-grand-full", "" },
            { "upright",    "Upright Piano",           "Pianos",  Kind::sampler,    "piano-upright",    "" },
            { "organ",      "Gospel Organ",            "Organ",   Kind::organ,      "",                 "" },
            // Step 30: recorded guitars from the sound packs; the built-in guitar plays until they are downloaded
            { "guitar",     "Clean Electric Guitar",   "Guitar",  Kind::sampler,    "guitar-electric",  "Clean.sfz" },
            { "guitarNylon", "Nylon Guitar",           "Guitar",  Kind::sampler,    "guitar-nylon",     "Nylon.sfz" },
            { "bassFinger", "Electric Bass (finger)",  "Bass",    Kind::sampler,    "bass-electric",    "Finger.sfz" },
            { "bassPick",   "Electric Bass (pick)",    "Bass",    Kind::sampler,    "bass-electric",    "Pick.sfz" },
            { "drums",      "Drum Kit",                "Drums",   Kind::sampler,    "drums-acoustic",   "" },
            // Step 31: recorded where the African percussion pack has the sound, built-in otherwise
            { "percussion", "African Percussion (congas, djembe, talking drum, shakers, bells)", "Drums", Kind::sampler, "percussion-african", "Percussion.sfz" },
            { "balafon",    "Balafon",                 "Percussion", Kind::sampler, "percussion-african", "Balafon.sfz" },
            { "mysounds",   "My Sounds (your own samples)", "Your sounds", Kind::sampler, "",            "" },
            { "studioKeys", "Studio Keys (simple)",    "Simple",  Kind::studioKeys, "",                 "" },
        };
        return list;
    }

    const Choice* find (const juce::String& id)
    {
        for (const auto& c : all())
            if (id == c.id)
                return &c;
        return nullptr;
    }
}

namespace SoundPacks
{
    juce::File getRootFolder()
    {
       #if JUCE_WINDOWS
        // Local (not roaming) app data: packs are large and belong to this computer.
        auto base = juce::File::getSpecialLocation (juce::File::windowsLocalAppData);
       #else
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #endif
        return base.getChildFile ("GospelProductionStudio").getChildFile ("Sounds");
    }

    juce::File getPackFolder (const juce::String& packId)
    {
        return getRootFolder().getChildFile (packId);
    }

    static juce::var readPackInfo (const juce::String& packId)
    {
        return juce::JSON::parse (getPackFolder (packId).getChildFile ("pack.json"));
    }

    bool isInstalled (const juce::String& packId)
    {
        return packId.isNotEmpty() && getPackFolder (packId).getChildFile ("pack.json").existsAsFile();
    }

    juce::String installedVersion (const juce::String& packId)
    {
        return isInstalled (packId) ? readPackInfo (packId)["version"].toString() : juce::String();
    }

    juce::File findInstrument (const juce::String& packId, const juce::String& patch)
    {
        if (! isInstalled (packId))
            return {};

        const auto folder = getPackFolder (packId);
        if (patch.isNotEmpty())
        {
            auto f = folder.getChildFile (patch);
            return f.existsAsFile() ? f : juce::File();
        }

        // The first instrument listed in pack.json, else the first .sfz found
        const auto info = readPackInfo (packId);
        if (auto* list = info["instruments"].getArray())
            for (auto& item : *list)
            {
                auto f = folder.getChildFile (item["sfz"].toString());
                if (f.existsAsFile())
                    return f;
            }

        auto files = folder.findChildFiles (juce::File::findFiles, false, "*.sfz");
        files.sort();
        return files.isEmpty() ? juce::File() : files.getFirst();
    }

    juce::ChangeBroadcaster& changes()
    {
        static juce::ChangeBroadcaster broadcaster;
        return broadcaster;
    }
}
