<!-- Ported from maqam-dev (GPL-3.0) and adapted for FM1 Open. -->
# Maqam scales

The 36 scales are Sinfon's two sets, in two plain text files: `west.txt` (WEST, 17) and `arab.txt` (ARAB, 19). They are **vendored from Sinfon** (github `vikramrojo/maqam-dev`, `SketchKit/Sources/SketchCore/Scales/`) together with `SOURCE.md`, which records every change since they left this repository; `UPSTREAM` holds the Sinfon commit and each file's SHA-256. Why two sets, and where the earlier Turkish, Indian and personal scales went, is explained in Sinfon's `openspec/assets/scale-system.md`.

`tools/gen_scales.py` turns them into `app/maqam/scales.h` (`SCALES[]`, `NSCALES`) and `app/maqam/scales.c` (the data; the scale codes go to a separate `SCALE_CODE_STR[]` that the firmware does not link); `tools/check_scales.sh` verifies that the committed header is current.

## Changing a scale

Edit the files in Sinfon, then copy them here:

```
tools/sync_scales.sh ~/Git/maqam-dev     # copies west.txt, arab.txt, SOURCE.md, rewrites UPSTREAM, regenerates
sh tools/check_scales.sh                 # header up to date, the checks, the vendored hashes
```

Do not edit `west.txt`, `arab.txt` or `SOURCE.md` here: `tests/scales_check.py` fails when they differ from the hashes in `UPSTREAM`.

## A scale

```
# a comment
scale CODE "Name" home <note>
  from <value>        (optional) the scale's own tonic
  up   <values>       ascending form, the tonic first
  down <values>       descending form (optional: the same as up)
  alias "Other name"  (any number) another name for the same notes: Sinfon only, ignored here
```

- **CODE**: at most 5 characters, unique over all scales. It names the scale in the files, tools and tests (the firmware does not store it; the pages show the "Name").
- **"Name"**: Sinfon allows long names; the FM1 shows at most 8 ASCII characters, so `tools/gen_scales.py` (`DISPLAY`) shortens three for the display: Phrygian dominant `Hicaz`, Bayati Shuri `BaySuri`, Bastanikar `Bastnkar`. Any other name over 8 characters fails the build.
- **home**: the traditional tonic, a hint shown on the scale page (`home Bb`); choosing a scale never moves the root. A letter `A`..`G` and at most one suffix: `b` flat, `#` sharp, `d` half-flat, `+` half-sharp.
- **Values** are Scala (`.scl`) syntax, relative to the tonic: **cents** if the value has a decimal point (`386.`), otherwise a **ratio** (`9/8`; `2` alone is `2/1`). WEST is in equal-tempered cents (every offset 0); ARAB in al-Ṣabbāgh's ratios written from Rast, with `from` naming the maqam's own tonic.
- 5 to 12 degrees, the tonic in both forms.

The generator rounds each value to the nearest quarter tone (50 cents) of the 24-note grid, which sets that degree in the scale's mask, and keeps the remainder as that degree's *tuning offset*, rounded to whole cents in the generated table (`off[12]`; exact cents stay in `--print`). TRADITIONAL intonation adds the offset at note-on; EQUAL ignores it.

## Indices and old projects

- A scale's index is its position in the files: WEST then ARAB, in file order. `CHR` must stay first (index 0, a new project's scale).
- Projects store the project scale's index. Projects saved with the earlier six-set library (73 scales, settings tag `0xA1`) are migrated on load through **`legacy.txt`**: the 73 old codes in their old index order, each with its new code or `-`. Turkish and Indian scales map to the scale Sinfon lists them as an alias of (Uşşak → BAYAT, Yaman → LYD, Turkish Rast → MAHUR, ...); the four ragas with no counterpart and the PSYCH and JINS scales map to `-` (the project scale falls back to CHR; the tracks keep their tuning tables). New saves use tag `0xA2`.
- `legacy.txt` is append-only and never reordered. Inserting a scale in the middle of a file in Sinfon renumbers the later ones in projects saved since: avoid it, or extend the migration.

## Build errors

The build fails, naming the file, line and scale, for: a value exactly halfway between two quarter-tone steps, or whose offset is within 0.001 cent of a half cent; two different exact pitches on one grid position; a scale whose two forms together cannot be assigned to 12 distinct keys at some root (the error names the root); a code over 5 characters or repeated; a display name over 8 ASCII characters; fewer than 5 or more than 12 degrees, or a tonic missing from a form; an unquoted `alias`; `CHR` not first; or a `legacy.txt` entry that is out of order or maps to a code that no longer exists.

## Check

```
python3 tools/gen_scales.py --print                  # a table: code, set, masks, exact cents per grid slot
python3 tools/gen_scales.py --assign RAST 0          # key assignment of a scale at a root (0..23)
sh tools/check_scales.sh                             # header up to date + the checks
python3 tests/scales_check.py                         # the checks, the error fixtures and the vendored hashes
```

Try a scale without touching the repository: copy this directory, edit the copy, and generate from it with `--dir`:

```
cp -r scales /tmp/mine
printf '\nscale TEST "TestScl" home D   # throwaway\n  up   1/1 9/8 5/4 4/3 3/2 5/3 15/8\n' >> /tmp/mine/arab.txt
python3 tools/gen_scales.py /tmp/scales.h --dir /tmp/mine
```
