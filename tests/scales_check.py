#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Ported from maqam-dev (GPL-3.0), adapted for FM1 Open (exact cents_x10, key assignment).
"""Checks tools/gen_scales.py output for the two-set library (WEST and ARAB, vendored from Sinfon):
the WEST masks, the ARAB maqamat against Sinfon's scale-system.md table (steps, home, traditional cents),
exact intonation, the key assignment at every root, the legacy map, and error fixtures built on the fly."""
import hashlib
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_scales as G  # noqa: E402

# WEST: 12-TET masks over the 12 semitones (bit i = i semitones above the tonic), in file order.
WEST = ["CHR", "MAJ", "MIN", "DOR", "MIX", "PEN", "MPEN", "HARM", "PHRY", "LYD", "LOC", "MEL", "BLUES", "WHOLE",
        "DIMHW", "DIMWH", "PHDOM"]
WEST12 = [0xFFF, 0xAB5, 0x5AD, 0x6AD, 0x6B5, 0x295, 0x4A9, 0x9AD, 0x5AB, 0xAD5, 0x56B, 0xAAD, 0x4E9, 0x555,
          0x6DB, 0xB6D, 0x5B3]
# ARAB, from Sinfon's openspec/assets/scale-system.md: code, home, ascending steps in quarter tones,
# traditional cents of the ascending degrees (none where the table gives none).
ARAB = """
RAST  C  4 3 3 4 4 3 3  0 204 357 498 702 906 1059
BAYAT D  3 3 4 4 2 4 4  0 153 294 498 702 816 996
SABA  D  3 3 2 6 2 4 4  0 153 294 386 702 816 996
SIKAH Ed 3 4 4 3 3 4 3  0 141 345 549 702 843 1047
HIJAZ D  2 6 2 4 3 3 4  0 112 386 498 702 855 996
NAHAW C  4 2 4 4 2 6 2  0 204 316 498 702 816 1088
KURD  D  2 4 4 4 2 4 4  0 112 294 498 702 816 996
AJAM  Bb 4 4 2 4 4 4 2
NIKRZ C  4 2 6 2 4 2 4
HJKAR C  2 6 2 4 2 6 2  0 112 386 498 702 816 1088
SUZNK C  4 3 3 4 2 6 2  0 204 357 498 702 816 1088
HUZAM Ed 3 4 2 6 2 4 3  0 141 345 459 731 843 1047
NAIRZ C  4 3 3 4 3 3 4
MAHUR C  4 3 3 4 4 4 2  0 204 357 498 702 906 1088
HUSNI D  3 3 4 4 3 3 4  0 153 294 498 702 855 996
BSHRI D  3 3 4 2 6 2 4  0 153 294 498 612 884 996
IRAQ  Bd 3 4 3 3 4 4 3
BSTNK Bd 3 4 3 3 2 6 3
JIHRK F  4 4 2 4 4 3 3
"""
# Turkish and Indian scales of the six-set library and where scales/legacy.txt sends them (scale-system.md 8-9).
LEGACY = {"T.RST": "MAHUR", "USSAK": "BAYAT", "HUSEY": "HUSNI", "KURDI": "KURD", "NIHAV": "NAHAW", "HICAZ": "PHDOM",
          "HCKAR": "HJKAR", "SUZIN": "SUZNK", "KARCI": "BSHRI", "T.SBA": "SABA", "SEGAH": "SIKAH", "HUZZM": "HUZAM",
          "YAMAN": "LYD", "BILAW": "MAJ", "KHMAJ": "MIX", "KAFI": "DOR", "ASAVR": "MIN", "BHRVI": "PHRY",
          "BHAIR": "HJKAR", "TODI": "-", "PURVI": "-", "MARWA": "-", "MALKS": "-", "BHUPL": "PEN"}


def spread(m):
    return sum(1 << (2 * i) for i in range(12) if m >> i & 1)


def tune_of(r):
    """Whole-cent grid offsets, recomputed here from the exact cents (and compared with the header's off[])."""
    return [G.rnd(c - 50.0 * i) if (c or i == 0) and (r["up"] | r["down"]) >> i & 1 else 0
            for i, c in enumerate(r["exact"])]


def wrap_f(v):
    """Signed remainder of a (float) cents value in [-600, 600)."""
    v %= 1200.0
    return v - 1200.0 if v >= 600.0 else v


def gen(d, *args):
    return subprocess.run([sys.executable, str(ROOT / "tools" / "gen_scales.py"), *args, "--dir", str(d)],
                          capture_output=True, text=True)


def edit(f, old, new):
    def apply(d):
        p = d / f
        t = p.read_text(encoding="utf-8")
        assert old in t, (f, old)
        p.write_text(t.replace(old, new, 1), encoding="utf-8")
    return apply


def append(text):
    def apply(d):
        with (d / "arab.txt").open("a", encoding="utf-8") as fh:
            fh.write(text)
    return apply


FIXTURES = (
    ("removed", "MIN", "not in the library", edit("west.txt", "scale MIN ", "scale MINX ")),
    ("collide", "MAJ", "exact pitches", edit("west.txt", '0. 200. 400. 500. 700. 900. 1100.\n  alias "Bilawal"',
                                             '0. 200. 400. 500. 700. 900. 1100. 410.\n  alias "Bilawal"')),
    ("first", "CHR", "first scale", edit("west.txt", "scale CHR ", "scale CHRX ")),
    ("midpoint", "MIDPT", "midpoint", append('\nscale MIDPT "Midpoint" home C\n  up   0. 200. 325. 500. 700. 900. 1100.\n')),
    ("tie", "TIEC", "rounding tie", append('\nscale TIEC "TieCents" home C\n  up   0. 200. 303.5 500. 700. 900. 1100.\n')),
    ("badcode", "TOOLONG", "longer than 5", append('\nscale TOOLONG "BadCode" home C\n  up   0. 200. 400. 500. 700. 900. 1100.\n')),
    ("longname", "LONGN", "longer than 8", append('\nscale LONGN "Far Too Long" home C\n  up   0. 200. 400. 500. 700. 900. 1100.\n')),
    ("fit13", "FIT13", "does not fit 12 keys", append('\nscale FIT13 "TooMany" home C\n  up   0. 200. 400. 500. 700. 900. 1100.\n'
                                                      '  down 0. 100. 300. 600. 800. 1000. 1150.\n')),
    ("alias", "BAYAT", "quoted name", edit("arab.txt", 'alias "Uşşak"', "alias Ussak")),
)


def main():
    res = G.load(ROOT / "scales")
    legacy = G.load_legacy(ROOT / "scales", res)
    by = {r["code"]: r for r in res}
    bad = 0

    def chk(ok, msg):
        nonlocal bad
        if not ok:
            bad += 1
            print("FAIL:", msg)
    arab = [ln.split() for ln in ARAB.strip().splitlines()]
    chk([r["code"] for r in res] == WEST + [a[0] for a in arab], "36 codes: WEST then ARAB, in Sinfon's file order")
    chk([r["set"] for r in res] == [0] * 17 + [1] * 19, "set of each scale")
    chk(all(len(r["code"]) <= 5 for r in res) and len(by) == 36, "codes")
    chk(all(len(r["name"]) <= 8 and r["name"].isascii() for r in res), "display names fit 8 ASCII characters")
    chk((by["PHDOM"]["name"], by["BSHRI"]["name"], by["BSTNK"]["name"]) == ("Hicaz", "BaySuri", "Bastnkar"),
        "shortened display names")
    for code, m12 in zip(WEST, WEST12):
        r = by[code]
        chk(r["up"] == r["down"] == spread(m12), f"{code} mask {r['up']:06X} vs {spread(m12):06X}")
        chk(not any(tune_of(r)) and r["home"] == 0, f"{code} tune/home")
    for a in arab:
        code, home, steps = a[0], a[1], [int(x) for x in a[2:9]]
        r = by[code]
        chk(sum(steps) == 24, f"{code} table steps sum to an octave")
        slots = [sum(steps[:i]) for i in range(7)]
        chk(r["up"] == sum(1 << s for s in slots), f"{code} up {r['up']:06X} != slots {slots}")
        chk(r["home"] == G.home_pc(home), f"{code} home {r['home']} != {home}")
        if len(a) > 9:
            want = [int(x) for x in a[9:]]
            got = [G.rnd(r["exact"][s]) for s in slots]
            chk(got == want, f"{code} traditional cents {got} != scale-system.md {want}")
    # exact cents: ARAB Rast's third is 356.9 c, stored as +7 from its slot
    chk(by["RAST"]["cents_x10"][7] == 3569, f"RAST slot 7 = {by['RAST']['cents_x10'][7]}")
    chk(abs(by["RAST"]["exact"][7] - 356.9) < 0.05 and by["RAST"]["off"][7] == 7, "RAST slot 7: exact 356.9 c, header +7")
    for r in res:
        chk(r["off"] == tune_of(r) and all(-127 <= o <= 127 for o in r["off"]), f"{r['code']} off[] = rounded exact - grid")
        used = r["up"] | r["down"]
        chk(all((r["cents_x10"][i] != 0) == bool(used >> i & 1 and i) for i in range(24)),
            f"{r['code']} cents_x10 nonzero exactly on used non-tonic slots")

    # Key assignment at every root (injective, neighbouring keys), and TRAD/EQUAL tables from the whole-cent offsets
    # (what the firmware does) equal tables rounded from the exact cents. Ties are excluded by the generator.
    def wrap(v, m):
        v %= m
        return v - m if v >= m // 2 else v
    n = 0
    for r in res:
        for root in range(24):
            k = G.assign_keys(r["up"] | r["down"], root)
            ok = k is not None and len(set(k.values())) == len(k)
            for s, key in (k or {}).items():
                a = (root + s) % 24
                ok = ok and (key == a // 2 if a % 2 == 0 else key in ((a - 1) // 2, ((a + 1) // 2) % 12))
                chk(wrap(root * 50 + s * 50 - key * 100, 1200) == G.rnd(wrap_f(root * 50 + s * 50 - key * 100)),
                    f"{r['code']} {root} {s} EQUAL")
                chk(wrap(root * 50 + s * 50 + r["off"][s] - key * 100, 1200) ==
                    G.rnd(wrap_f(root * 50 + r["exact"][s] - key * 100)), f"{r['code']} root {root} slot {s} TRAD")
            chk(ok, f"{r['code']} root {root}: key assignment")
            n += ok
    chk(n == len(res) * 24, f"{n} of {len(res) * 24} assignments")
    k = G.assign_keys(by["RAST"]["up"] | by["RAST"]["down"], 0)
    chk((k[7], k[21], k[20]) == (4, 11, 10), f"Rast on C: E-half-flat->E, B-half-flat->B, Bb->A#: {k}")
    # legacy map: the six-set codes in old index order; WEST and ARAB keep their code, the rest per sections 8-9
    olds = [ln.split()[1] for ln in (ROOT / "scales" / "legacy.txt").read_text(encoding="utf-8").splitlines()
            if ln.split("#", 1)[0].split()]
    chk(len(olds) == len(legacy) == 73, "73 legacy entries")
    for i, old in enumerate(olds):
        want = LEGACY.get(old, old if old in by else "-")
        got = "-" if legacy[i] == 255 else res[legacy[i]]["code"]
        chk(got == want, f"legacy {i} {old} -> {got}, expected {want}")
    # error fixtures: copy the real files, apply one edit, expect rc 1 and a message naming the scale / reason
    for name, who, why, apply in FIXTURES:
        with tempfile.TemporaryDirectory() as tmp:
            shutil.copytree(ROOT / "scales", tmp, dirs_exist_ok=True)
            apply(Path(tmp))
            p = gen(tmp, "--print")
            chk(p.returncode == 1 and who in p.stderr and why in p.stderr, f"fixture {name}: rc={p.returncode} stderr={p.stderr!r}")
            print(f"fixture {name}: {p.stderr.strip()}")
    # spec "Add a scale by editing text": a scale appended to arab.txt is an ARAB member at the next index
    with tempfile.TemporaryDirectory() as tmp:
        shutil.copytree(ROOT / "scales", tmp, dirs_exist_ok=True)
        append('\nscale ADDED "AddedScl" home D   # test\n  up   1/1 9/8 5/4 4/3 3/2 5/3 15/8\n')(Path(tmp))
        p = gen(tmp, "--print")
        rows = p.stdout.splitlines()
        chk(p.returncode == 0 and len(rows) == 37 and rows[36].split()[:2] == ["ADDED", "ARAB"],
            f"appended arab.txt scale is ARAB at index 36: rc={p.returncode} {p.stderr!r}")
        out = Path(tmp) / "out" / "scales.h"
        p = gen(tmp, str(out))
        chk(p.returncode == 0 and '{"AddedScl", SET_ARAB,' in out.with_suffix(".c").read_text(),
            "generated row of the appended scale carries SET_ARAB")
    # the vendored files are Sinfon's, unedited: their hashes match scales/UPSTREAM
    up = (ROOT / "scales" / "UPSTREAM").read_text(encoding="utf-8")
    for f in ("west.txt", "arab.txt", "SOURCE.md"):
        m = re.search(rf"^sha256 {re.escape(f)} ([0-9a-f]{{64}})$", up, re.M)
        h = hashlib.sha256((ROOT / "scales" / f).read_bytes()).hexdigest()
        chk(m is not None and m.group(1) == h,
            f"scales/{f} differs from Sinfon (scales/UPSTREAM): change it in Sinfon and run tools/sync_scales.sh")
    print("scales_check:", "FAIL" if bad else "ok", f"({bad} problems)" if bad else "")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
