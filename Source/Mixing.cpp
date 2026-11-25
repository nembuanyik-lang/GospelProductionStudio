#include "Mixing.h"
#include "StudioLookAndFeel.h"

namespace Mixing
{
namespace
{
    const juce::Identifier returnFlag ("gpsReturn");     // -1 or absent: a song track

    te::AuxSendPlugin* findSendPlugin (te::AudioTrack& track, int bus)
    {
        for (auto* p : track.pluginList.getPlugins())
            if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p))
                if (s->getBusNumber() == bus)
                    return s;
        return nullptr;
    }

    double tempoAtStart (te::Edit& edit)
    {
        return edit.tempoSequence.getTempos().isEmpty() ? 90.0 : edit.tempoSequence.getTempos()[0]->getBpm();
    }
}

juce::String busName (int bus)
{
    return bus == reverbBus ? "Reverb" : bus == delayBus ? "Delay" : "Bus " + juce::String (bus + 1);
}

bool isReturnTrack (const te::AudioTrack& t)
{
    return getReturnBus (t) >= 0;
}

int getReturnBus (const te::AudioTrack& t)
{
    return (int) t.state.getProperty (returnFlag, -1);
}

juce::Array<te::AudioTrack*> getSongTracks (te::Edit& edit)
{
    juce::Array<te::AudioTrack*> result;
    for (auto* t : te::getAudioTracks (edit))
        if (! isReturnTrack (*t))
            result.add (t);
    return result;
}

te::AudioTrack* findReturn (te::Edit& edit, int bus)
{
    for (auto* t : te::getAudioTracks (edit))
        if (getReturnBus (*t) == bus)
            return t;
    return nullptr;
}

te::AudioTrack* ensureReturn (te::Edit& edit, int bus)
{
    if (auto* t = findReturn (edit, bus))
        return t;

    auto track = edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr, true);
    if (track == nullptr)
        return nullptr;

    track->state.setProperty (returnFlag, bus, nullptr);
    track->setName (busName (bus) + " (shared)");
    track->setColour (bus == reverbBus ? juce::Colour (0xff7e8fe0) : juce::Colour (0xff5fb8b0));

    if (auto ret = edit.getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}))
    {
        if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (ret.get()))
            r->busNumber = bus;
        track->pluginList.insertPlugin (ret, 0, nullptr);
    }

    if (bus == reverbBus)
    {
        if (auto p = edit.getPluginCache().createNewPlugin (te::ReverbPlugin::xmlTypeName, {}))
        {
            if (auto* r = dynamic_cast<te::ReverbPlugin*> (p.get()))
            {
                // A warm church-like room, fully wet (the dry sound stays on each track)
                r->setRoomSize (0.72f);
                r->setDamp (0.45f);
                r->setWidth (1.0f);
                r->setWet (0.5f);             // about +3.5 dB: Freeverb is quiet
                r->setDry (0.0f);
            }
            track->pluginList.insertPlugin (p, 1, nullptr);
        }
    }
    else
    {
        if (auto p = edit.getPluginCache().createNewPlugin (te::DelayPlugin::xmlTypeName, {}))
        {
            if (auto* d = dynamic_cast<te::DelayPlugin*> (p.get()))
            {
                // A dotted-eighth echo at the song's tempo
                d->lengthMs = juce::roundToInt (60000.0 / tempoAtStart (edit) * 0.75);
                d->feedbackValue = -10.0f;
                d->mixValue = 1.0f;
            }
            track->pluginList.insertPlugin (p, 1, nullptr);
        }
    }
    return track.get();
}

void prepareSong (te::Edit& edit)
{
    for (int bus = 0; bus < numBuses; ++bus)
        ensureReturn (edit, bus);

    bool haveMeter = false;
    for (auto* p : edit.getMasterPluginList().getPlugins())
        haveMeter = haveMeter || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr;
    if (! haveMeter)
        if (auto p = edit.getPluginCache().createNewPlugin (te::LevelMeterPlugin::xmlTypeName, {}))
            edit.getMasterPluginList().insertPlugin (p, edit.getMasterPluginList().getPlugins().size(), nullptr);
}

float getSendDb (te::AudioTrack& track, int bus)
{
    if (auto* s = findSendPlugin (track, bus))
        return juce::jmax (sendOffDb, s->getGainDb());
    return sendOffDb;
}

void setSendDb (te::AudioTrack& track, int bus, float db)
{
    auto* send = findSendPlugin (track, bus);
    if (send == nullptr)
    {
        if (db <= sendOffDb)
            return;

        ensureReturn (track.edit, bus);
        auto p = track.edit.getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
        send = dynamic_cast<te::AuxSendPlugin*> (p.get());
        if (send == nullptr)
            return;
        send->busNumber = bus;

        // After the fader (post-fader send), before the meter
        int index = track.pluginList.getPlugins().size();
        const auto plugins = track.pluginList.getPlugins();
        for (int i = 0; i < plugins.size(); ++i)
            if (dynamic_cast<te::LevelMeterPlugin*> (plugins[i].get()) != nullptr)
                index = juce::jmin (index, i);
        track.pluginList.insertPlugin (p, index, nullptr);
    }
    send->setGainDb (db <= sendOffDb ? -100.0f : db);
}

bool isMixerPlugin (te::Plugin& p)
{
    return dynamic_cast<te::VolumeAndPanPlugin*> (&p) != nullptr
        || dynamic_cast<te::LevelMeterPlugin*> (&p) != nullptr
        || dynamic_cast<te::AuxSendPlugin*> (&p) != nullptr
        || dynamic_cast<te::AuxReturnPlugin*> (&p) != nullptr;
}

const std::vector<BuiltIn>& builtInEffects()
{
    static const std::vector<BuiltIn> list {
        { te::EqualiserPlugin::xmlTypeName,  "EQ (4 bands)",    "shape the tone: less mud, more clarity" },
        { te::CompressorPlugin::xmlTypeName, "Compressor",      "even out loud and soft words" },
        { te::ReverbPlugin::xmlTypeName,     "Reverb",          "a room on this track only" },
        { te::DelayPlugin::xmlTypeName,      "Delay (echo)",    "echoes" },
        { te::ChorusPlugin::xmlTypeName,     "Chorus",          "wider, shimmering keys and guitars" },
        { te::PhaserPlugin::xmlTypeName,     "Phaser",          "swirling sound" },
        { te::LowPassPlugin::xmlTypeName,    "Filter (low/high cut)", "cut rumble or harshness" },
    };
    return list;
}

te::Plugin::Ptr createBuiltIn (te::Edit& edit, const juce::String& type)
{
    auto p = edit.getPluginCache().createNewPlugin (type, {});
    if (auto* r = dynamic_cast<te::ReverbPlugin*> (p.get()))
    {
        r->setRoomSize (0.6f);
        r->setWet (0.1f);        // about -10 dB of room
        r->setDry (0.5f);        // dry sound unchanged (0 dB)
    }
    if (auto* d = dynamic_cast<te::DelayPlugin*> (p.get()))
    {
        d->lengthMs = juce::roundToInt (60000.0 / tempoAtStart (edit) * 0.75);
        d->mixValue = 0.25f;
    }
    return p;
}

std::vector<te::Plugin*> getMasterEffects (te::Edit& edit)
{
    std::vector<te::Plugin*> result;
    for (auto* p : edit.getMasterPluginList().getPlugins())
        if (! isMixerPlugin (*p))
            result.push_back (p);
    return result;
}

te::Plugin* addMasterEffect (te::Edit& edit, te::Plugin::Ptr p)
{
    if (p == nullptr)
        return nullptr;
    edit.getMasterPluginList().insertPlugin (p, edit.getMasterPluginList().getPlugins().size(), nullptr);
    return p.get();
}

//==============================================================================
//  Generic panel for built-in effects
//==============================================================================
namespace
{
    // Gospel starting points for the built-in effects: parameter ID -> value
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };

    const std::vector<Preset>& presetsFor (const juce::String& type)
    {
        static const std::vector<Preset> none;
        static const std::vector<Preset> eq {
            { "Flat (no change)",       { { "Low-pass gain", 0 }, { "Mid gain 1", 0 }, { "Mid gain 2", 0 }, { "High-pass gain", 0 } } },
            { "Lead vocal: clear",      { { "Low-pass freq", 100 }, { "Low-pass gain", -4 }, { "Mid freq 1", 300 }, { "Mid gain 1", -2 }, { "Mid Q 1", 0.8f },
                                          { "Mid freq 2", 3500 }, { "Mid gain 2", 2.5f }, { "Mid Q 2", 0.7f }, { "High-pass freq", 10000 }, { "High-pass gain", 2 } } },
            { "Choir: warm and soft",   { { "Low-pass freq", 120 }, { "Low-pass gain", -3 }, { "Mid freq 1", 250 }, { "Mid gain 1", -1.5f }, { "Mid Q 1", 0.8f },
                                          { "Mid freq 2", 2500 }, { "Mid gain 2", 1.5f }, { "Mid Q 2", 0.6f }, { "High-pass freq", 9000 }, { "High-pass gain", 1.5f } } },
            { "Piano: bright",          { { "Low-pass freq", 150 }, { "Low-pass gain", -1 }, { "Mid freq 1", 400 }, { "Mid gain 1", -1.5f }, { "Mid Q 1", 0.8f },
                                          { "Mid freq 2", 4000 }, { "Mid gain 2", 1.5f }, { "Mid Q 2", 0.7f }, { "High-pass freq", 12000 }, { "High-pass gain", 2 } } },
            { "Organ: smooth",          { { "Low-pass freq", 100 }, { "Low-pass gain", -1 }, { "Mid freq 1", 500 }, { "Mid gain 1", 0 },
                                          { "Mid freq 2", 3000 }, { "Mid gain 2", -2 }, { "Mid Q 2", 0.7f }, { "High-pass freq", 8000 }, { "High-pass gain", -3 } } },
            { "Bass: tight",            { { "Low-pass freq", 60 }, { "Low-pass gain", 2 }, { "Mid freq 1", 250 }, { "Mid gain 1", -3 }, { "Mid Q 1", 1.0f },
                                          { "Mid freq 2", 800 }, { "Mid gain 2", 1.5f }, { "Mid Q 2", 0.8f }, { "High-pass freq", 5000 }, { "High-pass gain", -3 } } },
            { "Guitar: present",        { { "Low-pass freq", 120 }, { "Low-pass gain", -3 }, { "Mid freq 1", 350 }, { "Mid gain 1", -1.5f },
                                          { "Mid freq 2", 2500 }, { "Mid gain 2", 2 }, { "Mid Q 2", 0.7f }, { "High-pass freq", 9000 }, { "High-pass gain", 1 } } },
        };
        static const std::vector<Preset> comp {
            { "Lead vocal: gentle",     { { "threshold", 0.126f }, { "ratio", 1.0f / 3.0f }, { "attack", 10 }, { "release", 120 }, { "output gain", 3 } } },
            { "Lead vocal: strong",     { { "threshold", 0.063f }, { "ratio", 0.2f }, { "attack", 5 }, { "release", 100 }, { "output gain", 6 } } },
            { "Choir: glue",            { { "threshold", 0.18f }, { "ratio", 0.5f }, { "attack", 30 }, { "release", 200 }, { "output gain", 2 } } },
            { "Bass: steady",           { { "threshold", 0.13f }, { "ratio", 0.25f }, { "attack", 20 }, { "release", 150 }, { "output gain", 3 } } },
            { "Drums: punch",           { { "threshold", 0.2f }, { "ratio", 0.25f }, { "attack", 25 }, { "release", 80 }, { "output gain", 2 } } },
            { "Whole song: gentle",     { { "threshold", 0.35f }, { "ratio", 0.5f }, { "attack", 50 }, { "release", 250 }, { "output gain", 1.5f } } },
        };
        static const std::vector<Preset> reverb {
            { "Small room",             { { "room size", 0.35f }, { "damping", 0.6f } } },
            { "Church",                 { { "room size", 0.72f }, { "damping", 0.45f } } },
            { "Big cathedral",          { { "room size", 0.9f },  { "damping", 0.3f } } },
        };
        if (type == te::EqualiserPlugin::xmlTypeName)  return eq;
        if (type == te::CompressorPlugin::xmlTypeName) return comp;
        if (type == te::ReverbPlugin::xmlTypeName)     return reverb;
        return none;
    }

    class ParamRow final : public juce::Component, private juce::Timer
    {
    public:
        explicit ParamRow (te::AutomatableParameter& p) : param (&p)
        {
            name.setText (p.getParameterName(), juce::dontSendNotification);
            name.setFont (juce::FontOptions (13.0f));
            name.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
            addAndMakeVisible (name);

            const auto range = p.getValueRange();
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setRange (range.getStart(), range.getEnd(), 0.0);
            const auto id = p.paramID.toLowerCase();
            if (id.contains ("freq") || id == "frequency")
                slider.setSkewFactorFromMidPoint (1000.0);                 // hearing is logarithmic
            else if (id.endsWith (" q") || id.contains (" q "))
                slider.setSkewFactorFromMidPoint (1.0);
            else if (id == "threshold")
                slider.setSkewFactorFromMidPoint (0.25);
            slider.setValue (p.getCurrentValue(), juce::dontSendNotification);
            slider.setDoubleClickReturnValue (true, p.getDefaultValue().value_or (range.getStart()));
            slider.onDragStart = [this] { param->getEdit().getUndoManager().beginNewTransaction(); };
            slider.onValueChange = [this] { param->setParameter ((float) slider.getValue(), juce::sendNotification); };
            addAndMakeVisible (slider);

            value.setFont (juce::FontOptions (12.5f));
            value.setJustificationType (juce::Justification::centredRight);
            value.setColour (juce::Label::textColourId, juce::Colour (0xffd4a843));
            addAndMakeVisible (value);
            timerCallback();
            startTimerHz (10);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            name.setBounds (r.removeFromLeft (130));
            value.setBounds (r.removeFromRight (90));
            slider.setBounds (r.reduced (4, 0));
        }

    private:
        void timerCallback() override
        {
            if (! slider.isMouseButtonDown())
                slider.setValue (param->getCurrentValue(), juce::dontSendNotification);
            value.setText (param->getCurrentValueAsString(), juce::dontSendNotification);
        }

        te::AutomatableParameter::Ptr param;
        juce::Label name, value;
        juce::Slider slider;
    };

    class GenericEditor final : public te::Plugin::EditorComponent
    {
    public:
        explicit GenericEditor (te::Plugin& p) : plugin (p)
        {
            title.setText (p.getName(), juce::dontSendNotification);
            title.setFont (juce::FontOptions (17.0f, juce::Font::bold));
            title.setColour (juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible (title);

            onOff.setButtonText ("On");
            onOff.setClickingTogglesState (true);
            onOff.setToggleState (p.isEnabled(), juce::dontSendNotification);
            onOff.onClick = [this] { plugin.setEnabled (onOff.getToggleState()); onOff.setButtonText (onOff.getToggleState() ? "On" : "Off"); };
            onOff.setButtonText (p.isEnabled() ? "On" : "Off");
            addAndMakeVisible (onOff);

            const auto& presets = presetsFor (p.getPluginType());
            if (! presets.empty())
            {
                for (size_t i = 0; i < presets.size(); ++i)
                    presetBox.addItem (presets[i].name, (int) i + 1);
                presetBox.setTextWhenNothingSelected ("Starting point...");
                presetBox.onChange = [this, &presets]
                {
                    const auto i = presetBox.getSelectedItemIndex();
                    if (i < 0) return;
                    plugin.edit.getUndoManager().beginNewTransaction();
                    for (auto& [id, value] : presets[(size_t) i].values)
                        if (auto param = plugin.getAutomatableParameterByID (id))
                            param->setParameter (value, juce::sendNotification);
                };
                addAndMakeVisible (presetBox);
            }

            for (auto* param : p.getAutomatableParameters())
                rows.add (new ParamRow (*param));
            for (auto* r : rows)
                addAndMakeVisible (r);

            // Delay: the echo time, also in step with the song's tempo
            if (auto* d = dynamic_cast<te::DelayPlugin*> (&p))
            {
                delayTime.setSliderStyle (juce::Slider::LinearHorizontal);
                delayTime.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 20);
                delayTime.setRange (10.0, 2000.0, 1.0);
                delayTime.setTextValueSuffix (" ms");
                delayTime.setValue (d->lengthMs.get(), juce::dontSendNotification);
                delayTime.onValueChange = [this, d] { d->lengthMs = (int) delayTime.getValue(); };
                addAndMakeVisible (delayTime);

                tempoNotes.addItemList ({ "1/4 note", "Dotted 1/8", "1/8 note", "1/2 note", "Triplet 1/4" }, 1);
                tempoNotes.setTextWhenNothingSelected ("In time with the song...");
                tempoNotes.onChange = [this, d]
                {
                    const double beats[] = { 1.0, 0.75, 0.5, 2.0, 2.0 / 3.0 };
                    const auto i = tempoNotes.getSelectedItemIndex();
                    if (i < 0) return;
                    const auto bpm = plugin.edit.tempoSequence.getTempos()[0]->getBpm();
                    d->lengthMs = juce::roundToInt (60000.0 / bpm * beats[i]);
                    delayTime.setValue (d->lengthMs.get(), juce::dontSendNotification);
                };
                addAndMakeVisible (tempoNotes);
            }

            const auto extra = dynamic_cast<te::DelayPlugin*> (&p) != nullptr ? 70 : 0;
            setSize (520, 70 + rows.size() * 30 + extra);
        }

        bool allowWindowResizing() override                          { return false; }
        juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

        void paint (juce::Graphics& g) override   { g.fillAll (juce::Colour (0xff1b1e24)); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (16, 12);
            auto top = r.removeFromTop (30);
            onOff.setBounds (top.removeFromRight (60));
            if (presetBox.isVisible())
            {
                top.removeFromRight (8);
                presetBox.setBounds (top.removeFromRight (200));
            }
            title.setBounds (top);
            r.removeFromTop (12);
            if (delayTime.isVisible())
            {
                auto row = r.removeFromTop (28);
                delayTime.setBounds (row);
                r.removeFromTop (6);
                tempoNotes.setBounds (r.removeFromTop (28).removeFromLeft (220));
                r.removeFromTop (10);
            }
            for (auto* row : rows)
                row->setBounds (r.removeFromTop (30).reduced (0, 3));
        }

    private:
        te::Plugin& plugin;
        juce::Label title;
        juce::TextButton onOff;
        juce::OwnedArray<ParamRow> rows;
        juce::Slider delayTime;
        juce::ComboBox tempoNotes, presetBox;
    };
}

std::unique_ptr<te::Plugin::EditorComponent> createGenericEditor (te::Plugin& p)
{
    return std::make_unique<GenericEditor> (p);
}

} // namespace Mixing
