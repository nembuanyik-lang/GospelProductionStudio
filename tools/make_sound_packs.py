#!/usr/bin/env python3
"""
Builds the Gospel Production Studio sound packs.

Runs on GitHub (workflow "Build sound packs"), so nothing large is downloaded
on the user's own connection. For each pack it:
  1. downloads the free source library (once, cached),
  2. reads its SFZ and flattens it (every region written out in full),
  3. optionally keeps fewer velocity layers (smaller download),
  4. converts the samples to Ogg Vorbis,
  5. zips the pack and lists it in packs.json with its size.

Usage:  python3 tools/make_sound_packs.py --out dist [--only id,id] [--source id=/local/archive]
"""
import argparse, concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys, tarfile, urllib.parse, urllib.request, zipfile

SALAMANDER = "https://freepats.zenvoid.org/Piano/SalamanderGrandPiano/SalamanderGrandPianoV3+20161209_44khz16bit.tar.xz"
UPRIGHT    = "https://freepats.zenvoid.org/Piano/UprightPianoKW/UprightPianoKW-SFZ+FLAC-20220221.7z"
BASS_FING  = "https://github.com/freepats/electric-bass-YR/releases/download/2019-09-30/FingerBassYR-SFZ+FLAC-20190930.7z"
BASS_PICK  = "https://github.com/freepats/electric-bass-YR/releases/download/2019-09-30/PickedBassYR-SFZ+FLAC-20190930.7z"
DRUMS      = "https://github.com/freepats/muldjordkit/releases/download/2020-10-18/MuldjordKit-SFZ+FLAC-20201018.7z"
# Step 30: recorded guitars (both CC0, FreePats)
GTR_CLEAN  = "https://github.com/freepats/electric-guitar-FSBS-clean/releases/download/2026-08-07/EGuitarFSBS-clean-SFZ+FLAC-20260807.7z"
GTR_NYLON  = "https://freepats.zenvoid.org/Guitar/SpanishClassicalGuitar/SpanishClassicalGuitar-SFZ+FLAC-20190618.7z"
# Step 31: hand percussion, bells and balafon from the Versilian Community Sample Library (CC0)
VCSL       = "vcsl"

PACKS = [
    dict(id="piano-grand", name="Grand Piano", category="Pianos", version="1",
         description="Yamaha C5 concert grand (Salamander), 8 velocity layers. The everyday piano.",
         licence="CC-BY 3.0", credit="Salamander Grand Piano V3 by Alexander Holm (via FreePats)",
         instruments=[dict(name="Grand Piano", file="Grand.sfz", source=SALAMANDER, layers=8, quality=6)]),
    dict(id="piano-grand-full", name="Grand Piano (full detail)", category="Pianos", version="1",
         description="The same grand with all 16 velocity layers, for the softest and strongest playing.",
         licence="CC-BY 3.0", credit="Salamander Grand Piano V3 by Alexander Holm (via FreePats)",
         instruments=[dict(name="Grand Piano (full)", file="GrandFull.sfz", source=SALAMANDER, layers=None, quality=6)]),
    dict(id="piano-upright", name="Upright Piano", category="Pianos", version="1",
         description="Kawai upright piano, warm and close. Good for hymns and quiet worship.",
         licence="CC0", credit="Upright Piano KW by Gonzalo and Roberto (FreePats)",
         instruments=[dict(name="Upright Piano", file="Upright.sfz", source=UPRIGHT, layers=None, quality=5)]),
    dict(id="bass-electric", name="Electric Bass", category="Bass", version="1",
         description="Yamaha electric bass, played with fingers and with a pick.",
         licence="CC0", credit="Electric bass samples by Andrea Biasior (FreePats)",
         instruments=[dict(name="Finger", file="Finger.sfz", source=BASS_FING, layers=None, quality=5),
                      dict(name="Pick", file="Pick.sfz", source=BASS_PICK, layers=None, quality=5)]),
    dict(id="drums-acoustic", name="Drum Kit", category="Drums", version="1",
         description="Acoustic drum kit (Muldjord): kicks, snare, toms, hi-hat, crashes, rides. General MIDI layout.",
         licence="CC-BY 4.0", credit="MuldjordKit by Lars Muldjord / DrumGizmo team (FreePats)",
         instruments=[dict(name="Drum Kit", file="Kit.sfz", source=DRUMS, layers=None, quality=5)]),
    dict(id="guitar-electric", name="Clean Electric Guitar", category="Guitars", version="1",
         description="A Fender electric guitar through a clean amp. Praise picking, highlife and makossa lines, worship chords.",
         licence="CC0", credit="FSBS Electric Guitar Clean #1 (FreePats)",
         instruments=[dict(name="Clean Electric", file="Clean.sfz", source=GTR_CLEAN, layers=3, quality=4)]),
    dict(id="guitar-nylon", name="Nylon Guitar", category="Guitars", version="1",
         description="Spanish classical guitar with nylon strings. Soft worship ballads and fingerpicking.",
         licence="CC0", credit="Spanish Classical Guitar by Roberto (FreePats)",
         instruments=[dict(name="Nylon Guitar", file="Nylon.sfz", source=GTR_NYLON, layers=None, quality=5)]),
    dict(id="percussion-african", name="African Percussion", category="Percussion", version="1",
         description="Recorded congas, shakers, cabasa, tambourine, agogo bells, cowbell, claves, woodblock, claps and a balafon. "
                     "Djembe and talking drum keep the built-in sound.",
         licence="CC0", credit="Versilian Community Sample Library by Versilian Studios",
         builder="vcsl_percussion", quality=5),
]

AUDIO = (".wav", ".flac", ".ogg", ".aif", ".aiff")

# ---------------------------------------------------------------------------
def log(*a):
    print(*a, flush=True)

def download(url, cache):
    os.makedirs(cache, exist_ok=True)
    target = os.path.join(cache, url.rsplit("/", 1)[-1])
    if not os.path.exists(target):
        log("Downloading", url)
        tmp = target + ".part"
        req = urllib.request.Request(url, headers={"User-Agent": "GospelProductionStudio-packs"})
        with urllib.request.urlopen(req) as r, open(tmp, "wb") as f:
            shutil.copyfileobj(r, f, 1 << 20)
        os.replace(tmp, target)
    return target

def extract(archive, dest):
    if os.path.isdir(archive):
        return archive
    if os.path.isdir(dest) and os.listdir(dest):
        return dest
    os.makedirs(dest, exist_ok=True)
    log("Extracting", os.path.basename(archive))
    if archive.endswith(".7z"):
        subprocess.run(["7z", "x", "-y", "-o" + dest, archive], check=True, stdout=subprocess.DEVNULL)
    elif archive.endswith(".zip"):
        zipfile.ZipFile(archive).extractall(dest)
    else:
        with tarfile.open(archive) as t:
            try:
                t.extractall(dest, filter="data")
            except TypeError:
                t.extractall(dest)
    return dest

def find_sfz(folder):
    found = []
    for root, _, files in os.walk(folder):
        for f in files:
            if f.lower().endswith(".sfz"):
                found.append(os.path.join(root, f))
    if not found:
        raise SystemExit("No .sfz file in " + folder)
    # Prefer the main instrument: not "retuned"/"pedal"/"rel", shortest name
    def score(p):
        n = os.path.basename(p).lower()
        return (any(w in n for w in ("pedal", "noise", "release", "harmonic")), "retuned" in n, len(n))
    found.sort(key=score)
    log("  SFZ files:", ", ".join(os.path.basename(p) for p in found), "-> using", os.path.basename(found[0]))
    return found[0]

# ---------------------------------------------------------------------------
#  SFZ reading (same rules as the app's own reader)
NOTE_BASE = dict(c=0, d=2, e=4, f=5, g=7, a=9, b=11)

def note_number(text):
    t = text.strip().lower()
    if re.fullmatch(r"-?\d+", t):
        return int(t)
    m = re.fullmatch(r"([a-g])([#b]?)(-?\d+)", t)
    if not m:
        return None
    n = NOTE_BASE[m.group(1)] + (1 if m.group(2) == "#" else -1 if m.group(2) == "b" else 0)
    return max(0, min(127, n + (int(m.group(3)) + 1) * 12))

def preprocess(path, defines, depth=0):
    if depth > 8 or not os.path.isfile(path):
        return ""
    text = open(path, encoding="utf-8", errors="replace").read()
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    out = []
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#define"):
            parts = s[7:].strip().split(None, 1)
            if len(parts) == 2 and parts[0].startswith("$"):
                defines[parts[0]] = parts[1].strip()
            continue
        if s.startswith("#include"):
            m = re.search(r'"([^"]+)"', s)
            if m:
                out.append(preprocess(os.path.join(os.path.dirname(path), m.group(1).replace("\\", "/")), defines, depth + 1))
            continue
        for name in sorted(defines, key=len, reverse=True):
            s = s.replace(name, defines[name])
        out.append(s)
    return "\n".join(out)

TOKEN = re.compile(r"<(\w+)>|([A-Za-z0-9_]+)=")

def tokenise(text):
    tokens = []
    for line in text.splitlines():
        matches = list(TOKEN.finditer(line))
        for i, m in enumerate(matches):
            if m.group(1):
                tokens.append(("h", m.group(1).lower(), None))
            else:
                end = matches[i + 1].start() if i + 1 < len(matches) else len(line)
                tokens.append(("o", m.group(2).lower(), line[m.end():end].strip()))
    return tokens

def find_file(base, rel):
    rel = rel.replace("\\", "/").strip()
    direct = os.path.join(base, rel)
    if os.path.isfile(direct):
        return direct
    cur = base
    for part in [p for p in rel.split("/") if p not in ("", ".")]:
        if part == "..":
            cur = os.path.dirname(cur); continue
        nxt = os.path.join(cur, part)
        if not os.path.exists(nxt) and os.path.isdir(cur):
            for c in os.listdir(cur):
                if c.lower() == part.lower():
                    nxt = os.path.join(cur, c); break
        cur = nxt
    return cur

def flatten(sfz_path):
    defines = {}
    tokens = tokenise(preprocess(sfz_path, defines))
    control, glob, master, group, region, ignored = {}, {}, {}, {}, {}, {}
    current, regions, in_region = None, [], False

    def flush():
        nonlocal in_region
        if in_region:
            merged = dict(glob); merged.update(master); merged.update(group); merged.update(region)
            regions.append(merged)
        in_region = False

    for kind, name, value in tokens:
        if kind == "h":
            flush()
            if name == "control": current = control
            elif name == "global": glob.clear(); master.clear(); group.clear(); current = glob
            elif name == "master": master.clear(); group.clear(); current = master
            elif name == "group": group.clear(); current = group
            elif name == "region": region.clear(); current = region; in_region = True
            else: ignored.clear(); current = ignored
        elif current is not None:
            current[name] = value
    flush()

    default_path = control.get("default_path", "")
    offset = int(float(control.get("note_offset", 0))) + 12 * int(float(control.get("octave_offset", 0)))
    active_sw = control.get("sw_default") or next((r["sw_default"] for r in regions if "sw_default" in r), None) \
        or next((r["sw_last"] for r in regions if "sw_last" in r), None)

    base = os.path.dirname(sfz_path)
    out, skipped = [], 0
    for r in regions:
        if "sample" not in r or r["sample"].startswith("*"):
            skipped += 1; continue
        if any(k.startswith(("on_locc", "on_hicc")) for k in r):
            skipped += 1; continue
        if "locc64" in r and float(r["locc64"]) >= 64:
            skipped += 1; continue
        if "sw_last" in r and active_sw and note_number(r["sw_last"]) != note_number(active_sw):
            skipped += 1; continue
        if r.get("trigger", "attack") not in ("attack", "first", "legato", "release", "release_key"):
            skipped += 1; continue
        sample = find_file(base, default_path + r["sample"])
        if not os.path.isfile(sample):
            log("  missing sample:", r["sample"]); skipped += 1; continue

        flat = {}
        for k, v in r.items():
            if k.startswith(("sw_", "on_lo", "on_hi")) or k in ("default_path",):
                continue
            if k in ("key", "lokey", "hikey", "pitch_keycenter"):
                n = note_number(v)
                if n is None:
                    continue
                v = str(max(0, min(127, n + offset)))
            flat[k] = v
        if "key" in flat:
            for k in ("lokey", "hikey", "pitch_keycenter"):
                flat.setdefault(k, flat["key"])
            del flat["key"]
        flat["sample"] = sample
        out.append(flat)
    log(f"  {len(out)} regions ({skipped} not used)")
    return out

def reduce_layers(regions, keep):
    """Keep `keep` evenly spaced velocity layers for each key zone; ranges are widened to cover 1-127."""
    zones = {}
    for r in regions:
        if r.get("trigger", "attack") not in ("attack", "first", "legato"):
            continue
        key = (r.get("lokey", "0"), r.get("hikey", "127"), r.get("seq_position", "1"), r.get("lorand", "0"))
        zones.setdefault(key, []).append(r)
    removed = set()
    for key, rs in zones.items():
        layers = sorted({(int(float(r.get("lovel", 1))), int(float(r.get("hivel", 127)))) for r in rs})
        if len(layers) <= keep:
            continue
        idx = sorted({round(i * (len(layers) - 1) / (keep - 1)) for i in range(keep)})
        kept = [layers[i] for i in idx]
        new_ranges, low = {}, 1
        for lo, hi in kept:
            new_ranges[(lo, hi)] = (low, 127 if (lo, hi) == kept[-1] else hi)
            low = hi + 1
        for r in rs:
            layer = (int(float(r.get("lovel", 1))), int(float(r.get("hivel", 127))))
            if layer in new_ranges:
                r["lovel"], r["hivel"] = map(str, new_ranges[layer])
            else:
                removed.add(id(r))
    out = [r for r in regions if id(r) not in removed]
    log(f"  velocity layers reduced to {keep}: {len(regions)} -> {len(out)} regions")
    return out

# ---------------------------------------------------------------------------
def convert(src, dst, quality):
    if os.path.exists(dst):
        return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", src, "-map_metadata", "-1", "-c:a", "libvorbis",
                    "-q:a", str(quality), dst], check=True)

def safe_name(path, used):
    base = re.sub(r"[^A-Za-z0-9._-]+", "_", os.path.splitext(os.path.basename(path))[0]).strip("_") or "s"
    name, n = base, 1
    while name.lower() in used:
        n += 1; name = f"{base}_{n}"
    used.add(name.lower())
    return name + ".ogg"

def build_instrument(inst, work, pack_dir, cache, overrides, used_names, sample_map):
    src = overrides.get(inst["source"]) or download(inst["source"], cache)
    folder = extract(src, os.path.join(work, re.sub(r"\W+", "_", os.path.basename(inst["source"]))))
    sfz = find_sfz(folder)
    regions = flatten(sfz)
    keys = {}
    for r in regions:
        if r.get("trigger", "attack") in ("attack", "first", "legato"):
            for k in range(int(r.get("lokey", 0)), int(r.get("hikey", 127)) + 1):
                keys.setdefault(k, os.path.basename(r["sample"]))
    if len(keys) <= 64:   # drum kits: show which note plays which sample
        log("  key map: " + ", ".join(f"{k}={v}" for k, v in sorted(keys.items())))
    else:
        log(f"  keys {min(keys)}-{max(keys)}")
    if inst.get("layers"):
        regions = reduce_layers(regions, inst["layers"])

    jobs = []
    for r in regions:
        s = r["sample"]
        if s not in sample_map:
            sample_map[s] = "samples/" + safe_name(s, used_names)
            jobs.append((s, os.path.join(pack_dir, sample_map[s])))
        r["sample"] = sample_map[s]

    log(f"  converting {len(jobs)} samples to Ogg Vorbis (quality {inst['quality']})")
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as ex:
        list(ex.map(lambda j: convert(j[0], j[1], inst["quality"]), jobs))

    order = ["sample", "lokey", "hikey", "pitch_keycenter", "lovel", "hivel"]
    lines = [f"// {inst['name']} - flattened for Gospel Production Studio", "<control> default_path="]
    for r in regions:
        keys = order + sorted(k for k in r if k not in order)
        lines.append("<region> " + " ".join(f"{k}={r[k]}" for k in keys if k in r))
    with open(os.path.join(pack_dir, inst["file"]), "w") as f:
        f.write("\n".join(lines) + "\n")


# ---------------------------------------------------------------------------
#  Step 31: African percussion from VCSL. The library has no SFZ for our note
#  layout, so the recordings are found by name, sorted by loudness into
#  velocity layers, and mapped onto the app's percussion notes.
VCSL_TREE = "https://api.github.com/repos/sgossner/VCSL/git/trees/master?recursive=1"
VCSL_RAW  = "https://raw.githubusercontent.com/sgossner/VCSL/master/"
VCSL_LFS  = "https://media.githubusercontent.com/media/sgossner/VCSL/master/"

# note: (what it is, words in the folder/file path, preferred words in the file name, words that rule a file out)
PERC_TARGETS = [
    (63, "conga open",     ["conga"],      ["open", "tone"],              ["mute", "muted", "slap", "muff", "muffled", "closed", "bass", "heel", "toe", "tip", "quinto", "tumba"]),
    (64, "conga low",      ["tumba"],      ["open", "tone"],              ["mute", "muted", "slap", "muff", "muffled", "closed", "heel", "toe", "tip"]),
    (62, "conga mute",     ["conga"],      ["mute", "muted", "muff", "muffled", "closed", "slap"], ["tumba", "quinto"]),
    (70, "shaker",         ["shaker"],     ["hit", "shake", "accent"],    ["roll", "loop", "trem", "tremolo"]),
    (69, "cabasa",         ["cabasa"],     ["hit", "shake", "scrape"],    ["roll", "loop", "trem", "tremolo"]),
    (54, "tambourine",     ["tambourine"], ["hit", "struck", "shake"],    ["roll", "loop", "trem", "tremolo", "thumb"]),
    (67, "bell high",      ["agogo"],      ["hi", "high"],                ["lo", "low"]),
    (68, "bell low",       ["agogo"],      ["lo", "low"],                 ["hi", "high"]),
    (56, "cowbell",        ["cowbell"],    [],                            ["roll", "trem"]),
    (75, "claves",         ["claves"],     [],                            ["roll", "trem"]),
    (76, "woodblock",      ["woodblock"],  ["hi", "high"],                ["lo", "low", "roll", "trem"]),
    (77, "woodblock low",  ["woodblock"],  ["lo", "low"],                 ["hi", "high", "roll", "trem"]),
    (39, "clap",           ["clap", "claps"], [],                         ["slapstick"]),
]
# Sounds with alternatives: only taken if the file names say which one it is
NEEDS_PREFERRED = {62, 67, 68, 76, 77}
MAX_PER_SOUND = 12

def tokens_of(text):
    return [t for t in re.split(r"[^a-z0-9]+", text.lower()) if t]

def vcsl_files(cache, local):
    """Paths of every WAV in VCSL (from the GitHub file list, or a local copy for testing)."""
    if local:
        out = []
        for root, _, files in os.walk(local):
            for f in files:
                if f.lower().endswith(".wav"):
                    out.append(os.path.relpath(os.path.join(root, f), local).replace(os.sep, "/"))
        return sorted(out)
    os.makedirs(cache, exist_ok=True)
    listing = os.path.join(cache, "vcsl-tree.json")
    if not os.path.exists(listing):
        log("Listing VCSL files on GitHub")
        req = urllib.request.Request(VCSL_TREE, headers={"User-Agent": "GospelProductionStudio-packs", "Accept": "application/vnd.github+json"})
        token = os.environ.get("GITHUB_TOKEN")
        if token:
            req.add_header("Authorization", "Bearer " + token)
        with urllib.request.urlopen(req) as r, open(listing, "wb") as f:
            f.write(r.read())
    tree = json.load(open(listing))
    if tree.get("truncated"):
        log("  warning: the GitHub file list was cut short; some sounds may be missing")
    return sorted(e["path"] for e in tree.get("tree", []) if e.get("type") == "blob" and e["path"].lower().endswith(".wav"))

def vcsl_fetch(path, cache, local):
    if local:
        return os.path.join(local, path)
    target = os.path.join(cache, "vcsl", path)
    if os.path.exists(target):
        return target
    os.makedirs(os.path.dirname(target), exist_ok=True)
    quoted = urllib.parse.quote(path)
    for base in (VCSL_RAW, VCSL_LFS):
        req = urllib.request.Request(base + quoted, headers={"User-Agent": "GospelProductionStudio-packs"})
        with urllib.request.urlopen(req) as r:
            data = r.read()
        if not data.startswith(b"version https://git-lfs"):     # a pointer: the real file is in large file storage
            with open(target + ".part", "wb") as f:
                f.write(data)
            os.replace(target + ".part", target)
            return target
    raise SystemExit("Could not download " + path)

def decode(path, rate=22050):
    """Mono float samples (via ffmpeg)."""
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-ac", "1", "-ar", str(rate), "-f", "f32le", "-"],
                         check=True, stdout=subprocess.PIPE).stdout
    import array
    a = array.array("f"); a.frombytes(raw)
    return a

def peak_db(path):
    a = decode(path)
    peak = max((abs(x) for x in a), default=0.0)
    return 20.0 * __import__("math").log10(max(peak, 1e-6))

NOTE_RE = re.compile(r"(?<![a-z0-9])([a-g])(#|s|sharp|b|flat)?(-?[0-8])(?![0-9])")

def note_from_name(name):
    m = NOTE_RE.search(os.path.splitext(name)[0].lower().replace("_", " "))
    if not m:
        return None
    acc = m.group(2) or ""
    n = NOTE_BASE[m.group(1)] + (1 if acc in ("#", "s", "sharp") else -1 if acc in ("b", "flat") else 0)
    return n + (int(m.group(3)) + 1) * 12

def pitch_note(path):
    """Estimate the note of a pitched sample (autocorrelation over 100-1500 Hz)."""
    import math
    rate = 22050
    a = decode(path, rate)
    start = int(0.03 * rate)
    seg = a[start:start + 4096]
    if len(seg) < 2048:
        return None
    best, lag_best, corr = 0.0, 0, {}
    for lag in range(rate // 1500, rate // 100):
        c = sum(seg[i] * seg[i + lag] for i in range(0, len(seg) - lag, 2))
        corr[lag] = c
        if c > best:
            best, lag_best = c, lag
    for lag in sorted(corr):
        if lag - 1 in corr and lag + 1 in corr and corr[lag] >= 0.9 * best and corr[lag] >= corr[lag - 1] and corr[lag] >= corr[lag + 1]:
            lag_best = lag; break
    if lag_best == 0:
        return None
    return int(round(69 + 12 * math.log2(rate / lag_best / 440.0)))

def layer_regions(files, keyspec, loudest_db=-6.0):
    """files: [(path, peak_db)] for one sound. Up to 3 velocity layers by loudness, random alternation inside a layer."""
    files = sorted(files, key=lambda f: f[1])
    if len(files) > MAX_PER_SOUND:
        step = (len(files) - 1) / (MAX_PER_SOUND - 1)
        files = [files[round(i * step)] for i in range(MAX_PER_SOUND)]
    # Layers only when the recordings really differ in loudness (otherwise they are alternatives)
    spread = files[-1][1] - files[0][1]
    n_layers = 1 if spread < 4.0 else min(3, len(files), 2 if spread < 10.0 else 3)
    edges = [round(i * len(files) / n_layers) for i in range(n_layers + 1)]
    vel = {1: [(1, 127)], 2: [(1, 79), (80, 127)], 3: [(1, 55), (56, 95), (96, 127)]}[n_layers]
    gain = max(-18.0, min(18.0, loudest_db - files[-1][1]))
    regions = []
    for li in range(n_layers):
        layer = files[edges[li]:edges[li + 1]]
        for i, (path, _) in enumerate(layer):
            r = dict(keyspec)
            r.update(sample=path, lovel=str(vel[li][0]), hivel=str(vel[li][1]), volume=f"{gain:.1f}")
            if len(layer) > 1:
                r["lorand"] = f"{i / len(layer):.3f}"
                r["hirand"] = f"{(i + 1) / len(layer):.3f}"
            regions.append(r)
    return regions

def choose_perc(paths, family, preferred, excluded, needs_preferred):
    def name_tokens(p): return tokens_of(os.path.basename(p))
    # The file name decides when it names the instrument (one folder can hold conga, quinto and tumba)
    fam = [p for p in paths if any(w in name_tokens(p) or w in os.path.basename(p).lower() for w in family)]
    if not fam:
        fam = [p for p in paths if any(w in tokens_of(p) or w in p.lower() for w in family)]
    ok = [p for p in fam if not any(w in name_tokens(p) for w in excluded)]
    pref = [p for p in ok if any(w in name_tokens(p) for w in preferred)] if preferred else []
    if pref:
        return pref
    return [] if needs_preferred else ok

def build_vcsl_percussion(pack, work, pack_dir, cache, overrides):
    local = overrides.get(VCSL)
    paths = vcsl_files(cache, local)
    log(f"  VCSL: {len(paths)} WAV files")
    used, sample_map, jobs = set(), {}, []

    def take(path):
        if path not in sample_map:
            src = vcsl_fetch(path, cache, local)
            sample_map[path] = ("samples/" + safe_name(src, used), src)
            jobs.append((src, os.path.join(pack_dir, sample_map[path][0])))
        return sample_map[path]

    # --- percussion kit
    perc_regions, report = [], []
    for note, what, family, preferred, excluded in PERC_TARGETS:
        chosen = choose_perc(paths, family, preferred, excluded, note in NEEDS_PREFERRED)
        if not chosen:
            report.append(f"    {note:3d} {what:14s} -> built-in (no recording found)")
            continue
        files = [(take(p)[1], 0.0) for p in chosen[:40]]
        files = [(src, peak_db(src)) for src, _ in files]
        regions = layer_regions(files, dict(lokey=str(note), hikey=str(note), pitch_keycenter=str(note)))
        for r in regions:
            r["sample"] = next(v[0] for v in sample_map.values() if v[1] == r["sample"])
        perc_regions += regions
        report.append(f"    {note:3d} {what:14s} -> {len(regions)} recordings, e.g. {os.path.basename(chosen[0])}")
    log("  percussion:\n" + "\n".join(report))

    # --- balafon
    bala = [p for p in paths if "balafon" in p.lower()]
    named = [(p, note_from_name(os.path.basename(p))) for p in bala]
    if sum(1 for _, n in named if n is not None) < max(5, len(bala) // 2):
        log("  balafon: no note names in the file names, measuring the pitch of each recording")
        named = [(p, pitch_note(take(p)[1])) for p in bala]
    by_key = {}
    for p, n in named:
        if n is not None and 36 <= n <= 96:
            by_key.setdefault(n, []).append(p)
    bala_regions = []
    keys = sorted(by_key)
    for i, k in enumerate(keys):
        lo = 21 if i == 0 else (keys[i - 1] + k) // 2 + 1
        hi = 108 if i + 1 == len(keys) else (k + keys[i + 1]) // 2
        files = [(take(p)[1], 0.0) for p in by_key[k][:MAX_PER_SOUND]]
        files = [(src, peak_db(src)) for src, _ in files]
        regions = layer_regions(files, dict(lokey=str(lo), hikey=str(hi), pitch_keycenter=str(k)), loudest_db=-8.0)
        for r in regions:
            r["sample"] = next(v[0] for v in sample_map.values() if v[1] == r["sample"])
        bala_regions += regions
    log(f"  balafon: {len(bala)} recordings, {len(keys)} notes" + (f" ({keys[0]}-{keys[-1]})" if keys else " -> built-in"))

    log(f"  converting {len(jobs)} samples to Ogg Vorbis (quality {pack['quality']})")
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as ex:
        list(ex.map(lambda j: convert(j[0], j[1], pack["quality"]), jobs))

    order = ["sample", "lokey", "hikey", "pitch_keycenter", "lovel", "hivel"]
    def write(file, title, regions):
        lines = [f"// {title} - from VCSL (CC0), for Gospel Production Studio", "<control> default_path="]
        for r in regions:
            lines.append("<region> " + " ".join(f"{k}={r[k]}" for k in order + sorted(k for k in r if k not in order) if k in r))
        open(os.path.join(pack_dir, file), "w").write("\n".join(lines) + "\n")
    write("Percussion.sfz", "African Percussion", perc_regions)
    write("Balafon.sfz", "Balafon", bala_regions)
    if not perc_regions:
        raise SystemExit("No percussion recordings were found in VCSL")
    return [dict(name="African Percussion", sfz="Percussion.sfz"), dict(name="Balafon", sfz="Balafon.sfz")]

def build_pack(pack, out, work, cache, overrides):
    log(f"\n== {pack['name']} ({pack['id']})")
    pack_dir = os.path.join(work, "packs", pack["id"])
    shutil.rmtree(pack_dir, ignore_errors=True)
    os.makedirs(pack_dir)
    used, sample_map = set(), {}
    if pack.get("builder") == "vcsl_percussion":
        instruments = build_vcsl_percussion(pack, work, pack_dir, cache, overrides)
    else:
        for inst in pack["instruments"]:
            build_instrument(inst, work, pack_dir, cache, overrides, used, sample_map)
        instruments = [dict(name=i["name"], sfz=i["file"]) for i in pack["instruments"]]

    info = {k: pack[k] for k in ("id", "name", "version", "description", "licence", "credit")}
    info["instruments"] = instruments
    json.dump(info, open(os.path.join(pack_dir, "pack.json"), "w"), indent=1)
    open(os.path.join(pack_dir, "LICENCE.txt"), "w").write(
        f"{pack['name']}\n{pack['credit']}\nLicence: {pack['licence']}\n\n"
        "Converted to Ogg Vorbis and remapped for Gospel Production Studio.\n")

    zip_path = os.path.join(out, pack["id"] + ".zip")
    installed = 0
    with zipfile.ZipFile(zip_path, "w") as z:
        for root, _, files in os.walk(pack_dir):
            for f in sorted(files):
                full = os.path.join(root, f)
                installed += os.path.getsize(full)
                z.write(full, os.path.relpath(full, pack_dir),
                        compress_type=zipfile.ZIP_STORED if f.endswith(".ogg") else zipfile.ZIP_DEFLATED)

    sha = hashlib.sha256(open(zip_path, "rb").read()).hexdigest()
    size = os.path.getsize(zip_path)
    log(f"  -> {os.path.basename(zip_path)}: {size / 1048576:.1f} MB")
    entry = {k: pack[k] for k in ("id", "name", "category", "version", "description", "licence", "credit")}
    entry.update(file=os.path.basename(zip_path), size=size, installedSize=installed, sha256=sha)
    return entry

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="dist")
    ap.add_argument("--work", default="build-packs")
    ap.add_argument("--only", default="")
    ap.add_argument("--source", action="append", default=[], help="URL=local path (testing)")
    a = ap.parse_args()

    overrides = {}
    for s in a.source:
        url, path = s.split("=", 1)
        overrides[url] = path
    os.makedirs(a.out, exist_ok=True)
    cache = os.path.join(a.work, "downloads")

    wanted = [p for p in PACKS if not a.only or p["id"] in a.only.split(",")]
    manifest = {"format": 1, "packs": []}
    for pack in wanted:
        manifest["packs"].append(build_pack(pack, a.out, a.work, cache, overrides))

    json.dump(manifest, open(os.path.join(a.out, "packs.json"), "w"), indent=1)
    log("\nPacks:")
    for p in manifest["packs"]:
        log(f"  {p['name']:28s} {p['size'] / 1048576:7.1f} MB download, {p['installedSize'] / 1048576:7.1f} MB on disk")

if __name__ == "__main__":
    main()
