<!-- Ported from maqam-dev (GPL-3.0) SCALES.md and adapted for FM1 Open. -->
# Scales and quarter tones on FM1 Open

How to play and edit the 73 scales (Western, Turkish, Arabic, Indian, plus the PSYCH and JINS sets), quarter tones and per-step alternate pitches on the FM1 screen. This is the fork's scale feature (`openspec/changes/maqam-scales`); stock FM1 Open V13 only provides the engine tables underneath.

Two words used throughout:
- **Half-flat** (written `Ed` in text) is a quarter tone, 50 cents, below the note: E half-flat sits halfway between E-flat and E.
- **Half-sharp** (written `E+`) is a quarter tone above.

Panel words: **SELECT** is the first encoder (track), **ALGO** the second (page), **K1-K4** the four knobs under the display, **PRESETS** the seventh encoder.

---

## 1. How it works

- Every melodic track has two tables of 12 whole-cent offsets, one per pitch class (C, C#, D ... B): a **main** table and an **alt** table (an engine feature of V13, saved with the project). The engine adds the offset at note-on. Nothing else about notes changes: a step stores an ordinary MIDI note plus a one-bit **alt** flag.
- Choosing a scale **computes the main table**. Choosing a root and a scale puts each degree on one of the 12 keys, so the keys play the scale's exact pitches.
- **Intonation** decides how exact:
  - **EQUAL** puts each degree on the quarter-tone grid: offsets are 0 or exactly -50 / +50 from the key.
  - **TRAD** (the default) uses the scale's exact pitch to a whole cent: Pythagorean for Turkish, al-Ṣabbāgh for Arabic, just intonation for Indian. Rast on C: E -43, B -41, A# +20, D +4, F -2, G +2, A +6. Western, PSYCH and JINS scales are equal in both.
  - It is a **project** setting. Switching it recomputes every track that follows a scale and does not touch alt edits.
- **Drum tracks** (808, sampler, track 8) are never tuned.

### The alt table

A semitone degree and a quarter-tone degree compete for the same key (E half-flat and E-flat both live near the E key), so each key has a second pitch:
- **Default alt table**: a **WEST** scale gets an all-zero alt table (a WEST project has all-zero tables; quarter tones are one edit away on the ALT page). For every other set: a key carrying a quarter-tone degree gets 0 (plain 12-TET: E-half-flat's alt is E); a key carrying a semitone degree gets -50 (its half-flat); keys with no degree get 0. Rast on C: alt 0 on E and B, -50 on C, D, F, G, A, A#.
- A **scale or root change resets** the track's alt table to these defaults. Edits survive everything else (intonation, saving, loading).
- Each entry is editable from -127 to +127 cents on the ALT page (section 4).

---

## 2. Playing

### Key layouts

The layout says what the 27 keys do on a tuned track. Set it on the KEYS page (section 4): a project default (saved with the project) and a per-track override (not saved).

- **SNAP** (default): every key plays. Each key plays its own pitch class if the scale has a degree there; otherwise it plays the next scale note **above** when you are going up and **below** when going down. In C major, C# plays D going up and C going down.
- **WHITE**: the white keys walk the scale's notes in order, starting at C4 = the root note, and the black keys are silent. A seven-note maqam fits in the seven white keys of an octave, so this is the easiest way to play any maqam. The layout comes from the form of the scale with more notes, so it does not shift with the direction.
- **CHROM**: plain chromatic keys. Chromatic and CUSTOM tracks always play like this.

### Up and down

Scales can have different ascending and descending forms (Rast: B half-flat up, B-flat down). A key **lower than the previous key** uses the descending form; any other key (higher or the same) the ascending form. After a pause of more than one second the next key counts as going up. The note that sounds is the note that gets recorded.

### Alt gesture and octave

On a melodic track, **hold OCT- or OCT+** and press a key: that note uses the **alt** table. While recording, the step is stored with the alt flag and plays back alt. Keys already sounding are not re-attacked when you press or release OCT.

The octave changes with a **double tap** of OCT- or OCT+ (two presses within 300 ms, no key between). A single tap does nothing on melodic tracks. On drum tracks and on the step pages the OCT buttons keep their stock behaviour (single press changes the octave / pages the steps). The scale pages behave like the main pages here.

### Key lights

On a melodic track with a scale, the LEDs of the keys that carry a degree light up (Rast on C: C D E F G A A# B; not C# D# F# G#).

---

## 3. Names on the screen

Wherever a melodic note is named (the step tiles, the step NOTE knob, the NOTE knob on the overview, the subtitle root, the alt readout), the pitch is the key plus its table offset, rounded to the nearest quarter tone. Names use one letter and at most one accidental: `b` flat, `d` half-flat, none, `+` half-sharp, `#` sharp, followed by the octave (the octave of the spelled letter: `Cd5` is a quarter tone under C5, so B4 +50 c reads `Cd5`).

**One letter per degree.** A scale whose two forms both have seven degrees is spelled like sheet music: degree *n* takes the letter *n* steps above the tonic's letter, and the accidental that fits its pitch. The tonic's letter is whichever spelling of the root lets every degree use a single accidental, with the fewest accidentals (flat on a tie). The up form is spelled first; a degree shared with the down form keeps that spelling.

| Scale (root) | Degrees read | Test |
|---|---|---|
| Rast on C, TRAD: E4 stored | `Ed4` | `names_test` "Rast C: E4 stored" |
| Bayati on D | D Ed F G A Bb C (A# key = `Bb4`, not `A#4`) | `names_test` "Bayati D degree 1-7" |
| Saba on D | fourth is `Gb`, not F# | `names_test` "Saba D" |
| Hijaz on D | up D Eb F# G A Bd C, down seventh `Bb` | `names_test` "Hijaz D up degree 1-7", "descending seventh" |
| Rast on C | up seventh `Bd`, down seventh `Bb` | `names_test` "Rast C: ascending / descending seventh" |
| Major on A#/Bb | Bb C D Eb F G A | `names_test` "MAJ on A#/Bb degree 1-7" |
| Major on C# | Db (flat wins the tie) | `names_test` "MAJ on C#" |

**Fixed names as the fallback.** The fixed 24 names

`C C+ C# Dd D D+ D# Ed E Fd F F+ F# Gd G G+ G# Ad A A+ A# Bd B Cd`

are used when the pitch is not a degree and its key carries none, and for the whole scale when a form does not have seven degrees (pentatonic, whole-tone, diminished, chromatic) or a degree would need a double accidental. A zero offset on such a scale gives exactly the stock 12-tone name, so Western scales look as they always did: Pentatonic on C plays its D# key as `D#4` (`names_test` "PEN on C"), and every one of the 128 notes under WEST chromatic is the stock name (`names_test` "WEST CHR"). An out-of-scale pitch on a key that carries a degree keeps the degree's letter: on a Rast-on-C track an alt E at -100 reads `Eb4` because the E key carries Ed (`names_test` "Rast C alt E at -100"). Without a degree on the key it is a fixed name: C# on Rast reads `C#4` (`names_test` "Rast C: C# key").

Spelling is computed from the scale and root every time; nothing is stored. Of the 73 scales on the 24 roots, 1093 of 1752 pairs are spelled and the rest use the fixed names (`names_test` "spelling:" line).

- **Alt steps carry a dot**: a step that holds alt E in a Rast track reads `E4.` while a plain E step reads `Ed4` (`names_test` "Rast TRAD, E4 alt"). The name of an alt step comes from the alt table.
- Drum tracks show the stock names.

---

## 4. The scale pages

Open them with **SCL**: they come up on the page you used last. **SCL** again moves to the next page (1/4, 2/4, 3/4, 4/4, then 1/4 again; **ALGO** does the same, both ways). The SCL button lights while you are there. **HOME** leaves, as on every page; **SAVE** opens the project page and returns here after a good save. The stock FM scaling screens (DX7 level and rate scaling, or the pitch page on other engines) are on **HOME + SCL** (hold HOME, press SCL): on that screen SCL keeps toggling level and rate as in stock, and HOME + SCL again goes back to the scale pages. The **keys play** on every scale page as on the main pages; on the ALT page a key also selects that key for editing. The root is set only with K1.

The pages use the same frame as the DX7 edit pages: the stock header (tempo, transport, mode) stays, and a small row under it reads `T1 DX7  PROJECT  1/4` (track, source, page name, position). The hint row reads `SCL PAGE  SELECT TRACK  HOME BACK`. Changing page redraws only the page name, the position, the content and the knobs (`pages_test` "frame").

**SELECT** picks the track, **ALGO** moves between the four pages like SCL. Under the section row every page shows the subtitle, for the project (page 1) or the selected track (pages 2-4):

`Ed arab sikah^ trad` (`pages_test` "subtitle after K1/K2/K3") = root Ed (E half-flat), set ARAB, scale Sikah, TRAD.

- The arrow after the scale name (up or down, as the last key of the track went) shows **only for scales whose two forms differ**. Scales with one form (Major, Bilawal ...) show none.
- The line under it says whether the track **FOLLOWS PROJECT**, has an **OVERRIDE**, or has a **CUSTOM TABLE**, and for drum tracks **DRUMS - NOT TUNED**.
- The **home hint** `home: Ed` shows the scale's traditional tonic for non-Western scales. It is a hint only: choosing a scale never moves the root. To play Sikah at home, set the root to E half-flat with K1.
- **Pages 1 and 2 show a degree row**: only the scale's degrees (both forms together), in pitch order from the root, each cell with its spelled name and, under it, its cents from the nearest quarter tone (24-TET); a row of all zeros (WEST, EQUAL) shows no cents. Green frame = the root. A degree of one form only is marked: `^` ascending only, `v` descending only. Keys that carry no degree do not appear. Bayati on D, TRAD: seven cells `D Ed F G A Bb C` with `0 +3 -6 -2 +2 +16 -4` (Ed is 47 c below E, 3 c above E half-flat) (`pages_test` "Bayati on D", "Bayati row cents"). Rast on C: `C D Ed F G A Bb Bd`, `Bb` marked down (descending only) and `Bd` marked up (ascending only) (`pages_test` "Rast on C"). Every scale gives one cell per degree (5 to 9; 12 for WEST chromatic) (`pages_test` "degree row: one cell per degree"). A CUSTOM track has no degrees and keeps the 12-key strip.
- Page 3 (KEYS) and page 4 (ALT) keep the strip of all 12 keys: coloured = carries a degree, green frame = root.
- If the last project save was refused because a sample bank leaves no room for the tuning tables, the pages show **SAVE FAILED: REDUCE SAMPLE BANK** (section 6).

### Page 1/4: PROJECT

| Control | Does |
|---|---|
| **K1 ROOT** | the project root, a quarter tone per click (24 positions, wraps) |
| **K2 SET** | WEST, TURK, ARAB, INDIA, PSYCH, JINS (jumps to that set's first scale) |
| **K3 SCALE** | the scale within the set (wraps) |
| **K4 TUNING** | right = TRAD, left = EQUAL |

Every melodic track follows these unless it overrides.

### Page 2/4: TRACK

| Control | Does |
|---|---|
| **K1 OVERRIDE** | right = this track gets its own root and scale (starting from the project's); left = it follows the project again |
| **K2 ROOT**, **K3 SET**, **K4 SCALE** | edit the track's root, set and scale (turning any of them switches the override on) |

A track always uses the project's intonation. Two maqamat at once: set the project to Rast on C, then override track 2 with Bayati on D.

**CUSTOM**: a track whose table matches no scale (edited by hand, or by another tool) shows `custom`; its table is kept exactly as it is. K1 left on this page replaces it with the project's scale.

### Page 3/4: KEYS

| Control | Does |
|---|---|
| **K1 PROJECT** | the project default layout: SNAP, WHITE, CHROM |
| **K2 TRACK** | this track's layout: FOLLOW (the project's), SNAP, WHITE, CHROM |

### Page 4/4: ALT table editor

| Control | Does |
|---|---|
| **a key** or **K1 KEY** | picks which pitch class's alt entry to edit (highlighted in the strip) |
| **K2** | the entry's cents, +/-1 per click, from -127 to +127 |
| **K3** | the same, +/-10 per click |
| **K4 RESET** | turn it: the track's alt table goes back to the scale's defaults |

The strip keeps all 12 keys, because an alt pitch can be set on any key: keys that carry a degree show the degree's spelled name, the others are dimmed with their fixed name. On Bayati on D the A# key reads `Bb` and C#, D#, F#, G# are dimmed (`pages_test` "alt strip: A# key is labelled Bb", C# D# F# G# dimmed). The knobs show the entry's raw value (-127..+127 from the 12-TET key). The large line shows the resulting pitch: its cents from the nearest quarter tone and its name at octave 4, spelled as for notes (alt E at -100 on Rast on C: `ALT 0 Eb4`), and the line under it the main entry the same way. Knob values are cut to four characters (`BAYA`), always in the large font. Edits are kept until the scale or root of that track next changes, and are saved with the project.

---

### The scale tag

On the overview and step pages the header shows the selected track's scale at the right, under the mode box: the spelled root and the scale name cut to four characters, `Ed RAST` (`pages_test` "Rast on Ed"). It is hidden for WEST chromatic and for drum tracks, and a hand-edited table shows `CUST`.

## 5. Recipes

- **Play a maqam in two moves.** SCL, K2 to ARAB, K3 to the maqam, K1 to the root (audition with the keys); ALGO twice to KEYS, K1 to WHITE; HOME; play the white keys up and down.
- **Hear the traditional tuning.** Hold a note on a Rast track and turn K4 on page 1 back and forth: the pitch moves by a few cents.
- **One quarter tone anywhere.** Hold OCT- (or OCT+) and press a key to get the alt pitch; with Rast alt E is plain E.
- **Record an alt note.** REC, hold OCT-, press the key: the step shows `E4.`.
- **Two maqamat.** See page 2.
- **Fine-tune an alt entry.** Page 4, press the key, K2 / K3.

---

## 6. Saving

- The tables are saved with the project by FM1 Open V13 itself (and travel with SysEx project transfer and undo). The fork adds an 8-byte trailer to the project record with the project scale, root, intonation, key layout default and which tracks override. Stock V13 firmware ignores the trailer and still plays the same tuning.
- A track's own scale and root are recovered from its saved table by matching it against the library at the project intonation (first match wins). A table with no match loads as CUSTOM and is never changed.
- A stock project without a trailer loads as WEST / C / TRAD; if it carries tables, they are kept and each track's scale is derived or shown as CUSTOM.
- **REDUCE SAMPLE BANK**: V13 keeps the tuning tables in the last flash sector, which an old, very large stored sample bank can overlap. Saving non-zero tuning then fails (the project page says `REDUCE SAMPLE BANK`, and the scale pages repeat it); saving a project with all-zero tables still works. Remove samples to make room. A project that only uses WEST scales has all-zero tables, so it always saves; non-WEST scales (and edited alt entries) make the tables non-zero.
- The per-track key layout is not saved; the project default is.

---

## 7. The scales

Codes are the names in the scale files and tools (the pages show the Name); **Home** is the traditional tonic (a hint). Scales are append-only: a scale's index is saved in projects and never moves.

### WEST (16)

Western modes and symmetric scales, 12-tone equal temperament. Every offset is 0, so EQUAL and TRAD are identical.

| Code | Name | Home | Forms differ |
|---|---|---|---|
| `CHR` | Chromat | C |  |
| `MAJ` | Major | C |  |
| `MIN` | Minor | C |  |
| `DOR` | Dorian | C |  |
| `MIX` | Mixolyd | C |  |
| `PEN` | Penta | C |  |
| `MPEN` | MinPenta | C |  |
| `HARM` | HarmMin | C |  |
| `PHRY` | Phrygian | C |  |
| `LYD` | Lydian | C |  |
| `LOC` | Locrian | C |  |
| `MEL` | MelMin | C |  |
| `BLUES` | Blues | C |  |
| `WHOLE` | Whole | C |  |
| `DIMHW` | DimHW | C |  |
| `DIMWH` | DimWH | C |  |

### TURK (12)

Turkish makamlar, Arel-Ezgi-Uzdilek system: Pythagorean ratios (built from 3/2).

| Code | Name | Home | Forms differ |
|---|---|---|---|
| `T.RST` | Rast | G | up / down |
| `USSAK` | Ussak | A |  |
| `HUSEY` | Huseyni | A | up / down |
| `KURDI` | Kurdi | A |  |
| `NIHAV` | Nihavend | G | up / down |
| `HICAZ` | Hicaz | A | up / down |
| `HCKAR` | Hicazkar | G |  |
| `SUZIN` | Suzinak | G |  |
| `KARCI` | Karcigar | A |  |
| `T.SBA` | Saba | A |  |
| `SEGAH` | Segah | Bd |  |
| `HUZZM` | Huzzam | Bd |  |

### ARAB (12)

Arabic maqamat, al-Ṣabbāgh (1954) 24-tone ratios; degree structure after Touma, *The Music of the Arabs*, and maqamworld.com.

| Code | Name | Home | Forms differ |
|---|---|---|---|
| `RAST` | Rast | C | up / down |
| `BAYAT` | Bayati | D |  |
| `SABA` | Saba | D |  |
| `SIKAH` | Sikah | Ed | up / down |
| `HIJAZ` | Hijaz | D | up / down |
| `NAHAW` | Nahawand | C | up / down |
| `KURD` | Kurd | D |  |
| `AJAM` | Ajam | A# |  |
| `NIKRZ` | Nikriz | C |  |
| `HJKAR` | Hijazkar | C |  |
| `SUZNK` | Suznak | C | up / down |
| `HUZAM` | Huzam | Ed | up / down |

### INDIA (12)

Hindustani thaats and ragas, just-intonation swara ratios. Sa is the root.

| Code | Name | Home | Forms differ |
|---|---|---|---|
| `YAMAN` | Yaman | C | up / down |
| `BILAW` | Bilawal | C |  |
| `KHMAJ` | Khamaj | C | up / down |
| `KAFI` | Kafi | C |  |
| `ASAVR` | Asavari | C | up / down |
| `BHRVI` | Bhairavi | C |  |
| `BHAIR` | Bhairav | C |  |
| `TODI` | Todi | C |  |
| `PURVI` | Purvi | C |  |
| `MARWA` | Marwa | C |  |
| `BHUPL` | Bhupali | C |  |
| `MALKS` | Malkauns | C |  |

### PSYCH (16)

Microtonal "psych" scales as King Gizzard and the Lizard Wizard play them on quarter-tone-fretted guitars: five families built from a 12-TET frame plus a pair of notes that move. Equal quarter tones, so every offset is 0 and EQUAL = TRAD.

| Code | Name | Home | Forms differ |
|---|---|---|---|
| `P.HUS` | Huseyni | A |  |
| `P.MSR` | UshMasri | D |  |
| `P.NEV` | Neva | A |  |
| `P.USK` | Ussak | A |  |
| `P.NHK` | NeutHkar | G |  |
| `P.HKR` | Hicazkar | G |  |
| `P.HJZ` | Hijaz | D |  |
| `P.SUB` | SubPhryg | G |  |
| `P.SU6` | SubPhr6 | G |  |
| `P.SEE` | SeeMe | F# |  |
| `P.BHL` | LowBhair | C |  |
| `P.CHG` | Chahrgah | C |  |
| `P.HMY` | Homayun | C |  |
| `P.RST` | Rast | C |  |
| `P.ACR` | AcemRast | G |  |
| `P.MHR` | Mahur | C |  |

### JINS (5)

Combination scales: a lower jins and a different upper jins on the 5th, each attested (a KGLW song, a named Arabic maqam, or a Hindustani raga). Equal quarter tones, so EQUAL = TRAD.

| Code | Name | Home | Forms differ |
|---|---|---|---|
| `J.KGL` | KGLW | B |  |
| `J.NRZ` | Nairuz | C |  |
| `J.SZK` | Suznak | C |  |
| `J.HJR` | HijazRst | D |  |
| `J.DRB` | Darbari | C |  |

---

## Credits and sources

- The scale library, the generator (`tools/gen_scales.py`), the key-assignment idea, the 24 names, the SNAP/WHITE layouts, the direction rule and the screen conventions come from **maqam-dev** (SLOOP maqam scales, GPL-3.0). The PSYCH and JINS sets come from maqam-dev's pull request #4.
- Turkish: Arel-Ezgi-Uzdilek system (Pythagorean ratios). Arabic: al-Ṣabbāgh (1954), Touma, *The Music of the Arabs*, maqamworld.com. Indian: just-intonation swara ratios (Bor (ed.), *The Raga Guide*). PSYCH / JINS: The Pop Descriptivist, "The Microtonal Scales of King Gizzard and the Lizard Wizard (a Primer and Compendium)", 2024; maqamworld.com; Wikipedia ("Acemli Rast", "Darbari Kanada"); Farhat, *The Dastgah Concept in Persian Music*. Each scale's source is cited in `scales/*.txt` and `scales/README.md`.
- Everything on this page is checked by host tests: `tests/host/pages_test.c`, `names_test.c`, `keys_test.c`, `maqam_test.c` (see `tests/host/README.md`).
