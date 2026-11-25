#include "AudioEdit.h"
#include "Takes.h"

namespace AudioEdit
{
    void prepare (te::AudioClipBase& c)
    {
        if (! c.getAutoCrossfade())
            c.setAutoCrossfade (true);
       #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
        if (c.getTimeStretchMode() != te::TimeStretcher::signalsmithDefault && c.getTimeStretchMode() != te::TimeStretcher::disabled)
            c.setTimeStretchMode (te::TimeStretcher::signalsmithDefault);
       #endif
    }

    te::Clip* splitAt (te::Clip& clip, te::TimePosition time)
    {
        const auto pos = clip.getPosition();
        if (time <= pos.getStart() || time >= pos.getEnd())
            return nullptr;

        auto* track = clip.getClipTrack();
        if (track == nullptr)
            return nullptr;

        te::Clip::Ptr left (&clip);
        auto* right = track->splitClip (clip, time);

        // Tiny fades at the cut so it never clicks
        if (auto* a = dynamic_cast<te::AudioClipBase*> (left.get()))
        {
            prepare (*a);
            a->setFadeOut (te::TimeDuration::fromSeconds (juce::jmax (microFadeSeconds, a->getFadeOut().inSeconds())));
        }
        if (auto* b = dynamic_cast<te::AudioClipBase*> (right))
        {
            prepare (*b);
            b->setFadeIn (te::TimeDuration::fromSeconds (microFadeSeconds));
        }
        return right;
    }

    float measurePeakDb (te::AudioClipBase& c)
    {
        const auto file = c.getSourceFileReference().getFile();
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr)
            return -100.0f;

        const auto pos = c.getPosition();
        const auto speed = juce::jmax (0.01, c.getSpeedRatio());
        const auto start = (juce::int64) (pos.getOffset().inSeconds() * speed * reader->sampleRate);
        const auto length = (juce::int64) (pos.getLength().inSeconds() * speed * reader->sampleRate);

        juce::Range<float> ranges[2];
        reader->readMaxLevels (juce::jmax ((juce::int64) 0, start), length, ranges, (int) juce::jmin (2u, reader->numChannels));
        float peak = 0.0f;
        for (int ch = 0; ch < (int) juce::jmin (2u, reader->numChannels); ++ch)
            peak = juce::jmax (peak, std::abs (ranges[ch].getStart()), std::abs (ranges[ch].getEnd()));
        return juce::Decibels::gainToDecibels (peak, -100.0f);
    }

    void normalise (te::AudioClipBase& c, float targetDb)
    {
        const auto peak = measurePeakDb (c);
        if (peak <= -90.0f)
            return;
        c.setGainDB (juce::jlimit (-40.0f, 24.0f, targetDb - peak));
    }

    void setPitch (te::AudioClipBase& c, float semitones)
    {
        if (c.getTimeStretchMode() == te::TimeStretcher::disabled && std::abs (semitones) > 0.001f)
           #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
            c.setTimeStretchMode (te::TimeStretcher::signalsmithDefault);
           #else
            c.setTimeStretchMode (te::TimeStretcher::defaultMode);
           #endif
        c.setPitchChange (semitones);
    }

    double getSpeed (te::AudioClipBase& c)
    {
        return c.getSpeedRatio();
    }

    void setSpeed (te::AudioClipBase& c, double ratio, bool keepPitch)
    {
        ratio = juce::jlimit (0.25, 4.0, ratio);
        if (keepPitch)
        {
           #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
            c.setTimeStretchMode (te::TimeStretcher::signalsmithDefault);
           #else
            c.setTimeStretchMode (te::TimeStretcher::defaultMode);
           #endif
        }
        else if (std::abs (c.getPitchChange()) < 0.001f)
        {
            c.setTimeStretchMode (te::TimeStretcher::disabled);     // like a tape: faster is higher
        }
        // The clip keeps playing the same part of the recording, so its length changes
        const auto old = juce::jmax (0.01, c.getSpeedRatio());
        const auto pos = c.getPosition();
        const auto newLength = pos.getLength().inSeconds() * old / ratio;
        c.setSpeedRatio (ratio);
        c.setLength (te::TimeDuration::fromSeconds (newLength), true);
    }
}

//==============================================================================
namespace Comp
{
    static const juce::Identifier compFlag ("gpsComp"), planProp ("gpsCompPlan");

    bool isCompTake (te::WaveAudioClip& c, int index)
    {
        return (bool) c.state.getChildWithName (te::IDs::TAKES).getChild (index).getProperty (compFlag, false);
    }

    std::vector<int> recordedTakes (te::WaveAudioClip& c)
    {
        std::vector<int> out;
        for (int i = 0; i < Takes::count (c); ++i)
            if (! isCompTake (c, i))
                out.push_back (i);
        return out;
    }

    std::vector<Segment> load (te::WaveAudioClip& c, double fileStart, double fileEnd, int defaultTake)
    {
        std::vector<Segment> segs;
        for (const auto& part : juce::StringArray::fromTokens (c.state.getProperty (planProp).toString(), ";", ""))
        {
            const auto take = part.upToFirstOccurrenceOf (":", false, false).getIntValue();
            const auto range = part.fromFirstOccurrenceOf (":", false, false);
            const auto a = range.upToFirstOccurrenceOf ("-", false, false).getDoubleValue();
            const auto b = range.fromFirstOccurrenceOf ("-", false, false).getDoubleValue();
            if (b > a)
                segs.push_back ({ a, b, take });
        }
        if (segs.empty())
            segs.push_back ({ fileStart, fileEnd, defaultTake });
        return segs;
    }

    void save (te::WaveAudioClip& c, const std::vector<Segment>& segs)
    {
        juce::StringArray parts;
        for (auto& s : segs)
            parts.add (juce::String (s.take) + ":" + juce::String (s.start, 4) + "-" + juce::String (s.end, 4));
        c.state.setProperty (planProp, parts.joinIntoString (";"), c.getUndoManager());
    }

    void assign (std::vector<Segment>& segs, double a, double b, int take)
    {
        if (b <= a || segs.empty())
            return;
        a = juce::jmax (a, segs.front().start);
        b = juce::jmin (b, segs.back().end);
        if (b - a < 1.0e-4)
            return;

        std::vector<Segment> out;
        for (const auto& s : segs)
        {
            if (s.end <= a || s.start >= b) { out.push_back (s); continue; }
            if (s.start < a) out.push_back ({ s.start, a, s.take });
            if (s.end > b)   out.push_back ({ b, s.end, s.take });
        }
        out.push_back ({ a, b, take });
        std::sort (out.begin(), out.end(), [] (auto& x, auto& y) { return x.start < y.start; });

        // Merge neighbours that use the same take
        segs.clear();
        for (const auto& s : out)
        {
            if (! segs.empty() && segs.back().take == s.take && std::abs (segs.back().end - s.start) < 1.0e-9)
                segs.back().end = s.end;
            else
                segs.push_back (s);
        }
    }

    juce::Result render (const std::vector<juce::File>& takeFiles, const std::vector<Segment>& segs,
                         double crossfadeSeconds, const juce::File& output)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        std::vector<std::unique_ptr<juce::AudioFormatReader>> readers;
        double sampleRate = 0.0;
        int channels = 1;
        juce::int64 length = 0;
        for (auto& f : takeFiles)
        {
            readers.emplace_back (fm.createReaderFor (f));
            if (readers.back() == nullptr)
                return juce::Result::fail ("Could not read " + f.getFileName());
            sampleRate = readers.back()->sampleRate;
            channels = juce::jmax (channels, (int) juce::jmin (2u, readers.back()->numChannels));
            length = juce::jmax (length, (juce::int64) readers.back()->lengthInSamples);
        }
        if (readers.empty() || sampleRate <= 0.0)
            return juce::Result::fail ("No takes to comp");

        // Gain per take over time: each segment fades in/out across the crossfade at its edges
        const auto xf = juce::jmax (1.0, crossfadeSeconds * sampleRate);
        auto weight = [&] (const Segment& s, juce::int64 i, bool firstSeg, bool lastSeg)
        {
            const auto t = (double) i;
            const auto a = s.start * sampleRate, b = s.end * sampleRate;
            double w = 1.0;
            if (! firstSeg) w *= juce::jlimit (0.0, 1.0, (t - (a - xf * 0.5)) / xf);
            else if (t < a) w = 0.0;
            if (! lastSeg)  w *= juce::jlimit (0.0, 1.0, ((b + xf * 0.5) - t) / xf);
            else if (t >= b) w = 0.0;
            // equal-power curve
            return std::sin (w * juce::MathConstants<double>::halfPi);
        };

        output.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (output.createOutputStream());
        if (stream == nullptr)
            return juce::Result::fail ("Could not create " + output.getFullPathName());

        juce::WavAudioFormat wav;
        auto options = juce::AudioFormatWriter::Options{}.withSampleRate (sampleRate)
                                                       .withNumChannels (channels)
                                                       .withBitsPerSample (24);
        auto writer = wav.createWriterFor (stream, options);
        if (writer == nullptr)
            return juce::Result::fail ("Could not write WAV");

        const int block = 8192;
        juce::AudioBuffer<float> out (channels, block), in (channels, block);

        for (juce::int64 pos = 0; pos < length; pos += block)
        {
            const auto n = (int) juce::jmin ((juce::int64) block, length - pos);
            out.clear();

            for (size_t si = 0; si < segs.size(); ++si)
            {
                const auto& s = segs[si];
                if (! juce::isPositiveAndBelow (s.take, (int) readers.size()))
                    continue;
                const auto a = (juce::int64) (s.start * sampleRate - xf), b = (juce::int64) (s.end * sampleRate + xf);
                if (b <= pos || a >= pos + n)
                    continue;

                in.clear();
                readers[(size_t) s.take]->read (&in, 0, n, pos, true, true);
                for (int i = 0; i < n; ++i)
                {
                    const auto w = (float) weight (s, pos + i, si == 0, si + 1 == segs.size());
                    if (w <= 0.0f) continue;
                    for (int ch = 0; ch < channels; ++ch)
                        out.addSample (ch, i, in.getSample (juce::jmin (ch, in.getNumChannels() - 1), i) * w);
                }
            }
            writer->writeFromAudioSampleBuffer (out, 0, n);
        }
        return juce::Result::ok();
    }

    void addCompTake (te::WaveAudioClip& c, const juce::File& file)
    {
        c.addTake (file);
        auto takes = c.state.getChildWithName (te::IDs::TAKES);
        auto node = takes.getChild (takes.getNumChildren() - 1);
        node.setProperty (compFlag, true, c.getUndoManager());
        Takes::select (c, takes.getNumChildren() - 1);
    }
}
