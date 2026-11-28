#include "VocalTunePlugin.h"
#include "StudioLookAndFeel.h"
#include <tracktion_engine/3rd_party/signalsmith-stretch/signalsmith-stretch.h>

const char* VocalTunePlugin::xmlTypeName = "GospelVocalTune";

namespace
{
    constexpr int downsample = 2;            // pitch is found at half the sample rate (enough for voices)
    constexpr int window = 1024;             // analysis window, in downsampled samples
    constexpr int hop = 256;                 // a new pitch every 256 downsampled samples (about 12 ms)

    const char* noteNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    juce::String noteName (float midi)
    {
        const auto n = juce::roundToInt (midi);
        return juce::String (noteNames[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    }
}

//==============================================================================
struct VocalTunePlugin::Impl
{
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    std::vector<float> ring;                 // downsampled mono, for the pitch finder
    int ringPos = 0, hopCounter = 0, dsPhase = 0;
    float dsSum = 0.0f;
    std::vector<float> frame, diff;
    std::vector<float> inL, inR, outL, outR;

    float shift = 0.0f;                      // semitones now applied
    int target = -1;                         // the note we are moving towards

    void prepare (double sr, int maxBlock)
    {
        stretch.configure (2, (int) std::round (sr * 0.06), (int) std::round (sr * 0.015));
        stretch.setFormantFactor (1.0f, true);         // keep the singer's own voice colour
        stretch.setTransposeSemitones (0.0f);
        ring.assign ((size_t) window, 0.0f);
        frame.assign ((size_t) window, 0.0f);
        diff.assign ((size_t) window, 0.0f);
        ensure (maxBlock);
        clear();
    }

    void ensure (int n)
    {
        if ((int) inL.size() < n)
        {
            inL.resize ((size_t) n); inR.resize ((size_t) n);
            outL.resize ((size_t) n); outR.resize ((size_t) n);
        }
    }

    void clear()
    {
        stretch.reset();
        std::fill (ring.begin(), ring.end(), 0.0f);
        ringPos = hopCounter = dsPhase = 0;
        dsSum = 0.0f;
        shift = 0.0f;
        target = -1;
        stretch.setTransposeSemitones (0.0f);
    }

    // YIN pitch detection on the latest window; returns MIDI note or 0 (no clear pitch)
    float detect (double dsRate, int rangeMode)
    {
        for (int i = 0; i < window; ++i)
            frame[(size_t) i] = ring[(size_t) ((ringPos + i) % window)];

        double energy = 0.0;
        for (auto v : frame) energy += v * v;
        if (energy / window < 1.0e-5)                  // about -50 dB: silence or breath
            return 0.0f;

        const auto minF = rangeMode == 0 ? 65.0 : 120.0;
        const auto maxF = rangeMode == 0 ? 650.0 : 1100.0;
        const int tauMin = juce::jmax (2, (int) (dsRate / maxF));
        const int tauMax = juce::jmin (window / 2, (int) (dsRate / minF) + 1);
        const int w = window - tauMax;

        diff[0] = 0.0f;
        double running = 0.0;
        int best = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double d = 0.0;
            const float* a = frame.data();
            const float* b = frame.data() + tau;
            for (int j = 0; j < w; ++j)
            {
                const auto x = a[j] - b[j];
                d += x * x;
            }
            running += d;
            const auto cmnd = running > 0.0 ? (float) (d * tau / running) : 1.0f;
            diff[(size_t) tau] = cmnd;

            if (best < 0 && tau >= tauMin && tau > 2 && diff[(size_t) tau - 1] < 0.15f && diff[(size_t) tau - 1] <= cmnd
                && diff[(size_t) tau - 1] <= diff[(size_t) tau - 2])
                best = tau - 1;
        }
        if (best < 0)
            return 0.0f;

        // Parabolic interpolation for a finer pitch
        const auto y0 = diff[(size_t) best - 1], y1 = diff[(size_t) best], y2 = diff[(size_t) best + 1];
        const auto denom = y0 - 2.0f * y1 + y2;
        const auto offset = std::abs (denom) > 1.0e-9f ? 0.5f * (y0 - y2) / denom : 0.0f;
        const auto period = (double) best + juce::jlimit (-0.5f, 0.5f, offset);
        const auto freq = dsRate / period;
        return (float) (69.0 + 12.0 * std::log2 (freq / 440.0));
    }
};

//==============================================================================
VocalTunePlugin::VocalTunePlugin (te::PluginCreationInfo info) : te::Plugin (info), impl (std::make_unique<Impl>())
{
    auto* um = getUndoManager();
    strength.referTo (state, "strength", um, 0.8f);
    speedMs.referTo (state, "speedMs", um, 60.0f);
    scaleMode.referTo (state, "scaleMode", um, 1);
    keyOverride.referTo (state, "keyOverride", um, -1);
    minorOverride.referTo (state, "minorOverride", um, false);
    range.referTo (state, "range", um, 1);
    refreshSettings();
    startTimer (1000);
}

VocalTunePlugin::~VocalTunePlugin()
{
    stopTimer();
    notifyListenersOfDeletion();
}

void VocalTunePlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v == state)
        refreshSettings();
    te::Plugin::valueTreePropertyChanged (v, id);
}

std::array<bool, 12> VocalTunePlugin::allowedNotes() const
{
    std::array<bool, 12> notes {};
    if (scaleMode.get() == 2)
    {
        notes.fill (true);
        return notes;
    }

    int root = 0;
    bool minor = false;
    if (keyOverride.get() >= 0)
    {
        root = keyOverride.get() % 12;
        minor = minorOverride.get();
    }
    else if (edit.pitchSequence.getNumPitches() > 0)
    {
        if (auto* p = edit.pitchSequence.getPitch (0))
        {
            root = ((p->getPitch() % 12) + 12) % 12;
            minor = p->getScale() == te::Scale::minor;
        }
    }

    const int major[] = { 0, 2, 4, 5, 7, 9, 11 };
    const int natMinor[] = { 0, 2, 3, 5, 7, 8, 10 };
    for (auto s : minor ? natMinor : major)
        notes[(size_t) ((root + s) % 12)] = true;

    if (scaleMode.get() == 1)
    {
        // Gospel runs: the "blue" notes (b3 and b7 in major; 4# and 7 in minor)
        if (minor) { notes[(size_t) ((root + 6) % 12)] = true;  notes[(size_t) ((root + 11) % 12)] = true; }
        else       { notes[(size_t) ((root + 3) % 12)] = true;  notes[(size_t) ((root + 10) % 12)] = true; }
    }
    return notes;
}

juce::String VocalTunePlugin::describeKey() const
{
    int root = 0;
    bool minor = false;
    if (keyOverride.get() >= 0) { root = keyOverride.get() % 12; minor = minorOverride.get(); }
    else if (edit.pitchSequence.getNumPitches() > 0)
        if (auto* p = edit.pitchSequence.getPitch (0)) { root = ((p->getPitch() % 12) + 12) % 12; minor = p->getScale() == te::Scale::minor; }
    return juce::String (noteNames[root]) + (minor ? " minor" : " major");
}

void VocalTunePlugin::refreshSettings()
{
    aStrength = juce::jlimit (0.0f, 1.0f, strength.get());
    aSpeed = juce::jlimit (1.0f, 500.0f, speedMs.get());
    aRange = range.get();
    std::uint16_t mask = 0;
    const auto notes = allowedNotes();
    for (int i = 0; i < 12; ++i)
        if (notes[(size_t) i]) mask |= (std::uint16_t) (1 << i);
    aAllowed = mask == 0 ? (std::uint16_t) 0xfff : mask;
}

double VocalTunePlugin::getLatencySeconds()
{
    return (impl->stretch.inputLatency() + impl->stretch.outputLatency()) / sampleRate;
}

void VocalTunePlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    impl->prepare (sampleRate, juce::jmax (4096, info.blockSizeSamples * 2));
    refreshSettings();      // the song's key may have changed
}

void VocalTunePlugin::reset()
{
    impl->clear();
}

void VocalTunePlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    auto& im = *impl;
    auto& buffer = *fc.destBuffer;
    const auto n = fc.bufferNumSamples;
    im.ensure (n);

    auto* L = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : L;

    const auto dsRate = sampleRate / downsample;
    const auto hopSeconds = (double) (hop * downsample) / sampleRate;
    const auto strengthNow = aStrength.load();
    const auto allowed = aAllowed.load();
    const auto rangeMode = aRange.load();

    int done = 0;
    while (done < n)
    {
        // Feed the pitch finder until the next hop
        int chunk = 0;
        bool hopReady = false;
        while (done + chunk < n && ! hopReady)
        {
            const auto i = done + chunk;
            im.dsSum += 0.5f * (L[i] + R[i]);
            if (++im.dsPhase == downsample)
            {
                im.ring[(size_t) im.ringPos] = im.dsSum / downsample;
                im.ringPos = (im.ringPos + 1) % window;
                im.dsSum = 0.0f;
                im.dsPhase = 0;
                if (++im.hopCounter >= hop)
                {
                    im.hopCounter = 0;
                    hopReady = true;
                }
            }
            ++chunk;
        }

        if (hopReady)
        {
            const auto sung = im.detect (dsRate, rangeMode);
            float desired = 0.0f, tau = 0.04f;

            if (sung > 0.0f)
            {
                // The nearest allowed note, but keep the current one unless the voice clearly moved away
                auto isAllowed = [allowed] (int note) { return (allowed >> (((note % 12) + 12) % 12)) & 1; };
                int nearest = -1;
                for (int d = 0; d <= 6 && nearest < 0; ++d)
                {
                    const auto base = juce::roundToInt (sung);
                    for (auto c : { base + d, base - d })
                        if (isAllowed (c) && (nearest < 0 || std::abs ((float) c - sung) < std::abs ((float) nearest - sung)))
                            nearest = c;
                }
                if (im.target < 0 || ! isAllowed (im.target) || std::abs ((float) im.target - sung) > 0.65f)
                    im.target = nearest;
                if (im.target >= 0)
                    desired = juce::jlimit (-2.0f, 2.0f, ((float) im.target - sung) * strengthNow);
                tau = aSpeed.load() * 0.001f;
            }
            else
            {
                im.target = -1;
            }

            const auto coeff = 1.0f - std::exp (-(float) hopSeconds / juce::jmax (0.001f, tau));
            im.shift += (desired - im.shift) * coeff;
            im.stretch.setTransposeSemitones (im.shift);

            const auto w = history.write.load();
            history.sung[(size_t) w] = sung;
            history.heard[(size_t) w] = sung > 0.0f ? sung + im.shift : 0.0f;
            history.write = (w + 1) % History::size;
            currentSung = sung;
            currentHeard = sung > 0.0f ? sung + im.shift : 0.0f;
        }

        // Shift this piece of audio
        float* ins[2]  = { im.inL.data(), im.inR.data() };
        float* outs[2] = { im.outL.data(), im.outR.data() };
        std::copy (L + done, L + done + chunk, im.inL.data());
        std::copy (R + done, R + done + chunk, im.inR.data());
        im.stretch.process (ins, chunk, outs, chunk);
        std::copy (im.outL.data(), im.outL.data() + chunk, L + done);
        if (R != L)
            std::copy (im.outR.data(), im.outR.data() + chunk, R + done);

        done += chunk;
    }
}

//==============================================================================
//  The window: key, strength, speed, and the pitch line
//==============================================================================
namespace
{
    using namespace StudioColours;

    class PitchView final : public juce::Component
    {
    public:
        explicit PitchView (VocalTunePlugin& p) : plugin (p) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (header);
            g.fillRoundedRectangle (r, 6.0f);

            // Centre the view on what is being sung
            const auto& h = plugin.history;
            const auto w = h.write.load();
            float sum = 0; int count = 0;
            for (int i = 0; i < VocalTunePlugin::History::size; ++i)
                if (auto v = h.sung[(size_t) i].load(); v > 0) { sum += v; ++count; }
            if (count > 0) centre = centre * 0.8f + 0.2f * (sum / (float) count);
            const auto span = 14.0f;
            auto toY = [&] (float midi) { return r.getCentreY() - (midi - centre) / span * r.getHeight(); };

            // Note lines: allowed notes brighter
            const auto allowed = plugin.allowedNotes();
            g.setFont (studioFont (10.0f));
            for (int note = (int) (centre - span / 2) - 1; note <= (int) (centre + span / 2) + 1; ++note)
            {
                const auto y = toY ((float) note);
                const auto ok = allowed[(size_t) (((note % 12) + 12) % 12)];
                g.setColour (ok ? cardBorder.brighter (0.35f) : cardBorder.withAlpha (0.35f));
                g.drawHorizontalLine ((int) y, 34.0f, r.getRight());
                if (ok)
                {
                    g.setColour (textDim);
                    g.drawText (noteName ((float) note), juce::Rectangle<float> (2.0f, y - 7.0f, 30.0f, 14.0f), juce::Justification::centredLeft);
                }
            }

            // Sung (grey) and heard (gold)
            const auto x0 = 36.0f, width = r.getWidth() - x0 - 4.0f;
            auto drawLine = [&] (const std::array<std::atomic<float>, VocalTunePlugin::History::size>& data, juce::Colour c, float thickness)
            {
                juce::Path p;
                bool down = false;
                for (int i = 0; i < VocalTunePlugin::History::size; ++i)
                {
                    const auto v = data[(size_t) ((w + i) % VocalTunePlugin::History::size)].load();
                    const auto x = x0 + width * (float) i / (float) VocalTunePlugin::History::size;
                    if (v <= 0) { down = false; continue; }
                    const auto y = toY (v);
                    if (! down) p.startNewSubPath (x, y); else p.lineTo (x, y);
                    down = true;
                }
                g.setColour (c);
                g.strokePath (p, juce::PathStrokeType (thickness));
            };
            drawLine (h.sung, textDim.withAlpha (0.9f), 1.5f);
            drawLine (h.heard, gold, 2.2f);

            g.setColour (textDim);
            g.drawText ("grey: as sung    gold: as heard", r.reduced (8.0f, 4.0f), juce::Justification::bottomRight);
        }

    private:
        VocalTunePlugin& plugin;
        float centre = 64.0f;
    };

    class Editor final : public te::Plugin::EditorComponent, private juce::Timer
    {
    public:
        explicit Editor (VocalTunePlugin& p) : plugin (p), view (p)
        {
            title.setText ("Vocal tuning", juce::dontSendNotification);
            title.setFont (studioFont (18.0f, true));
            title.setColour (juce::Label::textColourId, text);
            addAndMakeVisible (title);

            onOff.setClickingTogglesState (true);
            onOff.onClick = [this] { plugin.setEnabled (onOff.getToggleState()); };
            addAndMakeVisible (onOff);

            scaleBox.addItem ("The song's key", 1);
            scaleBox.addItem ("The song's key + blue notes (gospel runs)", 2);
            scaleBox.addItem ("Every note (chromatic)", 3);
            scaleBox.onChange = [this] { plugin.scaleMode = scaleBox.getSelectedItemIndex(); };
            addAndMakeVisible (scaleBox);

            keyBox.addItem ("Follow the song", 1);
            for (int i = 0; i < 12; ++i) keyBox.addItem (juce::String (noteNames[i]) + " major", 10 + i);
            for (int i = 0; i < 12; ++i) keyBox.addItem (juce::String (noteNames[i]) + " minor", 30 + i);
            keyBox.onChange = [this]
            {
                const auto id = keyBox.getSelectedId();
                plugin.keyOverride = id == 1 ? -1 : (id >= 30 ? id - 30 : id - 10);
                plugin.minorOverride = id >= 30;
            };
            addAndMakeVisible (keyBox);

            rangeBox.addItem ("Lower voice (men)", 1);
            rangeBox.addItem ("Higher voice (women, children)", 2);
            rangeBox.onChange = [this] { plugin.range = rangeBox.getSelectedItemIndex(); };
            addAndMakeVisible (rangeBox);

            for (auto* s : { &strengthSlider, &speedSlider })
            {
                s->setSliderStyle (juce::Slider::LinearHorizontal);
                s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                s->setColour (juce::Slider::trackColourId, gold.withAlpha (0.8f));
                s->onDragStart = [this] { plugin.edit.getUndoManager().beginNewTransaction(); };
                addAndMakeVisible (s);
            }
            strengthSlider.setRange (0, 100, 1);
            strengthSlider.setDoubleClickReturnValue (true, 80);
            strengthSlider.onValueChange = [this] { plugin.strength = (float) strengthSlider.getValue() / 100.0f; };
            // Speed: left natural (300 ms), right fast (5 ms)
            speedSlider.setRange (0, 100, 1);
            speedSlider.setDoubleClickReturnValue (true, toSlider (60.0f));
            speedSlider.onValueChange = [this] { plugin.speedMs = toMs ((float) speedSlider.getValue()); };

            for (auto* l : { &scaleLabel, &keyLabel, &rangeLabel, &strengthLabel, &speedLabel, &strengthValue, &speedValue, &note, &now })
            {
                l->setFont (studioFont (12.5f, l == &now));
                l->setColour (juce::Label::textColourId, l == &now ? gold : (l == &note ? textDim : text));
                addAndMakeVisible (l);
            }
            scaleLabel.setText ("Notes", juce::dontSendNotification);
            keyLabel.setText ("Key", juce::dontSendNotification);
            rangeLabel.setText ("Voice", juce::dontSendNotification);
            strengthLabel.setText ("Strength", juce::dontSendNotification);
            speedLabel.setText ("Speed", juce::dontSendNotification);
            now.setFont (studioFont (20.0f, true));
            note.setJustificationType (juce::Justification::topLeft);
            note.setText ("Works on one voice at a time (lead vocal, a soloist). For a choir recorded together, keep the Strength low. "
                          "Slow speed keeps slides and vibrato natural; fast speed makes the robotic effect.",
                          juce::dontSendNotification);

            addAndMakeVisible (view);
            setSize (680, 460);
            timerCallback();
            startTimerHz (30);
        }

        bool allowWindowResizing() override                               { return false; }
        juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

        void paint (juce::Graphics& g) override   { g.fillAll (juce::Colour (0xff1b1e24)); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (16, 12);
            auto top = r.removeFromTop (30);
            onOff.setBounds (top.removeFromRight (60));
            title.setBounds (top);
            r.removeFromTop (8);

            auto row = [&] (juce::Label& l, juce::Component& c, juce::Label* value = nullptr)
            {
                auto a = r.removeFromTop (28);
                l.setBounds (a.removeFromLeft (80));
                if (value != nullptr) value->setBounds (a.removeFromRight (110));
                c.setBounds (a.removeFromLeft (juce::jmin (a.getWidth(), 380)));
                r.removeFromTop (6);
            };
            row (keyLabel, keyBox);
            row (scaleLabel, scaleBox);
            row (rangeLabel, rangeBox);
            row (strengthLabel, strengthSlider, &strengthValue);
            row (speedLabel, speedSlider, &speedValue);
            now.setBounds (r.removeFromTop (28));
            note.setBounds (r.removeFromBottom (34));
            r.removeFromBottom (6);
            view.setBounds (r);
        }

    private:
        static float toMs (float v)       { return 300.0f * std::pow (5.0f / 300.0f, v / 100.0f); }
        static float toSlider (float ms)  { return 100.0f * std::log (ms / 300.0f) / std::log (5.0f / 300.0f); }

        void timerCallback() override
        {
            onOff.setToggleState (plugin.isEnabled(), juce::dontSendNotification);
            onOff.setButtonText (plugin.isEnabled() ? "On" : "Off");
            if (! scaleBox.isPopupActive()) scaleBox.setSelectedItemIndex (juce::jlimit (0, 2, plugin.scaleMode.get()), juce::dontSendNotification);
            if (! rangeBox.isPopupActive()) rangeBox.setSelectedItemIndex (juce::jlimit (0, 1, plugin.range.get()), juce::dontSendNotification);
            if (! keyBox.isPopupActive())
            {
                const auto k = plugin.keyOverride.get();
                keyBox.setSelectedId (k < 0 ? 1 : (plugin.minorOverride.get() ? 30 + k : 10 + k), juce::dontSendNotification);
                keyBox.changeItemText (1, "Follow the song (" + (k < 0 ? plugin.describeKey() : juce::String ("...")) + ")");
            }
            if (! strengthSlider.isMouseButtonDown()) strengthSlider.setValue (plugin.strength.get() * 100.0, juce::dontSendNotification);
            if (! speedSlider.isMouseButtonDown())    speedSlider.setValue (toSlider (plugin.speedMs.get()), juce::dontSendNotification);

            strengthValue.setText (juce::String ((int) strengthSlider.getValue()) + " %", juce::dontSendNotification);
            const auto ms = plugin.speedMs.get();
            speedValue.setText (ms > 120 ? "very natural" : ms > 40 ? "natural" : ms > 15 ? "quick" : "robotic", juce::dontSendNotification);

            const auto sung = plugin.currentSung.load(), heard = plugin.currentHeard.load();
            if (sung > 0)
            {
                const auto cents = juce::roundToInt ((sung - (float) juce::roundToInt (sung)) * 100.0f);
                now.setText ("Singing " + noteName (sung) + (cents >= 0 ? " +" : " ") + juce::String (cents) + " cents   "
                             + juce::String::fromUTF8 ("\xe2\x86\x92") + "   heard " + noteName (heard)
                             + (std::abs (heard - (float) juce::roundToInt (heard)) < 0.1f ? "  (in tune)" : ""),
                             juce::dontSendNotification);
            }
            else
            {
                now.setText ("Play the song: the voice's notes show here", juce::dontSendNotification);
            }
            view.repaint();
        }

        VocalTunePlugin& plugin;
        PitchView view;
        juce::Label title, scaleLabel, keyLabel, rangeLabel, strengthLabel, speedLabel, strengthValue, speedValue, note, now;
        juce::TextButton onOff;
        juce::ComboBox scaleBox, keyBox, rangeBox;
        juce::Slider strengthSlider, speedSlider;
    };
}

std::unique_ptr<te::Plugin::EditorComponent> VocalTunePlugin::createEditor()
{
    return std::make_unique<Editor> (*this);
}
