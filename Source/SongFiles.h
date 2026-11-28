#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
//  The song's own files (plan Step 25).
//
//  Tools make new recordings and keep the old ones while you work, so Ctrl+Z
//  can bring them back: harmony parts (Audio/Harmony), stems (Audio/Stems),
//  recordings stretched to another tempo (Audio/Tempo) and timing corrections
//  (Audio/Timed). When the song is opened again the undo history is empty, so
//  any of those files that no clip uses any more is deleted. Your recordings,
//  takes and imported files are never touched: only these four folders, and
//  only files that nothing in the song points to.
//==============================================================================
namespace SongFiles
{
    struct Tidied { int files = 0; juce::int64 bytes = 0; };

    // Every file the song still needs (clips, takes, originals the clips remember)
    juce::Array<juce::File> filesInUse (te::Edit&);

    // Deletes unused files in the song's Harmony, Stems, Tempo and Timed folders
    Tidied tidy (te::Edit&);

    juce::String describeSize (juce::int64 bytes);          // "320 MB"
}
