#pragma once

#include "StudioEngine.h"

//==============================================================================
//  Instrument window for one track: choose the instrument and shape its sound.
//  Organ: drawbars, presets, percussion, vibrato, Leslie, drive.
//  Sampled instruments: volume, tone, release, touch.
//==============================================================================
namespace InstrumentPanel
{
    void open (StudioEngine&, te::AudioTrack&);
    void closeAll();
}
