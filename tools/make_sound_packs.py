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
import argparse, concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys, tarfile, urllib.request, zipfile

SALAMANDER = "https://freepats.zenvoid.org/Piano/SalamanderGrandPiano/SalamanderGrandPianoV3+20161209_44khz16bit.tar.xz"
UPRIGHT    = "https://freepats.zenvoid.org/Piano/UprightPianoKW/UprightPianoKW-SFZ+FLAC-20220221.7z"
BASS_FING  = "https://github.com/freepats/electric-bass-YR/releases/download/2019-09-30/FingerBassYR-SFZ+FLAC-20190930.7z"
BASS_PICK  = "https://github.com/freepats/electric-bass-YR/releases/download/2019-09-30/PickedBassYR-SFZ+FLAC-20190930.7z"
DRUMS      = "https://github.com/freepats/muldjordkit/releases/download/2020-10-18/MuldjordKit-SFZ+FLAC-20201018.7z"

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

def build_pack(pack, out, work, cache, overrides):
    log(f"\n== {pack['name']} ({pack['id']})")
    pack_dir = os.path.join(work, "packs", pack["id"])
    shutil.rmtree(pack_dir, ignore_errors=True)
    os.makedirs(pack_dir)
    used, sample_map = set(), {}
    for inst in pack["instruments"]:
        build_instrument(inst, work, pack_dir, cache, overrides, used, sample_map)

    info = {k: pack[k] for k in ("id", "name", "version", "description", "licence", "credit")}
    info["instruments"] = [dict(name=i["name"], sfz=i["file"]) for i in pack["instruments"]]
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
