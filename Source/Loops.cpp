#include "Loops.h"
#include "WavFiles.h"
#include "AudioEdit.h"
#include <tracktion_engine/3rd_party/signalsmith-stretch/signalsmith-stretch.h>

namespace Loops
{
    namespace
    {
        juce::StringArray tokens (const juce::String& name)
        {
            auto n = name.toLowerCase();
            for (auto c : juce::String ("-_.,()[]{}+&"))
                n = n.replaceCharacter (c, ' ');
            return juce::StringArray::fromTokens (n, " ", "");
        }
    }

    std::optional<double> tempoFromName (const juce::String& name)
    {
        auto t = tokens (name);

        // "96bpm", "96 bpm", "bpm96"
        for (int i = 0; i < t.size(); ++i)
        {
            auto s = t[i];
            if (s.contains ("bpm"))
            {
                auto num = s.replace ("bpm", "").trim();
                if (num.isEmpty() && i > 0) num = t[i - 1];
                if (num.isEmpty() && i + 1 < t.size()) num = t[i + 1];
                const auto v = num.getDoubleValue();
                if (num.containsOnly ("0123456789.") && v >= 50.0 && v <= 220.0)
                    return v;
            }
        }

        // A lone number that looks like a tempo ("Shaker 96")
        for (int i = t.size(); --i >= 0;)
        {
            const auto& s = t[i];
            if (s.containsOnly ("0123456789") && s.length() >= 2 && s.length() <= 3)
            {
                const auto v = s.getIntValue();
                if (v >= 60 && v <= 200)
                    return (double) v;
            }
        }
        return std::nullopt;
    }

    int rootFromName (const juce::String& name)
    {
        static const char* names[] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
        static const std::pair<const char*, int> flats[] = { { "db", 1 }, { "eb", 3 }, { "gb", 6 }, { "ab", 8 }, { "bb", 10 } };

        auto t = tokens (name.replace ("\xe2\x99\xaf", "#").replace ("\xe2\x99\xad", "b"));
        for (int i = t.size(); --i >= 0;)
        {
            auto s = t[i];
            if (s == "key" || s.isEmpty()) continue;
            for (auto suffix : { "minor", "major", "min", "maj", "m" })
                if (s.endsWith (suffix) && s.length() > (int) strlen (suffix)) { s = s.dropLastCharacters ((int) strlen (suffix)); break; }
            for (int n = 0; n < 12; ++n)
                if (s == names[n]) return n;
            for (auto& f : flats)
                if (s == f.first) return f.second;
        }
        return -1;
    }

    Guess guess (te::Engine& engine, const juce::File& file, double songBpm, int beatsPerBar, bool sure)
    {
        Guess g;
        te::AudioFile af (engine, file);
        const auto info = af.getInfo();
        if (! af.isValid() || info.sampleRate <= 0.0 || info.lengthInSamples <= 0)
            return g;
        const auto seconds = (double) info.lengthInSamples / info.sampleRate;
        beatsPerBar = juce::jmax (1, beatsPerBar);

        g.root = rootFromName (file.getFileNameWithoutExtension());

        // 1. The file says so (Acid or Apple loop information)
        const auto& li = info.loopInfo;
        if (li.getNumBeats() > 0.5 && ! li.isOneShot())
        {
            const auto bpm = li.getBpm (info);
            if (bpm >= 40.0 && bpm <= 250.0)
            {
                g.bpm = bpm;
                g.beats = li.getNumBeats();
                if (g.root < 0 && li.getRootNote() >= 0 && li.getRootNote() < 128)
                    g.root = li.getRootNote() % 12;
                g.how = "from the file";
                return g;
            }
        }

        // 2. The name says so ("96bpm")
        if (auto bpm = tempoFromName (file.getFileNameWithoutExtension()))
        {
            auto beats = *bpm * seconds / 60.0;
            const auto whole = std::round (beats);
            if (whole >= 1.0 && std::abs (beats - whole) < 0.2)
                beats = whole;           // the file is usually trimmed to whole beats
            g.beats = beats;
            g.bpm = beats * 60.0 / seconds;
            g.how = "from its name";
            return g;
        }

        // 3. Its length in whole bars: the tempo closest to the song's
        if (! sure)
            return g;
        double bestScore = 1.0e9;
        for (int bars : { 1, 2, 4, 8, 16 })
        {
            const double beats = bars * beatsPerBar;
            const double bpm = beats * 60.0 / seconds;
            if (bpm < 55.0 || bpm > 200.0)
                continue;
            const auto score = std::abs (std::log2 (bpm / juce::jmax (30.0, songBpm)));
            if (score < bestScore)
            {
                bestScore = score;
                g.bpm = bpm;
                g.beats = beats;
            }
        }
        if (g.valid())
            g.how = "from its length";
        return g;
    }

    //==========================================================================
    //  Following the tempo: we stretch a copy of the loop ourselves (Signalsmith,
    //  offline, so every repeat lines up exactly) and play that copy. When the
    //  song's tempo or key changes, a new copy is made.
    //==========================================================================
    namespace
    {
        const juce::Identifier srcId ("gpsLoopSource"), bpmId ("gpsLoopBpm"), beatsId ("gpsLoopBeats"),
                               rootId ("gpsLoopRoot"), keyId ("gpsLoopFollowKey"),
                               madeBpmId ("gpsLoopMadeBpm"), madeShiftId ("gpsLoopMadeShift");

        double songBpmAt (te::Clip& c)
        {
            return c.edit.tempoSequence.getTempoAt (c.getPosition().getStart()).getBpm();
        }

        int songKeyPc (te::Clip& c)
        {
            return c.edit.pitchSequence.getPitchAt (c.getPosition().getStart()).getPitch() % 12;
        }

        int shiftFor (te::WaveAudioClip& clip)
        {
            const int root = clip.state.getProperty (rootId, -1);
            if (root < 0 || ! (bool) clip.state.getProperty (keyId, false))
                return 0;
            auto d = (songKeyPc (clip) - root + 12) % 12;
            return d > 6 ? d - 12 : d;          // the nearer way: up to 6 semitones up or 5 down
        }

        juce::File makeStretched (te::Engine& engine, const juce::File& src, double beats, double bpm, int shift)
        {
            juce::AudioFormatManager& fm = engine.getAudioFileFormatManager().readFormatManager;
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (src));
            if (r == nullptr || r->lengthInSamples < 64 || r->lengthInSamples > (juce::int64) (r->sampleRate * 120.0))
                return {};

            const auto sr = r->sampleRate;
            const int ch = (int) juce::jlimit (1u, 2u, r->numChannels);
            const int n = (int) r->lengthInSamples;
            const int m = juce::jmax (64, (int) std::lround (beats * 60.0 / bpm * sr));

            auto folder = src.getParentDirectory().getChildFile ("Following");
            folder.createDirectory();
            const auto out = folder.getChildFile (src.getFileNameWithoutExtension()
                                                  + " - " + juce::String (bpm, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + "bpm"
                                                  + (shift != 0 ? (shift > 0 ? " +" : " ") + juce::String (shift) : juce::String())
                                                  + ".wav");
            if (out.existsAsFile())
                return out;

            juce::AudioBuffer<float> in (ch, n);
            r->read (&in, 0, n, 0, true, ch > 1);

            if (m == n && shift == 0)
            {
                src.copyFileTo (out);
                return out;
            }

            // Three copies in a row, stretched in one go; the middle one loops without a click
            std::vector<std::vector<float>> input ((size_t) ch, std::vector<float> ((size_t) n * 3)),
                                            output ((size_t) ch, std::vector<float> ((size_t) m * 3));
            for (int c = 0; c < ch; ++c)
                for (int k = 0; k < 3; ++k)
                    std::copy (in.getReadPointer (c), in.getReadPointer (c) + n, input[(size_t) c].begin() + (size_t) k * (size_t) n);

            signalsmith::stretch::SignalsmithStretch<float> stretch;
            stretch.presetDefault (ch, (float) sr);
            stretch.setTransposeSemitones ((float) shift);
            stretch.exact (input, n * 3, output, m * 3);

            juce::AudioBuffer<float> result (ch, m);
            for (int c = 0; c < ch; ++c)
                std::copy (output[(size_t) c].begin() + m, output[(size_t) c].begin() + 2 * m, result.getWritePointer (c));

            juce::WavAudioFormat wav;
            auto tmp = out.getSiblingFile (out.getFileNameWithoutExtension() + ".part");
            tmp.deleteFile();
            {
                std::unique_ptr<juce::AudioFormatWriter> w (WavFiles::writer (tmp, sr, (int) (ch), 24).release());
                if (w == nullptr || ! w->writeFromAudioSampleBuffer (result, 0, m))
                    return {};
            }
            return tmp.moveFileTo (out) ? out : juce::File();
        }

        // Makes (or finds) the copy for the song's tempo and key now, and plays it.
        bool apply (te::WaveAudioClip& clip, std::optional<double> keepBeats)
        {
            const juce::File src (clip.state[srcId].toString());
            const double beats = clip.state.getProperty (beatsId, 0.0);
            if (! src.existsAsFile() || beats <= 0.0)
                return false;

            const auto bpm = songBpmAt (clip);
            const auto shift = shiftFor (clip);
            const auto lengthBeats = keepBeats ? *keepBeats : clip.getLengthInBeats().inBeats();

            const auto file = makeStretched (clip.edit.engine, src, beats, bpm, shift);
            if (! file.existsAsFile())
                return false;

            const auto start = clip.getPosition().getStart();
            clip.setAutoTempo (false);
            clip.setAutoPitch (false);
            clip.setSpeedRatio (1.0);
            clip.getSourceFileReference().setToFile (file, te::SourceFileReference::PathStyle::chooseBest, false);
            clip.setTimeStretchMode (te::TimeStretcher::disabled);
            clip.setOffset ({});
            const auto loopLen = te::TimeDuration::fromSeconds (beats * 60.0 / bpm);
            clip.setLoopRange ({ te::TimePosition(), te::TimePosition() + loopLen });

            auto& ts = clip.edit.tempoSequence;
            const auto startBeat = ts.toBeats (start);
            clip.setEnd (ts.toTime (startBeat + te::BeatDuration::fromBeats (lengthBeats)), false);

            clip.state.setProperty (madeBpmId, bpm, nullptr);
            clip.state.setProperty (madeShiftId, shift, nullptr);
            return true;
        }
    }

    bool follow (te::WaveAudioClip& clip, const Guess& g, int repeats)
    {
        if (! g.valid())
            return false;
        auto src = isFollowing (clip) ? juce::File (clip.state[srcId].toString()) : clip.getOriginalFile();
        if (! src.existsAsFile())
            return false;

        auto* um = &clip.edit.getUndoManager();
        clip.state.setProperty (srcId, src.getFullPathName(), um);
        clip.state.setProperty (bpmId, g.bpm, um);
        clip.state.setProperty (beatsId, g.beats, um);
        clip.state.setProperty (rootId, g.root, um);
        if (! clip.state.hasProperty (keyId))
            clip.state.setProperty (keyId, false, um);
        return apply (clip, g.beats * juce::jmax (1, repeats));
    }

    bool refresh (te::WaveAudioClip& clip)
    {
        if (! isFollowing (clip))
            return false;
        const double made = clip.state.getProperty (madeBpmId, 0.0);
        const int madeShift = clip.state.getProperty (madeShiftId, 0);
        const auto bpm = songBpmAt (clip);
        if (std::abs (made - bpm) < 0.005 && madeShift == shiftFor (clip) && clip.getOriginalFile().existsAsFile())
            return false;

        // Keep the same number of beats on the timeline
        const auto beatsLong = made > 0.0 ? clip.getPosition().getLength().inSeconds() * made / 60.0 : clip.getLengthInBeats().inBeats();
        return apply (clip, beatsLong);
    }

    int refreshAll (te::Edit& edit)
    {
        int n = 0;
        std::vector<te::WaveAudioClip*> following;
        for (auto* t : te::getAudioTracks (edit))
            for (auto* c : t->getClips())
                if (auto* w = dynamic_cast<te::WaveAudioClip*> (c); w != nullptr && isFollowing (*w))
                {
                    following.push_back (w);
                    if (refresh (*w)) ++n;
                }

        // Tidy up copies made for tempos no clip uses any more (undo makes them again if needed)
        if (n > 0)
        {
            juce::Array<juce::File> used, folders;
            for (auto* w : following)
            {
                used.addIfNotAlreadyThere (w->getOriginalFile());
                folders.addIfNotAlreadyThere (juce::File (w->state[srcId].toString()).getParentDirectory().getChildFile ("Following"));
            }
            for (auto& folder : folders)
                for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav"))
                    if (! used.contains (f))
                        for (auto* w : following)
                            if (f.getFileName().startsWith (juce::File (w->state[srcId].toString()).getFileNameWithoutExtension() + " - "))
                            {
                                f.deleteFile();
                                break;
                            }
        }
        return n;
    }

    void stopFollowing (te::WaveAudioClip& clip)
    {
        if (! isFollowing (clip))
            return;
        const juce::File src (clip.state[srcId].toString());
        auto* um = &clip.edit.getUndoManager();
        for (auto id : { srcId, bpmId, beatsId, rootId, keyId, madeBpmId, madeShiftId })
            clip.state.removeProperty (id, um);

        clip.disableLooping();
        if (src.existsAsFile())
            clip.getSourceFileReference().setToFile (src, te::SourceFileReference::PathStyle::chooseBest, false);
        clip.setOffset ({});
        AudioEdit::prepare (clip);
        const auto len = te::AudioFile (clip.edit.engine, src).getLength();
        if (len > 0.0)
            clip.setLength (te::TimeDuration::fromSeconds (len), true);
    }

    bool isFollowing (const te::WaveAudioClip& clip)    { return clip.state.hasProperty (srcId); }

    double getLoopTempo (te::WaveAudioClip& clip)       { return clip.state.getProperty (bpmId, 0.0); }

    void setLoopTempo (te::WaveAudioClip& clip, double bpm)
    {
        const juce::File src (clip.state[srcId].toString());
        if (! isFollowing (clip) || bpm < 30.0 || bpm > 300.0)
            return;
        const auto seconds = te::AudioFile (clip.edit.engine, src).getLength();
        Guess g;
        g.bpm = bpm;
        g.beats = bpm * seconds / 60.0;
        g.root = getRoot (clip);
        const double oldBeats = clip.state.getProperty (beatsId, 1.0);
        const auto repeats = juce::jmax (1, (int) std::lround (clip.getLengthInBeats().inBeats() / juce::jmax (0.25, oldBeats)));
        follow (clip, g, repeats);
    }

    int getRoot (te::WaveAudioClip& clip)               { return clip.state.getProperty (rootId, -1); }

    void setRoot (te::WaveAudioClip& clip, int root)
    {
        clip.state.setProperty (rootId, root, &clip.edit.getUndoManager());
        if (root < 0)
            clip.state.setProperty (keyId, false, &clip.edit.getUndoManager());
        refresh (clip);
    }

    void setFollowKey (te::WaveAudioClip& clip, bool b)
    {
        if (b && getRoot (clip) < 0)
            return;
        clip.state.setProperty (keyId, b, &clip.edit.getUndoManager());
        refresh (clip);
    }

    bool isFollowingKey (const te::WaveAudioClip& clip)  { return clip.state.getProperty (keyId, false); }

    juce::String describe (te::WaveAudioClip& clip)
    {
        if (! isFollowing (clip))
            return {};
        static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        const double beats = clip.state.getProperty (beatsId, 0.0);
        juce::String s = "Loop " + juce::String (std::round (getLoopTempo (clip) * 10.0) / 10.0) + " BPM, "
                         + juce::String (beats, std::abs (beats - std::round (beats)) < 0.01 ? 0 : 1) + " beats";
        if (auto r = getRoot (clip); r >= 0)
            s << ", " << names[r % 12] << (isFollowingKey (clip) ? " (follows the key)" : "");
        return s;
    }
}
