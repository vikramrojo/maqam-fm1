#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Ported from maqam-dev (GPL-3.0) and adapted for FM1 Open: whole-cent grid offsets, per-root key assignment.
"""Scale table generator: scales/*.txt -> app/maqam/scales.h (scale_def_t SCALES[]).

usage: gen_scales.py OUT.h            write the header and OUT.c (the data, one copy)
       gen_scales.py --print          print a human table (code set up down exact cents per slot)
       gen_scales.py --assign CODE [ROOT]   print the key assignment of a scale (all roots if no ROOT)
       ... --dir DIR                  read the scale files from DIR (default: scales/ next to tools/)

Scale file format (one file per set: west.txt arab.txt, vendored from Sinfon; '#' starts a comment):

    scale CODE "Name" home <note>
      from <value>      (optional) the maqam's own tonic; every value is divided by it and
                        octave-reduced into [0, 1200) cents. Lets ARAB rows stay "from rast".
      up   <values>     ascending form; the first value is the tonic
      down <values>     descending form (optional; defaults to up)
      alias "Name"      (any number) another name for the same notes; Sinfon-only, ignored here

A value is Scala syntax: cents if it contains '.', else a ratio 9/8 (an integer N alone = N/1).

home <note>: a letter A..G plus at most one suffix: b flat, # sharp, d half-flat (demi-flat),
+ half-sharp.  C, Bb, F#, Ed (E half-flat), C+ (C half-sharp).  Pitch class 0..23 (C = 0, one
step per quarter tone), a display hint only.

Each value is assigned to the nearest 50 c grid slot (mod 24), rounding half away from zero; a
value within 1e-6 c of a grid midpoint (25 c mod 50) is an error, so no result depends on
tie-breaking. The generator computes the exact pitch above the tonic of every used slot (--print
shows it to 0.1 c). The header stores, per scale, off[12]: for each used slot (up|down), in
ascending slot order, that exact pitch minus the slot's grid pitch (slot * 50 c), rounded half away
from zero to whole cents (int8); the entry of a slot is off[rank], rank = the number of used slots
below it. Both forms of a scale share one off[]: two degrees on one slot must agree to within
0.05 c, else it is an error. An exact remainder within 0.001 c of a .5 tie is an error naming the scale, so
rounding the offset first and rounding the sum later can never differ. up/down are stored as
three little-endian bytes each.

KEY ASSIGNMENT (the C port in app/maqam/ MUST implement exactly this; the generator proves
that every scale fits at every root, but does not emit per-root tables):

  Input: scale, root r in 0..23 (quarter tones above C).
  U = up | down (the union of both forms' 24-bit slot masks); slots are visited in ASCENDING
  slot order (scale-degree order: slot 0, the tonic, first).
  For slot s: a = (r + s) % 24.
    even a: the only candidate is key a/2.
    odd a:  candidates, in order of preference: UPPER key ((a+1)/2) % 12, then LOWER key (a-1)/2.
  Result: the lexicographically first assignment, by that visiting order and candidate
  preference, in which no two slots share a key (depth-first search with backtracking):

      assign(i, used):                    # i indexes the ascending list of slots in U
          if i == n: return True
          for k in candidates(slots[i]):  # upper first for odd a
              if not used & (1 << k):
                  key[i] = k
                  if assign(i + 1, used | 1 << k): return True
          return False

  Key numbers are pitch classes 0..11 from C (C=0, C#=1, ... B=11). If assign(0, 0) fails the
  build fails, naming the scale and root. Example, ARAB RAST on C: the third (slot 7, E-half-flat)
  -> key 4 (E); slot 21 (B-half-flat) -> key 11 (B); slot 20 (B-flat) -> key 10 (A#).
"""
import math
import re
import sys
from fractions import Fraction
from pathlib import Path

SETS = ["WEST", "ARAB"]
FILES = {"WEST": "west.txt", "ARAB": "arab.txt"}

# Indices follow the files (west, then arab), as in Sinfon. CHR must stay index 0: a new project's scale,
# all-zero tables. Projects saved with the six-set library's indices are mapped by scales/legacy.txt.
FIRST = "CHR"
LEGACY_COUNT = 73
# The FM1 shows at most 8 ASCII characters; Sinfon's longer names are shortened for the display only.
DISPLAY = {"PHDOM": "Hicaz", "BSHRI": "BaySuri", "BSTNK": "Bastnkar"}

LETTER = {"C": 0, "D": 4, "E": 8, "F": 10, "G": 14, "A": 18, "B": 22}
SUFFIX = {"": 0, "b": -2, "#": 2, "d": -1, "+": 1}


class Err(Exception):
    pass


def rnd(x):      # round half away from zero
    return int(math.floor(x + 0.5)) if x >= 0 else -int(math.floor(-x + 0.5))


def cents(tok):
    if "." in tok:
        return float(tok)
    f = Fraction(tok)
    if f <= 0:
        raise ValueError("ratio must be positive")
    return 1200.0 * math.log2(f.numerator / f.denominator)


def home_pc(s):
    m = re.fullmatch(r"([A-G])([b#d+]?)", s)
    if not m:
        raise ValueError(f"bad home note '{s}' (letter A..G + optional b # d +)")
    return (LETTER[m.group(1)] + SUFFIX[m.group(2)]) % 24


def parse_file(path, setname):
    scales = []
    cur = None
    for ln, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        where = f"{path.name}:{ln}"
        m = re.fullmatch(r'scale\s+(\S+)\s+"([^"]*)"\s+home\s+(\S+)', line)
        if m:
            cur = dict(code=m.group(1), name=DISPLAY.get(m.group(1), m.group(2)), home=m.group(3), set=setname,
                       src=where, **{"from": None, "up": None, "down": None})
            scales.append(cur)
            continue
        if cur is None:
            raise Err(f"{where}: line outside a scale: {line}")
        key, _, rest = line.partition(" ")
        if key == "alias":
            if not re.fullmatch(r'"[^"]+"', rest.strip()):
                raise Err(f"{where}: scale {cur['code']}: alias needs a quoted name")
            continue
        if key not in ("from", "up", "down") or cur[key] is not None:
            raise Err(f"{where}: scale {cur['code']}: unexpected or repeated '{key}'")
        cur[key] = rest.split()
    return scales


def build(sc):
    """-> (up mask, down mask, exact cents[24] (float, 0.0 unused), off[24] whole-cent grid offsets, home pc);
    raises Err naming the scale."""
    code = sc["code"]

    def fail(msg):
        raise Err(f"{sc['src']}: scale {code}: {msg}")
    if len(code) > 5 or not code:
        fail(f"code '{code}' longer than 5 characters")
    if len(sc["name"].encode()) > 8 or not sc["name"].isascii():
        fail(f"name '{sc['name']}' longer than 8 ASCII characters")
    if sc["up"] is None:
        fail("no 'up' line")
    try:
        home = home_pc(sc["home"])
        frm = 0.0
        if sc["from"] is not None:
            if len(sc["from"]) != 1:
                fail("'from' takes one value")
            frm = cents(sc["from"][0])
    except ValueError as e:
        fail(str(e))
    exact = {}      # slot -> exact cents above the tonic
    offs = {}       # slot -> whole-cent offset from the grid
    masks = []
    for form in ("up", "down"):
        vals = sc[form] if sc[form] is not None else sc["up"]
        if form == "down" and sc["down"] is None:
            masks.append(masks[0])
            continue
        mask = 0
        for tok in vals:
            try:
                c = (cents(tok) - frm) % 1200.0
            except (ValueError, ZeroDivisionError) as e:
                fail(f"{form}: bad value '{tok}' ({e})")
            if c > 1199.9999:
                c = 0.0
            q = c / 50.0
            if abs(q - math.floor(q) - 0.5) < 1e-6 / 50:
                fail(f"{form}: value {tok} ({c:.4f} c) is on a grid midpoint")
            slot = rnd(q) % 24
            rem = c - 50.0 * rnd(q)       # exact offset from the grid slot
            if abs(abs(rem) - math.floor(abs(rem)) - 0.5) < 0.001:
                fail(f"{form}: value {tok} is {c:.4f} c, its offset {rem:+.4f} c from the grid is within 0.001 c of a "
                     f"whole-cent rounding tie")
            off = rnd(rem)
            if abs(off) > 63:
                fail(f"{form}: value {tok} is {off:+d} c from slot {slot} (limit 63)")
            if slot in exact and abs(exact[slot] - c) > 0.05:
                fail(f"slot {slot} has exact pitches {exact[slot]:.2f} and {c:.2f} c ({form} value {tok})")
            if slot in offs and offs[slot] != off:
                fail(f"slot {slot}: both forms give different whole-cent offsets {offs[slot]:+d} and {off:+d}")
            exact.setdefault(slot, c)
            offs[slot] = off
            mask |= 1 << slot
        n = bin(mask).count("1")
        if not 5 <= n <= 12:
            fail(f"{form}: {n} degrees (need 5..12)")
        if not mask & 1:
            fail(f"{form}: slot 0 (the tonic) missing")
        masks.append(mask)
    return (masks[0], masks[1], [exact.get(i, 0.0) for i in range(24)], [offs.get(i, 0) for i in range(24)], home)


def assign_keys(mask, root):
    """Key assignment (see the module docstring). -> {slot: key 0..11} or None if no assignment."""
    slots = [s for s in range(24) if mask >> s & 1]

    def cands(s):
        a = (root + s) % 24
        return [a // 2] if a % 2 == 0 else [((a + 1) // 2) % 12, (a - 1) // 2]
    key = {}

    def rec(i, used):
        if i == len(slots):
            return True
        for k in cands(slots[i]):
            if not used & (1 << k):
                key[slots[i]] = k
                if rec(i + 1, used | 1 << k):
                    return True
        return False
    return dict(key) if rec(0, 0) else None


def load(d):
    d = Path(d)
    out = []
    for s in SETS:
        p = d / FILES[s]
        if not p.exists():
            raise Err(f"{p}: missing scale file")
        out += parse_file(p, s)
    seen = {}
    for sc in out:
        if sc["code"] in seen:
            raise Err(f"{sc['src']}: scale {sc['code']}: duplicate code (also {seen[sc['code']]})")
        seen[sc["code"]] = sc["src"]
    if not out or out[0]["code"] != FIRST:
        raise Err(f"{d / FILES[SETS[0]]}: the first scale must be {FIRST} (index 0, a new project's scale)")
    res = []
    for sc in out:
        up, down, ex, off, home = build(sc)
        for root in range(24):   # also proves up|down has at most 12 slots, so off[12] always fits
            if assign_keys(up | down, root) is None:
                raise Err(f"{sc['src']}: scale {sc['code']}: does not fit 12 keys at root {root} "
                          f"({bin(up | down).count('1')} grid positions across up and down)")
        res.append(dict(code=sc["code"], name=sc["name"], set=SETS.index(sc["set"]), home=home,
                        up=up, down=down, exact=ex, off=off,
                        cents_x10=[rnd(e * 10.0) for e in ex]))   # exact pitch in 0.1 c, for --print and tests only
    return res


def load_legacy(d, res):
    """scales/legacy.txt: the six-set library's 73 codes in old index order, each mapped to a code or '-'.
    -> [new index or 255] in old index order."""
    p = Path(d) / "legacy.txt"
    if not p.exists():
        raise Err(f"{p}: missing legacy map")
    index = {r["code"]: i for i, r in enumerate(res)}
    out, olds = [], set()
    for ln, raw in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.split("#", 1)[0].split()
        if not line:
            continue
        where = f"{p.name}:{ln}"
        if len(line) != 3 or not line[0].isdigit() or int(line[0]) != len(out):
            raise Err(f"{where}: expected '<index> <old code> <new code or ->' with index {len(out)}")
        if line[1] in olds:
            raise Err(f"{where}: old code {line[1]} repeated")
        olds.add(line[1])
        if line[2] != "-" and line[2] not in index:
            raise Err(f"{where}: {line[1]} maps to {line[2]}, which is not in the library")
        out.append(255 if line[2] == "-" else index[line[2]])
    if len(out) != LEGACY_COUNT:
        raise Err(f"{p}: {len(out)} entries, expected {LEGACY_COUNT}")
    return out


def emit(res, legacy):
    """-> (header, source): declarations in scales.h, the one copy of the data in scales.c."""
    H = ["/* generated by tools/gen_scales.py from the scales/ files: do not edit */",
         "/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */",
         "#ifndef MAQAM_SCALES_H", "#define MAQAM_SCALES_H",
         "#include <stdint.h>", "",
         "/* up/down: 24-bit masks of used grid slots, little-endian bytes (bit i = i quarter tones above the tonic).",
         "   off[k]: for the k-th used slot of up|down (ascending), its pitch above the tonic minus its grid pitch",
         "   slot * 50 c, whole cents. At most 12 used slots. No padding: 29 bytes. */",
         "typedef struct { char name[9]; uint8_t set, home; uint8_t up[3], down[3]; int8_t off[12]; } scale_def_t;",
         "static inline uint32_t scale_up(const scale_def_t *s) { return s->up[0] | (uint32_t)s->up[1] << 8 | (uint32_t)s->up[2] << 16; }",
         "static inline uint32_t scale_down(const scale_def_t *s) { return s->down[0] | (uint32_t)s->down[1] << 8 | (uint32_t)s->down[2] << 16; }",
         "static inline unsigned scale_popcount(uint32_t m) { unsigned n = 0; for (; m; m &= m - 1U) ++n; return n; }",
         "/* Whole cents above the tonic of grid slot `slot` (0..23); meaningful for used slots. */",
         "static inline int scale_cents(const scale_def_t *s, unsigned slot)",
         "{",
         "    return (int)slot * 50 + s->off[scale_popcount((scale_up(s) | scale_down(s)) & ((1U << slot) - 1U))];",
         "}",
         "enum { " + ", ".join(f"SET_{s}" for s in SETS) + ", NSETS };",
         f"#define NSCALES {len(res)}",
         "/* defined once in scales.c */",
         "extern const char *const SET_NAMES[NSETS];",
         "extern const scale_def_t SCALES[NSCALES];",
         "extern const char SCALE_CODE_STR[NSCALES][6]; /* the codes: host tools and tests only, the firmware does not use them */",
         "/* Six-set library index (settings tag 0xA1) -> index here, or 0xFF when the scale has no counterpart. */",
         f"#define NSCALES_LEGACY {len(legacy)}",
         "extern const uint8_t SCALE_LEGACY[NSCALES_LEGACY];",
         "#endif", ""]
    C = ["/* generated by tools/gen_scales.py from the scales/ files: do not edit */",
         "/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */",
         '#include "scales.h"', "",
         "const char *const SET_NAMES[NSETS] = { " + ", ".join(f'"{s}"' for s in SETS) + " };",
         "",
         "const scale_def_t SCALES[NSCALES] = {"]
    for r in res:
        used = [i for i in range(24) if (r["up"] | r["down"]) >> i & 1]
        off12 = [r["off"][i] for i in used] + [0] * (12 - len(used))
        C.append('    {"%s", SET_%s, %d, {0x%02X,0x%02X,0x%02X}, {0x%02X,0x%02X,0x%02X}, {%s}},' % (
            (r["name"], SETS[r["set"]], r["home"])
            + tuple(r["up"] >> (8 * i) & 255 for i in range(3)) + tuple(r["down"] >> (8 * i) & 255 for i in range(3))
            + (",".join(str(t) for t in off12),)))
    C += ["};", "", "const char SCALE_CODE_STR[NSCALES][6] = {"]
    C += ["    " + ", ".join('"%s"' % res[i]["code"] for i in range(r, min(r + 8, len(res)))) + "," for r in range(0, len(res), 8)]
    C += ["};", "", "const uint8_t SCALE_LEGACY[NSCALES_LEGACY] = {"]
    C += ["    " + ", ".join(str(legacy[i]) for i in range(r, min(r + 16, len(legacy)))) + "," for r in range(0, len(legacy), 16)]
    C += ["};", ""]
    return "\n".join(H), "\n".join(C)


def table(res):
    L = []
    for r in res:
        t = " ".join(f"{i}:{v / 10:.1f}" for i, v in enumerate(r["cents_x10"]) if v or i == 0 and r["up"] & 1)
        L.append(f"{r['code']:<6}{SETS[r['set']]:<6}0x{r['up']:06X} 0x{r['down']:06X}  {t}")
    return "\n".join(L)


KEYS = "C C# D D# E F F# G G# A A# B".split()


def assign_text(r, root):
    k = assign_keys(r["up"] | r["down"], root)
    return f"{r['code']} root {root}: " + " ".join(f"{s}->{KEYS[k[s]]}" for s in sorted(k))


def main(argv):
    d = Path(__file__).resolve().parent.parent / "scales"
    pr, out, asg = False, None, None
    a = list(argv)
    while a:
        x = a.pop(0)
        if x == "--print":
            pr = True
        elif x == "--dir":
            d = Path(a.pop(0))
        elif x == "--assign":
            asg = [a.pop(0)]
            if a and a[0].isdigit():
                asg.append(int(a.pop(0)))
        else:
            out = x
    if not pr and out is None and asg is None:
        print(__doc__.split("\n\n")[0], file=sys.stderr)
        return 2
    try:
        res = load(d)
        legacy = load_legacy(d, res)
    except Err as e:
        print(f"gen_scales: error: {e}", file=sys.stderr)
        return 1
    if pr:
        print(table(res))
    if asg:
        r = next((x for x in res if x["code"] == asg[0]), None)
        if r is None:
            print(f"gen_scales: error: no scale {asg[0]}", file=sys.stderr)
            return 1
        for root in ([asg[1]] if len(asg) > 1 else range(24)):
            print(assign_text(r, root))
    if out:
        h, c = emit(res, legacy)
        out = Path(out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(h)
        out.with_suffix(".c").write_text(c)   # scales.h -> scales.c next to it
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
