#!/usr/bin/env python3
"""
Real-drummer feel for Gospel Production Studio (plan Step 32).

Reads the Groove MIDI Dataset (Magenta, Google LLC, CC BY 4.0): professional
drummers playing to a click on an electronic kit. From it this script writes
Source/RealFeel.cpp with:

  * feel templates, one per style: for each kind of hit (kick, snare, hi-hat,
    cymbals, toms) and each 16th of a 4/4 bar, how early or late the drummers
    played it on average, how much it varied, and how hard they played it;
  * short 2-bar grooves played by the drummers themselves.

The GitHub build runs this before compiling:
    python tools/make_feel.py --out Source/RealFeel.cpp
It downloads the 3 MB MIDI-only zip and checks it. If anything fails it
writes the empty version, so the app still builds (the feel menu then says
the real-drummer feels are not in this build).

    python tools/make_feel.py --empty --out Source/RealFeel.cpp      (empty version)
    python tools/make_feel.py --gmd folder-or-zip --out ...         (no download)
"""
import argparse, csv, hashlib, io, math, os, statistics, struct, sys, urllib.request, zipfile

GMD_URL = "https://storage.googleapis.com/magentadata/datasets/groove/groove-v1.0.0-midionly.zip"
GMD_SHA256 = "651cbc524ffb891be1a3e46d89dc82a1cecb09a57c748c7b45b844c4841dcc1e"
CREDIT = "Groove MIDI Dataset by Google LLC (Magenta), CC BY 4.0"

# Styles shown in the app, in menu order: dataset label -> name
STYLES = [("gospel", "Gospel"), ("soul", "Soul"), ("funk", "Funk"), ("highlife", "Highlife"),
          ("afrobeat", "Afrobeat"), ("afrocuban", "Afro-Cuban"), ("reggae", "Reggae"), ("latin", "Latin"),
          ("pop", "Pop"), ("rock", "Rock"), ("neworleans", "New Orleans"), ("hiphop", "Hip-hop"), ("jazz", "Jazz")]
GROOVES_PER_STYLE = 3
GROOVE_STYLES = {"gospel", "soul", "funk", "highlife", "afrobeat", "afrocuban", "reggae", "latin", "pop", "neworleans"}

# Roland TD-11 notes in the dataset -> the app's General MIDI drum notes
TO_GM = {36: 36, 38: 38, 40: 38, 37: 37, 48: 50, 50: 50, 45: 47, 47: 47, 43: 43, 58: 43,
         46: 46, 26: 46, 42: 42, 22: 42, 44: 44, 49: 49, 55: 49, 57: 49, 52: 49, 51: 51, 59: 51, 53: 53}
# Kind of hit: 0 kick, 1 snare, 2 hi-hat, 3 ride and cymbals, 4 toms
CLASS = {36: 0, 38: 1, 37: 1, 42: 2, 44: 2, 46: 2, 49: 3, 51: 3, 53: 3, 43: 4, 47: 4, 50: 4}
NUM_CLASSES = 5
MIN_COUNT = 8


def log(*a):
    print(*a, flush=True)


# ---------------------------------------------------------------------------
#  A small MIDI reader: note-ons with their time in beats
def read_midi(data):
    if data[:4] != b"MThd":
        raise ValueError("not a MIDI file")
    _, ntracks, division = struct.unpack(">HHH", data[8:14])
    if division & 0x8000:
        raise ValueError("SMPTE time not supported")
    pos, notes = 14, []
    for _ in range(ntracks):
        if data[pos:pos + 4] != b"MTrk":
            break
        length = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        p, end, tick, status = pos + 8, pos + 8 + length, 0, 0
        while p < end:
            delta = 0
            while True:
                b = data[p]; p += 1
                delta = (delta << 7) | (b & 0x7F)
                if b < 0x80:
                    break
            tick += delta
            b = data[p]
            if b == 0xFF:
                ln = 0; p += 2
                while True:
                    c = data[p]; p += 1
                    ln = (ln << 7) | (c & 0x7F)
                    if c < 0x80:
                        break
                p += ln
                continue
            if b in (0xF0, 0xF7):
                ln = 0; p += 1
                while True:
                    c = data[p]; p += 1
                    ln = (ln << 7) | (c & 0x7F)
                    if c < 0x80:
                        break
                p += ln
                continue
            if b & 0x80:
                status = b; p += 1
            kind = status & 0xF0
            if kind in (0xC0, 0xD0):
                p += 1
                continue
            d1, d2 = data[p], data[p + 1]; p += 2
            if kind == 0x90 and d2 > 0:
                notes.append((tick / division, d1, d2))
        pos = end
    notes.sort()
    return notes


def bar_phase(hits):
    """Where beat 1 is (0-3 beats from the start): kick on 1, snare on 2 and 4."""
    best, phase = -1.0, 0
    for p in range(4):
        score = 0.0
        for beat, note, vel in hits:
            b = (beat - p) % 4.0
            near = min(abs(b - round(b)), 1.0)
            if near > 0.08:
                continue
            q = int(round(b)) % 4
            if note == 36 and q == 0: score += 1.0
            if note == 36 and q == 2: score += 0.4
            if note in (38, 37) and q in (1, 3): score += 1.0
        if score > best:
            best, phase = score, p
    return phase


# ---------------------------------------------------------------------------
def load_dataset(source):
    if os.path.isdir(source):
        def read(name):
            return open(os.path.join(source, name), "rb").read()
        names = None
    else:
        z = zipfile.ZipFile(source)
        names = z.namelist()
        prefix = next((n[:-len("info.csv")] for n in names if n.endswith("info.csv")), "")
        def read(name):
            return z.read(prefix + name)
    rows = list(csv.DictReader(io.StringIO(read("info.csv").decode("utf-8"))))
    return rows, read


def build(source):
    rows, read = load_dataset(source)
    log(f"{len(rows)} performances in the dataset")
    keys = {k for k, _ in STYLES}
    cells = {k: [[[] for _ in range(16)] for _ in range(NUM_CLASSES)] for k in keys}
    vels = {k: [[] for _ in range(NUM_CLASSES)] for k in keys}
    perfs = {k: 0 for k in keys}
    candidates = {k: [] for k in keys}

    for row in rows:
        style = row.get("style", "").split("/")[0].strip().lower()
        if style not in keys or row.get("beat_type") != "beat" or row.get("time_signature") != "4-4":
            continue
        try:
            notes = read_midi(read(row["midi_filename"]))
        except Exception as e:
            log("  skipped", row.get("midi_filename"), e)
            continue
        hits = [(b, TO_GM[n], v) for b, n, v in notes if n in TO_GM]
        if len(hits) < 32:
            continue
        phase = bar_phase(hits)
        perfs[style] += 1
        for beat, note, vel in hits:
            c = CLASS.get(note)
            if c is None:
                continue
            pos16 = ((beat - phase) % 4.0) * 4.0
            step = int(round(pos16))
            dev = pos16 - step
            if abs(dev) > 0.45:
                continue
            cells[style][c][step % 16].append((dev, vel))
            vels[style][c].append(vel)
        try:
            bpm = float(row.get("bpm", 0))
        except ValueError:
            bpm = 0
        if 60 <= bpm <= 150 and float(row.get("duration", 0) or 0) > 15:
            candidates[style].append((row["drummer"], row["id"], int(bpm), hits, phase))

    styles = []
    for key, name in STYLES:
        if perfs[key] == 0:
            log(f"  {name}: no performances, left out")
            continue
        out_cells, class_vel = [], []
        for c in range(NUM_CLASSES):
            allv = vels[key][c]
            cv = statistics.mean(allv) if allv else 90.0
            class_vel.append(cv)
            # Thin cells borrow from the same place in the other beats (e.g. every "e" of the beat)
            for s in range(16):
                pts = cells[key][c][s]
                if len(pts) < MIN_COUNT:
                    pts = [p for t in range(s % 4, 16, 4) for p in cells[key][c][t]]
                if len(pts) < MIN_COUNT:
                    out_cells.append((len(cells[key][c][s]), 0.0, 0.0, cv))
                    continue
                devs = [p[0] for p in pts]
                out_cells.append((len(cells[key][c][s]), statistics.mean(devs),
                                  statistics.pstdev(devs), statistics.mean(p[1] for p in pts)))
        styles.append((name, key, perfs[key], out_cells, class_vel))
        snare = [cl for i, cl in enumerate(out_cells) if i // 16 == 1 and i % 16 in (4, 12)]
        log(f"  {name}: {perfs[key]} performances; snare on 2 and 4 "
            + ", ".join(f"{cl[1] * 1000 / 4 / 2:+.0f} ms at 120 BPM" for cl in snare))

    grooves = []
    for key, name in STYLES:
        if key not in GROOVE_STYLES:
            continue
        picked, drummers = [], set()
        for cand in sorted(candidates[key], key=lambda c: (c[0] in drummers, c[1])):
            if len(picked) >= GROOVES_PER_STYLE:
                break
            drummer, pid, bpm, hits, phase = cand
            g = two_bars(hits, phase)
            if g:
                picked.append((bpm, g))
                drummers.add(drummer)
        for i, (bpm, g) in enumerate(picked):
            grooves.append((f"{name} {i + 1} ({bpm} BPM)", name, bpm, g))
    log(f"{len(grooves)} real-drummer grooves")
    return styles, grooves


def two_bars(hits, phase):
    """The most typical 2 bars (no fill): the one closest to the median number of hits."""
    first = phase
    last = max(b for b, _, _ in hits)
    windows = []
    start = first + 4.0          # skip the first bar
    while start + 8.0 <= last - 4.0 and len(windows) < 16:
        inside = [(b - start, n, v) for b, n, v in hits if start - 0.06 <= b < start + 8.0 - 0.06]
        windows.append(inside)
        start += 8.0
    windows = [w for w in windows if len(w) >= 16]
    if not windows:
        return None
    med = statistics.median(len(w) for w in windows)
    best = min(windows, key=lambda w: (abs(len(w) - med), sum(1 for h in w if CLASS.get(h[1]) == 4)))
    return [(max(0.0, round(b, 4)), n, v) for b, n, v in sorted(best)]


# ---------------------------------------------------------------------------
def write_cpp(path, styles, grooves, real):
    L = []
    L.append("// GENERATED by tools/make_feel.py - do not edit.")
    if real:
        L.append(f"// Data: {CREDIT}.")
    else:
        L.append("// Empty version: the GitHub build fills this from the Groove MIDI Dataset.")
    L.append('#include "RealFeel.h"\n')
    L.append("namespace RealFeel\n{")
    L.append(f"    bool fromRealDrummers()  {{ return {'true' if real else 'false'}; }}")
    L.append(f'    const char* credit()     {{ return "{CREDIT if real else ""}"; }}\n')
    L.append("    const std::vector<Style>& styles()\n    {")
    L.append("        static const std::vector<Style> list {")
    for name, key, n, cells, cv in styles:
        cs = ", ".join(f"{{ {c[0]}, {c[1]:.4f}f, {c[2]:.4f}f, {c[3]:.1f}f }}" for c in cells)
        vs = ", ".join(f"{v:.1f}f" for v in cv)
        L.append(f'            {{ "{name}", "{key}", {n}, {{{{ {cs} }}}}, {{{{ {vs} }}}} }},')
    L.append("        };\n        return list;\n    }\n")
    L.append("    const std::vector<Groove>& grooves()\n    {")
    L.append("        static const std::vector<Groove> list {")
    for name, style, bpm, hits in grooves:
        hs = ", ".join(f"{{ {b:.4f}, {n}, {v} }}" for b, n, v in hits)
        L.append(f'            {{ "{name}", "{style}", {bpm}, 8.0, {{ {hs} }} }},')
    L.append("        };\n        return list;\n    }\n}")
    data = "\n".join(L) + "\n"
    tmp = path + ".part"
    open(tmp, "w").write(data)
    os.replace(tmp, path)
    log(f"Wrote {path}: {len(styles)} feels, {len(grooves)} grooves")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "Source", "RealFeel.cpp"))
    ap.add_argument("--gmd", help="the dataset zip or folder (default: download it)")
    ap.add_argument("--empty", action="store_true")
    a = ap.parse_args()
    if a.empty:
        write_cpp(a.out, [], [], False)
        return
    try:
        source = a.gmd
        if not source:
            source = os.path.join(os.environ.get("RUNNER_TEMP", "/tmp"), "groove-midionly.zip")
            if not os.path.exists(source):
                log("Downloading", GMD_URL)
                with urllib.request.urlopen(GMD_URL) as r:
                    data = r.read()
                if hashlib.sha256(data).hexdigest() != GMD_SHA256:
                    raise RuntimeError("the download does not match the published checksum")
                open(source, "wb").write(data)
        styles, grooves = build(source)
        if not styles:
            raise RuntimeError("no styles found")
        write_cpp(a.out, styles, grooves, True)
    except Exception as e:
        log("Real-drummer feel not available:", e)
        write_cpp(a.out, [], [], False)


if __name__ == "__main__":
    main()
