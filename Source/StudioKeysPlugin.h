#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "StudioKeys.h"

namespace te = tracktion;

//==============================================================================
//  Studio Keys as a proper engine instrument.
//  It sits on the "Keys" track, plays your MIDI keyboard live and plays back
//  anything recorded on that track. Replaced by the sampled piano in Step 8.
//==============================================================================
class StudioKeysPlugin final : public te::Plugin
{
public:
    explicit StudioKeysPlugin (te::PluginCreationInfo info)
        : te::Plugin (info)
    {
        for (int i = 0; i < 32; ++i)
            synth.addVoice (new StudioKeysVoice());
        synth.addSound (new StudioKeysSound());
    }

    ~StudioKeysPlugin() override
    {
        notifyListenersOfDeletion();
    }

    static const char* getPluginName()                           { return "Studio Keys"; }
    static const char* xmlTypeName;

    juce::String getName() const override                        { return getPluginName(); }
    juce::String getPluginType() override                        { return xmlTypeName; }
    juce::String getShortName (int) override                     { return "Keys"; }
    juce::String getSelectableDescription() override             { return getName(); }

    bool isSynth() override                                      { return true; }
    bool takesMidiInput() override                               { return true; }
    bool takesAudioInput() override                              { return false; }
    bool producesAudioWhenNoAudioInput() override                { return true; }
    int  getNumOutputChannelsGivenInputs (int) override          { return 2; }
    BusLayout getBusses() const override                         { return BusLayout::singleStereoInOut(); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        synth.setCurrentPlaybackSampleRate (sampleRate);
        midi.ensureSize (8192);
    }

    void deinitialise() override {}

    void reset() override
    {
        synth.allNotesOff (0, false);
    }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr)
            return;

        fc.destBuffer->clear (fc.bufferStartSample, fc.bufferNumSamples);
        midi.clear();

        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff)
                synth.allNotesOff (0, true);

            for (auto& m : *fc.bufferForMidiMessages)
            {
                const auto offset = juce::jlimit (0, juce::jmax (0, fc.bufferNumSamples - 1),
                                                  juce::roundToInt (m.getTimeStamp() * sampleRate));
                midi.addEvent (m, fc.bufferStartSample + offset);
            }
        }

        synth.renderNextBlock (*fc.destBuffer, midi, fc.bufferStartSample, fc.bufferNumSamples);
    }

private:
    juce::Synthesiser synth;
    juce::MidiBuffer midi;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioKeysPlugin)
};
