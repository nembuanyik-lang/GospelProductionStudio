#include "Takes.h"

namespace Takes
{
    static juce::ValueTree takesTree (te::WaveAudioClip& c)
    {
        return c.state.getChildWithName (te::IDs::TAKES);
    }

    int count (te::WaveAudioClip& c)
    {
        return takesTree (c).getNumChildren();
    }

    juce::File fileFor (te::WaveAudioClip& c, int index)
    {
        auto take = takesTree (c).getChild (index);
        if (! take.isValid())
            return {};
        te::SourceFileReference ref (c.edit, take, te::IDs::source);
        return ref.getFile();
    }

    int current (te::WaveAudioClip& c)
    {
        const auto playing = c.getSourceFileReference().getFile();
        for (int i = 0; i < count (c); ++i)
            if (fileFor (c, i) == playing)
                return i;
        return -1;
    }

    void select (te::WaveAudioClip& c, int index)
    {
        const auto f = fileFor (c, index);
        if (f.existsAsFile())
            c.getSourceFileReference().setToFile (f, te::SourceFileReference::PathStyle::chooseBest, false);
    }

    void keepOnly (te::WaveAudioClip& c, int index, bool deleteOtherFiles)
    {
        const auto keep = fileFor (c, index);
        select (c, index);

        juce::Array<juce::File> others;
        for (int i = 0; i < count (c); ++i)
            if (i != index)
                others.addIfNotAlreadyThere (fileFor (c, i));

        c.state.removeChild (takesTree (c), c.getUndoManager());

        if (deleteOtherFiles)
            for (auto& f : others)
                if (f != keep)
                    f.moveToTrash();
    }
}
