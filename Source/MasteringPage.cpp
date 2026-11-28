#include "MasteringPage.h"
#include "Exporter.h"
#include "StudioLookAndFeel.h"

using namespace StudioColours;

namespace
{
    const float targets[] = { -16.0f, -14.0f, -13.0f, -11.0f, -10.0f };
    const char* targetNames[] = { "Gentle (-16 LUFS)", "Streaming (-14 LUFS)", "Full (-13 LUFS)", "Loud (-11 LUFS)", "Very loud, radio (-10 LUFS)" };

    // Integrated loudness of a rendered file
    float measureFile (const juce::File& f)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
        if (reader == nullptr)
            return -100.0f;

        LoudnessMeter meter;
        meter.prepare (reader->sampleRate, (double) reader->lengthInSamples / reader->sampleRate + 2.0);
        juce::AudioBuffer<float> buffer (2, 8192);
        for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += 8192)
        {
            const auto num = (int) juce::jmin ((juce::int64) 8192, reader->lengthInSamples - pos);
            reader->read (&buffer, 0, num, pos, true, true);
            meter.process (buffer.getReadPointer (0), reader->numChannels > 1 ? buffer.getReadPointer (1) : nullptr, num);
        }
        return meter.getIntegrated();
    }
}

MasteringPage::MasteringPage (te::Edit& e) : edit (e)
{
    onButton.onClick = [this] { setOn (onButton.getToggleState()); };
    addAndMakeVisible (onButton);

    for (size_t i = 0; i < MasterPlugin::presets().size(); ++i)
        presetBox.addItem (MasterPlugin::presets()[i].name, (int) i + 1);
    presetBox.setTextWhenNothingSelected ("Choose a starting point...");
    presetBox.onChange = [this]
    {
        const auto i = presetBox.getSelectedItemIndex();
        if (i < 0) return;
        if (plugin() == nullptr) setOn (true);
        if (auto* p = plugin())
        {
            edit.getUndoManager().beginNewTransaction();
            p->applyPreset (MasterPlugin::presets()[(size_t) i]);
            refreshFromPlugin();
        }
    };
    addAndMakeVisible (presetBox);

    compareButton.setClickingTogglesState (true);
    compareButton.setTooltip ("Hear the song without mastering, at the same loudness, to judge the difference fairly");
    compareButton.setColour (juce::TextButton::buttonOnColourId, warn);
    compareButton.onClick = [this]
    {
        if (auto* p = plugin())
            p->compare = compareButton.getToggleState();
        compareButton.setButtonText (compareButton.getToggleState() ? "Hearing: BEFORE (click for after)" : "Compare: hear before");
    };
    addAndMakeVisible (compareButton);

    for (auto* l : { &presetHint, &help, &autoStatus, &faderNote })
    {
        l->setFont (studioFont (12.0f));
        l->setColour (juce::Label::textColourId, textDim);
        l->setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (l);
    }
    help.setText ("Mastering is the last polish on the whole song, so it sounds good on phones, speakers and the radio. "
                  "Start from a preset, play the loudest part of the song, and use Compare to hear the difference.",
                  juce::dontSendNotification);

    auto set = [this] (auto member) { return [this, member] (float v) { if (auto* p = plugin()) p->*member = v; }; };
    setupRow (warmth,  "Warmth",  -6, 6, 0, 0.1, set (&MasterPlugin::warmth));
    setupRow (clarity, "Clarity", -6, 6, 0, 0.1, set (&MasterPlugin::clarity));
    setupRow (air,     "Air",     -6, 6, 0, 0.1, set (&MasterPlugin::air));
    setupRow (glue,    "Glue",    0, 100, 25, 1, [this] (float v) { if (auto* p = plugin()) p->glue = v / 100.0f; });
    setupRow (width,   "Width",   50, 150, 100, 1, [this] (float v) { if (auto* p = plugin()) p->width = v / 100.0f; });
    setupRow (push,    "Push",    -12, 18, 3, 0.1, set (&MasterPlugin::push));
    warmth.slider.setTooltip ("Low end (around 110 Hz): more body, or less boom");
    clarity.slider.setTooltip ("Presence (around 3 kHz): voices come forward, or less harsh");
    air.slider.setTooltip ("Top (above 11 kHz): sparkle and openness");
    glue.slider.setTooltip ("Gentle compression that makes the band sound together");
    width.slider.setTooltip ("Stereo width: 100% is as mixed");
    push.slider.setTooltip ("How hard the song is pushed into the limiter: louder, but too much flattens it");

    lowCutButton.onClick = [this] { if (auto* p = plugin()) p->lowCut = lowCutButton.getToggleState(); };
    addAndMakeVisible (lowCutButton);

    for (int i = 0; i < 5; ++i)
        targetBox.addItem (targetNames[i], i + 1);
    targetBox.setSelectedItemIndex (1, juce::dontSendNotification);
    targetBox.onChange = [this] { if (auto* p = plugin()) p->targetLufs = targets[juce::jmax (0, targetBox.getSelectedItemIndex())]; };
    addAndMakeVisible (targetBox);

    autoButton.setColour (juce::TextButton::buttonColourId, gold);
    autoButton.setColour (juce::TextButton::textColourOffId, header);
    autoButton.onClick = [this] { startAutoLoudness(); };
    addAndMakeVisible (autoButton);

    resetFaderButton.onClick = [this]
    {
        if (auto v = edit.getMasterVolumePlugin())
        {
            edit.getUndoManager().beginNewTransaction();
            v->setVolumeDb (0.0f);
        }
    };
    addChildComponent (resetFaderButton);

    refreshFromPlugin();
    startTimerHz (15);
}

MasteringPage::~MasteringPage()
{
    alive->store (false);
    if (auto* p = plugin())
        p->compare = false;            // never leave the song playing "before"
    renderHandle.reset();
    tempFile.deleteFile();
}

void MasteringPage::setupRow (Row& row, const juce::String& name, double min, double max, double def, double step,
                              std::function<void (float)> onChange)
{
    row.name.setText (name, juce::dontSendNotification);
    row.name.setFont (studioFont (13.0f, true));
    row.name.setColour (juce::Label::textColourId, text);
    row.value.setFont (studioFont (12.5f));
    row.value.setColour (juce::Label::textColourId, gold);
    row.value.setJustificationType (juce::Justification::centredRight);
    row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    row.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    row.slider.setRange (min, max, step);
    row.slider.setDoubleClickReturnValue (true, def);
    row.slider.setValue (def, juce::dontSendNotification);
    row.slider.setColour (juce::Slider::trackColourId, gold.withAlpha (0.8f));
    row.slider.onDragStart = [this] { edit.getUndoManager().beginNewTransaction(); };
    row.slider.onValueChange = [&row, onChange]
    {
        onChange ((float) row.slider.getValue());
    };
    for (juce::Component* c : { (juce::Component*) &row.name, (juce::Component*) &row.value, (juce::Component*) &row.slider })
        addAndMakeVisible (c);
}

void MasteringPage::setOn (bool on)
{
    edit.getUndoManager().beginNewTransaction();
    auto* p = plugin();
    if (on && p == nullptr)
    {
        p = MasterPlugin::addTo (edit);
        // The Master fader comes after mastering: at 0 dB the limiter's ceiling is the real ceiling
        if (auto v = edit.getMasterVolumePlugin())
            v->setVolumeDb (0.0f);
    }
    if (p != nullptr)
        p->setEnabled (on);
    refreshFromPlugin();
}

void MasteringPage::refreshFromPlugin()
{
    auto* p = plugin();
    const auto on = p != nullptr && p->isEnabled();
    onButton.setToggleState (on, juce::dontSendNotification);

    for (auto* row : { &warmth, &clarity, &air, &glue, &width, &push })
        row->slider.setEnabled (on && ! autoRunning);
    for (juce::Component* c : { (juce::Component*) &lowCutButton, (juce::Component*) &targetBox,
                                (juce::Component*) &compareButton })
        c->setEnabled (on && ! autoRunning);
    autoButton.setEnabled (on && ! autoRunning);
    presetBox.setEnabled (! autoRunning);

    if (p == nullptr)
    {
        presetHint.setText ("Mastering is off. Choose a starting point or tick \"Mastering on\".", juce::dontSendNotification);
        return;
    }

    auto sync = [] (Row& r, double v) { if (! r.slider.isMouseButtonDown()) r.slider.setValue (v, juce::dontSendNotification); };
    sync (warmth, p->warmth.get());
    sync (clarity, p->clarity.get());
    sync (air, p->air.get());
    sync (glue, p->glue.get() * 100.0);
    sync (width, p->width.get() * 100.0);
    sync (push, p->push.get());
    lowCutButton.setToggleState (p->lowCut.get(), juce::dontSendNotification);

    int ti = 1;
    for (int i = 0; i < 5; ++i)
        if (std::abs (targets[i] - p->targetLufs.get()) < 0.05f) ti = i;
    targetBox.setSelectedItemIndex (ti, juce::dontSendNotification);

    int pi = -1;
    for (size_t i = 0; i < MasterPlugin::presets().size(); ++i)
        if (p->presetName.get() == MasterPlugin::presets()[i].name) pi = (int) i;
    presetBox.setSelectedItemIndex (pi, juce::dontSendNotification);
    presetHint.setText (pi >= 0 ? juce::String (MasterPlugin::presets()[(size_t) pi].hint) : juce::String(), juce::dontSendNotification);
}

void MasteringPage::timerCallback()
{
    refreshFromPlugin();

    // Meters start again each time the song starts playing
    const auto playing = edit.getTransport().isPlaying();
    if (playing && ! wasPlaying)
        if (auto* p = plugin())
            p->resetLoudness();
    wasPlaying = playing;

    auto fmt = [] (float v, const char* unit, bool sign) { return (sign && v > 0.05f ? "+" : "") + juce::String (v, 1) + unit; };
    warmth.value.setText (fmt ((float) warmth.slider.getValue(), " dB", true), juce::dontSendNotification);
    clarity.value.setText (fmt ((float) clarity.slider.getValue(), " dB", true), juce::dontSendNotification);
    air.value.setText (fmt ((float) air.slider.getValue(), " dB", true), juce::dontSendNotification);
    glue.value.setText (juce::String ((int) glue.slider.getValue()) + " %", juce::dontSendNotification);
    width.value.setText (juce::String ((int) width.slider.getValue()) + " %", juce::dontSendNotification);
    push.value.setText (fmt ((float) push.slider.getValue(), " dB", true), juce::dontSendNotification);

    // The Master fader comes after mastering
    const auto faderDb = edit.getMasterVolumePlugin() != nullptr ? edit.getMasterVolumePlugin()->getVolumeDb() : 0.0f;
    const auto faderOff = std::abs (faderDb) > 0.05f && plugin() != nullptr;
    faderNote.setText (faderOff ? "The Master fader is at " + juce::String (faderDb, 1) + " dB, so the song comes out that much "
                                  + (faderDb < 0 ? "quieter" : "louder") + " than shown here." : juce::String(),
                       juce::dontSendNotification);
    resetFaderButton.setVisible (faderOff);

    if (auto* p = plugin())
    {
        shownGlue = juce::jmax (p->meters.glueReductionDb.load(), shownGlue * 0.85f);
        shownLimit = juce::jmax (p->meters.limiterReductionDb.load(), shownLimit * 0.85f);
    }
    repaint (meterCard);
}

void MasteringPage::paint (juce::Graphics& g)
{
    for (auto* r : { &toneCard, &glueCard, &loudCard, &meterCard })
    {
        g.setColour (card);
        g.fillRoundedRectangle (r->toFloat(), 8.0f);
        g.setColour (cardBorder);
        g.drawRoundedRectangle (r->toFloat().reduced (0.5f), 8.0f, 1.0f);
    }

    g.setColour (text);
    g.setFont (studioFont (14.0f, true));
    g.drawText ("Tone", toneCard.reduced (14, 8).removeFromTop (20), juce::Justification::topLeft);
    g.drawText ("Glue and width", glueCard.reduced (14, 8).removeFromTop (20), juce::Justification::topLeft);
    g.drawText ("Loudness", loudCard.reduced (14, 8).removeFromTop (20), juce::Justification::topLeft);
    g.drawText ("Meters (play the song)", meterCard.reduced (14, 8).removeFromTop (20), juce::Justification::topLeft);

    // Meters
    auto r = meterCard.reduced (14, 8);
    r.removeFromTop (26);
    auto* p = plugin();
    auto lufs = [] (float v) { return v > -70.0f ? juce::String (v, 1) + " LUFS" : juce::String ("-"); };

    auto big = r.removeFromTop (44);
    g.setColour (gold);
    g.setFont (studioFont (28.0f, true));
    g.drawText (p != nullptr ? lufs (p->meters.shortTermOut.load()) : juce::String ("-"), big, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (studioFont (11.5f));
    g.drawText ("now (last 3 s)", r.removeFromTop (16), juce::Justification::centredLeft);
    r.removeFromTop (6);

    auto line = [&] (const juce::String& label, const juce::String& value)
    {
        auto row = r.removeFromTop (20);
        g.setColour (textDim);
        g.drawText (label, row, juce::Justification::centredLeft);
        g.setColour (text);
        g.drawText (value, row, juce::Justification::centredRight);
    };
    g.setFont (studioFont (12.5f));
    line ("Whole song so far", p != nullptr ? lufs (p->meters.integratedOut.load()) : "-");
    line ("Before mastering", p != nullptr ? lufs (p->meters.shortTermIn.load()) : "-");
    line ("Highest peak", p != nullptr && p->meters.peakOutDb.load() > -90.0f ? juce::String (p->meters.peakOutDb.load(), 1) + " dB" : "-");
    r.removeFromTop (8);

    auto bar = [&] (const juce::String& label, float db, float maxDb)
    {
        auto row = r.removeFromTop (34);
        g.setColour (textDim);
        g.drawText (label + "  " + juce::String (db, 1) + " dB", row.removeFromTop (16), juce::Justification::centredLeft);
        auto b = row.reduced (0, 3).toFloat();
        g.setColour (header);
        g.fillRoundedRectangle (b, 3.0f);
        const auto frac = juce::jlimit (0.0f, 1.0f, db / maxDb);
        g.setColour (db > maxDb * 0.66f ? warn : gold);
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * frac), 3.0f);
    };
    bar ("Glue working", shownGlue, 8.0f);
    bar ("Limiter working", shownLimit, 8.0f);
    if (shownLimit > 6.0f)
    {
        g.setColour (warn);
        g.drawFittedText ("The limiter works very hard: lower Push for a more natural sound.", r.removeFromTop (34), juce::Justification::topLeft, 2);
    }
}

void MasteringPage::resized()
{
    auto r = getLocalBounds();
    const auto compact = getHeight() < 470;

    auto top = r.removeFromTop (32);
    onButton.setBounds (top.removeFromLeft (150));
    top.removeFromLeft (8);
    presetBox.setBounds (top.removeFromLeft (230));
    top.removeFromLeft (12);
    compareButton.setBounds (top.removeFromLeft (250));
    r.removeFromTop (4);
    auto hints = r.removeFromTop (compact ? 18 : 36);
    presetHint.setBounds (hints.removeFromTop (18));
    help.setBounds (hints);
    help.setVisible (! compact);
    r.removeFromTop (6);

    const auto gap = 12;
    const auto w = (r.getWidth() - 3 * gap) / 4;
    toneCard = r.removeFromLeft (w);   r.removeFromLeft (gap);
    glueCard = r.removeFromLeft (w);   r.removeFromLeft (gap);
    loudCard = r.removeFromLeft (w);   r.removeFromLeft (gap);
    meterCard = r;

    const auto rowH = compact ? 44 : 52;
    auto layoutRow = [rowH] (Row& row, juce::Rectangle<int>& area)
    {
        auto a = area.removeFromTop (rowH);
        auto labels = a.removeFromTop (18);
        row.value.setBounds (labels.removeFromRight (70));
        row.name.setBounds (labels);
        row.slider.setBounds (a.reduced (0, 2));
    };

    auto t = toneCard.reduced (14, 8); t.removeFromTop (28);
    layoutRow (warmth, t); layoutRow (clarity, t); layoutRow (air, t);
    lowCutButton.setBounds (t.removeFromTop (26));

    auto gl = glueCard.reduced (14, 8); gl.removeFromTop (28);
    layoutRow (glue, gl); layoutRow (width, gl);

    auto l = loudCard.reduced (14, 8); l.removeFromTop (28);
    targetBox.setBounds (l.removeFromTop (28));
    l.removeFromTop (6);
    autoButton.setBounds (l.removeFromTop (34));
    l.removeFromTop (4);
    autoStatus.setBounds (l.removeFromTop (compact ? 34 : 48));
    layoutRow (push, l);
    faderNote.setBounds (l.removeFromTop (34));
    resetFaderButton.setBounds (l.removeFromTop (26).removeFromLeft (200));
}

//==============================================================================
//  "Set loudness for the whole song": render through the mastering, measure,
//  adjust Push, and once more to be precise.
//==============================================================================
void MasteringPage::startAutoLoudness()
{
    auto* p = plugin();
    if (p == nullptr || autoRunning)
        return;

    const auto range = Exporter::songRange (edit);
    if (range.getLength().inSeconds() < 1.0)
    {
        finishAuto ("There is nothing in the song to measure yet.", false);
        return;
    }

    edit.getTransport().stop (false, true);
    p->compare = false;
    compareButton.setToggleState (false, juce::dontSendNotification);
    edit.getUndoManager().beginNewTransaction();
    autoRunning = true;
    autoPass = 0;
    refreshFromPlugin();
    autoStep();
}

void MasteringPage::autoStep()
{
    te::Renderer::Parameters params (edit);
    params.time = Exporter::songRange (edit);
    params.endAllowance = te::TimeDuration::fromSeconds (3.0);
    params.sampleRateForAudio = 44100.0;
    params.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    params.bitDepth = 32;
    params.usePlugins = true;
    params.useMasterPlugins = true;
    params.canRenderInMono = false;
    params.checkNodesForAudio = false;
    tempFile = juce::File::createTempFile (".wav");
    params.destFile = tempFile;

    autoStatus.setColour (juce::Label::textColourId, gold);
    autoStatus.setText ("Listening to the whole song (pass " + juce::String (autoPass + 1) + ")...", juce::dontSendNotification);

    auto a = alive;
    renderHandle = te::EditRenderer::render (params, [this, a] (tl::expected<juce::File, std::string> result)
    {
        const bool ok = result.has_value();
        juce::MessageManager::callAsync ([this, a, ok]
        {
            if (! a->load())
                return;
            renderHandle.reset();
            if (! ok)
            {
                tempFile.deleteFile();
                finishAuto ("Could not play the song through. Try again.", false);
                return;
            }

            const auto measured = measureFile (tempFile);
            tempFile.deleteFile();
            auto* p = plugin();
            if (p == nullptr || measured < -60.0f)
            {
                finishAuto ("The song is silent: nothing to measure.", false);
                return;
            }

            p->resetLoudness();      // the meters start fresh when the song plays again
            const auto diff = p->targetLufs.get() - measured;
            if (std::abs (diff) <= 0.5f || (autoPass >= 2 && std::abs (diff) <= 1.0f))
            {
                finishAuto ("Done: the whole song measures " + juce::String (measured, 1) + " LUFS (aim "
                            + juce::String (p->targetLufs.get(), 0) + "). Push is " + juce::String (p->push.get(), 1) + " dB.", true);
                return;
            }

            const auto newPush = juce::jlimit (-12.0f, 18.0f, p->push.get() + diff);
            const auto stuck = std::abs (newPush - p->push.get()) < 0.05f;
            p->push = newPush;

            if (! stuck && ++autoPass < 3)
            {
                autoStep();      // check again: the limiter changes the result a little
                return;
            }

            const juce::String msg = newPush >= 17.9f
                ? "The song needs more than Push allows. Raise the track faders a little, then try again."
                : "Nearly there: the song measured " + juce::String (measured, 1) + " LUFS. Push is now " + juce::String (newPush, 1) + " dB.";
            finishAuto (msg, newPush < 17.9f);
        });
    });
}

void MasteringPage::finishAuto (const juce::String& message, bool good)
{
    autoRunning = false;
    autoStatus.setColour (juce::Label::textColourId, good ? StudioColours::good : warn);
    autoStatus.setText (message, juce::dontSendNotification);
    refreshFromPlugin();
}
