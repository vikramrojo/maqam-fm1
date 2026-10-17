<!-- Ported from maqam-dev (GPL-3.0) and adapted for FM1 Open. -->
# Maqam scales

The 73 scales live in six plain text files, one per set: `west.txt`, `turk.txt`, `arab.txt`, `india.txt`, `psych.txt`, `jins.txt`. `tools/gen_scales.py` turns them into `app/maqam/scales.h` (`SCALES[]`, `NSCALES`); `tools/check_scales.sh` verifies that the committed header is current. Adding a scale is a text edit: no firmware source changes.

## A scale

```
# a comment
scale CODE "Name" home <note>
  from <value>        (optional) the scale's own tonic
  up   <values>       ascending form, the tonic first
  down <values>       descending form (optional: the same as up)
```

- **CODE**: at most 5 characters, unique over all scales. It is what the parameter pages and the web editor show. Distinguish look-alikes (`T.RST` for the Turkish Rast, `RAST` for the Arabic one).
- **"Name"**: at most 8 ASCII characters.
- **home**: the traditional tonic, a hint shown on the scale page (`home Bb`); choosing a scale never moves the root. A letter `A`..`G` and at most one suffix: `b` flat, `#` sharp, `d` half-flat, `+` half-sharp. `C`, `Bb`, `Ed` (E half-flat).
- **Values** are Scala (`.scl`) syntax, relative to the tonic: **cents** if the value has a decimal point (`386.`, `701.955`), otherwise a **ratio** (`9/8`, `5/4`; `2` alone is `2/1`). You can paste values from any `.scl` file.
- `from <value>` names the scale's own tonic when the values are written from another note (ARAB rows are written as ratios from Rast; Bayati says `from 9/8`). Every value is divided by it and reduced into one octave.
- 5 to 12 degrees, the tonic in both forms.

How the generator reads a value: it rounds it to the nearest quarter tone (50 cents) of the 24-note grid, which sets that degree in the scale's mask, and keeps the remainder (at most ±25 cents) as that degree's *tuning offset*. TRADITIONAL intonation adds the offset at note-on; EQUAL ignores it. Both forms of a scale share one offset per grid position, so the build fails if two pitches land on the same position with different offsets.

By set (the convention of the files, kept in each file's header comment):

- **WEST**: plain equal-tempered cents (`0. 200. 400. ...`), so every offset is 0.
- **TURK**: Pythagorean ratios (Arel-Ezgi-Uzdilek). Write comma counts only in comments.
- **ARAB**: al-Ṣabbāgh (1954) ratios from Rast.
- **INDIA**: just-intonation ratios.
- **PSYCH**: equal quarter tones (24-TET cents), so every offset is 0. Microtonal scales as King Gizzard and the Lizard Wizard play them, in five families.
- **JINS**: equal quarter tones; combinations of a lower jins and a different upper jins, each attested.

## Rules

- **Append only.** A scale's index is saved in projects, so existing scales never move. The generator holds the frozen list of the 73 shipped scales (`FROZEN` in `tools/gen_scales.py`): they keep their indices whatever the line order in the files, and removing or renaming one is a build error that names it. **Add a new scale to any set's file**: it gets the next free index (73, 74, ...) and belongs to that file's set. Once a new scale ships, add its code to the end of `FROZEN`.
- **Cite the source** of every new row in a comment (a book, a page, a website, a recording): ratios and cents need to be checkable.
- The build fails, naming the file, line and scale, for: a value exactly halfway between two quarter-tone steps (the rounding would be ambiguous: nudge it by a cent or write it as a ratio), two different exact pitches on one grid position, a scale whose two forms together cannot be assigned to 12 distinct keys at some root (the error names the scale and root), a code over 5 characters or a repeated code, fewer than 5 or more than 12 degrees, or a tonic missing from a form.

## Check

```
python3 tools/gen_scales.py --print                  # a table: code, set, masks, exact cents per grid slot
python3 tools/gen_scales.py app/maqam/scales.h       # write the header
python3 tools/gen_scales.py --assign RAST 0         # key assignment of a scale at a root (0..23)
sh tools/check_scales.sh                            # header up to date + the checks
python3 tests/scales_check.py                        # the checks and the error fixtures
```

Try a scale without touching the repository: copy this directory, edit the copy, and generate from it with `--dir`:

```
cp -r scales /tmp/mine
printf '\nscale TEST "TestScl" home D   # throwaway\n  up   1/1 9/8 5/4 4/3 3/2 5/3 15/8\n' >> /tmp/mine/india.txt
python3 tools/gen_scales.py /tmp/scales.h --dir /tmp/mine
```

`NSCALES` is then 74. `tests/scales_check.py` pins the shipped 73 and will say so; a scale you keep needs its row added there and its code to `FROZEN` in the generator.
