#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Ported from maqam-dev (GPL-3.0), adapted for FM1 Open (exact cents_x10, key assignment).
"""Checks tools/gen_scales.py output against a hard-coded copy of the maqam-dev design appendix,
the WEST masks, the key assignment, and the negative fixtures in tests/scales_bad/."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_scales as G  # noqa: E402

# (code, asc, desc or None, tune) -- transcribed from the appendix (TURK: Pythagorean AEU ratios)
EXP = """
T.RST 444511 144511 4:+4 8:-16 10:-2 14:+2 18:+6 20:-4 22:-14
USSAK 114451 - 4:-20 6:-6 10:-2 14:+2 16:-8 20:-4
HUSEY 144451 114451 4:-20 6:-6 10:-2 14:+2 16:-8 18:-18 20:-4
KURDI 114445 - 2:-10 6:-6 10:-2 14:+2 16:-8 20:-4
NIHAV 414451 114451 4:+4 6:-6 10:-2 14:+2 16:-8 20:-4 22:-14
HICAZ 144505 114505 2:+14 8:-16 10:-2 14:+2 16:-8 18:-18 20:-4
HCKAR 414505 - 2:+14 8:-16 10:-2 14:+2 16:+16 22:-14
SUZIN 414511 - 4:+4 8:-16 10:-2 14:+2 16:+16 22:-14
KARCI 141451 - 4:-20 6:-6 10:-2 12:+12 18:-18 20:-4
T.SBA 114151 - 4:-20 6:-6 8:-16 14:+2 16:-8 20:-4
SEGAH 114445 - 2:+14 6:+18 10:+22 14:+2 16:+16 20:+20
HUZZM 114245 - 2:+14 6:+18 9:-19 14:+2 16:+16 20:+20
RAST 244491 144491 4:+4 7:+7 10:-2 14:+2 18:+6 20:+20 21:+9
BAYAT 114449 - 3:+3 6:-6 10:-2 14:+2 16:+16 20:-4
SABA 114149 - 3:+3 6:-6 8:-14 14:+2 16:+16 20:-4
SIKAH 224889 222889 3:-9 7:-5 11:-1 13:+13 14:+2 17:-7 21:-3
HIJAZ 124505 114505 2:+12 8:-14 10:-2 14:+2 16:+16 17:+5 20:-4
NAHAW 414451 114451 4:+4 6:+16 10:-2 14:+2 16:+16 20:+20 22:-12
KURD 114445 - 2:+12 6:-6 10:-2 14:+2 16:+16 20:-4
AJAM 444511 - 4:-20 8:-16 10:-4 14:-22 18:-18 22:-14
NIKRZ 145051 - 4:+4 6:+16 12:+12 14:+2 18:+6 20:+20
HJKAR 414505 - 2:+12 8:-14 10:-2 14:+2 16:+16 22:-12
SUZNK 414491 144491 4:+4 7:+7 10:-2 14:+2 16:+16 18:+6 20:+20 22:-12
HUZAM 228289 222889 3:-9 7:-5 9:+9 11:-1 13:+13 15:-19 17:-7 21:-3
YAMAN 441111 445111 4:+4 8:-14 12:-10 14:+2 18:-16 22:-12
BILAW 444511 - 4:+4 8:-14 10:-2 14:+2 18:-16 22:-12
KHMAJ 444501 144511 4:+4 8:-14 10:-2 14:+2 18:-16 20:-4 22:-12
KAFI 144451 - 4:+4 6:+16 10:-2 14:+2 18:-16 20:-4
ASAVR 014411 114451 4:+4 6:+16 10:-2 14:+2 16:+14 20:-4
BHRVI 114445 - 2:+12 6:+16 10:-2 14:+2 16:+14 20:-4
BHAIR 414505 - 2:+12 8:-14 10:-2 14:+2 16:+14 22:-12
TODI 415045 - 2:+12 6:+16 12:-10 14:+2 16:+14 22:-12
PURVI 415105 - 2:+12 8:-14 12:-10 14:+2 16:+14 22:-12
MARWA 441105 - 2:+12 8:-14 12:-10 18:-16 22:-12
BHUPL 044111 - 4:+4 8:-14 14:+2 18:-16
MALKS 110441 - 6:+16 10:-2 16:+14 20:-4
"""
# seq.c SCALE_MASK, 12-bit (bit i -> 24-bit bit 2i)
WEST12 = [0xFFF, 0xAB5, 0x5AD, 0x6AD, 0x6B5, 0x295, 0x4A9, 0x9AD, 0x5AB, 0xAD5, 0x56B, 0xAAD, 0x4E9, 0x555,
          0x6DB, 0xB6D]
HOMES = {"RAST": "C", "BAYAT": "D", "SABA": "D", "SIKAH": "Ed", "HIJAZ": "D", "NAHAW": "C", "KURD": "D",
         "AJAM": "Bb", "NIKRZ": "C", "HJKAR": "C", "SUZNK": "C", "HUZAM": "Ed", "T.RST": "G", "USSAK": "A",
         "HUSEY": "A", "KURDI": "A", "NIHAV": "G", "HICAZ": "A", "HCKAR": "G", "SUZIN": "G", "KARCI": "A",
         "T.SBA": "A", "SEGAH": "Bd", "HUZZM": "Bd"}


PSYCH_HOMES = {"P.HUS": "A", "P.MSR": "D", "P.NEV": "A", "P.USK": "A", "P.HJZ": "D", "P.HKR": "G", "P.NHK": "G",
               "P.SUB": "G", "P.SU6": "G", "P.SEE": "Gb", "P.BHL": "C", "P.CHG": "C",
               "P.HMY": "C", "P.RST": "C", "P.ACR": "G", "P.MHR": "C"}
JINS_HOMES = {"J.KGL": "B", "J.NRZ": "C", "J.SZK": "C", "J.HJR": "D", "J.DRB": "C"}


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


def main():
    res = G.load(ROOT / "scales")
    by = {r["code"]: r for r in res}
    bad = 0

    def chk(ok, msg):
        nonlocal bad
        if not ok:
            bad += 1
            print("FAIL:", msg)
    chk(len(res) == 73 and [r["code"] for r in res] == G.FROZEN, "73 codes in frozen order")
    chk([r["set"] for r in res] == G.EXPECT_SET, "set of each scale")
    chk([sum(1 for r in res if r["set"] == s) for s in range(6)] == [16, 12, 12, 12, 16, 5], "set sizes 16/12/12/12/16/5")
    chk(all(len(r["code"]) <= 5 for r in res) and len({r["code"] for r in res}) == 73, "codes")
    for r, m12 in zip(res[:16], WEST12):
        chk(r["up"] == r["down"] == spread(m12), f"{r['code']} mask {r['up']:06X} vs {spread(m12):06X}")
        chk(not any(tune_of(r)) and r["home"] == 0, f"{r['code']} tune/home")
    for line in EXP.strip().splitlines():
        f = line.split()
        code, asc, desc = f[0], int(f[1], 16), int(f[2], 16) if f[2] != "-" else int(f[1], 16)
        tune = [0] * 24
        for t in f[3:]:
            s, v = t.split(":")
            tune[int(s)] = int(v)
        r = by[code]
        chk(r["up"] == asc, f"{code} up {r['up']:06X} != {asc:06X}")
        chk(r["down"] == desc, f"{code} down {r['down']:06X} != {desc:06X}")
        chk(tune_of(r) == tune, f"{code} tune {tune_of(r)} != {tune}")
        if code in HOMES:
            chk(r["home"] == G.home_pc(HOMES[code]), f"{code} home")
    for c in [x for x in by if by[x]["set"] == 3]:
        chk(by[c]["home"] == 0, f"{c} home")
    # PSYCH and JINS: equal quarter tones, so every used slot sits exactly on the grid; homes as written
    for c, h in list(PSYCH_HOMES.items()) + list(JINS_HOMES.items()):
        chk(by[c]["set"] == (4 if c.startswith("P.") else 5) and not any(tune_of(by[c])), f"{c} set / tune")
        chk(by[c]["home"] == G.home_pc(h), f"{c} home")
    # exact cents: unused slots are 0, used slots within the grid, ARAB Rast's third is 356.9 c
    chk(by["RAST"]["cents_x10"][7] == 3569, f"RAST slot 7 = {by['RAST']['cents_x10'][7]}")
    chk(abs(by["RAST"]["exact"][7] - 356.9) < 0.05 and by["RAST"]["off"][7] == 7, "RAST slot 7: exact 356.9 c, header +7")
    for r in res:
        chk(r["off"] == tune_of(r) and all(-127 <= o <= 127 for o in r["off"]), f"{r['code']} off[] = rounded exact - grid")
    # TRAD tables from the whole-cent offsets (what the firmware does) equal tables rounded from the exact cents:
    # 73 scales x 24 roots x EQUAL/TRAD. Ties are excluded by the generator (0.001 c margin).
    def wrap(v, m):
        v %= m
        return v - m if v >= m // 2 else v
    pairs = 0
    for r in res:
        for root in range(24):
            k = G.assign_keys(r["up"] | r["down"], root)
            for s, key in k.items():
                eq = wrap(root * 50 + s * 50 - key * 100, 1200)
                chk(eq == G.rnd(wrap_f(root * 50 + s * 50 - key * 100)), f"{r['code']} {root} {s} EQUAL")
                new = wrap(root * 50 + s * 50 + r["off"][s] - key * 100, 1200)
                ex = G.rnd(wrap_f(root * 50 + r["exact"][s] - key * 100))
                chk(new == ex, f"{r['code']} root {root} slot {s} TRAD: offsets {new} != exact {ex}")
                pairs += 1
    chk(pairs > 0, "TRAD/EQUAL identity compared")
    for r in res:
        used = r["up"] | r["down"]
        chk(all((r["cents_x10"][i] != 0) == bool(used >> i & 1 and i) for i in range(24)),
            f"{r['code']} cents_x10 nonzero exactly on used non-tonic slots")
    # key assignment: 1,248 combinations, injective, within neighbours; Rast on C
    n = 0
    for r in res:
        for root in range(24):
            k = G.assign_keys(r["up"] | r["down"], root)
            ok = k is not None and len(set(k.values())) == len(k)
            if ok:
                for s, key in k.items():
                    a = (root + s) % 24
                    ok = ok and (key == a // 2 if a % 2 == 0 else key in ((a - 1) // 2, ((a + 1) // 2) % 12))
            chk(ok, f"{r['code']} root {root}: key assignment")
            n += ok
    chk(n == 73 * 24, f"{n} of {73 * 24} assignments")
    k = G.assign_keys(by["RAST"]["up"] | by["RAST"]["down"], 0)
    chk((k[7], k[21], k[20]) == (4, 11, 10), f"Rast on C: E-half-flat->E, B-half-flat->B, Bb->A#: {k}")
    # negative fixtures: must fail with rc 1 and a message naming the scale / reason
    for sub, who, why in (("removed", "MIN", "missing"), ("collide", "MAJ", "exact pitches"),
                          ("midpoint", "MIDPT", "midpoint"),
                          ("tie", "TIEC", "rounding tie"), ("badcode", "TOOLONG", "longer than 5"),
                          ("fit13", "FIT13", "does not fit 12 keys")):
        d = ROOT / "tests" / "scales_bad" / sub
        p = subprocess.run([sys.executable, str(ROOT / "tools" / "gen_scales.py"), "--print", "--dir", str(d)],
                           capture_output=True, text=True)
        chk(p.returncode == 1 and who in p.stderr and why in p.stderr,
            f"fixture {sub}: rc={p.returncode} stderr={p.stderr!r}")
        print(f"fixture {sub}: {p.stderr.strip()}")
    # spec "Add a scale by editing text": a scale appended to india.txt is an INDIA member at the next free index
    import shutil
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        shutil.copytree(ROOT / "scales", tmp, dirs_exist_ok=True)
        with open(Path(tmp) / "india.txt", "a", encoding="utf-8") as f:
            f.write('\nscale ADDED "AddedScl" home D   # test\n  up   1/1 9/8 5/4 4/3 3/2 5/3 15/8\n')
        p = subprocess.run([sys.executable, str(ROOT / "tools" / "gen_scales.py"), "--print", "--dir", tmp],
                           capture_output=True, text=True)
        rows = p.stdout.splitlines()
        chk(p.returncode == 0 and len(rows) == len(G.FROZEN) + 1 and rows[len(G.FROZEN)].split()[:2] == ["ADDED", "INDIA"],
            f"appended india.txt scale is INDIA at index {len(G.FROZEN)}: rc={p.returncode} {p.stderr!r}")
        out = Path(tmp) / "out" / "scales.h"
        p = subprocess.run([sys.executable, str(ROOT / "tools" / "gen_scales.py"), str(out), "--dir", tmp],
                           capture_output=True, text=True)
        chk(p.returncode == 0 and f'{{"AddedScl", SET_INDIA,' in out.with_suffix(".c").read_text(),
            "generated row of the appended scale carries SET_INDIA")
    print("scales_check:", "FAIL" if bad else "ok", f"({bad} problems)" if bad else "")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
