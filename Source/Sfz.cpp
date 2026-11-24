#include "Sfz.h"

namespace Sfz
{

//==============================================================================
int noteNumberFromText (const juce::String& textIn)
{
    const auto text = textIn.trim().toLowerCase();
    if (text.isEmpty())
        return -1;
    if (juce::CharacterFunctions::isDigit (text[0]) || text[0] == '-')
        return text.getIntValue();

    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };     // a b c d e f g
    const auto letter = text[0];
    if (letter < 'a' || letter > 'g')
        return -1;

    int note = base[letter - 'a'];
    int i = 1;
    if (text[i] == '#')      { ++note; ++i; }
    else if (text[i] == 'b') { --note; ++i; }

    const auto octave = text.substring (i).getIntValue();
    return juce::jlimit (0, 127, note + (octave + 1) * 12);
}

namespace
{
    // Reads a file with #include and #define handled, comments removed.
    juce::String preprocess (const juce::File& file, std::map<juce::String, juce::String>& defines, int depth = 0)
    {
        if (depth > 8 || ! file.existsAsFile())
            return {};

        auto text = file.loadFileAsString();
        juce::String out;
        out.preallocateBytes ((size_t) text.length() + 16);

        // Remove comments
        juce::String clean;
        {
            auto p = text.getCharPointer();
            bool inBlock = false;
            while (! p.isEmpty())
            {
                const auto c = *p;
                if (inBlock)
                {
                    if (c == '*' && p[1] == '/') { inBlock = false; p += 2; continue; }
                    ++p;
                    continue;
                }
                if (c == '/' && p[1] == '/')
                {
                    while (! p.isEmpty() && *p != '\n') ++p;
                    continue;
                }
                if (c == '/' && p[1] == '*') { inBlock = true; p += 2; continue; }
                clean += c;
                ++p;
            }
        }

        for (auto line : juce::StringArray::fromLines (clean))
        {
            line = line.trim();
            if (line.startsWith ("#define"))
            {
                auto rest = line.substring (7).trim();
                const auto name = rest.upToFirstOccurrenceOf (" ", false, false).trim();
                const auto value = rest.fromFirstOccurrenceOf (" ", false, false).trim();
                if (name.startsWithChar ('$'))
                    defines[name] = value;
                continue;
            }
            if (line.startsWith ("#include"))
            {
                const auto name = line.fromFirstOccurrenceOf ("\"", false, false).upToLastOccurrenceOf ("\"", false, false);
                out << preprocess (file.getParentDirectory().getChildFile (name.replaceCharacter ('\\', '/')), defines, depth + 1) << "\n";
                continue;
            }

            // Longest names first so $A does not replace part of $AB
            std::vector<std::pair<juce::String, juce::String>> sorted (defines.begin(), defines.end());
            std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.first.length() > b.first.length(); });
            for (const auto& [name, value] : sorted)
                line = line.replace (name, value);

            out << line << "\n";
        }
        return out;
    }

    using Opcodes = std::map<juce::String, juce::String>;

    struct Token { bool isHeader; juce::String name, value; };

    // Splits into headers and opcodes. A value runs until the next "name=" or "<header>",
    // so sample paths with spaces work.
    std::vector<Token> tokenise (const juce::String& text)
    {
        std::vector<Token> tokens;
        for (const auto& lineIn : juce::StringArray::fromLines (text))
        {
            const auto line = lineIn.trim();
            int i = 0;
            const int n = line.length();

            while (i < n)
            {
                while (i < n && juce::CharacterFunctions::isWhitespace (line[i])) ++i;
                if (i >= n) break;

                if (line[i] == '<')
                {
                    const auto close = line.indexOfChar (i, '>');
                    if (close < 0) break;
                    tokens.push_back ({ true, line.substring (i + 1, close).trim().toLowerCase(), {} });
                    i = close + 1;
                    continue;
                }

                const auto eq = line.indexOfChar (i, '=');
                if (eq < 0) break;
                const auto name = line.substring (i, eq).trim();

                // Find where the value ends: before the next " opcode=" or "<"
                int end = n;
                for (int j = eq + 1; j < n; ++j)
                {
                    if (line[j] == '<') { end = j; break; }
                    if (juce::CharacterFunctions::isWhitespace (line[j]))
                    {
                        int k = j;
                        while (k < n && juce::CharacterFunctions::isWhitespace (line[k])) ++k;
                        int m = k;
                        while (m < n && (juce::CharacterFunctions::isLetterOrDigit (line[m]) || line[m] == '_')) ++m;
                        if (m > k && m < n && line[m] == '=') { end = j; break; }
                    }
                }

                tokens.push_back ({ false, name.toLowerCase(), line.substring (eq + 1, end).trim() });
                i = end;
            }
        }
        return tokens;
    }

    float f (const Opcodes& o, const char* key, float def)
    {
        auto it = o.find (key);
        return it != o.end() ? it->second.getFloatValue() : def;
    }

    int note (const Opcodes& o, const char* key, int def)
    {
        auto it = o.find (key);
        if (it == o.end()) return def;
        const auto n = noteNumberFromText (it->second);
        return n >= 0 ? n : def;
    }

    bool has (const Opcodes& o, const char* key)    { return o.find (key) != o.end(); }

    // Case-insensitive file lookup (Windows paths in packs made on other systems)
    juce::File findFile (const juce::File& base, const juce::String& relativeIn)
    {
        const auto relative = relativeIn.replaceCharacter ('\\', '/').trim();
        auto direct = base.getChildFile (relative);
        if (direct.existsAsFile())
            return direct;

        auto dir = base;
        for (const auto& part : juce::StringArray::fromTokens (relative, "/", ""))
        {
            if (part.isEmpty() || part == ".") continue;
            if (part == "..") { dir = dir.getParentDirectory(); continue; }

            auto next = dir.getChildFile (part);
            if (! next.exists())
                for (const auto& child : dir.findChildFiles (juce::File::findFilesAndDirectories, false))
                    if (child.getFileName().equalsIgnoreCase (part)) { next = child; break; }
            dir = next;
        }
        return dir;
    }
}

//==============================================================================
std::vector<ParsedRegion> parse (const juce::File& sfzFile, juce::StringArray& warnings)
{
    std::map<juce::String, juce::String> defines;
    const auto tokens = tokenise (preprocess (sfzFile, defines));

    Opcodes control, global, master, group, region;
    Opcodes* current = nullptr;
    Opcodes ignored;
    std::vector<Opcodes> regions;
    bool inRegion = false;

    auto flush = [&]
    {
        if (! inRegion) return;
        Opcodes merged = global;
        for (auto& [k, v] : master) merged[k] = v;
        for (auto& [k, v] : group)  merged[k] = v;
        for (auto& [k, v] : region) merged[k] = v;
        regions.push_back (merged);
        inRegion = false;
    };

    for (const auto& t : tokens)
    {
        if (t.isHeader)
        {
            flush();
            if (t.name == "control")      { current = &control; }
            else if (t.name == "global")  { global.clear(); master.clear(); group.clear(); current = &global; }
            else if (t.name == "master")  { master.clear(); group.clear(); current = &master; }
            else if (t.name == "group")   { group.clear(); current = &group; }
            else if (t.name == "region")  { region.clear(); current = &region; inRegion = true; }
            else                          { ignored.clear(); current = &ignored; }
            continue;
        }
        if (current != nullptr)
            (*current)[t.name] = t.value;
    }
    flush();

    const auto defaultPath = control.count ("default_path") ? control["default_path"] : juce::String();
    const auto noteOffset = (int) f (control, "note_offset", 0.0f) + 12 * (int) f (control, "octave_offset", 0.0f);

    // Key switches: keep only the default articulation
    juce::String activeSwitch;
    if (control.count ("sw_default")) activeSwitch = control["sw_default"];
    for (auto& r : regions)
        if (activeSwitch.isEmpty() && r.count ("sw_default")) activeSwitch = r["sw_default"];
    for (auto& r : regions)
        if (activeSwitch.isEmpty() && r.count ("sw_last")) activeSwitch = r["sw_last"];

    std::vector<ParsedRegion> out;
    int skipped = 0;
    const auto base = sfzFile.getParentDirectory();

    for (auto& o : regions)
    {
        if (! has (o, "sample")) { ++skipped; continue; }

        // Regions triggered by controllers (pedal noises etc.) are not used
        bool ccTriggered = false;
        for (auto& [k, v] : o)
            if (k.startsWith ("on_locc") || k.startsWith ("on_hicc")) ccTriggered = true;
        if (ccTriggered) { ++skipped; continue; }

        // Pedal-down sample sets: keep the pedal-up ones
        if (has (o, "locc64") && f (o, "locc64", 0.0f) >= 64.0f) { ++skipped; continue; }

        if (has (o, "sw_last") && activeSwitch.isNotEmpty()
             && noteNumberFromText (o["sw_last"]) != noteNumberFromText (activeSwitch)) { ++skipped; continue; }

        const auto sampleName = o["sample"];
        if (sampleName.startsWithChar ('*')) { ++skipped; continue; }      // generated waves (*sine) not supported

        ParsedRegion pr;
        auto& r = pr.region;
        pr.sample = findFile (base, defaultPath + sampleName);

        if (has (o, "key"))
        {
            r.lokey = r.hikey = r.keycenter = note (o, "key", 60);
        }
        r.lokey = juce::jlimit (0, 127, note (o, "lokey", r.lokey) + noteOffset);
        r.hikey = juce::jlimit (0, 127, note (o, "hikey", r.hikey) + noteOffset);
        r.keycenter = note (o, "pitch_keycenter", has (o, "key") ? r.keycenter : 60) + noteOffset;
        r.lovel = juce::jlimit (1, 127, (int) f (o, "lovel", 1.0f));
        r.hivel = juce::jlimit (1, 127, (int) f (o, "hivel", 127.0f));
        r.lorand = f (o, "lorand", 0.0f);
        r.hirand = f (o, "hirand", 1.0f);
        r.seqLength = juce::jmax (1, (int) f (o, "seq_length", 1.0f));
        r.seqPosition = juce::jmax (1, (int) f (o, "seq_position", 1.0f));
        r.tune = f (o, "tune", f (o, "pitch", 0.0f));
        r.transpose = f (o, "transpose", 0.0f);
        r.keytrack = f (o, "pitch_keytrack", 100.0f);
        r.volume = f (o, "volume", 0.0f) + (has (o, "amplitude") ? juce::Decibels::gainToDecibels (f (o, "amplitude", 100.0f) / 100.0f) : 0.0f);
        r.pan = f (o, "pan", 0.0f);
        r.ampVeltrack = f (o, "amp_veltrack", 100.0f);
        r.attack = f (o, "ampeg_attack", 0.0f);
        r.hold = f (o, "ampeg_hold", 0.0f);
        r.decay = f (o, "ampeg_decay", 0.0f);
        r.sustain = f (o, "ampeg_sustain", 100.0f);
        r.release = juce::jmax (0.001f, f (o, "ampeg_release", 0.001f));
        r.rtDecay = f (o, "rt_decay", 0.0f);
        r.offset = (juce::int64) f (o, "offset", 0.0f);
        r.end = has (o, "end") ? (juce::int64) f (o, "end", -1.0f) : -1;
        r.loopStart = (juce::int64) f (o, "loop_start", f (o, "loopstart", 0.0f));
        r.loopEnd = has (o, "loop_end") || has (o, "loopend") ? (juce::int64) f (o, "loop_end", f (o, "loopend", -1.0f)) : -1;

        const auto loop = has (o, "loop_mode") ? o["loop_mode"] : (has (o, "loopmode") ? o["loopmode"] : juce::String());
        r.loopMode = loop == "one_shot" ? LoopMode::oneShot
                   : loop == "loop_continuous" ? LoopMode::continuous
                   : loop == "loop_sustain" ? LoopMode::sustain : LoopMode::none;

        const auto trig = has (o, "trigger") ? o["trigger"] : juce::String ("attack");
        if (trig == "release" || trig == "release_key") r.trigger = Trigger::release;
        else if (trig == "attack" || trig == "first" || trig == "legato") r.trigger = Trigger::attack;
        else { ++skipped; continue; }

        r.group = (int) f (o, "group", f (o, "polyphony_group", 0.0f));
        r.offBy = (int) f (o, "off_by", 0.0f);
        r.offFast = ! has (o, "off_mode") || o["off_mode"] != "normal";

        out.push_back (pr);
    }

    if (skipped > 0)
        warnings.add (juce::String (skipped) + " regions not used (controller-triggered or unsupported)");
    return out;
}

//==============================================================================
InstrumentPtr load (const juce::File& sfzFile, std::atomic<bool>* cancel)
{
    auto inst = std::make_shared<Instrument>();
    inst->file = sfzFile;
    inst->name = sfzFile.getFileNameWithoutExtension();

    const auto parsed = parse (sfzFile, inst->warnings);
    if (parsed.empty())
        return nullptr;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::map<juce::String, int> sampleIndex;
    juce::AudioBuffer<float> temp;

    for (const auto& pr : parsed)
    {
        if (cancel != nullptr && cancel->load())
            return nullptr;

        const auto path = pr.sample.getFullPathName();
        auto it = sampleIndex.find (path);
        if (it == sampleIndex.end())
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (pr.sample));
            if (reader == nullptr)
            {
                inst->warnings.add ("Missing sample: " + pr.sample.getFileName());
                sampleIndex[path] = -1;
                continue;
            }

            Sample s;
            s.sampleRate = reader->sampleRate;
            s.length = (juce::int64) reader->lengthInSamples;
            const int numCh = juce::jlimit (1, 2, (int) reader->numChannels);
            s.channels.assign ((size_t) numCh, std::vector<juce::int16> ((size_t) s.length + 4, 0));

            // Loop points stored in WAV files
            const auto& meta = reader->metadataValues;
            if (meta.getValue ("NumSampleLoops", "0").getIntValue() > 0)
            {
                s.loopStart = meta.getValue ("Loop0Start", "-1").getLargeIntValue();
                s.loopEnd = meta.getValue ("Loop0End", "-1").getLargeIntValue();
            }

            const int chunk = 65536;
            temp.setSize (numCh, chunk, false, false, true);
            for (juce::int64 pos = 0; pos < s.length; pos += chunk)
            {
                const auto n = (int) juce::jmin ((juce::int64) chunk, s.length - pos);
                reader->read (&temp, 0, n, pos, true, numCh > 1);
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const auto* src = temp.getReadPointer (ch);
                    auto* dst = s.channels[(size_t) ch].data() + pos;
                    for (int i = 0; i < n; ++i)
                        dst[i] = (juce::int16) juce::jlimit (-32767, 32767, juce::roundToInt (src[i] * 32767.0f));
                }
            }

            inst->memoryBytes += s.length * numCh * 2;
            sampleIndex[path] = (int) inst->samples.size();
            inst->samples.push_back (std::move (s));
            it = sampleIndex.find (path);
        }

        if (it->second < 0)
            continue;

        auto r = pr.region;
        r.sampleIndex = it->second;
        const auto& s = inst->samples[(size_t) r.sampleIndex];
        if (r.end < 0 || r.end > s.length) r.end = s.length;
        if (r.loopMode != LoopMode::none && r.loopMode != LoopMode::oneShot && r.loopEnd < 0)
        {
            r.loopStart = s.loopStart >= 0 ? s.loopStart : 0;
            r.loopEnd = s.loopEnd > 0 ? s.loopEnd : s.length - 1;
        }
        // Samples with their own loop and no loop_mode loop by default (SFZ rule)
        if (r.loopMode == LoopMode::none && s.loopEnd > s.loopStart && s.loopStart >= 0)
        {
            r.loopMode = LoopMode::continuous;
            r.loopStart = s.loopStart;
            r.loopEnd = s.loopEnd;
        }
        inst->regions.push_back (r);
    }

    if (inst->regions.empty())
        return nullptr;
    return inst;
}

//==============================================================================
Library* Library::instance = nullptr;

Library& Library::getInstance()
{
    if (instance == nullptr)
        instance = new Library();
    return *instance;
}

Library::Library() {}

Library::~Library()
{
    pool.removeAllJobs (true, 10000);
    instance = nullptr;
}

void Library::request (const juce::File& sfzFile, std::function<void (InstrumentPtr)> onLoaded)
{
    const auto key = sfzFile.getFullPathName();
    {
        const juce::ScopedLock sl (lock);
        if (auto existing = cache[key].lock())
        {
            juce::MessageManager::callAsync ([onLoaded, existing] { onLoaded (existing); });
            return;
        }

        auto& queue = waiting[key];
        queue.push_back (onLoaded);
        if (queue.size() > 1)
            return;     // already loading
    }

    pool.addJob ([this, sfzFile, key]
    {
        auto inst = load (sfzFile);

        std::vector<std::function<void (InstrumentPtr)>> callbacks;
        {
            const juce::ScopedLock sl (lock);
            cache[key] = inst;
            callbacks.swap (waiting[key]);
            waiting.erase (key);
        }

        juce::MessageManager::callAsync ([callbacks, inst]
        {
            for (auto& cb : callbacks)
                cb (inst);
        });
    });
}

void Library::forget (const juce::File& sfzFile)
{
    const juce::ScopedLock sl (lock);
    cache.erase (sfzFile.getFullPathName());
}

juce::int64 Library::getMemoryInUse()
{
    const juce::ScopedLock sl (lock);
    juce::int64 total = 0;
    for (auto& [k, w] : cache)
        if (auto p = w.lock())
            total += p->memoryBytes;
    return total;
}

}
