#include "AnalyserPlugin.h"
#include "BackgroundWork.h"

const char* AnalyserPlugin::xmlTypeName = "gpsAnalyser";

AnalyserPlugin::AnalyserPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto* um = getUndoManager();
    referencePath.referTo (state, "referenceFile", um, {});
    referenceOffset.referTo (state, "referenceOffset", um, 0.0f);
    fifoL.assign ((size_t) fifo.getTotalSize(), 0.0f);
    fifoR.assign ((size_t) fifo.getTotalSize(), 0.0f);
    meter.prepare (44100.0);

    // A song opened with a reference chosen before: load it again
    if (getReferenceFile().existsAsFile())
        juce::MessageManager::callAsync ([flag = alive, this]
        {
            if (flag->load() && ! refReady.load() && ! refLoading.load() && getReferenceFile().existsAsFile())
                loadReference (getReferenceFile());
        });
}

AnalyserPlugin::~AnalyserPlugin()
{
    alive->store (false);
    notifyListenersOfDeletion();
}

void AnalyserPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    const bool changed = std::abs (info.sampleRate - sampleRate) > 1.0;
    sampleRate = info.sampleRate;
    meter.prepare (sampleRate);
    if (changed && getReferenceFile().existsAsFile())
        juce::MessageManager::callAsync ([flag = alive, this] { if (flag->load()) loadReference (getReferenceFile()); });
}

AnalyserPlugin* AnalyserPlugin::findIn (te::Edit& edit)
{
    for (auto* p : edit.getMasterPluginList().getPlugins())
        if (auto* a = dynamic_cast<AnalyserPlugin*> (p))
            return a;
    return nullptr;
}

AnalyserPlugin* AnalyserPlugin::addTo (te::Edit& edit)
{
    if (auto* a = findIn (edit))
        return a;
    auto p = edit.getPluginCache().createNewPlugin (xmlTypeName, {});
    if (p == nullptr)
        return nullptr;

    // After everything (mastering included), only the master meter after it
    auto& list = edit.getMasterPluginList();
    int index = list.getPlugins().size();
    for (int i = 0; i < list.getPlugins().size(); ++i)
        if (dynamic_cast<te::LevelMeterPlugin*> (list.getPlugins()[i].get()) != nullptr)
        {
            index = i;
            break;
        }
    list.insertPlugin (p, index, nullptr);
    auto* a = dynamic_cast<AnalyserPlugin*> (p.get());
    if (a != nullptr && a->getReferenceFile().existsAsFile())
        a->loadReference (a->getReferenceFile());
    return a;
}

//==============================================================================
void AnalyserPlugin::clearReference()
{
    ++loadGeneration;
    listenToReference = false;
    {
        const juce::SpinLock::ScopedLockType sl (refLock);
        ref.reset();
    }
    refReady = false;
    refLoading = false;
    refLengthSeconds = 0.0;
    refAnalysis = {};
    referencePath = juce::String();
    ++version; changed();
}

void AnalyserPlugin::loadReference (const juce::File& file)
{
    const auto generation = ++loadGeneration;
    referencePath = file.getFullPathName();
    refReady = false;
    refLoading = true;
    ++version; changed();

    const auto targetRate = sampleRate;
    auto flag = alive;
    auto* self = this;
    BackgroundWork::run ("Loading the reference song", [file, targetRate, flag, self, generation] (const std::atomic<bool>& stopRequested)
    {
        // Measure it (spectrum, loudness, stereo)
        std::atomic<bool> cancel { false };
        auto analysis = Analysis::analyseFile (file, &cancel);

        // Decode it at the playback rate (16-bit is plenty for listening)
        auto audio = std::make_shared<RefAudio>();
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        if (std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file)); r != nullptr && r->sampleRate > 0.0
             && r->lengthInSamples < (juce::int64) (r->sampleRate * 60.0 * 12.0))
        {
            const auto ratio = r->sampleRate / targetRate;
            const auto outLength = (juce::int64) ((double) r->lengthInSamples / ratio);
            audio->left.resize ((size_t) outLength);
            audio->right.resize ((size_t) outLength);
            audio->sampleRate = targetRate;
            audio->length = outLength;

            juce::LagrangeInterpolator interp[2];
            juce::AudioBuffer<float> in (2, 16384);
            std::vector<float> out[2];
            juce::int64 written = 0;
            for (juce::int64 pos = 0; pos < r->lengthInSamples && written < outLength && ! stopRequested.load(); pos += 16384)
            {
                const auto n = (int) juce::jmin ((juce::int64) 16384, r->lengthInSamples - pos);
                r->read (&in, 0, n, pos, true, true);
                if (r->numChannels < 2)
                    in.copyFrom (1, 0, in, 0, 0, n);
                const auto produce = (int) juce::jmin ((juce::int64) std::floor (n / ratio), outLength - written);
                for (int c = 0; c < 2; ++c)
                {
                    out[c].assign ((size_t) juce::jmax (1, produce), 0.0f);
                    if (std::abs (ratio - 1.0) < 1.0e-9)
                        std::copy (in.getReadPointer (c), in.getReadPointer (c) + produce, out[c].begin());
                    else
                        interp[c].process (ratio, in.getReadPointer (c), out[c].data(), produce, n, 0);
                }
                for (int i = 0; i < produce; ++i)
                {
                    audio->left[(size_t) (written + i)]  = (juce::int16) juce::jlimit (-32767, 32767, juce::roundToInt (out[0][(size_t) i] * 32767.0f));
                    audio->right[(size_t) (written + i)] = (juce::int16) juce::jlimit (-32767, 32767, juce::roundToInt (out[1][(size_t) i] * 32767.0f));
                }
                written += produce;
            }
            audio->length = written;
        }

        BackgroundWork::onMessageThread ([flag, self, generation, audio, analysis]
        {
            if (! flag->load() || generation != self->loadGeneration)
                return;
            self->refAnalysis = analysis;
            {
                const juce::SpinLock::ScopedLockType sl (self->refLock);
                self->ref = audio->length > 0 ? std::shared_ptr<const RefAudio> (audio) : nullptr;
            }
            self->refLengthSeconds = audio->length / juce::jmax (1.0, audio->sampleRate);
            self->refReady = audio->length > 0 && analysis.ok;
            self->refLoading = false;
            ++self->version; self->changed();
        });
    }, false);
}

//==============================================================================
int AnalyserPlugin::readLive (float* left, float* right, int maxSamples)
{
    const auto scope = fifo.read (juce::jmin (maxSamples, fifo.getNumReady()));
    int done = 0;
    for (auto [start, size] : { std::pair { scope.startIndex1, scope.blockSize1 }, std::pair { scope.startIndex2, scope.blockSize2 } })
    {
        std::copy (fifoL.begin() + start, fifoL.begin() + start + size, left + done);
        std::copy (fifoR.begin() + start, fifoR.begin() + start + size, right + done);
        done += size;
    }
    return done;
}

void AnalyserPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.isRendering || fc.bufferNumSamples <= 0)
        return;                                   // exports never include the reference

    auto& buffer = *fc.destBuffer;
    const int start = fc.bufferStartSample, n = fc.bufferNumSamples;
    float* l = buffer.getWritePointer (0, start);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : l;

    // The loudness of your mix since you pressed Play
    if (fc.isPlaying && ! wasPlaying)
        meter.reset();
    wasPlaying = fc.isPlaying;
    if (fc.isPlaying)
    {
        meter.process (l, r, n);
        mixIntegrated = meter.getIntegrated();
        mixShortTerm = meter.getShortTerm();
    }

    // B: the reference instead, at the same loudness
    const juce::SpinLock::ScopedTryLockType sl (refLock);
    const auto* refAudio = sl.isLocked() ? ref.get() : nullptr;
    const auto target = listenToReference.load() && refAudio != nullptr && fc.isPlaying
                          ? juce::Decibels::decibelsToGain (referenceGainDb.load()) : 0.0f;
    if (refAudio != nullptr && (target > 0.0f || smoothGain > 0.0f) && fc.isPlaying)
    {
        // A short crossfade when switching, so A/B never clicks
        const auto step = 1.0f / (0.02f * (float) sampleRate);
        auto pos = (juce::int64) std::llround ((fc.editTime.getStart().inSeconds() + referenceOffset.get()) * refAudio->sampleRate);
        const auto len = juce::jmax ((juce::int64) 1, refAudio->length);
        pos = ((pos % len) + len) % len;
        const float fullGain = juce::Decibels::decibelsToGain (referenceGainDb.load());
        for (int i = 0; i < n; ++i)
        {
            const float want = target > 0.0f ? 1.0f : 0.0f;
            smoothGain = smoothGain < want ? juce::jmin (want, smoothGain + step) : juce::jmax (want, smoothGain - step);
            const auto rl = refAudio->left[(size_t) pos] / 32767.0f * fullGain;
            const auto rr = refAudio->right[(size_t) pos] / 32767.0f * fullGain;
            l[i] = l[i] * (1.0f - smoothGain) + rl * smoothGain;
            if (r != l) r[i] = r[i] * (1.0f - smoothGain) + rr * smoothGain;
            if (++pos >= len) pos = 0;
        }
    }
    else if (target <= 0.0f)
    {
        smoothGain = 0.0f;
    }

    // What you hear, for the screen
    if (fc.isPlaying)
    {
        const auto scope = fifo.write (juce::jmin (n, fifo.getFreeSpace()));
        int done = 0;
        for (auto [s0, size] : { std::pair { scope.startIndex1, scope.blockSize1 }, std::pair { scope.startIndex2, scope.blockSize2 } })
        {
            std::copy (l + done, l + done + size, fifoL.begin() + s0);
            std::copy (r + done, r + done + size, fifoR.begin() + s0);
            done += size;
        }
    }
}
