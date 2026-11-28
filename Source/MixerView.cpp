#include "MixerView.h"
#include "Mixing.h"
#include "Automation.h"
#include "MasterPlugin.h"
#include "AnalyserPlugin.h"
#include "PluginHost.h"
#include "InstrumentPanel.h"
#include "StudioLookAndFeel.h"

namespace
{
    using namespace StudioColours;

    constexpr int stripWidth = 100, narrowStripWidth = 86;   // narrower strips so more fit on a 1366 screen

    juce::String dbText (float db)
    {
        if (db <= -59.9f) return "-inf";
        return (db > 0.05f ? "+" : "") + juce::String (db, 1);
    }

    juce::String panText (float pan)
    {
        const auto v = juce::roundToInt (pan * 100.0f);
        if (v == 0) return "C";
        return (v < 0 ? "L" : "R") + juce::String (std::abs (v));
    }

    // Knobs: a dark body, a coloured arc for the amount and a white pointer
    struct KnobLook final : public juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& s) override
        {
            const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
            const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
            const auto centre = bounds.getCentre();
            const auto angle = startAngle + pos * (endAngle - startAngle);
            const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);
            const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0 && std::abs (s.getMinimum() + s.getMaximum()) < 0.001;

            juce::Path track;
            track.addCentredArc (centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f, startAngle, endAngle, true);
            g.setColour (cardBorder.brighter (0.35f));
            g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const auto from = bipolar ? (startAngle + endAngle) / 2.0f : startAngle;
            if (std::abs (angle - from) > 0.01f)
            {
                juce::Path amount;
                amount.addCentredArc (centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
                g.setColour (colour);
                g.strokePath (amount, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            g.setColour (header.brighter (0.15f));
            g.fillEllipse (bounds.reduced (5.0f));
            juce::Path pointer;
            pointer.addRoundedRectangle (-1.2f, -(radius - 6.0f), 2.4f, radius * 0.55f, 1.0f);
            g.setColour (text);
            g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
        }
    };

    KnobLook& knobLook()
    {
        static KnobLook look;
        return look;
    }

    void styleKnob (juce::Slider& s, juce::Colour c)
    {
        s.setLookAndFeel (&knobLook());
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::rotarySliderFillColourId, c);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, cardBorder.brighter (0.2f));
        s.setColour (juce::Slider::thumbColourId, text);
        s.setWantsKeyboardFocus (false);
    }

    //==========================================================================
    //  Stereo level meter (green, then amber near the top, red when too loud)
    //==========================================================================
    class Meter final : public juce::Component
    {
    public:
        ~Meter() override   { detach(); }

        void attach (te::Plugin::Ptr levelMeterPlugin)
        {
            detach();
            plugin = levelMeterPlugin;
            if (auto* lm = dynamic_cast<te::LevelMeterPlugin*> (plugin.get()))
                lm->measurer.addClient (client);
        }

        void detach()
        {
            if (auto* lm = dynamic_cast<te::LevelMeterPlugin*> (plugin.get()))
                lm->measurer.removeClient (client);
            plugin = nullptr;
        }

        void update()
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto db = plugin != nullptr ? client.getAndClearAudioLevel (ch).dB : -100.0f;
                level[ch] = juce::jmax (db, level[ch] - 1.2f);
                if (level[ch] >= peak[ch]) { peak[ch] = level[ch]; peakHold[ch] = 30; }
                else if (--peakHold[ch] <= 0) peak[ch] = juce::jmax (-60.0f, peak[ch] - 0.8f);
                if (db > -0.1f) clipped = true;
            }
            repaint();
        }

        void mouseDown (const juce::MouseEvent&) override   { clipped = false; repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto clipArea = r.removeFromTop (5.0f);
            g.setColour (clipped ? bad : cardBorder);
            g.fillRoundedRectangle (clipArea.reduced (1.0f, 0.0f), 1.5f);
            r.removeFromTop (2.0f);

            const auto w = (r.getWidth() - 2.0f) / 2.0f;
            for (int ch = 0; ch < 2; ++ch)
            {
                auto bar = juce::Rectangle<float> (r.getX() + (float) ch * (w + 2.0f), r.getY(), w, r.getHeight());
                g.setColour (header);
                g.fillRect (bar);

                auto toY = [&bar] (float db) { return bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); };
                const auto top = toY (level[ch]);
                juce::ColourGradient grad (good, 0.0f, bar.getBottom(), bad, 0.0f, toY (6.0f), false);
                grad.addColour (juce::jlimit (0.0, 1.0, (double) (bar.getBottom() - toY (-12.0f)) / bar.getHeight()), good);
                grad.addColour (juce::jlimit (0.0, 1.0, (double) (bar.getBottom() - toY (-3.0f)) / bar.getHeight()), warn);
                g.setGradientFill (grad);
                g.fillRect (bar.withTop (top));

                if (peak[ch] > -59.0f)
                {
                    g.setColour (peak[ch] > -0.5f ? bad : text.withAlpha (0.8f));
                    g.fillRect (bar.getX(), toY (peak[ch]), bar.getWidth(), 1.5f);
                }
            }

            // 0 dB mark
            g.setColour (text.withAlpha (0.35f));
            g.fillRect (r.getX(), r.getBottom() - r.getHeight() * (60.0f / 66.0f), r.getWidth(), 1.0f);
        }

    private:
        te::Plugin::Ptr plugin;
        te::LevelMeasurer::Client client;
        float level[2] { -100.0f, -100.0f }, peak[2] { -100.0f, -100.0f };
        int peakHold[2] {};
        bool clipped = false;
    };

    //==========================================================================
    //  Effects list on a strip (click: open, right-click: options, + : add)
    //==========================================================================
    class InsertList final : public juce::Component
    {
    public:
        std::function<std::vector<te::Plugin*>()> getPlugins;
        std::function<te::Plugin* (te::Plugin::Ptr)> addPlugin;
        std::function<void (te::Plugin&, int delta)> movePlugin;
        te::Edit* edit = nullptr;
        StudioEngine* engine = nullptr;
        static constexpr int rowHeight = 18;
        int maxRows = 5;

        juce::String signature() const
        {
            juce::String s;
            if (getPlugins)
                for (auto* p : getPlugins())
                    s << p->itemID.toString() << (p->isEnabled() ? "+" : "-") << PluginHost::describe (*p) << ";";
            return s;
        }

        void refreshIfChanged()
        {
            const auto s = signature();
            if (s != last) { last = s; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            const auto plugins = getPlugins ? getPlugins() : std::vector<te::Plugin*>();
            g.setFont (studioFont (11.5f));
            for (int i = 0; i <= maxRows; ++i)
            {
                auto r = juce::Rectangle<int> (0, i * rowHeight, getWidth(), rowHeight - 2);
                if (i < (int) plugins.size() && i < maxRows)
                {
                    auto* p = plugins[(size_t) i];
                    const auto on = p->isEnabled();
                    g.setColour (on ? juce::Colour (0xff2c3a52) : card.brighter (0.05f));
                    g.fillRoundedRectangle (r.toFloat(), 3.0f);
                    g.setColour (on ? text : textDim);
                    auto name = PluginHost::describe (*p).replace (" (off)", "");
                    g.drawText (name, r.reduced (5, 0), juce::Justification::centredLeft, true);
                    if (! on)
                    {
                        g.setColour (textDim.withAlpha (0.6f));
                        g.drawLine ((float) r.getX() + 4.0f, (float) r.getCentreY(), (float) r.getRight() - 4.0f, (float) r.getCentreY());
                    }
                }
                else if (i == juce::jmin ((int) plugins.size(), maxRows))
                {
                    g.setColour (textDim.withAlpha (0.55f));
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.0f, 1.0f);
                    g.setColour (textDim);
                    const auto extra = (int) plugins.size() - maxRows;
                    g.drawText (extra > 0 ? "+ " + juce::String (extra) + " more / add" : "+ Effect", r, juce::Justification::centred);
                    break;
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto plugins = getPlugins ? getPlugins() : std::vector<te::Plugin*>();
            const auto row = e.y / rowHeight;
            const auto shown = juce::jmin ((int) plugins.size(), maxRows);

            if (row < shown)
            {
                auto* p = plugins[(size_t) row];
                if (e.mods.isPopupMenu())
                    showPluginMenu (*p, row, (int) plugins.size());
                else
                    PluginHost::showWindow (*p);
            }
            else if (row == shown)
            {
                showAddMenu (plugins);
            }
        }

    private:
        juce::String last;

        void showPluginMenu (te::Plugin& p, int index, int count)
        {
            juce::PopupMenu m;
            m.addItem (1, "Open window");
            m.addItem (2, p.isEnabled() ? "Turn off (bypass)" : "Turn on again");
            m.addItem (3, "Move earlier", index > 0);
            m.addItem (4, "Move later", index + 1 < count);
            m.addSeparator();
            m.addItem (5, "Remove");

            juce::Component::SafePointer<InsertList> safe (this);
            const auto id = p.itemID;
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, id] (int r)
            {
                if (safe == nullptr || r == 0 || safe->edit == nullptr)
                    return;
                te::Plugin* plugin = nullptr;
                for (auto* q : safe->getPlugins())
                    if (q->itemID == id)
                        plugin = q;
                if (plugin == nullptr)
                    return;

                if (r == 1) { PluginHost::showWindow (*plugin); return; }
                safe->edit->getUndoManager().beginNewTransaction();
                if (r == 2)      plugin->setEnabled (! plugin->isEnabled());
                else if (r == 3) safe->movePlugin (*plugin, -1);
                else if (r == 4) safe->movePlugin (*plugin, 1);
                else if (r == 5)
                {
                    if (plugin->windowState != nullptr)
                        plugin->windowState->closeWindowExplicitly();
                    plugin->deleteFromParent();
                }
                safe->refreshIfChanged();
            });
        }

        void showAddMenu (const std::vector<te::Plugin*>& current)
        {
            juce::PopupMenu m;
            if ((int) current.size() > maxRows)
            {
                m.addSectionHeader ("More effects on this strip");
                for (int i = maxRows; i < (int) current.size(); ++i)
                    m.addItem (900 + i, juce::String (i + 1) + ". " + PluginHost::describe (*current[(size_t) i]));
            }
            m.addSectionHeader ("Built-in effects");
            for (int i = 0; i < (int) Mixing::builtInEffects().size(); ++i)
                m.addItem (100 + i, Mixing::builtInEffects()[(size_t) i].name);
            m.addSectionHeader ("Plugin effects (VST3)");
            const auto effects = engine != nullptr ? PluginHost::getEffects (engine->getEngine()) : juce::Array<juce::PluginDescription>();
            for (int i = 0; i < effects.size(); ++i)
                m.addItem (300 + i, effects.getReference (i).name);
            if (effects.isEmpty())
                m.addItem (299, "None yet: open Plugins and press Scan", false, false);
            m.addSeparator();
            m.addItem (1, "Plugins...");

            juce::Component::SafePointer<InsertList> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, effects] (int r)
            {
                if (safe == nullptr || r == 0 || safe->edit == nullptr)
                    return;
                if (r == 1) { PluginHost::openManager (safe->engine->getEngine()); return; }
                if (r >= 900)
                {
                    const auto list = safe->getPlugins();
                    if ((size_t) (r - 900) < list.size())
                        PluginHost::showWindow (*list[(size_t) (r - 900)]);
                    return;
                }

                te::Plugin::Ptr p;
                if (r >= 300 && r - 300 < effects.size())
                    p = safe->edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, effects.getReference (r - 300));
                else if (r >= 100 && (size_t) (r - 100) < Mixing::builtInEffects().size())
                    p = Mixing::createBuiltIn (*safe->edit, Mixing::builtInEffects()[(size_t) (r - 100)].type);

                if (p != nullptr)
                {
                    safe->edit->getUndoManager().beginNewTransaction();
                    if (auto* added = safe->addPlugin (p))
                        PluginHost::showWindow (*added);
                    safe->refreshIfChanged();
                }
            });
        }
    };
}

//==============================================================================
//  Where a track goes: the master or a group bus (plan Step 14)
//==============================================================================
namespace
{
    void showOutputMenu (te::AudioTrack& track, juce::Component* target, std::function<void()> changed)
    {
        auto& edit = track.edit;
        juce::PopupMenu menu, newBus;
        auto* current = Mixing::getBusOf (track);
        menu.addSectionHeader ("Where " + track.getName() + " goes");
        menu.addItem (1, "Master (no bus)", true, current == nullptr);
        const auto buses = Mixing::getGroupBuses (edit);
        for (int i = 0; i < buses.size(); ++i)
            menu.addItem (10 + i, buses[i]->getName(), true, buses[i] == current);
        for (size_t i = 0; i < Mixing::busKinds().size(); ++i)
            newBus.addItem (100 + (int) i, juce::String ("New ") + Mixing::busKinds()[i].name);
        menu.addSeparator();
        menu.addSubMenu ("Into a new bus", newBus);

        te::Track::Ptr keep (&track);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [keep, changed] (int r)
        {
            auto* t = dynamic_cast<te::AudioTrack*> (keep.get());
            if (t == nullptr || r == 0) return;
            t->edit.getUndoManager().beginNewTransaction ("Output");
            if (r == 1) Mixing::routeToBus (*t, nullptr);
            else if (r >= 100) { if (auto* b = Mixing::createGroupBus (t->edit, r - 100)) Mixing::routeToBus (*t, b); }
            else
            {
                const auto list = Mixing::getGroupBuses (t->edit);
                if (juce::isPositiveAndBelow (r - 10, list.size())) Mixing::routeToBus (*t, list[r - 10]);
            }
            if (changed) changed();
        });
    }

    void askForBusName (juce::Component* parent, te::AudioTrack& bus)
    {
        auto* window = new juce::AlertWindow ("Rename the bus", {}, juce::MessageBoxIconType::NoIcon, parent);
        window->addTextEditor ("name", bus.getName());
        window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        te::Track::Ptr keep (&bus);
        window->enterModalState (true, juce::ModalCallbackFunction::create ([window, keep] (int result)
        {
            const auto text = window->getTextEditorContents ("name").trim();
            if (result == 1 && text.isNotEmpty() && keep != nullptr)
            {
                keep->edit.getUndoManager().beginNewTransaction ("Rename bus");
                keep->setName (text);
            }
        }), true);
    }
}

//==============================================================================
//  One channel strip
//==============================================================================
class MixerView::Strip final : public juce::Component
{
public:
    enum class Kind { track, returnTrack, master };

    Strip (StudioEngine& e, te::Edit& ed, te::AudioTrack* t)
        : engine (e), edit (ed), track (t),
          kind (t == nullptr ? Kind::master : Mixing::isReturnTrack (*t) ? Kind::returnTrack : Kind::track)
    {
        const auto colour = track != nullptr ? track->getColour() : gold;

        // Effects
        inserts.edit = &edit;
        inserts.engine = &engine;
        if (track != nullptr)
        {
            inserts.getPlugins = [this] { return StudioEngine::getEffects (*track); };
            inserts.addPlugin  = [this] (te::Plugin::Ptr p) { return engine.addEffect (*track, p); };
            inserts.movePlugin = [] (te::Plugin& p, int d) { StudioEngine::moveEffect (p, d); };
        }
        else
        {
            inserts.getPlugins = [this] { return Mixing::getMasterEffects (edit); };
            inserts.addPlugin  = [this] (te::Plugin::Ptr p) { return Mixing::addMasterEffect (edit, p); };
            inserts.movePlugin = [this] (te::Plugin& p, int d) { moveMasterEffect (p, d); };
        }
        addAndMakeVisible (inserts);

        // Sends to the shared reverb and delay
        if (kind == Kind::track)
        {
            for (int bus = 0; bus < Mixing::numBuses; ++bus)
            {
                auto& s = sends[bus];
                styleKnob (s, bus == Mixing::reverbBus ? juce::Colour (0xff7e8fe0) : juce::Colour (0xff5fb8b0));
                s.setRange (Mixing::sendOffDb, 6.0, 0.1);
                s.setSkewFactorFromMidPoint (-18.0);
                s.setDoubleClickReturnValue (true, Mixing::sendOffDb);
                s.setTooltip ("Send to the shared " + Mixing::busName (bus).toLowerCase() + " (double-click: off)");
                s.onDragStart = [this, bus] { edit.getUndoManager().beginNewTransaction();
                                              Automation::beginGesture (Automation::resolve (*track, "send" + juce::String (bus), true)); };
                s.onDragEnd = [this, bus] { Automation::endGesture (Automation::resolve (*track, "send" + juce::String (bus))); };
                s.onValueChange = [this, bus] { Mixing::setSendDb (*track, bus, (float) sends[bus].getValue()); repaint(); };
                addAndMakeVisible (s);
            }
        }

        // Pan
        styleKnob (pan, colour);
        pan.setRange (-1.0, 1.0, 0.01);
        pan.setDoubleClickReturnValue (true, 0.0);
        pan.setTooltip ("Pan: left or right (double-click: centre)");
        pan.onDragStart = [this] { edit.getUndoManager().beginNewTransaction(); Automation::beginGesture (panParam()); };
        pan.onDragEnd   = [this] { Automation::endGesture (panParam()); };
        pan.onValueChange = [this] { if (auto* v = volumePlugin()) v->setPan ((float) pan.getValue()); repaint(); };
        addAndMakeVisible (pan);

        // Fader
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (-60.0, 6.0, 0.1);
        fader.setSkewFactorFromMidPoint (-12.0);
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.setColour (juce::Slider::trackColourId, colour.withAlpha (0.85f));
        fader.setColour (juce::Slider::backgroundColourId, header);
        fader.setColour (juce::Slider::thumbColourId, text);
        fader.setTooltip ("Volume (double-click: 0 dB)");
        fader.setWantsKeyboardFocus (false);
        fader.onDragStart = [this] { edit.getUndoManager().beginNewTransaction(); Automation::beginGesture (volParam()); };
        fader.onDragEnd   = [this] { Automation::endGesture (volParam()); };
        fader.onValueChange = [this] { if (auto* v = volumePlugin()) v->setVolumeDb ((float) fader.getValue()); repaint(); };
        addAndMakeVisible (fader);

        addAndMakeVisible (meter);
        if (track != nullptr)
            meter.attach (track->getLevelMeterPlugin());
        else
            meter.attach (ensureMasterMeter());

        // Mastering (Step 15), where the master has no sends
        if (kind == Kind::master)
        {
            masteringButton.setTooltip ("Mastering: the last polish and loudness for the whole song");
            masteringButton.setWantsKeyboardFocus (false);
            masteringButton.onClick = [] { if (MasterPlugin::onOpenRequest) MasterPlugin::onOpenRequest(); };
            addAndMakeVisible (masteringButton);
            analyserButton.setTooltip ("Analyser: spectrum, stereo, loudness, and comparison with a reference song");
            analyserButton.setWantsKeyboardFocus (false);
            analyserButton.onClick = [] { if (AnalyserPlugin::onOpenRequest) AnalyserPlugin::onOpenRequest(); };
            addAndMakeVisible (analyserButton);
        }

        // Mute and solo
        if (track != nullptr)
        {
            for (auto* b : { &muteButton, &soloButton })
            {
                b->setWantsKeyboardFocus (false);
                addAndMakeVisible (*b);
            }
            muteButton.setTooltip ("Mute");
            soloButton.setTooltip ("Solo: hear only this (and other soloed tracks)");
            muteButton.onClick = [this] { edit.getUndoManager().beginNewTransaction(); track->setMute (! track->isMuted (false)); update(); };
            soloButton.onClick = [this] { edit.getUndoManager().beginNewTransaction(); track->setSolo (! track->isSolo (false)); update(); };
        }

        update();
    }

    ~Strip() override
    {
        meter.detach();
        for (auto& s : sends) s.setLookAndFeel (nullptr);
        pan.setLookAndFeel (nullptr);
    }

    te::AudioTrack* getTrack() const    { return track; }

    void update()
    {
        if (auto* v = volumePlugin())
        {
            if (! fader.isMouseButtonDown()) fader.setValue (v->getVolumeDb(), juce::dontSendNotification);
            if (! pan.isMouseButtonDown())   pan.setValue (v->getPan(), juce::dontSendNotification);
        }
        if (kind == Kind::track)
            for (int bus = 0; bus < Mixing::numBuses; ++bus)
                if (! sends[bus].isMouseButtonDown())
                    sends[bus].setValue (Mixing::getSendDb (*track, bus), juce::dontSendNotification);

        if (track != nullptr)
        {
            const auto muted = track->isMuted (false), solo = track->isSolo (false);
            muteButton.setColour (juce::TextButton::buttonColourId, muted ? warn : card.brighter (0.08f));
            muteButton.setColour (juce::TextButton::textColourOffId, muted ? header : text);
            soloButton.setColour (juce::TextButton::buttonColourId, solo ? gold : card.brighter (0.08f));
            soloButton.setColour (juce::TextButton::textColourOffId, solo ? header : text);
        }

        if (kind == Kind::master)
        {
            const auto on = MasterPlugin::isActive (edit);
            masteringButton.setColour (juce::TextButton::buttonColourId, on ? gold : card.brighter (0.08f));
            masteringButton.setColour (juce::TextButton::textColourOffId, on ? header : text);
        }

        const auto sub = subtitle();
        if (sub != lastSubtitle) { lastSubtitle = sub; repaint(); }
        const auto out = outputText();
        if (out != lastOutput) { lastOutput = out; repaint(); }

        inserts.refreshIfChanged();
        meter.update();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (3, 0).toFloat();
        g.setColour (kind == Kind::master ? card.brighter (0.04f) : card);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (cardBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        const auto colour = track != nullptr ? track->getColour() : gold;
        g.setColour (colour);
        g.fillRoundedRectangle (r.withHeight (4.0f), 2.0f);

        g.setColour (text);
        g.setFont (studioFont (13.0f, true));
        g.drawText (name(), nameArea, juce::Justification::centred, true);
        g.setColour (textDim);
        g.setFont (studioFont (10.5f));
        g.drawText (lastSubtitle, subArea, juce::Justification::centred, true);
        if (canRoute())
        {
            auto* bus = Mixing::getBusOf (*track);
            g.setColour (bus != nullptr ? bus->getColour().brighter (0.3f) : textDim.withAlpha (0.7f));
            g.drawText (lastOutput, outArea, juce::Justification::centred, true);
        }

        g.setFont (studioFont (10.5f));
        if (kind == Kind::track)
        {
            for (int bus = 0; bus < Mixing::numBuses; ++bus)
            {
                const auto v = (float) sends[bus].getValue();
                g.setColour (v > Mixing::sendOffDb ? text : textDim);
                g.drawText (Mixing::busName (bus) == "Reverb" ? "Rev" : "Dly", sendLabels[bus].withTrimmedBottom (12), juce::Justification::centred);
                g.drawText (v > Mixing::sendOffDb ? dbText (v) : "off", sendLabels[bus].withTrimmedTop (12), juce::Justification::centred);
            }
        }
        g.setColour (textDim);
        g.drawText ("Pan " + panText ((float) pan.getValue()), panLabel, juce::Justification::centred);

        g.setColour (text);
        g.setFont (studioFont (12.0f, true));
        g.drawText (dbText ((float) fader.getValue()) + " dB", dbArea, juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 0);
        r.removeFromTop (8);
        nameArea = r.removeFromTop (18);
        subArea = r.removeFromTop (14);
        outArea = canRoute() ? r.removeFromTop (14) : juce::Rectangle<int>();
        r.removeFromTop (canRoute() ? 2 : 6);
        inserts.maxRows = getHeight() < 500 ? 3 : 5;          // small screens: more room for the fader
        inserts.setBounds (r.removeFromTop (InsertList::rowHeight * (inserts.maxRows + 1)));
        r.removeFromTop (6);

        auto sendRow = r.removeFromTop (58);
        if (kind == Kind::master)
        {
            masteringButton.setBounds (sendRow.removeFromTop (28));
            sendRow.removeFromTop (4);
            analyserButton.setBounds (sendRow.removeFromTop (24));
        }
        if (kind == Kind::track)
        {
            for (int bus = 0; bus < Mixing::numBuses; ++bus)
            {
                auto col = sendRow.removeFromLeft (sendRow.getWidth() / (Mixing::numBuses - bus));
                sends[bus].setBounds (col.removeFromTop (34).withSizeKeepingCentre (34, 34));
                sendLabels[bus] = col;
            }
        }

        auto panRow = r.removeFromTop (48);
        pan.setBounds (panRow.removeFromTop (32).withSizeKeepingCentre (32, 32));
        panLabel = panRow;

        auto bottom = r.removeFromBottom (track != nullptr ? 26 : 4);
        if (track != nullptr)
        {
            auto b = bottom.reduced (0, 2);
            const auto w = (b.getWidth() - 4) / 2;
            muteButton.setBounds (b.removeFromLeft (w));
            soloButton.setBounds (b.removeFromRight (w));
        }
        dbArea = r.removeFromBottom (18);
        r.removeFromBottom (2);

        auto faderArea = r;
        meter.setBounds (faderArea.removeFromRight (18));
        faderArea.removeFromRight (4);
        fader.setBounds (faderArea);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Clicking the output chooses where the track goes
        if (canRoute() && outArea.contains (e.getPosition()))
        {
            showOutputMenu (*track, this, [] {});
            return;
        }

        // A bus: rename or remove it
        if (kind == Kind::track && Mixing::isGroupBus (*track) && (e.mods.isPopupMenu() || nameArea.contains (e.getPosition())))
        {
            juce::PopupMenu m;
            m.addItem (1, "Rename...");
            m.addItem (2, "Remove the bus (its tracks go straight to the master)");
            te::Track::Ptr keep (track);
            juce::Component::SafePointer<Strip> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [keep, safe] (int r)
            {
                auto* bus = dynamic_cast<te::AudioTrack*> (keep.get());
                if (bus == nullptr || r == 0) return;
                if (r == 1) askForBusName (safe.getComponent(), *bus);
                if (r == 2)
                {
                    bus->edit.getUndoManager().beginNewTransaction ("Remove bus");
                    Mixing::deleteGroupBus (*bus);
                }
            });
            return;
        }

        // Clicking the instrument name opens it
        if (kind == Kind::track && subArea.contains (e.getPosition()) && StudioEngine::isInstrumentTrack (*track))
            InstrumentPanel::open (engine, *track);
    }

private:
    juce::String name() const
    {
        if (kind == Kind::master) return "Master";
        if (kind == Kind::returnTrack) return Mixing::busName (Mixing::getReturnBus (*track));
        return track->getName();
    }

    juce::String subtitle() const
    {
        if (kind == Kind::master) return "Whole song";
        if (kind == Kind::returnTrack) return "Shared";
        if (Mixing::isGroupBus (*track))
        {
            const auto n = Mixing::getBusMembers (*track).size();
            return "Bus  -  " + juce::String (n) + (n == 1 ? " track" : " tracks");
        }
        if (StudioEngine::isInstrumentTrack (*track))
            return StudioEngine::describeInstrument (*track);
        return "Audio";
    }

    te::AutomatableParameter* volParam()   { auto* v = volumePlugin(); return v != nullptr ? v->volParam.get() : nullptr; }
    te::AutomatableParameter* panParam()   { auto* v = volumePlugin(); return v != nullptr ? v->panParam.get() : nullptr; }

    te::VolumeAndPanPlugin* volumePlugin()
    {
        if (track != nullptr) return track->getVolumePlugin();
        return edit.getMasterVolumePlugin().get();
    }

    te::Plugin::Ptr ensureMasterMeter()
    {
        for (auto* p : edit.getMasterPluginList().getPlugins())
            if (dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr)
                return p;
        auto p = edit.getPluginCache().createNewPlugin (te::LevelMeterPlugin::xmlTypeName, {});
        if (p != nullptr)
            edit.getMasterPluginList().insertPlugin (p, edit.getMasterPluginList().getPlugins().size(), nullptr);
        return p;
    }

    void moveMasterEffect (te::Plugin& p, int delta)
    {
        auto effects = Mixing::getMasterEffects (edit);
        const auto it = std::find (effects.begin(), effects.end(), &p);
        if (it == effects.end()) return;
        const auto pos = (int) (it - effects.begin()) + delta;
        if (pos < 0 || pos >= (int) effects.size()) return;
        auto& list = edit.getMasterPluginList();
        const auto target = list.getPlugins().indexOf (effects[(size_t) pos]);
        te::Plugin::Ptr keep (&p);
        p.removeFromParent();
        list.insertPlugin (keep, target, nullptr);
    }

    StudioEngine& engine;
    te::Edit& edit;
    te::AudioTrack* track;
    const Kind kind;

    InsertList inserts;
    juce::Slider sends[Mixing::numBuses], pan, fader;
    Meter meter;
    juce::TextButton muteButton { "M" }, soloButton { "S" }, masteringButton { "Mastering" }, analyserButton { "Analyser" };
    juce::Rectangle<int> nameArea, subArea, outArea, sendLabels[Mixing::numBuses], panLabel, dbArea;
    juce::String lastSubtitle, lastOutput;

    bool canRoute() const   { return kind == Kind::track && ! Mixing::isGroupBus (*track); }

    juce::String outputText() const
    {
        if (! canRoute()) return {};
        auto* bus = Mixing::getBusOf (*track);
        return juce::String::fromUTF8 ("\xe2\x86\x92 ") + (bus != nullptr ? bus->getName() : juce::String ("Master"));
    }
};

//==============================================================================
MixerView::MixerView (StudioEngine& e) : engine (e)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);

    hint.setText ("Faders set each part's volume. Turn \"Rev\" up on the voices and keys to place everyone in the same room. "
                  "Click \"+ Effect\" for EQ, compressor and your plugins. Double-click any control to reset it.",
                  juce::dontSendNotification);
    hint.setFont (studioFont (12.0f));
    hint.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (hint);

    addBusButton.setTooltip ("A bus gathers several tracks (all the choir voices) under one fader with shared effects");
    addBusButton.setWantsKeyboardFocus (false);
    addBusButton.onClick = [this]
    {
        juce::PopupMenu m;
        for (size_t i = 0; i < Mixing::busKinds().size(); ++i)
            m.addItem ((int) i + 1, juce::String (Mixing::busKinds()[i].name) + "   (" + Mixing::busKinds()[i].hint + ")");
        juce::Component::SafePointer<MixerView> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addBusButton), [safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            if (auto* ed = safe->engine.getEdit())
            {
                ed->getUndoManager().beginNewTransaction ("New bus");
                Mixing::createGroupBus (*ed, r - 1);
                safe->hint.setText ("New bus added. Click the arrow line under a track's name (\"" + juce::String::fromUTF8 ("\xe2\x86\x92") + " Master\") to send it into the bus.",
                                    juce::dontSendNotification);
            }
        });
    };
    addAndMakeVisible (addBusButton);
}

MixerView::~MixerView()
{
    stopTimer();
    releaseEdit();
}

void MixerView::releaseEdit()
{
    strips.clear();
    master.reset();
    lastSignature = {};
}

juce::String MixerView::signature() const
{
    juce::String s;
    if (auto* ed = engine.getEdit())
    {
        s << (juce::pointer_sized_int) ed << ":";
        for (auto* t : te::getAudioTracks (*ed))
            s << t->itemID.toString() << t->getName() << (int) t->getColour().getARGB() << ",";
    }
    return s;
}

void MixerView::rebuild()
{
    strips.clear();
    master.reset();
    lastSignature = signature();

    auto* ed = engine.getEdit();
    if (ed == nullptr)
        return;

    for (auto* t : Mixing::getSongTracks (*ed))
        content.addAndMakeVisible (strips.add (new Strip (engine, *ed, t)));
    for (auto* b : Mixing::getGroupBuses (*ed))
        content.addAndMakeVisible (strips.add (new Strip (engine, *ed, b)));
    for (int bus = 0; bus < Mixing::numBuses; ++bus)
        if (auto* r = Mixing::findReturn (*ed, bus))
            content.addAndMakeVisible (strips.add (new Strip (engine, *ed, r)));

    master = std::make_unique<Strip> (engine, *ed, nullptr);
    addAndMakeVisible (*master);
    resized();
}

void MixerView::visibilityChanged()
{
    if (isVisible())
    {
        rebuild();
        startTimerHz (24);
    }
    else
    {
        stopTimer();
        releaseEdit();
    }
}

void MixerView::timerCallback()
{
    if (signature() != lastSignature)
        rebuild();

    for (auto* s : strips)
        s->update();
    if (master != nullptr)
        master->update();
}

void MixerView::paint (juce::Graphics& g)
{
    g.setColour (card.darker (0.25f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
}

void MixerView::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto top = r.removeFromTop (30);
    addBusButton.setBounds (top.removeFromRight (90).reduced (0, 3));
    hint.setBounds (top);
    hint.setVisible (getHeight() > 420);

    if (master != nullptr)
    {
        master->setBounds (r.removeFromRight (stripWidth + 6));
        r.removeFromRight (8);
    }
    viewport.setBounds (r);

    // Gaps before the group buses and the shared reverb and delay
    int gaps = 0;
    {
        int group = 0;
        for (auto* s : strips)
        {
            const int g = s->getTrack() == nullptr ? 0 : Mixing::isReturnTrack (*s->getTrack()) ? 2 : Mixing::isGroupBus (*s->getTrack()) ? 1 : 0;
            if (g != group) { gaps += 10; group = g; }
        }
    }
    // Full width when everything fits; otherwise narrower strips, and scroll only if still needed
    const auto n = juce::jmax (1, strips.size());
    const auto w = juce::jlimit (narrowStripWidth, stripWidth, (r.getWidth() - gaps) / n);
    const auto total = strips.size() * w + gaps;
    const auto h = r.getHeight() - (total > r.getWidth() ? viewport.getScrollBarThickness() : 0);
    content.setSize (juce::jmax (r.getWidth(), total), h);

    int x = 0;
    int group = 0;
    for (auto* s : strips)
    {
        const int g = s->getTrack() == nullptr ? 0 : Mixing::isReturnTrack (*s->getTrack()) ? 2 : Mixing::isGroupBus (*s->getTrack()) ? 1 : 0;
        if (g != group) { x += 10; group = g; }
        s->setBounds (x, 0, w, h);
        x += w;
    }
    if (content.getWidth() < x)
        content.setSize (x, h);
}
