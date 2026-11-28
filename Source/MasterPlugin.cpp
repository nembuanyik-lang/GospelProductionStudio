#include "MasterPlugin.h"

const char* MasterPlugin::xmlTypeName = "GospelMastering";

namespace
{
    // RBJ cookbook filters
    template <typename B>
    void makeShelf (B& f, double sr, double hz, double gainDb, bool high)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / sr;
        const auto c = std::cos (w), s = std::sin (w);
        const auto alpha = s / 2.0 * std::sqrt (2.0);         // shelf slope 1
        const auto sq = 2.0 * std::sqrt (A) * alpha;
        double b0, b1, b2, a0, a1, a2;
        if (high)
        {
            b0 = A * ((A + 1) + (A - 1) * c + sq); b1 = -2 * A * ((A - 1) + (A + 1) * c); b2 = A * ((A + 1) + (A - 1) * c - sq);
            a0 = (A + 1) - (A - 1) * c + sq;       a1 = 2 * ((A - 1) - (A + 1) * c);       a2 = (A + 1) - (A - 1) * c - sq;
        }
        else
        {
            b0 = A * ((A + 1) - (A - 1) * c + sq); b1 = 2 * A * ((A - 1) - (A + 1) * c);  b2 = A * ((A + 1) - (A - 1) * c - sq);
            a0 = (A + 1) + (A - 1) * c + sq;       a1 = -2 * ((A - 1) + (A + 1) * c);      a2 = (A + 1) + (A - 1) * c - sq;
        }
        f.b0 = (float) (b0 / a0); f.b1 = (float) (b1 / a0); f.b2 = (float) (b2 / a0); f.a1 = (float) (a1 / a0); f.a2 = (float) (a2 / a0);
    }

    template <typename B>
    void makePeak (B& f, double sr, double hz, double gainDb, double q)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / sr;
        const auto c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const auto a0 = 1 + alpha / A;
        f.b0 = (float) ((1 + alpha * A) / a0); f.b1 = (float) (-2 * c / a0); f.b2 = (float) ((1 - alpha * A) / a0);
        f.a1 = (float) (-2 * c / a0);          f.a2 = (float) ((1 - alpha / A) / a0);
    }

    template <typename B>
    void makeHighPass (B& f, double sr, double hz)
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / sr;
        const auto c = std::cos (w), alpha = std::sin (w) / (2.0 * 0.7071);
        const auto a0 = 1 + alpha;
        f.b0 = (float) ((1 + c) / 2 / a0); f.b1 = (float) (-(1 + c) / a0); f.b2 = f.b0;
        f.a1 = (float) (-2 * c / a0);      f.a2 = (float) ((1 - alpha) / a0);
    }

    float dbToGain (float db)   { return std::pow (10.0f, db / 20.0f); }
    float gainToDb (float g)    { return g > 1.0e-6f ? 20.0f * std::log10 (g) : -120.0f; }
}

const std::vector<MasterPlugin::Preset>& MasterPlugin::presets()
{
    static const std::vector<Preset> list {
        { "Worship ballad",           1.0f, 1.0f, 1.5f, 0.25f, 1.10f, -14.0f, "warm and open, room to breathe" },
        { "Uptempo praise",           1.5f, 2.0f, 1.5f, 0.45f, 1.15f, -11.0f, "punchy and bright for dancing songs" },
        { "Choir anthem",             0.0f, 1.5f, 2.0f, 0.35f, 1.20f, -13.0f, "wide and clear for big choir sounds" },
        { "Soft and acoustic",        0.5f, 0.5f, 1.0f, 0.15f, 1.00f, -16.0f, "gentle, for piano and voice" },
        { "Loud for radio",           1.0f, 2.5f, 2.0f, 0.60f, 1.10f, -10.0f, "as loud as it gets, still clean" },
        { "Clean (only loudness)",    0.0f, 0.0f, 0.0f, 0.00f, 1.00f, -14.0f, "no tone change, just the level" },
    };
    return list;
}

MasterPlugin::MasterPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto* um = getUndoManager();
    warmth.referTo (state, "warmth", um, 1.0f);
    clarity.referTo (state, "clarity", um, 1.0f);
    air.referTo (state, "air", um, 1.5f);
    lowCut.referTo (state, "lowCut", um, true);
    glue.referTo (state, "glue", um, 0.25f);
    width.referTo (state, "width", um, 1.1f);
    push.referTo (state, "push", um, 3.0f);
    targetLufs.referTo (state, "targetLufs", um, -14.0f);
    presetName.referTo (state, "preset", um, "Worship ballad");
    valueTreePropertyChanged (state, {});
}

MasterPlugin::~MasterPlugin()
{
    notifyListenersOfDeletion();
}

void MasterPlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v == state)
    {
        aWarmth = juce::jlimit (-6.0f, 6.0f, warmth.get());
        aClarity = juce::jlimit (-6.0f, 6.0f, clarity.get());
        aAir = juce::jlimit (-6.0f, 6.0f, air.get());
        aLowCut = lowCut.get();
        aGlue = juce::jlimit (0.0f, 1.0f, glue.get());
        aWidth = juce::jlimit (0.5f, 1.5f, width.get());
        aPush = juce::jlimit (-12.0f, 18.0f, push.get());
        filtersDirty = true;
    }
    if (id.isValid())
        te::Plugin::valueTreePropertyChanged (v, id);
}

void MasterPlugin::applyPreset (const Preset& p)
{
    warmth = p.warmth; clarity = p.clarity; air = p.air;
    glue = p.glue; width = p.width; targetLufs = p.target;
    presetName = p.name;
}

void MasterPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    lookahead = juce::jmax (1, (int) std::round (lookaheadSeconds * sampleRate));
    delayL.assign ((size_t) lookahead + 1, 0.0f);
    delayR.assign ((size_t) lookahead + 1, 0.0f);
    dryL.assign ((size_t) lookahead + 1, 0.0f);
    dryR.assign ((size_t) lookahead + 1, 0.0f);
    avgWindow.assign ((size_t) lookahead, 1.0f);
    minQueue.assign ((size_t) lookahead + 4, { 0, 1.0f });
    loudIn.prepare (sampleRate);
    loudOut.prepare (sampleRate);
    filtersDirty = true;
    reset();
}

void MasterPlugin::reset()
{
    for (auto* f : { &lowCutFilter, &lowShelf, &presence, &highShelf })
        f->clear();
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    std::fill (dryL.begin(), dryL.end(), 0.0f);
    std::fill (dryR.begin(), dryR.end(), 0.0f);
    std::fill (avgWindow.begin(), avgWindow.end(), 1.0f);
    avgSum = (float) avgWindow.size();
    delayPos = 0;
    releaseGain = 1.0f;
    mqHead = mqTail = 0;
    sampleIndex = 0;
    glueEnvDb = -100.0f;
    glueGainDb = 0.0f;
}

void MasterPlugin::updateFilters()
{
    makeHighPass (lowCutFilter, sampleRate, 30.0);
    makeShelf (lowShelf, sampleRate, 110.0, aWarmth.load(), false);
    makePeak (presence, sampleRate, 3000.0, aClarity.load(), 0.7);
    makeShelf (highShelf, sampleRate, 11000.0, aAir.load(), true);
}

void MasterPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    if (filtersDirty.exchange (false))
        updateFilters();
    if (resetRequested.exchange (false))
    {
        loudIn.reset();
        loudOut.reset();
        peakHold = 0.0f;
    }

    auto& buffer = *fc.destBuffer;
    const auto n = fc.bufferNumSamples;
    auto* L = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : L;

    loudIn.process (L, R, n);

    // Glue settings: threshold -10 .. -24 dB, ratio 1.5 .. 3, a little make-up
    const auto g = aGlue.load();
    const auto threshold = -10.0f - 14.0f * g;
    const auto ratio = 1.5f + 1.5f * g;
    const auto makeup = 3.0f * g;
    const auto knee = 6.0f;
    const auto attack = std::exp (-1.0f / (0.030f * (float) sampleRate));
    const auto release = std::exp (-1.0f / (0.250f * (float) sampleRate));
    const auto grSmooth = std::exp (-1.0f / (0.005f * (float) sampleRate));

    const auto w = aWidth.load();
    const auto pushGain = dbToGain (aPush.load());
    const auto ceiling = dbToGain (ceilingDb - 0.2f);          // a little margin for peaks between samples
    const auto limRelease = 1.0f - std::exp (-1.0f / (0.080f * (float) sampleRate));
    const auto lowCutOn = aLowCut.load();
    const auto comparing = compare.load();
    const auto size = (int) minQueue.size();

    float maxGlue = 0.0f, minLim = 1.0f, peak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const auto inL = L[i], inR = R[i];
        float l = inL, r = inR;

        // Tone
        if (lowCutOn) { l = lowCutFilter.process (0, l); r = lowCutFilter.process (1, r); }
        l = highShelf.process (0, presence.process (0, lowShelf.process (0, l)));
        r = highShelf.process (1, presence.process (1, lowShelf.process (1, r)));

        // Glue (stereo-linked, soft knee)
        if (g > 0.001f)
        {
            const auto levelDb = gainToDb (juce::jmax (std::abs (l), std::abs (r)));
            glueEnvDb = levelDb > glueEnvDb ? attack * glueEnvDb + (1.0f - attack) * levelDb
                                            : release * glueEnvDb + (1.0f - release) * levelDb;
            const auto over = glueEnvDb - threshold;
            float reduction = 0.0f;
            if (over > knee * 0.5f)        reduction = over * (1.0f - 1.0f / ratio);
            else if (over > -knee * 0.5f)  reduction = (1.0f - 1.0f / ratio) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
            glueGainDb = grSmooth * glueGainDb + (1.0f - grSmooth) * reduction;
            maxGlue = juce::jmax (maxGlue, glueGainDb);
            const auto gain = dbToGain (makeup - glueGainDb);
            l *= gain; r *= gain;
        }

        // Width (mid/side)
        if (std::abs (w - 1.0f) > 0.001f)
        {
            const auto mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
            l = mid + side; r = mid - side;
        }

        // Push into the limiter
        l *= pushGain; r *= pushGain;

        // Lookahead limiter: the gain needed for this sample, with a smooth release...
        const auto absPeak = juce::jmax (std::abs (l), std::abs (r));
        const auto needed = absPeak > ceiling ? ceiling / absPeak : 1.0f;
        releaseGain = juce::jmin (needed, releaseGain + (1.0f - releaseGain) * limRelease);

        // ...the lowest gain over the look-ahead window (monotonic queue)...
        while (mqTail != mqHead && minQueue[(size_t) ((mqTail - 1 + size) % size)].second >= releaseGain)
            mqTail = (mqTail - 1 + size) % size;
        minQueue[(size_t) mqTail] = { sampleIndex, releaseGain };
        mqTail = (mqTail + 1) % size;
        while (minQueue[(size_t) mqHead].first <= sampleIndex - lookahead - 1)
            mqHead = (mqHead + 1) % size;
        const auto windowMin = minQueue[(size_t) mqHead].second;

        // ...smoothed by a moving average so the gain is already down when the peak arrives
        const auto avgIndex = sampleIndex % lookahead;
        avgSum += windowMin - avgWindow[(size_t) avgIndex];
        avgWindow[(size_t) avgIndex] = windowMin;
        const auto gainNow = juce::jmin (1.0f, avgSum / (float) lookahead);
        minLim = juce::jmin (minLim, gainNow);
        ++sampleIndex;
        if (sampleIndex > 1000000000) sampleIndex = 0, mqHead = mqTail = 0;

        // The audio is delayed by the look-ahead
        const auto readPos = (delayPos + 1) % (lookahead + 1);
        const auto outL = delayL[(size_t) readPos] * gainNow;
        const auto outR = delayR[(size_t) readPos] * gainNow;
        const auto oldDryL = dryL[(size_t) readPos], oldDryR = dryR[(size_t) readPos];
        delayL[(size_t) delayPos] = l;
        delayR[(size_t) delayPos] = r;
        dryL[(size_t) delayPos] = inL;
        dryR[(size_t) delayPos] = inR;

        // Compare: the unmastered input, delayed the same, at matched loudness
        if (comparing)
        {
            L[i] = juce::jlimit (-1.0f, 1.0f, oldDryL * matchGain);
            if (R != L) R[i] = juce::jlimit (-1.0f, 1.0f, oldDryR * matchGain);
        }
        else
        {
            L[i] = juce::jlimit (-ceiling, ceiling, outL);
            if (R != L) R[i] = juce::jlimit (-ceiling, ceiling, outR);
        }
        delayPos = (delayPos + 1) % (lookahead + 1);
        peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
    }

    if (! comparing)
        loudOut.process (L, R, n);

    // Compare matches the loudness the listener heard last (updated while not comparing)
    if (! comparing && loudOut.getShortTerm() > -70.0f && loudIn.getShortTerm() > -70.0f)
        matchGain = juce::jlimit (0.1f, 8.0f, dbToGain (loudOut.getShortTerm() - loudIn.getShortTerm()));

    peakHold = juce::jmax (peak, peakHold);           // the highest since playing started
    meters.shortTermIn = loudIn.getShortTerm();
    if (! comparing)
    {
        meters.shortTermOut = loudOut.getShortTerm();
        meters.integratedOut = loudOut.getIntegrated();
    }
    meters.glueReductionDb = maxGlue;
    meters.limiterReductionDb = -gainToDb (minLim);
    meters.peakOutDb = gainToDb (peakHold);
}

//==============================================================================
MasterPlugin* MasterPlugin::findIn (te::Edit& edit)
{
    for (auto* p : edit.getMasterPluginList().getPlugins())
        if (auto* m = dynamic_cast<MasterPlugin*> (p))
            return m;
    return nullptr;
}

bool MasterPlugin::isActive (te::Edit& edit)
{
    auto* m = findIn (edit);
    return m != nullptr && m->isEnabled();
}

MasterPlugin* MasterPlugin::addTo (te::Edit& edit)
{
    if (auto* m = findIn (edit))
        return m;

    auto p = edit.getPluginCache().createNewPlugin (xmlTypeName, {});
    if (p == nullptr)
        return nullptr;

    // Last in the chain (only the analyser and the master meter after it)
    auto& list = edit.getMasterPluginList();
    int index = list.getPlugins().size();
    for (int i = 0; i < list.getPlugins().size(); ++i)
        if (list.getPlugins()[i]->getPluginType() == "gpsAnalyser")
        {
            index = i;
            break;
        }
    list.insertPlugin (p, index, nullptr);
    for (auto* q : list.getPlugins())
    {
        if (dynamic_cast<te::LevelMeterPlugin*> (q) != nullptr)
        {
            te::Plugin::Ptr meter (q);
            meter->removeFromParent();
            list.insertPlugin (meter, list.getPlugins().size(), nullptr);
            break;
        }
    }
    return dynamic_cast<MasterPlugin*> (p.get());
}
