#include "Automation.h"
#include "Mixing.h"
#include "PluginHost.h"
#include "StudioEngine.h"

namespace Automation
{
namespace
{
    const juce::Identifier shownId ("gpsAutoShown"), keyId ("gpsAutoKey");

    te::AuxSendPlugin* findSend (te::AudioTrack& track, int bus)
    {
        for (auto* p : track.pluginList.getPlugins())
            if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p))
                if (s->getBusNumber() == bus)
                    return s;
        return nullptr;
    }

    // Effects and plugin instruments whose settings can be automated
    std::vector<te::Plugin*> automatablePlugins (te::AudioTrack& track)
    {
        std::vector<te::Plugin*> result;
        if (auto* inst = StudioEngine::getInstrumentPlugin (track); PluginHost::isPlugin (inst))
            result.push_back (inst);
        for (auto* p : StudioEngine::getEffects (track))
            result.push_back (p);
        return result;
    }

    bool isUsefulParameter (te::AutomatableParameter& p)
    {
        // VST3 plugins list every MIDI controller as a parameter: not useful here
        const auto name = p.getParameterName();
        return name.isNotEmpty() && ! name.startsWith ("MIDI CC") && ! name.startsWith ("MIDI Ch");
    }
}

std::vector<Choice> choicesFor (te::AudioTrack& track)
{
    std::vector<Choice> result;
    auto add = [&] (const juce::String& key, const juce::String& name, const juce::String& group)
    {
        Choice c { key, name, group };
        if (auto* p = resolve (track, key))
            c.hasPoints = p->getCurve().getNumPoints() > 0;
        result.push_back (c);
    };

    add ("vol", "Volume", {});
    add ("pan", "Pan", {});
    add ("send0", "Reverb send", {});
    add ("send1", "Delay send", {});

    for (auto* plugin : automatablePlugins (track))
    {
        int count = 0;
        for (auto* p : plugin->getAutomatableParameters())
        {
            if (! isUsefulParameter (*p) || ++count > 80)
                continue;
            add ("fx:" + plugin->itemID.toString() + ":" + p->paramID, p->getParameterName(), plugin->getName());
        }
    }
    return result;
}

te::AutomatableParameter* resolve (te::AudioTrack& track, const juce::String& key, bool create)
{
    if (key == "vol" || key == "pan")
    {
        if (auto* v = track.getVolumePlugin())
            return key == "vol" ? v->volParam.get() : v->panParam.get();
        return nullptr;
    }

    if (key.startsWith ("send"))
    {
        const auto bus = key.getTrailingIntValue();
        auto* send = findSend (track, bus);
        if (send == nullptr && create)
        {
            Mixing::setSendDb (track, bus, -59.0f);     // creates the send...
            Mixing::setSendDb (track, bus, Mixing::sendOffDb);   // ...switched off
            send = findSend (track, bus);
        }
        return send != nullptr ? send->getAutomatableParameterByID ("send level").get() : nullptr;
    }

    if (key.startsWith ("fx:"))
    {
        const auto pluginId = key.fromFirstOccurrenceOf ("fx:", false, false).upToFirstOccurrenceOf (":", false, false);
        const auto paramId  = key.fromFirstOccurrenceOf ("fx:", false, false).fromFirstOccurrenceOf (":", false, false);
        for (auto* plugin : automatablePlugins (track))
            if (plugin->itemID.toString() == pluginId)
                return plugin->getAutomatableParameterByID (paramId).get();
    }
    return nullptr;
}

juce::String describe (te::AudioTrack& track, const juce::String& key)
{
    if (key == "vol")   return "Volume";
    if (key == "pan")   return "Pan";
    if (key == "send0") return "Reverb send";
    if (key == "send1") return "Delay send";
    if (auto* p = resolve (track, key))
    {
        auto pluginName = p->getPlugin() != nullptr ? p->getPlugin()->getName() : juce::String();
        return (pluginName.isNotEmpty() ? pluginName + ": " : juce::String()) + p->getParameterName();
    }
    return "(effect removed)";
}

bool isShown (const te::AudioTrack& t)               { return (bool) t.state.getProperty (shownId, false); }

juce::String getShownKey (const te::AudioTrack& t)
{
    const auto k = t.state.getProperty (keyId).toString();
    return k.isEmpty() ? juce::String ("vol") : k;
}

void show (te::AudioTrack& t, const juce::String& key)
{
    t.state.setProperty (keyId, key, nullptr);
    t.state.setProperty (shownId, true, nullptr);
    resolve (t, key, true);
}

void hide (te::AudioTrack& t)
{
    t.state.setProperty (shownId, false, nullptr);
}

int countAutomated (te::AudioTrack& track)
{
    int n = 0;
    for (auto& c : choicesFor (track))
        n += c.hasPoints ? 1 : 0;
    return n;
}

bool isWriting (te::Edit& edit)
{
    return edit.getAutomationRecordManager().isWritingAutomation();
}

void setWriting (te::Edit& edit, bool on)
{
    // "Touch": while the song plays, a fader records from the moment you grab it
    // until you let go; then the song's existing automation takes over again.
    for (auto* t : te::getAudioTracks (edit))
        t->automationMode = on ? te::AutomationMode::touch : te::AutomationMode::read;
    if (auto mt = edit.getMasterTrack())
        mt->automationMode = on ? te::AutomationMode::touch : te::AutomationMode::read;

    edit.getAutomationRecordManager().setReadingAutomation (true);
    edit.getAutomationRecordManager().setWritingAutomation (on);
}

void beginGesture (te::AutomatableParameter* p)   { if (p != nullptr) p->parameterChangeGestureBegin(); }
void endGesture (te::AutomatableParameter* p)     { if (p != nullptr) p->parameterChangeGestureEnd(); }

} // namespace Automation
