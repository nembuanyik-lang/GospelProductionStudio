#include "Timing.h"
#include "WavFiles.h"
#include "AudioEdit.h"
#include "Takes.h"
#include "Loops.h"
#include "TempoFollow.h"
#include "StudioLookAndFeel.h"
#include <tracktion_engine/3rd_party/signalsmith-stretch/signalsmith-stretch.h>

namespace Timing
{
    namespace
    {
        const juce::Identifier srcId ("gpsTimingSource"), gridId ("gpsTimingGrid"), strengthId ("gpsTimingStrength"),
                               toleranceId ("gpsTimingTolerance"), sensId ("gpsTimingSensitivity");

        juce::AudioFormatManager& formats()
        {
            static juce::AudioFormatManager fm;
            static bool done = false;
            if (! done) { fm.registerBasicFormats(); done = true; }
            return fm;
        }

        juce::File originalOf (const te::WaveAudioClip& clip)
        {
            if (clip.state.hasProperty (srcId))
                return juce::File (clip.state[srcId].toString());
            return clip.getOriginalFile();
        }
    }

    //==========================================================================
    //  Finding note starts: where the sound suddenly gets brighter or louder
    //==========================================================================
    std::vector<double> findNoteStarts (const juce::AudioBuffer<float>& buffer, double sr, int sensitivity)
    {
        std::vector<double> result;
        const int n = buffer.getNumSamples();
        if (n < 4096 || sr <= 0.0)
            return result;

        // Mono
        std::vector<float> x ((size_t) n, 0.0f);
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            juce::FloatVectorOperations::addWithMultiply (x.data(), buffer.getReadPointer (c), 1.0f / (float) buffer.getNumChannels(), n);

        float peak = 0.0f;
        for (auto s : x) peak = juce::jmax (peak, std::abs (s));
        if (peak < 1.0e-4f)
            return result;

        constexpr int order = 10, win = 1 << order, hop = 256;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) win, juce::dsp::WindowingFunction<float>::hann, false);
        const int frames = (n - win) / hop + 1;
        const int topBin = juce::jmin (win / 2, (int) (11000.0 / sr * win));

        std::vector<float> flux ((size_t) frames, 0.0f), prev ((size_t) win / 2 + 1, 0.0f), level ((size_t) frames, -120.0f);
        std::vector<float> buf ((size_t) win * 2);
        for (int f = 0; f < frames; ++f)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            std::copy (x.begin() + f * hop, x.begin() + f * hop + win, buf.begin());
            double e = 0.0;
            for (int i = 0; i < win; ++i) e += buf[(size_t) i] * buf[(size_t) i];
            level[(size_t) f] = (float) (10.0 * std::log10 (e / win + 1.0e-12));
            window.multiplyWithWindowingTable (buf.data(), (size_t) win);
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);

            float sum = 0.0f;
            for (int k = 1; k < topBin; ++k)
            {
                const auto m = std::log1p (100.0f * buf[(size_t) k]);
                sum += juce::jmax (0.0f, m - prev[(size_t) k]);
                prev[(size_t) k] = m;
            }
            flux[(size_t) f] = sum;
        }

        float maxFlux = 0.0f;
        for (auto v : flux) maxFlux = juce::jmax (maxFlux, v);
        if (maxFlux <= 0.0f)
            return result;
        for (auto& v : flux) v /= maxFlux;

        const float mult[]  = { 1.7f, 1.45f, 1.25f };
        const float delta[] = { 0.10f, 0.06f, 0.035f };
        const auto s = juce::jlimit (0, 2, sensitivity);
        const auto peakDb = 20.0f * std::log10 (peak);
        const int half = 8;
        const int minGap = (int) std::ceil (0.07 * sr / hop);
        int last = -1000;

        std::vector<float> neighbourhood;
        for (int f = 1; f < frames - 1; ++f)
        {
            if (level[(size_t) f] < peakDb - 45.0f)
                continue;                                   // silence or breath
            neighbourhood.clear();
            for (int j = juce::jmax (0, f - half); j <= juce::jmin (frames - 1, f + half); ++j)
                neighbourhood.push_back (flux[(size_t) j]);
            std::nth_element (neighbourhood.begin(), neighbourhood.begin() + (long) neighbourhood.size() / 2, neighbourhood.end());
            const auto threshold = neighbourhood[neighbourhood.size() / 2] * mult[s] + delta[s];

            bool isPeak = flux[(size_t) f] > threshold;
            for (int j = juce::jmax (0, f - 3); j <= juce::jmin (frames - 1, f + 3) && isPeak; ++j)
                if (j != f && flux[(size_t) j] > flux[(size_t) f]) isPeak = false;
            if (! isPeak || f - last < minGap)
                continue;
            last = f;

            // Exact spot: where the energy rises most, in 64-sample steps, within this frame
            const int from = f * hop - hop, to = juce::jmin (n - 64, f * hop + win + 2 * hop);
            double best = 0.0;
            int at = f * hop + win / 2;
            double prevE = 1.0e-9;
            for (int i = juce::jmax (64, from); i < to; i += 32)
            {
                double e = 1.0e-9;
                for (int j = 0; j < 64; ++j) e += x[(size_t) (i + j)] * x[(size_t) (i + j)];
                double eBefore = 1.0e-9;
                for (int j = 0; j < 64; ++j) eBefore += x[(size_t) (i - 64 + j)] * x[(size_t) (i - 64 + j)];
                const auto rise = e / eBefore;
                if (rise > best) { best = rise; at = i; }
                prevE = e;
            }
            juce::ignoreUnused (prevE);
            result.push_back (at / sr);
        }

        // Keep them in order and apart (the refinement can move neighbours together)
        std::sort (result.begin(), result.end());
        result.erase (std::unique (result.begin(), result.end(), [] (double a, double b) { return b - a < 0.05; }), result.end());
        return result;
    }

    std::vector<double> findNoteStarts (const juce::File& file, int sensitivity)
    {
        static std::map<juce::String, std::vector<double>> cache;
        const auto key = file.getFullPathName() + "|" + juce::String (sensitivity) + "|" + juce::String (file.getLastModificationTime().toMilliseconds());
        if (auto it = cache.find (key); it != cache.end())
            return it->second;

        std::unique_ptr<juce::AudioFormatReader> r (formats().createReaderFor (file));
        if (r == nullptr || r->lengthInSamples > (juce::int64) (r->sampleRate * 60.0 * 20.0))
            return {};
        juce::AudioBuffer<float> b ((int) juce::jlimit (1u, 2u, r->numChannels), (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, b.getNumChannels() > 1);
        auto starts = findNoteStarts (b, r->sampleRate, sensitivity);
        if (cache.size() > 20) cache.clear();
        cache[key] = starts;
        return starts;
    }

    //==========================================================================
    bool isCorrected (const te::WaveAudioClip& clip)   { return clip.state.hasProperty (srcId); }

    Settings lastSettings (const te::WaveAudioClip& clip)
    {
        Settings s;
        s.gridBeats = clip.state.getProperty (gridId, s.gridBeats);
        s.strength = clip.state.getProperty (strengthId, s.strength);
        s.toleranceMs = clip.state.getProperty (toleranceId, s.toleranceMs);
        s.sensitivity = clip.state.getProperty (sensId, s.sensitivity);
        return s;
    }

    Analysis analyse (te::WaveAudioClip& clip, const Settings& settings)
    {
        Analysis a;
        if (std::abs (clip.getSpeedRatio() - 1.0) > 0.001 || clip.getAutoTempo() || Loops::isFollowing (clip))
        {
            a.problem = "Timing correction works on recordings played at their own speed. This clip has its speed changed or follows the tempo as a loop.";
            return a;
        }
        if (Takes::count (clip) > 1)
        {
            a.problem = "This clip has several takes. First choose the best one: right-click > \"Keep this take, delete the others...\".";
            return a;
        }
        a.source = originalOf (clip);
        std::unique_ptr<juce::AudioFormatReader> r (formats().createReaderFor (a.source));
        if (r == nullptr)
        {
            a.problem = "The recording of this clip could not be read.";
            return a;
        }
        a.lengthSec = (double) r->lengthInSamples / r->sampleRate;

        auto& ts = clip.edit.tempoSequence;
        const auto pos = clip.getPosition();
        const auto clipStart = pos.getStart().inSeconds();
        const auto offset = pos.getOffset().inSeconds();
        const auto playedFrom = offset, playedTo = offset + pos.getLength().inSeconds();
        const auto grid = juce::jmax (1.0 / 12.0, settings.gridBeats);

        double lastNew = -1.0, sumMove = 0.0;
        for (auto s : findNoteStarts (a.source, settings.sensitivity))
        {
            NoteStart n;
            n.sourceSec = s;
            n.newSourceSec = s;
            n.inClip = s >= playedFrom && s < playedTo;

            const auto t = clipStart + (s - offset);
            const auto beat = ts.toBeats (te::TimePosition::fromSeconds (t)).inBeats();
            const auto target = std::round (beat / grid) * grid;
            const auto targetSec = ts.toTime (te::BeatPosition::fromBeats (target)).inSeconds();
            n.offMs = (t - targetSec) * 1000.0;

            if (n.inClip && std::abs (n.offMs) > settings.toleranceMs && std::abs (beat - target) < grid * 0.45)
            {
                const auto newT = t + (targetSec - t) * juce::jlimit (0.0f, 1.0f, settings.strength);
                n.newSourceSec = offset + (newT - clipStart);
                n.moves = true;
            }

            // Never cross the previous note start, and never squeeze a gap below a third of its length
            if (lastNew >= 0.0 && n.newSourceSec < lastNew + 0.02)
            {
                n.newSourceSec = juce::jmax (n.sourceSec, lastNew + 0.02);
                n.moves = std::abs (n.newSourceSec - n.sourceSec) > 0.0005;
            }
            if (n.moves)
            {
                ++a.numMoving;
                const auto moved = std::abs (n.newSourceSec - n.sourceSec) * 1000.0;
                sumMove += moved;
                a.biggestMoveMs = juce::jmax (a.biggestMoveMs, moved);
            }
            lastNew = n.newSourceSec;
            a.notes.push_back (n);
        }

        // Keep the stretch between neighbours gentle (between 0.5x and 2x)
        for (int pass = 0; pass < 3; ++pass)
            for (size_t i = 1; i < a.notes.size(); ++i)
            {
                auto& p = a.notes[i - 1];
                auto& n = a.notes[i];
                const auto inGap = n.sourceSec - p.sourceSec, outGap = n.newSourceSec - p.newSourceSec;
                if (inGap > 0.0 && (outGap < inGap * 0.5 || outGap > inGap * 2.0))
                {
                    n.newSourceSec = n.sourceSec + (n.newSourceSec - n.sourceSec) * 0.5;
                    n.moves = std::abs (n.newSourceSec - n.sourceSec) > 0.0005;
                }
            }

        a.averageMoveMs = a.numMoving > 0 ? sumMove / a.numMoving : 0.0;
        a.ok = true;
        if (a.notes.empty())
            a.problem = "No note starts were found. Try \"More note starts\".";
        return a;
    }

    //==========================================================================
    //  Stretching between note starts (Signalsmith, offline, time-varying)
    //==========================================================================
    bool renderWarped (const juce::File& src, const std::vector<std::pair<double, double>>& pairs, const juce::File& dest)
    {
        std::unique_ptr<juce::AudioFormatReader> r (formats().createReaderFor (src));
        if (r == nullptr)
            return false;
        const auto sr = r->sampleRate;
        const int ch = (int) juce::jlimit (1u, 2u, r->numChannels);
        const int n = (int) r->lengthInSamples;

        juce::AudioBuffer<float> in (ch, n);
        r->read (&in, 0, n, 0, true, ch > 1);

        // Cut points a little before each note start (in the quiet just before it), so the
        // note start itself is never inside a crossfade. Input -> output, in samples.
        const int before = (int) (0.005 * sr), fade = (int) (0.003 * sr);
        std::vector<std::pair<int, int>> cuts { { 0, 0 } };
        for (auto& [i, o] : pairs)
        {
            const int ci = (int) std::lround (i * sr) - before, co = (int) std::lround (o * sr) - before;
            if (ci > cuts.back().first + 2 * fade && co > cuts.back().second + 2 * fade && ci < n - 2 * fade && co < n - 2 * fade)
                cuts.push_back ({ ci, co });
        }
        cuts.push_back ({ n, n });

        // Shorter blocks than the default: note starts stay sharp (less "pre-echo")
        signalsmith::stretch::SignalsmithStretch<float> stretch;
        stretch.configure (ch, (int) std::lround (sr * 0.05), (int) std::lround (sr * 0.0125));

        juce::AudioBuffer<float> out (ch, n);
        out.clear();
        std::vector<std::vector<float>> segIn ((size_t) ch), segOut ((size_t) ch);

        for (size_t k = 0; k + 1 < cuts.size(); ++k)
        {
            const int inStart = cuts[k].first, inLen = cuts[k + 1].first - cuts[k].first;
            const int outStart = cuts[k].second, outLen = cuts[k + 1].second - cuts[k].second;
            if (inLen <= 0 || outLen <= 0)
                continue;
            const bool last = k + 2 == cuts.size();
            const int want = outLen + (last ? 0 : fade);       // a little extra to crossfade into the next part

            for (int c = 0; c < ch; ++c)
                segOut[(size_t) c].assign ((size_t) want, 0.0f);

            if (inLen == outLen)
            {
                // Not moved: the original audio, untouched
                for (int c = 0; c < ch; ++c)
                    for (int i = 0; i < want; ++i)
                        segOut[(size_t) c][(size_t) i] = inStart + i < n ? in.getSample (c, inStart + i) : 0.0f;
            }
            else
            {
                // Stretched or squeezed so its start lands exactly where it should
                const double rate = (double) inLen / outLen;
                const int seek = stretch.outputSeekLength ((float) rate);
                int totalOut = want;
                if (totalOut * rate < seek * 1.2)
                    totalOut = (int) std::ceil (seek * 1.2 / rate);
                const int totalIn = (int) std::lround (totalOut * rate);
                for (int c = 0; c < ch; ++c)
                {
                    // Only this part (and the bit used for the crossfade): the next note start must
                    // not be stretched in here, or it would be heard early as a faint echo
                    const int useIn = juce::jmin (totalIn, inLen + (int) std::ceil ((last ? 0 : fade) * rate) + fade);
                    segIn[(size_t) c].assign ((size_t) totalIn, 0.0f);
                    for (int i = 0; i < useIn && inStart + i < n; ++i)
                    {
                        const float tail = i < useIn - fade ? 1.0f : (float) (useIn - i) / (float) fade;
                        segIn[(size_t) c][(size_t) i] = in.getSample (c, inStart + i) * tail;
                    }
                    segOut[(size_t) c].assign ((size_t) totalOut, 0.0f);
                }
                stretch.exact (segIn, totalIn, segOut, totalOut);
            }

            // Add it in, with short fades where it meets its neighbours
            for (int c = 0; c < ch; ++c)
                for (int i = 0; i < want && outStart + i < n; ++i)
                {
                    float g = 1.0f;
                    if (k > 0 && i < fade)         g *= (float) i / (float) fade;
                    if (! last && i >= outLen)     g *= 1.0f - (float) (i - outLen) / (float) fade;
                    out.addSample (c, outStart + i, segOut[(size_t) c][(size_t) i] * g);
                }
        }

        dest.getParentDirectory().createDirectory();
        auto tmp = dest.getSiblingFile (dest.getFileNameWithoutExtension() + ".part");
        tmp.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (tmp, sr, (int) (ch), 24).release());
            if (w == nullptr || ! w->writeFromAudioSampleBuffer (out, 0, n))
                return false;
        }
        dest.deleteFile();
        return tmp.moveFileTo (dest);
    }

    bool apply (te::WaveAudioClip& clip, const Settings& settings, juce::String& problem)
    {
        const auto a = analyse (clip, settings);
        if (! a.ok || a.notes.empty())
        {
            problem = a.problem;
            return false;
        }

        std::vector<std::pair<double, double>> pairs;
        for (auto& note : a.notes)
            pairs.push_back ({ note.sourceSec, note.newSourceSec });

        auto folder = a.source.getParentDirectory().getChildFile ("Timed");
        const auto dest = folder.getNonexistentChildFile (a.source.getFileNameWithoutExtension() + " - timed", ".wav", false);
        if (! renderWarped (a.source, pairs, dest))
        {
            problem = "The corrected audio could not be written.";
            return false;
        }

        auto* um = &clip.edit.getUndoManager();
        const auto previous = isCorrected (clip) ? clip.getOriginalFile() : juce::File();
        clip.state.setProperty (srcId, a.source.getFullPathName(), um);
        clip.state.setProperty (gridId, settings.gridBeats, um);
        clip.state.setProperty (strengthId, settings.strength, um);
        clip.state.setProperty (toleranceId, settings.toleranceMs, um);
        clip.state.setProperty (sensId, settings.sensitivity, um);
        clip.getSourceFileReference().setToFile (dest, te::SourceFileReference::PathStyle::chooseBest, false);
        AudioEdit::prepare (clip);

        // The copy made before is no longer used (Ctrl+Z still works: undo restores the name, so keep it)
        juce::ignoreUnused (previous);
        return true;
    }

    void remove (te::WaveAudioClip& clip)
    {
        if (! isCorrected (clip))
            return;
        const juce::File src (clip.state[srcId].toString());
        auto* um = &clip.edit.getUndoManager();
        for (auto id : { srcId, gridId, strengthId, toleranceId, sensId })
            clip.state.removeProperty (id, um);
        if (src.existsAsFile())
        {
            clip.getSourceFileReference().setToFile (src, te::SourceFileReference::PathStyle::chooseBest, false);
            TempoFollow::sourceReplaced (clip);       // stretched again if the song's tempo changed since
        }
    }

    //==========================================================================
    //  The panel
    //==========================================================================
    namespace
    {
        class Panel final : public juce::Component
        {
        public:
            Panel (te::WaveAudioClip& c, std::function<void()> changed) : clip (&c), keep (&c), onChanged (std::move (changed))
            {
                using namespace StudioColours;
                const auto s = lastSettings (c);

                title.setText ("Timing correction", juce::dontSendNotification);
                title.setFont (studioFont (17.0f, true));
                title.setColour (juce::Label::textColourId, text);
                addAndMakeVisible (title);

                gridBox.addItem ("1/4 notes", 1);
                gridBox.addItem ("1/8 notes", 2);
                gridBox.addItem ("1/8 triplets (shuffle, 12/8)", 3);
                gridBox.addItem ("1/16 notes", 4);
                gridBox.addItem ("1/16 triplets", 5);
                const double grids[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0 };
                int sel = 4;
                for (int i = 0; i < 5; ++i)
                    if (std::abs (grids[i] - s.gridBeats) < 1.0e-6) sel = i + 1;
                gridBox.setSelectedId (sel, juce::dontSendNotification);
                gridBox.onChange = [this] { refresh(); };
                addAndMakeVisible (gridBox);

                sensBox.addItem ("Fewer note starts (only clear ones)", 1);
                sensBox.addItem ("Normal", 2);
                sensBox.addItem ("More note starts (soft singing)", 3);
                sensBox.setSelectedId (s.sensitivity + 1, juce::dontSendNotification);
                sensBox.onChange = [this] { refresh(); };
                addAndMakeVisible (sensBox);

                auto setup = [this] (juce::Slider& sl, double lo, double hi, double v, const juce::String& suffix, const juce::String& tip)
                {
                    sl.setSliderStyle (juce::Slider::LinearHorizontal);
                    sl.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
                    sl.setRange (lo, hi, 1.0);
                    sl.setValue (v, juce::dontSendNotification);
                    sl.setTextValueSuffix (suffix);
                    sl.setTooltip (tip);
                    sl.onValueChange = [this] { refresh(); };
                    addAndMakeVisible (sl);
                };
                setup (strength, 0.0, 100.0, s.strength * 100.0, " %", "100 % puts every note start exactly on the grid (stiff). 40-60 % tightens but keeps the feel.");
                setup (tolerance, 0.0, 50.0, s.toleranceMs, " ms", "Note starts already this close to the grid are left alone");

                summary.setColour (juce::Label::textColourId, textDim);
                summary.setFont (studioFont (12.5f));
                summary.setJustificationType (juce::Justification::topLeft);
                addAndMakeVisible (summary);

                applyButton.onClick = [this]
                {
                    if (clip == nullptr) return;
                    clip->edit.getUndoManager().beginNewTransaction ("Timing correction");
                    juce::String problem;
                    juce::MouseCursor::showWaitCursor();
                    const bool ok = apply (*clip, current(), problem);
                    juce::MouseCursor::hideWaitCursor();
                    summary.setText (ok ? juce::String ("Done. Play it: if it is too stiff, lower Strength and press Apply again.") : problem,
                                     juce::dontSendNotification);
                    removeButton.setEnabled (isCorrected (*clip));
                    if (onChanged) onChanged();
                };
                addAndMakeVisible (applyButton);

                removeButton.onClick = [this]
                {
                    if (clip == nullptr) return;
                    clip->edit.getUndoManager().beginNewTransaction ("Remove timing correction");
                    remove (*clip);
                    removeButton.setEnabled (false);
                    refresh();
                    summary.setText ("The original timing is back.", juce::dontSendNotification);
                    if (onChanged) onChanged();
                };
                removeButton.setEnabled (isCorrected (c));
                addAndMakeVisible (removeButton);

                setSize (560, 360);
                refresh();
            }

            Settings current() const
            {
                const double grids[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0 };
                Settings s;
                s.gridBeats = grids[juce::jlimit (0, 4, gridBox.getSelectedId() - 1)];
                s.strength = (float) strength.getValue() / 100.0f;
                s.toleranceMs = tolerance.getValue();
                s.sensitivity = juce::jlimit (0, 2, sensBox.getSelectedId() - 1);
                return s;
            }

            void refresh()
            {
                if (clip == nullptr) return;
                analysis = analyse (*clip, current());
                juce::String t;
                if (! analysis.problem.isEmpty())
                    t = analysis.problem;
                else
                {
                    int inClip = 0;
                    for (auto& n : analysis.notes) if (n.inClip) ++inClip;
                    t << "Found " << inClip << " note starts. ";
                    if (analysis.numMoving == 0)
                        t << "None needs moving with these settings.";
                    else
                        t << analysis.numMoving << " will move, on average " << juce::roundToInt (analysis.averageMoveMs)
                          << " ms (the most " << juce::roundToInt (analysis.biggestMoveMs) << " ms).";
                    if (isCorrected (*clip))
                        t << "  This clip is corrected now; Apply redoes it from the original recording.";
                }
                summary.setText (t, juce::dontSendNotification);
                applyButton.setEnabled (analysis.ok && ! analysis.notes.empty());
                repaint();
            }

            void paint (juce::Graphics& g) override
            {
                using namespace StudioColours;
                g.fillAll (card);
                g.setColour (textDim);
                g.setFont (studioFont (12.0f));
                for (auto& [r, label] : labels)
                    g.drawText (label, r, juce::Justification::centredLeft);

                // The clip: grey = note start now, gold = where it goes. Grid lines behind.
                auto r = strip.toFloat();
                g.setColour (background);
                g.fillRoundedRectangle (r, 4.0f);
                if (clip == nullptr || analysis.notes.empty())
                    return;

                const auto pos = clip->getPosition();
                const auto from = pos.getOffset().inSeconds(), len = juce::jmax (0.1, pos.getLength().inSeconds());
                auto xOf = [&] (double s) { return r.getX() + (float) ((s - from) / len) * r.getWidth(); };

                auto& ts = clip->edit.tempoSequence;
                const auto grid = current().gridBeats;
                const auto b0 = ts.toBeats (pos.getStart()).inBeats(), b1 = ts.toBeats (pos.getEnd()).inBeats();
                const auto step = (b1 - b0) / grid > 300 ? grid * std::ceil ((b1 - b0) / grid / 300) : grid;
                for (double b = std::ceil (b0 / step) * step; b <= b1; b += step)
                {
                    const auto s = from + (ts.toTime (te::BeatPosition::fromBeats (b)).inSeconds() - pos.getStart().inSeconds());
                    const bool beat = std::abs (b - std::round (b)) < 1.0e-6;
                    g.setColour (cardBorder.withAlpha (beat ? 1.0f : 0.5f));
                    g.drawVerticalLine ((int) xOf (s), r.getY() + 2, r.getBottom() - 2);
                }
                for (auto& n : analysis.notes)
                {
                    if (! n.inClip) continue;
                    const auto x0 = xOf (n.sourceSec), x1 = xOf (n.newSourceSec);
                    g.setColour (textDim);
                    g.fillRect (juce::Rectangle<float> (x0 - 1.0f, r.getY() + 6, 2.0f, r.getHeight() * 0.4f));
                    if (n.moves)
                    {
                        g.setColour (gold);
                        g.fillRect (juce::Rectangle<float> (x1 - 1.0f, r.getCentreY() + 2, 2.0f, r.getHeight() * 0.4f - 6));
                    }
                }
                g.setColour (textDim);
                g.setFont (studioFont (10.5f));
                g.drawText ("grey: now    gold: after", strip.withTrimmedBottom (-16).removeFromBottom (14), juce::Justification::centredRight);
            }

            void resized() override
            {
                labels.clear();
                auto r = getLocalBounds().reduced (14, 10);
                title.setBounds (r.removeFromTop (26));
                r.removeFromTop (6);
                auto row = [&] (const juce::String& label, juce::Component& c, int h = 26)
                {
                    auto line = r.removeFromTop (h);
                    labels.push_back ({ line.removeFromLeft (110), label });
                    c.setBounds (line);
                    r.removeFromTop (6);
                };
                row ("Line up to", gridBox);
                row ("Strength", strength);
                row ("Leave alone if", tolerance);
                row ("Find", sensBox);
                r.removeFromTop (4);
                strip = r.removeFromTop (54);
                r.removeFromTop (18);
                auto buttons = r.removeFromBottom (30);
                applyButton.setBounds (buttons.removeFromRight (110));
                buttons.removeFromRight (8);
                removeButton.setBounds (buttons.removeFromRight (200));
                r.removeFromBottom (6);
                summary.setBounds (r);
            }

        private:
            te::WaveAudioClip* clip;
            te::Clip::Ptr keep;            // the clip stays alive while the panel is open
            std::function<void()> onChanged;
            Analysis analysis;
            juce::Label title, summary;
            juce::ComboBox gridBox, sensBox;
            juce::Slider strength, tolerance;
            juce::TextButton applyButton { "Apply" }, removeButton { "Remove timing correction" };
            juce::Rectangle<int> strip;
            std::vector<std::pair<juce::Rectangle<int>, juce::String>> labels;
        };
    }

    std::unique_ptr<juce::Component> createPanel (te::WaveAudioClip& clip, std::function<void()> onChanged)
    {
        return std::make_unique<Panel> (clip, std::move (onChanged));
    }
}
