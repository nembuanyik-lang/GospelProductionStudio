#include "InstrumentPanel.h"
#include "OrganPlugin.h"
#include "SamplerPlugin.h"
#include "SoundLibrary.h"
#include "StudioLookAndFeel.h"

namespace
{
    //==========================================================================
    //  One organ drawbar: drag down to pull it out (0-8), like the real thing.
    //==========================================================================
    class Drawbar final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Drawbar (OrganPlugin& o, int i) : organ (o), index (i)
        {
            setTooltip (juce::String (OrganPlugin::drawbarNames[i]) + " drawbar: drag down to pull out (0-8), double-click for 0/8");
        }

        void paint (juce::Graphics& g) override
        {
            const auto value = juce::jlimit (0, 8, organ.drawbars[(size_t) index].get());
            const auto colour = index < 2 ? juce::Colour (0xff6e3b22)
                              : (index == 4 || index == 6 || index == 7) ? juce::Colour (0xff1c1c1c)
                                                                          : juce::Colour (0xffeee8da);
            const auto textColour = (index == 4 || index == 6 || index == 7 || index < 2) ? juce::Colour (0xffeee8da) : juce::Colour (0xff1c1c1c);

            auto area = getLocalBounds().toFloat();
            auto labelArea = area.removeFromTop (18.0f);
            auto valueArea = area.removeFromBottom (20.0f);

            g.setColour (StudioColours::textDim);
            g.setFont (studioFont (11.0f, true));
            g.drawText (OrganPlugin::drawbarNames[index], labelArea, juce::Justification::centred);

            // Slot
            auto slot = area.withSizeKeepingCentre (area.getWidth() * 0.5f, area.getHeight());
            g.setColour (juce::Colour (0xff2e3138));
            g.fillRoundedRectangle (slot, 4.0f);

            // Bar pulled out to its value, with the number markings
            const auto step = slot.getHeight() / 9.0f;
            auto bar = slot.withHeight (step * (float) (value + 1)).reduced (2.0f, 1.0f);
            g.setColour (colour);
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (textColour.withAlpha (0.65f));
            g.setFont (studioFont (9.5f));
            for (int n = 1; n <= value; ++n)
                g.drawText (juce::String (n), bar.withY (slot.getY() + step * (float) n).withHeight (step), juce::Justification::centred);

            // Handle
            auto handle = juce::Rectangle<float> (slot.getX() - 6.0f, bar.getBottom() - 10.0f, slot.getWidth() + 12.0f, 12.0f);
            g.setColour (colour.brighter (0.15f));
            g.fillRoundedRectangle (handle, 4.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (handle, 4.0f, 1.0f);

            g.setColour (StudioColours::gold);
            g.setFont (studioFont (14.0f, true));
            g.drawText (juce::String (value), valueArea, juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent& e) override  { organ.getUndoManager()->beginNewTransaction(); setFromY (e.y); }
        void mouseDrag (const juce::MouseEvent& e) override  { setFromY (e.y); }
        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            auto& v = organ.drawbars[(size_t) index];
            v = v.get() > 0 ? 0 : 8;
            changed();
        }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            auto& v = organ.drawbars[(size_t) index];
            v = juce::jlimit (0, 8, v.get() + (w.deltaY < 0 ? 1 : -1));
            changed();
        }

        std::function<void()> onChange;

    private:
        void setFromY (int y)
        {
            const auto top = 18.0f, height = (float) getHeight() - 38.0f;
            const auto value = juce::jlimit (0, 8, (int) std::floor (((float) y - top) / height * 9.0f));
            if (value != organ.drawbars[(size_t) index].get())
            {
                organ.drawbars[(size_t) index] = value;
                changed();
            }
        }

        void changed()
        {
            organ.presetName = "Custom";
            repaint();
            if (onChange) onChange();
        }

        OrganPlugin& organ;
        int index;
    };

    //==========================================================================
    class PanelContent final : public juce::Component,
                               private juce::Timer
    {
    public:
        PanelContent (StudioEngine& e, te::AudioTrack& t) : engine (e), track (&t)
        {
            instrumentBox.setTooltip ("Instrument for this track");
            int id = 1;
            juce::String lastGroup;
            for (const auto& c : Instruments::all())
            {
                if (lastGroup != c.group)
                {
                    instrumentBox.addSectionHeading (c.group);
                    lastGroup = c.group;
                }
                instrumentBox.addItem (c.name, id++);
            }
            instrumentBox.onChange = [this]
            {
                const auto i = instrumentBox.getSelectedId() - 1;
                if (! juce::isPositiveAndBelow (i, (int) Instruments::all().size()) || track == nullptr)
                    return;
                track->edit.getUndoManager().beginNewTransaction();
                engine.setInstrument (*track, Instruments::all()[(size_t) i].id);
                rebuild();
            };
            addAndMakeVisible (instrumentBox);

            libraryButton.onClick = [] { SoundLibrary::openWindow(); };
            addChildComponent (libraryButton);

            rebuild();
            startTimerHz (8);
            setSize (800, 490);
        }

        void rebuild()
        {
            // Remove the controls of the previous instrument
            for (auto* c : controls)
                removeChildComponent (c);
            controls.clear();
            drawbarComps.clear();
            organ = nullptr;
            sampler = nullptr;

            if (track == nullptr)
                return;

            plugin = StudioEngine::getInstrumentPlugin (*track);
            organ = dynamic_cast<OrganPlugin*> (plugin.get());
            sampler = dynamic_cast<SamplerPlugin*> (plugin.get());

            const auto currentId = StudioEngine::getInstrumentId (*track);
            for (size_t i = 0; i < Instruments::all().size(); ++i)
                if (currentId == Instruments::all()[i].id)
                    instrumentBox.setSelectedId ((int) i + 1, juce::dontSendNotification);

            if (organ != nullptr)      buildOrgan();
            else if (sampler != nullptr) buildSampler();

            resized();
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            using namespace StudioColours;
            g.fillAll (background);

            g.setColour (textDim);
            g.setFont (studioFont (12.0f));
            for (const auto& [area, text] : labels)
                g.drawText (text, area, juce::Justification::bottomLeft, true);

            g.setColour (text);
            g.setFont (studioFont (18.0f, true));
            g.drawText (track != nullptr ? track->getName() : juce::String(), titleArea, juce::Justification::centredLeft, true);

            if (sampler != nullptr)
            {
                g.setFont (studioFont (13.5f));
                const auto st = sampler->getStatus();
                g.setColour (st == SamplerPlugin::Status::ready ? good : (st == SamplerPlugin::Status::loading ? textDim : warn));
                juce::String msg;
                switch (st)
                {
                    case SamplerPlugin::Status::ready:
                        msg = juce::String::fromUTF8 ("Ready  ·  ") + SoundLibrary::formatSize (sampler->getMemoryBytes()) + " of samples in memory";
                        break;
                    case SamplerPlugin::Status::loading:
                        msg = "Loading samples...";
                        break;
                    case SamplerPlugin::Status::notInstalled:
                        msg = "This sound pack is not downloaded yet. Until it is, the track plays the simple Studio Keys sound.";
                        break;
                    case SamplerPlugin::Status::failed:
                        msg = "The sound pack could not be loaded. Remove it and download it again in the Sound Library.";
                        break;
                }
                g.drawFittedText (msg, statusArea, juce::Justification::centredLeft, 2);
            }

            if (organ != nullptr)
            {
                g.setColour (gold);
                g.setFont (studioFont (14.0f, true));
                g.drawText ("Registration  " + organ->getRegistration(), statusArea, juce::Justification::centredLeft);

                // Leslie speed light
                const auto fast = organ->isLeslieFast() && organ->leslie.get() != 0;
                g.setColour (organ->leslie.get() == 0 ? cardBorder : (fast ? bad : good));
                g.fillEllipse (leslieLight.toFloat());
                g.setColour (textDim);
                g.setFont (studioFont (11.5f));
                g.drawText (organ->leslie.get() == 0 ? "Leslie off" : (fast ? "Fast" : "Slow"),
                            leslieLight.translated (16, -2).withWidth (60).withHeight (16), juce::Justification::centredLeft);
            }

            if (plugin != nullptr && organ == nullptr && sampler == nullptr)
            {
                g.setColour (textDim);
                g.setFont (studioFont (14.0f));
                g.drawText ("Studio Keys is the simple built-in sound. Choose Grand Piano or Gospel Organ above for real instruments.",
                            statusArea, juce::Justification::centredLeft, true);
            }
        }

        void resized() override
        {
            labels.clear();
            auto area = getLocalBounds().reduced (18, 14);
            auto top = area.removeFromTop (30);
            titleArea = top.removeFromLeft (200);
            instrumentBox.setBounds (top.removeFromLeft (280));
            top.removeFromLeft (10);
            libraryButton.setBounds (top.removeFromLeft (150));
            area.removeFromTop (8);
            statusArea = area.removeFromTop (34);
            area.removeFromTop (6);

            libraryButton.setVisible (sampler != nullptr && sampler->getStatus() != SamplerPlugin::Status::ready);

            if (organ != nullptr)
                layoutOrgan (area);
            else if (sampler != nullptr)
                layoutSampler (area);
        }

    private:
        //---------------------------------------------------------------------
        void buildOrgan()
        {
            for (int i = 0; i < 9; ++i)
            {
                auto* d = new Drawbar (*organ, i);
                d->onChange = [this] { refreshOrganControls(); };
                drawbarComps.add (d);
                addControl (d);
            }

            auto* presets = addControl (new juce::ComboBox());
            presets->setTextWhenNothingSelected ("Preset...");
            for (size_t i = 0; i < OrganPlugin::presets().size(); ++i)
                presets->addItem (juce::String (OrganPlugin::presets()[i].name) + "   " + OrganPlugin::presets()[i].registration, (int) i + 1);
            presets->setText (organ->presetName.get(), juce::dontSendNotification);
            presets->onChange = [this, presets]
            {
                const auto i = presets->getSelectedId() - 1;
                if (! juce::isPositiveAndBelow (i, (int) OrganPlugin::presets().size()))
                    return;
                organ->getUndoManager()->beginNewTransaction();
                organ->applyPreset (OrganPlugin::presets()[(size_t) i]);
                refreshOrganControls();
            };
            presetBox = presets;

            auto toggle = [this] (const juce::String& text, juce::CachedValue<bool>& value, const juce::String& tip)
            {
                auto* t = addControl (new juce::ToggleButton (text));
                t->setTooltip (tip);
                t->setToggleState (value.get(), juce::dontSendNotification);
                t->onClick = [this, t, &value] { value = t->getToggleState(); organ->presetName = "Custom"; repaint(); };
                t->setWantsKeyboardFocus (false);
                return t;
            };
            percOn    = toggle ("Percussion", organ->percOn, "Percussion: a bright 'ping' on the first note of each phrase (turns off the 1' drawbar)");
            percThird = toggle ("Third", organ->percThird, "On: 3rd harmonic (2 2/3'), off: 2nd harmonic (4')");
            percSoft  = toggle ("Soft", organ->percSoft, "Softer percussion");
            percFast  = toggle ("Fast", organ->percFast, "Fast decay (on) or slow decay (off)");

            auto* vib = addControl (new juce::ComboBox());
            vib->addItemList ({ "Vibrato off", "Vibrato V1", "Vibrato V2", "Vibrato V3", "Chorus C1", "Chorus C2", "Chorus C3" }, 1);
            vib->setSelectedItemIndex (organ->vibrato.get(), juce::dontSendNotification);
            vib->onChange = [this, vib] { organ->vibrato = vib->getSelectedItemIndex(); organ->presetName = "Custom"; };
            vibratoBox = vib;

            const char* leslieNames[] = { "Off", "Slow", "Fast" };
            for (int i = 0; i < 3; ++i)
            {
                auto* b = addControl (new juce::TextButton (leslieNames[i]));
                b->setClickingTogglesState (true);
                b->setRadioGroupId (4101);
                b->setWantsKeyboardFocus (false);
                b->setTooltip ("Leslie speaker. The mod wheel on your keyboard also switches between slow and fast.");
                b->onClick = [this, i] { organ->setLeslie (i); repaint(); };
                leslieButtons.add (b);
            }

            auto slider = [this] (juce::CachedValue<float>& value, double lo, double hi, const juce::String& suffix, const juce::String& tip)
            {
                auto* s = addControl (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight));
                s->setRange (lo, hi, 0.01);
                s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
                s->setTextValueSuffix (suffix);
                s->setValue (value.get(), juce::dontSendNotification);
                s->setTooltip (tip);
                s->onDragStart = [this] { organ->getUndoManager()->beginNewTransaction(); };
                s->onValueChange = [s, &value] { value = (float) s->getValue(); };
                return s;
            };
            driveSlider  = slider (organ->drive, 0.0, 1.0, "", "Overdrive: the growl of the tube amplifier");
            clickSlider  = slider (organ->click, 0.0, 1.0, "", "Key click at the start of each note");
            volumeSlider = slider (organ->volumeDb, -24.0, 6.0, " dB", "Organ volume");

            refreshOrganControls();
        }

        void refreshOrganControls()
        {
            if (organ == nullptr)
                return;
            percOn->setToggleState (organ->percOn.get(), juce::dontSendNotification);
            percThird->setToggleState (organ->percThird.get(), juce::dontSendNotification);
            percSoft->setToggleState (organ->percSoft.get(), juce::dontSendNotification);
            percFast->setToggleState (organ->percFast.get(), juce::dontSendNotification);
            vibratoBox->setSelectedItemIndex (organ->vibrato.get(), juce::dontSendNotification);
            leslieButtons[juce::jlimit (0, 2, organ->leslie.get())]->setToggleState (true, juce::dontSendNotification);
            driveSlider->setValue (organ->drive.get(), juce::dontSendNotification);
            clickSlider->setValue (organ->click.get(), juce::dontSendNotification);
            volumeSlider->setValue (organ->volumeDb.get(), juce::dontSendNotification);
            presetBox->setText (organ->presetName.get(), juce::dontSendNotification);
            for (auto* d : drawbarComps) d->repaint();
            repaint();
        }

        void layoutOrgan (juce::Rectangle<int> area)
        {
            auto bars = area.removeFromLeft (9 * 46);
            for (int i = 0; i < drawbarComps.size(); ++i)
                drawbarComps[i]->setBounds (bars.removeFromLeft (46).reduced (3, 0));

            area.removeFromLeft (24);
            auto row = [&] (int h, const juce::String& label)
            {
                labels.push_back ({ area.removeFromTop (16), label });
                auto r = area.removeFromTop (h);
                area.removeFromTop (8);
                return r;
            };

            presetBox->setBounds (row (28, "Preset"));
            {
                auto r = row (26, "Percussion");
                percOn->setBounds (r.removeFromLeft (110));
                percThird->setBounds (r.removeFromLeft (72));
                percSoft->setBounds (r.removeFromLeft (64));
                percFast->setBounds (r);
            }
            vibratoBox->setBounds (row (28, "Vibrato / chorus"));
            {
                auto r = row (28, "Leslie (or use the mod wheel)");
                leslieLight = r.removeFromRight (80).withSizeKeepingCentre (80, 12).removeFromLeft (12);
                const auto w = r.getWidth() / 3;
                for (auto* b : leslieButtons)
                    b->setBounds (r.removeFromLeft (w).reduced (2, 0));
            }
            driveSlider->setBounds (row (24, "Drive"));
            clickSlider->setBounds (row (24, "Key click"));
            volumeSlider->setBounds (row (24, "Volume"));
        }

        //---------------------------------------------------------------------
        void buildSampler()
        {
            auto slider = [this] (juce::CachedValue<float>& value, double lo, double hi, double step, const juce::String& suffix, const juce::String& tip)
            {
                auto* s = addControl (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight));
                s->setRange (lo, hi, step);
                s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
                s->setTextValueSuffix (suffix);
                s->setValue (value.get(), juce::dontSendNotification);
                s->setTooltip (tip);
                s->onDragStart = [this] { sampler->getUndoManager()->beginNewTransaction(); };
                s->onValueChange = [s, &value] { value = (float) s->getValue(); };
                return s;
            };
            volumeSlider  = slider (sampler->volumeDb, -24.0, 6.0, 0.1, " dB", "Instrument volume");
            toneSlider    = slider (sampler->tone, 0.0, 1.0, 0.01, "", "Tone: lower is darker and warmer, 1 is the natural sound");
            releaseSlider = slider (sampler->releaseScale, 0.25, 3.0, 0.01, "x", "How long notes ring after you let go (1x = natural)");
            toneSlider->setDoubleClickReturnValue (true, 1.0);
            releaseSlider->setDoubleClickReturnValue (true, 1.0);
            volumeSlider->setDoubleClickReturnValue (true, 0.0);

            auto* touch = addControl (new juce::ComboBox());
            touch->addItemList ({ "Light touch (easier to play loud)", "Normal touch", "Firm touch (more dynamic range)" }, 1);
            touch->setSelectedItemIndex (sampler->touch.get(), juce::dontSendNotification);
            touch->onChange = [this, touch] { sampler->touch = touch->getSelectedItemIndex(); };
            touchBox = touch;
        }

        void layoutSampler (juce::Rectangle<int> area)
        {
            area = area.removeFromLeft (520);
            auto row = [&] (int h, const juce::String& label)
            {
                labels.push_back ({ area.removeFromTop (16), label });
                auto r = area.removeFromTop (h);
                area.removeFromTop (10);
                return r;
            };
            volumeSlider->setBounds (row (26, "Volume"));
            toneSlider->setBounds (row (26, "Tone"));
            releaseSlider->setBounds (row (26, "Release"));
            touchBox->setBounds (row (28, "Touch (how your keyboard's velocity is felt)"));
        }

        //---------------------------------------------------------------------
        template <typename C>
        C* addControl (C* c)
        {
            controls.add (c);
            addAndMakeVisible (c);
            return c;
        }

        void timerCallback() override
        {
            // Close if the track or its instrument has gone (deleted or undone)
            if (track == nullptr || plugin == nullptr || plugin->getOwnerTrack() != track)
            {
                stopTimer();
                juce::MessageManager::callAsync ([] { InstrumentPanel::closeAll(); });
                return;
            }
            if (StudioEngine::getInstrumentPlugin (*track) != plugin.get())
            {
                rebuild();
                return;
            }
            libraryButton.setVisible (sampler != nullptr && sampler->getStatus() != SamplerPlugin::Status::ready);
            repaint (statusArea);
            if (organ != nullptr)
                repaint (leslieLight.expanded (4, 4).withWidth (100));
        }

        StudioEngine& engine;
        te::AudioTrack* track;
        te::Plugin::Ptr plugin;
        OrganPlugin* organ = nullptr;
        SamplerPlugin* sampler = nullptr;

        juce::ComboBox instrumentBox;
        juce::TextButton libraryButton { "Open Sound Library" };
        juce::OwnedArray<juce::Component> controls;
        juce::Array<Drawbar*> drawbarComps;
        juce::Array<juce::TextButton*> leslieButtons;
        juce::ComboBox* presetBox = nullptr; juce::ComboBox* vibratoBox = nullptr; juce::ComboBox* touchBox = nullptr;
        juce::ToggleButton* percOn = nullptr; juce::ToggleButton* percThird = nullptr;
        juce::ToggleButton* percSoft = nullptr; juce::ToggleButton* percFast = nullptr;
        juce::Slider* driveSlider = nullptr; juce::Slider* clickSlider = nullptr; juce::Slider* volumeSlider = nullptr;
        juce::Slider* toneSlider = nullptr; juce::Slider* releaseSlider = nullptr;

        juce::Rectangle<int> titleArea, statusArea, leslieLight;
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> labels;
    };

    //==========================================================================
    struct PanelWindow final : public juce::DocumentWindow
    {
        PanelWindow (StudioEngine& e, te::AudioTrack& t)
            : juce::DocumentWindow ("Instrument  -  " + t.getName(), StudioColours::background, juce::DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanelContent (e, t), true);
            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
            setAlwaysOnTop (true);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::MessageManager::callAsync ([] { InstrumentPanel::closeAll(); });
        }
    };

    std::unique_ptr<PanelWindow>& currentWindow()
    {
        static std::unique_ptr<PanelWindow> w;
        return w;
    }
}

namespace InstrumentPanel
{
    void open (StudioEngine& engine, te::AudioTrack& track)
    {
        // A VST3 instrument has its own window
        if (auto* p = StudioEngine::getInstrumentPlugin (track); PluginHost::isPlugin (p))
        {
            PluginHost::showWindow (*p);
            return;
        }

        currentWindow() = std::make_unique<PanelWindow> (engine, track);
        currentWindow()->toFront (true);
    }

    void closeAll()
    {
        currentWindow() = nullptr;
    }
}
