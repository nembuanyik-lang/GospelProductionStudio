#include "AnalyserPage.h"
#include "Exporter.h"
#include "StudioLookAndFeel.h"

using namespace StudioColours;

namespace
{
    const juce::Colour mixColour  { 0xffd4a843 };     // gold: your mix
    const juce::Colour wholeColour { 0xffeceae4 };    // white: your whole mix, measured
    const juce::Colour refColour  { 0xff5fb0d8 };     // blue: the reference

    float xForFreq (float f, juce::Rectangle<float> r)
    {
        return r.getX() + r.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0));
    }
}

AnalyserPage::AnalyserPage (te::Edit& e) : edit (e)
{
    auto* p = AnalyserPlugin::addTo (edit);
    keep = p;

    auto style = [this] (juce::TextButton& b, const juce::String& tip)
    {
        b.setTooltip (tip);
        b.setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    };
    style (playButton, "Play or stop the song (the analyser shows what you hear)");
    style (mixButton, "Hear your mix");
    style (refButton, "Hear the reference song instead, turned to the same loudness as your mix");
    style (chooseButton, "A finished gospel song you like the sound of (MP3 or WAV)");
    style (clearButton, "Stop using this reference");
    style (measureButton, "Plays the whole song through in the background and measures it (a few seconds)");
    mixButton.setClickingTogglesState (false);
    refButton.setClickingTogglesState (false);

    playButton.onClick = [this]
    {
        auto& t = edit.getTransport();
        if (t.isPlaying()) t.stop (false, false);
        else               t.play (false);
    };
    mixButton.onClick = [this] { if (auto* a = plugin()) a->listenToReference = false; repaint(); };
    refButton.onClick = [this] { if (auto* a = plugin(); a != nullptr && a->isReferenceReady()) { updateGain(); a->listenToReference = true; } repaint(); };
    chooseButton.onClick = [this] { chooseReference(); };
    clearButton.onClick = [this] { if (auto* a = plugin()) a->clearReference(); refreshTexts(); };
    measureButton.onClick = [this] { measureWholeMix(); };

    offset.setSliderStyle (juce::Slider::LinearHorizontal);
    offset.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    offset.setTextValueSuffix (" s");
    offset.setRange (0.0, 1.0, 0.1);
    offset.setTooltip ("Where in the reference to start when your song is at its start (line up the choruses)");
    offset.onValueChange = [this] { if (auto* a = plugin()) a->referenceOffset = (float) offset.getValue(); };
    addAndMakeVisible (offset);
    offsetLabel.setText ("Reference starts at", juce::dontSendNotification);
    offsetLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (offsetLabel);

    for (auto* l : { &refName, &gainNote, &measureStatus, &differences, &help })
    {
        l->setColour (juce::Label::textColourId, textDim);
        l->setFont (studioFont (12.5f));
        l->setJustificationType (juce::Justification::topLeft);
        l->setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (*l);
    }
    refName.setFont (studioFont (13.5f, true));
    refName.setColour (juce::Label::textColourId, refColour);
    differences.setColour (juce::Label::textColourId, text);
    help.setText ("Gold = what you hear now. White = your whole mix (measured). Blue = the reference, at the same loudness. "
                  "Click a band in Tone balance to hear only... (coming with the help assistant). Press Play, then switch A and B.",
                  juce::dontSendNotification);
    help.setText ("Gold = what you hear now. White = your whole song, measured. Blue = the reference at the same loudness. "
                  "Press Play and switch between A and B: the louder one is not the better one, so they are matched.",
                  juce::dontSendNotification);

    liveRate = p != nullptr ? p->getSampleRate() : 44100.0;
    live = Analysis::Averager (liveRate);
    live.setDecay (3.0);
    scratchL.assign (1 << 16, 0.0f);
    scratchR.assign (1 << 16, 0.0f);

    refreshTexts();
    startTimerHz (20);
}

AnalyserPage::~AnalyserPage()
{
    alive->store (false);
    stopTimer();
    renderHandle.reset();
    tempFile.deleteFile();
    if (auto* a = plugin())
        a->listenToReference = false;            // never leave the reference playing
}


void AnalyserPage::refreshTexts()
{
    auto* a = plugin();
    if (a == nullptr)
        return;

    if (a->isReferenceLoading())
        refName.setText ("Loading " + a->getReferenceFile().getFileName() + "...", juce::dontSendNotification);
    else if (a->isReferenceReady())
        refName.setText (a->getReferenceFile().getFileNameWithoutExtension(), juce::dontSendNotification);
    else if (a->getReferenceFile() != juce::File() && ! a->getReferenceFile().existsAsFile())
        refName.setText ("Reference not found: " + a->getReferenceFile().getFileName(), juce::dontSendNotification);
    else
        refName.setText ("No reference song yet", juce::dontSendNotification);

    refButton.setEnabled (a->isReferenceReady());
    clearButton.setVisible (a->getReferenceFile() != juce::File());
    offset.setRange (0.0, juce::jmax (1.0, a->getReferenceLength()), 0.1);
    offset.setValue (a->referenceOffset.get(), juce::dontSendNotification);
    offset.setEnabled (a->isReferenceReady());

    updateGain();

    juce::StringArray lines;
    if (wholeMix.ok && a->isReferenceReady())
        lines = Analysis::describeDifferences (wholeMix, a->getReferenceAnalysis());
    else if (wholeMix.ok)
        lines.add ("Your whole song: " + juce::String (wholeMix.lufs, 1) + " LUFS, loudest peak " + juce::String (wholeMix.truePeakDb, 1)
                   + " dB" + (wholeMix.truePeakDb > 0.0f ? " (over 0 dB: it clips; turn the Master down or use Mastering)" : "")
                   + ". Choose a reference song to compare with.");
    else if (a->isReferenceReady())
        lines.add ("Press \"Measure my whole mix\" for a comparison in words.");
    differences.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);
    repaint();
}

void AnalyserPage::updateGain()
{
    auto* a = plugin();
    if (a == nullptr || ! a->isReferenceReady())
        return;
    // Match the reference to your mix: measured whole song first, else what was heard, else streaming level
    const auto mixLufs = wholeMix.ok ? wholeMix.lufs
                       : a->getLiveIntegratedLufs() > -60.0f ? a->getLiveIntegratedLufs() : -14.0f;
    const auto gain = juce::jlimit (-30.0f, 12.0f, mixLufs - a->getReferenceAnalysis().lufs);
    a->referenceGainDb = gain;
    gainNote.setText ("Reference " + juce::String (a->getReferenceAnalysis().lufs, 1) + " LUFS, played "
                      + (gain <= 0.0f ? "down " : "up ") + juce::String (std::abs (gain), 1) + " dB to match your mix ("
                      + juce::String (mixLufs, 1) + " LUFS" + (wholeMix.ok ? "" : a->getLiveIntegratedLufs() > -60.0f ? ", as heard" : ", estimated") + ")",
                      juce::dontSendNotification);
}

void AnalyserPage::chooseReference()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a reference song", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.mp3;*.wav;*.aif;*.aiff;*.flac;*.ogg");
    juce::Component::SafePointer<AnalyserPage> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        if (auto* a = safe->plugin())
        {
            a->listenToReference = false;
            a->referenceOffset = 0.0f;
            a->loadReference (fc.getResult());
        }
        safe->refreshTexts();
    });
}

void AnalyserPage::measureWholeMix()
{
    if (renderHandle != nullptr)
        return;
    edit.getTransport().stop (false, true);

    te::Renderer::Parameters params (edit);
    params.time = Exporter::songRange (edit);
    if (params.time.getLength() <= te::TimeDuration::fromSeconds (0.5))
    {
        measureStatus.setText ("The song is empty: nothing to measure.", juce::dontSendNotification);
        return;
    }
    params.endAllowance = te::TimeDuration::fromSeconds (1.0);
    params.sampleRateForAudio = 44100.0;
    params.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    params.bitDepth = 32;
    params.usePlugins = true;
    params.useMasterPlugins = true;
    params.canRenderInMono = false;
    params.checkNodesForAudio = false;
    tempFile = juce::File::createTempFile (".wav");
    params.destFile = tempFile;

    measureStatus.setColour (juce::Label::textColourId, gold);
    measureStatus.setText ("Listening to the whole song...", juce::dontSendNotification);
    measureButton.setEnabled (false);

    auto a = alive;
    renderHandle = te::EditRenderer::render (params, [this, a] (tl::expected<juce::File, std::string> result)
    {
        const bool ok = result.has_value();
        juce::MessageManager::callAsync ([this, a, ok]
        {
            if (! a->load()) return;
            renderHandle.reset();
            measureButton.setEnabled (true);
            if (! ok)
            {
                tempFile.deleteFile();
                measureStatus.setText ("Could not play the song through. Try again.", juce::dontSendNotification);
                return;
            }
            wholeMix = Analysis::analyseFile (tempFile);
            tempFile.deleteFile();
            measureStatus.setColour (juce::Label::textColourId, textDim);
            measureStatus.setText (wholeMix.ok ? "Measured: " + juce::String (wholeMix.lufs, 1) + " LUFS, "
                                                 + juce::String (juce::roundToInt (wholeMix.seconds)) + " s" : juce::String ("The song is silent."),
                                   juce::dontSendNotification);
            refreshTexts();
        });
    });
}

void AnalyserPage::timerCallback()
{
    auto* a = plugin();
    if (a == nullptr)
        return;

    if (std::abs (a->getSampleRate() - liveRate) > 1.0)
    {
        liveRate = a->getSampleRate();
        live = Analysis::Averager (liveRate);
        live.setDecay (3.0);
    }
    if (a->getVersion() != seenVersion)
    {
        seenVersion = a->getVersion();
        refreshTexts();
    }
    const auto n = a->readLive (scratchL.data(), scratchR.data(), (int) scratchL.size());
    if (n > 0)
    {
        live.add (scratchL.data(), scratchR.data(), n);
        liveSpectrum = live.getSpectrum();
    }

    const auto playing = edit.getTransport().isPlaying();
    playButton.setButtonText (playing ? "Stop" : "Play");
    const bool b = a->listenToReference.load();
    mixButton.setColour (juce::TextButton::buttonColourId, ! b ? mixColour : card.brighter (0.08f));
    mixButton.setColour (juce::TextButton::textColourOffId, ! b ? header : text);
    refButton.setColour (juce::TextButton::buttonColourId, b ? refColour : card.brighter (0.08f));
    refButton.setColour (juce::TextButton::textColourOffId, b ? header : text);
    if (! wholeMix.ok && playing)
        updateGain();
    repaint (spectrumArea.getUnion (balanceArea).getUnion (stereoArea).getUnion (loudArea));
}

//==============================================================================
void AnalyserPage::paint (juce::Graphics& g)
{
    g.fillAll (background);
    for (auto r : { spectrumArea, balanceArea, stereoArea, loudArea })
    {
        g.setColour (card);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
    }
    drawSpectrum (g, spectrumArea.toFloat().reduced (12.0f, 10.0f));
    drawBalance (g, balanceArea.toFloat().reduced (12.0f, 10.0f));
    drawStereo (g, stereoArea.toFloat().reduced (12.0f, 10.0f));

    // Loudness
    auto r = loudArea.reduced (12, 10);
    g.setColour (text);
    g.setFont (studioFont (14.0f, true));
    g.drawText ("Loudness", r.removeFromTop (20), juce::Justification::centredLeft);
    g.setFont (studioFont (12.5f));
    auto line = [&] (const juce::String& label, float v, juce::Colour c)
    {
        auto row = r.removeFromTop (19);
        g.setColour (textDim);
        g.drawText (label, row.removeFromLeft (juce::jmin (150, row.getWidth() - 74)), juce::Justification::centredLeft, true);
        g.setColour (c);
        g.drawText (v > -70.0f ? juce::String (v, 1) + " LUFS" : juce::String ("-"), row, juce::Justification::centredLeft);
    };
    if (auto* a = plugin())
    {
        line ("Now (last 3 s)", a->getLiveShortTermLufs(), mixColour);
        line ("Since you pressed Play", a->getLiveIntegratedLufs(), mixColour);
        line ("Your whole song", wholeMix.ok ? wholeMix.lufs : -100.0f, wholeColour);
        line ("Reference", a->isReferenceReady() ? a->getReferenceAnalysis().lufs : -100.0f, refColour);
    }
}

void AnalyserPage::drawSpectrum (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (text);
    g.setFont (studioFont (14.0f, true));
    g.drawText ("Spectrum: from deep bass (left) to air (right)", r.removeFromTop (20), juce::Justification::centredLeft);
    r.removeFromTop (4);
    auto labels = r.removeFromBottom (16);
    auto plot = r;

    // Grid
    const float top = -20.0f, bottom = -100.0f;
    auto yFor = [&] (float db) { return plot.getY() + plot.getHeight() * (top - juce::jlimit (bottom, top, db)) / (top - bottom); };
    g.setFont (studioFont (10.5f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const auto x = xForFreq (f, plot);
        g.setColour (cardBorder);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (textDim);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f), juce::Rectangle<float> (x - 20, labels.getY(), 40, 14),
                    juce::Justification::centred);
    }
    for (float db = -30.0f; db > bottom; db -= 20.0f)
    {
        g.setColour (cardBorder.withAlpha (0.6f));
        g.drawHorizontalLine ((int) yFor (db), plot.getX(), plot.getRight());
    }
    // Names of the regions
    g.setColour (textDim.withAlpha (0.7f));
    for (auto& t : Analysis::toneBands())
    {
        const auto x0 = xForFreq (t.from, plot), x1 = xForFreq (t.to, plot);
        g.drawText (t.name, juce::Rectangle<float> (x0, plot.getY() + 2, x1 - x0, 14), juce::Justification::centred);
    }

    auto curve = [&] (const Analysis::Spectrum& s, float shiftDb, juce::Colour c, float thickness)
    {
        if (! s.valid()) return;
        juce::Path path;
        const auto& centres = Analysis::bandCentres();
        for (size_t i = 0; i < centres.size() && i < s.db.size(); ++i)
        {
            const auto p = juce::Point<float> (xForFreq (centres[i], plot), yFor (s.db[i] + shiftDb));
            if (i == 0) path.startNewSubPath (p); else path.lineTo (p);
        }
        g.setColour (c);
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved));
    };

    auto* a = plugin();
    if (a != nullptr && a->isReferenceReady())
        curve (a->getReferenceAnalysis().spectrum, a->referenceGainDb.load(), refColour, 2.0f);
    curve (wholeMix.spectrum, 0.0f, wholeColour.withAlpha (0.8f), 1.5f);
    curve (liveSpectrum, 0.0f, (a != nullptr && a->listenToReference.load()) ? refColour.brighter (0.4f) : mixColour, 2.5f);
}

void AnalyserPage::drawBalance (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (text);
    g.setFont (studioFont (14.0f, true));
    auto* a = plugin();
    const bool haveRef = a != nullptr && a->isReferenceReady();
    const bool haveMix = wholeMix.ok || liveSpectrum.valid();
    g.drawText (haveRef ? "Tone balance against the reference" : "Tone balance", r.removeFromTop (20), juce::Justification::centredLeft);
    r.removeFromTop (6);

    const auto& bands = Analysis::toneBands();
    const auto mine = Analysis::toneBalance (wholeMix.ok ? wholeMix.spectrum : liveSpectrum);
    const auto reference = haveRef ? Analysis::toneBalance (a->getReferenceAnalysis().spectrum) : std::vector<float> (bands.size(), 0.0f);
    const auto shift = haveRef ? a->referenceGainDb.load() : 0.0f;

    // Without a reference: each band against the average of all five
    float avg = 0.0f;
    for (auto v : mine) avg += v;
    avg /= (float) juce::jmax ((size_t) 1, mine.size());

    const auto rowH = r.getHeight() / (float) bands.size();
    for (size_t i = 0; i < bands.size(); ++i)
    {
        auto row = r.removeFromTop (rowH).reduced (0, 3);
        g.setColour (text);
        g.setFont (studioFont (12.5f, true));
        g.drawText (bands[i].name, row.removeFromLeft (80), juce::Justification::centredLeft);
        auto valueArea = row.removeFromRight (70);
        auto bar = row.reduced (4, rowH * 0.15f);
        g.setColour (header);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (cardBorder);
        g.drawVerticalLine ((int) bar.getCentreX(), bar.getY() - 2, bar.getBottom() + 2);
        if (! haveMix) continue;

        const auto d = haveRef ? mine[i] - (reference[i] + shift) : mine[i] - avg;
        const auto clamped = juce::jlimit (-12.0f, 12.0f, d);
        const auto w = bar.getWidth() * 0.5f * std::abs (clamped) / 12.0f;
        const auto c = std::abs (d) < 1.5f ? good : std::abs (d) < 4.0f ? warn : bad;
        g.setColour (haveRef ? c : mixColour.withAlpha (0.8f));
        g.fillRoundedRectangle (clamped >= 0 ? bar.withX (bar.getCentreX()).withWidth (w) : bar.withX (bar.getCentreX() - w).withWidth (w), 3.0f);
        g.setColour (text);
        g.setFont (studioFont (12.0f));
        g.drawText ((d > 0 ? "+" : "") + juce::String (d, 1) + " dB", valueArea, juce::Justification::centredRight);
    }
}

void AnalyserPage::drawStereo (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (text);
    g.setFont (studioFont (14.0f, true));
    g.drawText ("Stereo", r.removeFromTop (20), juce::Justification::centredLeft);

    auto meterRow = [&] (const juce::String& label, float v, float lo, float hi, const juce::String& left, const juce::String& right, juce::Colour c)
    {
        auto row = r.removeFromTop (34);
        g.setColour (textDim);
        g.setFont (studioFont (11.5f));
        g.drawText (label, row.removeFromTop (14), juce::Justification::centredLeft);
        auto bar = row.removeFromTop (8);
        g.setColour (header);
        g.fillRoundedRectangle (bar, 3.0f);
        const auto x = bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
        g.setColour (c);
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 3, bar.getY() - 2, 6, bar.getHeight() + 4), 2.0f);
        g.setColour (textDim.withAlpha (0.7f));
        g.setFont (studioFont (9.5f));
        g.drawText (left, row.removeFromTop (11), juce::Justification::centredLeft);
        g.drawText (right, row.withY (row.getY() - 11).withHeight (11), juce::Justification::centredRight);
    };
    const auto corr = live.getSecondsAnalysed() > 0.5 ? live.getCorrelation() : (wholeMix.ok ? wholeMix.correlation : 1.0f);
    meterRow ("Phase (now)", corr, -1.0f, 1.0f, "out of phase", "mono", corr < 0.0f ? bad : good);
    const auto w = live.getSecondsAnalysed() > 0.5 ? live.getWidth() : wholeMix.width;
    meterRow ("Width (now)", w, 0.0f, 1.0f, "narrow", "wide", mixColour);
    if (auto* a = plugin(); a != nullptr && a->isReferenceReady())
        meterRow ("Width of the reference", a->getReferenceAnalysis().width, 0.0f, 1.0f, "narrow", "wide", refColour);
}

void AnalyserPage::mouseDown (const juce::MouseEvent&) {}

void AnalyserPage::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    topArea = r.removeFromTop (64);
    {
        auto t = topArea;
        auto row1 = t.removeFromTop (30);
        playButton.setBounds (row1.removeFromLeft (80));
        row1.removeFromLeft (10);
        mixButton.setBounds (row1.removeFromLeft (130));
        row1.removeFromLeft (6);
        refButton.setBounds (row1.removeFromLeft (130));
        row1.removeFromLeft (14);
        chooseButton.setBounds (row1.removeFromLeft (190));
        row1.removeFromLeft (6);
        clearButton.setBounds (row1.removeFromLeft (70));
        row1.removeFromLeft (12);
        refName.setBounds (row1);
        t.removeFromTop (6);
        auto row2 = t;
        offsetLabel.setBounds (row2.removeFromLeft (130));
        offset.setBounds (row2.removeFromLeft (260));
        row2.removeFromLeft (14);
        gainNote.setBounds (row2);
    }
    r.removeFromTop (8);

    auto bottom = r.removeFromBottom (juce::jmin (150, r.getHeight() / 3));
    {
        auto b = bottom;
        auto left = b.removeFromLeft (220);
        measureButton.setBounds (left.removeFromTop (30));
        left.removeFromTop (6);
        measureStatus.setBounds (left);
        b.removeFromLeft (14);
        help.setBounds (b.removeFromBottom (40));
        differences.setBounds (b);
    }
    r.removeFromBottom (8);

    auto right = r.removeFromRight (juce::jmin (420, r.getWidth() * 2 / 5));
    r.removeFromRight (10);
    spectrumArea = r;
    balanceArea = right.removeFromTop (right.getHeight() / 2);
    right.removeFromTop (8);
    auto lower = right;
    stereoArea = lower.removeFromLeft (lower.getWidth() / 2 - 4);
    lower.removeFromLeft (8);
    loudArea = lower;
}
