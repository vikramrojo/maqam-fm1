# Host test harness

Native build of the open engine mirror (`web/engine`, `web/synth`,
`third_party/synth`, `app/*.c`) with the browser platform adapter
`web/engine/Simulator.c`, plus an offline driver (`render.c`). Sources, include
order and defines match `tools/Build-Web.ps1`; no engine file is modified.

    tests/host/build.sh                 # -> build/host/fm1_host (CC defaults to clang, else cc)
    tests/host/check_golden.sh          # re-render 16 demos, compare to golden/demos.sha256
    tests/host/check_golden.sh --record # rewrite the golden hashes (only on purpose)
    tests/host/check_golden.sh --determinism # render twice, compare (any platform)
    build/host/fm1_host pitch-test      # A4 per pitched engine (exit 1 on failure)
    build/host/tuning_test              # per-track tuning tables (exit 1 on failure)
    build/host/fm1_host pitch <src> <note>        # src: 0 DX7, 1 303, 3 PSG, 4 PLUCK
    build/host/fm1_host render <0..15> [blocks] [out.f32|-]  # float32 stereo PCM

Demos render 2756 blocks of 128 frames (about 8 s) after a MIDI Start. The
engine takes no host time or randomness, so output is deterministic for a given
compiler and CPU. Golden hashes cover this platform's float behaviour
(clang, `-O2 -ffp-contract=off`), recorded on V13 (`b178ac4`) and identical to
the pre-V13 hashes: the zero-table renders did not change. Re-record elsewhere
only on an unmodified tree.

## Pitch helper

`measure_hz(source, note)` and `measure_cents(source, note)` (in `render.c`)
boot a clean engine, set track 0 to `source` with default patch and zero sends,
hold `note`, and estimate the fundamental from the left channel by normalised
autocorrelation: coarse lag, then repeated lag doubling with parabolic
interpolation, so the interpolation error shrinks by the final multiple.
The 303 is set to a stationary timbre (cutoff max, reso 0, env mod 0) so the
filter sweep does not bias the estimate. TUNE (303), TUNE (PSG) and Pluck
default pots are at their neutral values after boot.

Measured A4 (cents from 440 Hz): DX7 +0.21, 303 -0.09, PLUCK +0.05, PSG +1.56.
PSG is structural: the chip pitch is an integer period (1789772/(16*440) =
254.23 -> 254), giving +1.56 c at A4, so its limit is 2 c and the result must
also match that prediction within 0.5 c. DX7 (default patch, program 0) measures +0.14..+0.33 c over notes 36..84,
a small constant engine offset.

## Tuning test

`tuning_test` (built by `build.sh` like every `tests/host/*_test.c`) covers the
`groove_tuning_*` API (round trip, track 8 and -128 rejected with old tables
kept, boot reset), drum tracks rendering bit-identically with non-zero tables,
tuned pitch per engine as a delta from the untuned measurement (DX7, 303, PLUCK
within 1 c; PSG within 0.5 c of the prediction from its integer chip period),
alt hold / `groove_note_ex` alt selection, stacking with pitch bend, a 303 mono
slide and a DX7 glide settling on the tuned target, and offsets captured at
note-on (a table edit leaves a held note alone; the next note-on uses it). The
DX7 glide is driven on a bare `synth_dx7` because the engine never sets a glide
amount. Measurement code is duplicated from `render.c` (static, so test
binaries stay independent).

## Step alt test

`step_alt_test` includes `tuning_test.c` (with `TUNING_NO_MAIN`) to reuse its
measurement helpers. Sequenced tied steps on every pitched engine measure each
of C4/E4/G4 at the main or alt entry (alt hold has no effect on a sequenced
step). A C/E/G chord step is measured with a Hann-window spectral peak scan per
chord note (DX7), against an untuned chord baseline. Live recording is driven
through `groove_edit_record` and MIDI (hold on/off, chord, overdub replace,
drum track untouched), then played back with hold off. The arpeggiator test
measures the first arp step (gate 100%, rate 0) for a key pressed with and
without alt hold; a two-key case (E4 alt, G4 not, then the reverse) measures the first
and second arp steps in one run (MIDI Start is sent once, since it restarts the arp) and
checks each note against its own key's table.


## Storage test

`storage_test` covers `groove_tuning_capture/apply` (round trip, any -128 or
NULL rejected with nothing changed), the 462,848 B sample cap
(`groove_samples_begin` accepts it, rejects 462,849; SysEx cmd 11 reports it; a
stored bank of 463,352 B still validates and reads), and V13's storage:

- the version-2 tuning sector at `GROOVE_TUNING_OFFSET` (save/reload per slot;
  blank/wrong-magic/wrong-version sector and a corrupted entry load as zero
  tables with an identical render; a stale entry after a slot replacement is
  ignored; factory loads zero);
- the entry key follows project contents, not the record generation or device
  settings (a re-save or a settings write keeps the entry);
- the tuning journal in the shared staging sector: a committed journal is
  authoritative and the next write finishes the copy; an uncommitted or
  CRC-bad journal is ignored;
- a version-1 sector (generation + record CRC keyed) is still read and is
  converted to version 2 by a settings-only write;
- an over-size bank: saving nonzero tables returns `GROOVE_PROJECT_TUNING_SPACE`
  and leaves Flash byte-identical; zero tables still save; loads give zero;
- undo/redo and factory/slot loads carry the tables; the dirty hash includes them
  (a caller must `groove_project_touch()` after changing tables, because
  `groove_tuning_set` does not);
- SysEx project transfer with the optional tuning block (kind 3, 4264 B; kind 0
  stays 4064 B and a block-less import zeroes the tables; bad block CRC, magic
  or a -128 entry is rejected whole).

The test `#include`s `app/transfer.c` with its external symbols renamed, to
capture replies (the simulator's `fm1_midi_send` discards them) and drive the
real framing.

## Maqam test

`maqam_test` (run from the repository root: it calls `python3 tools/gen_scales.py --assign`) covers
`app/maqam/`: Rast on C under EQUAL and TRAD, all WEST scales zero, the key assignment of every
scale x 24 roots against the generator (and the tables derived from it, both intonations), alt
defaults, pushing through the state API and `groove_tuning_get`, the dirty flag, alt edits across an
intonation toggle, per-track override, V13 undo plus `maqam_refresh`, the 8-byte record trailer
(round trip, staging copy, settings-only rewrite, unchanged save writes nothing, record CRC and
fields as V13 writes them), stock projects without a trailer (defaults; tables kept and scale
derived), and table -> scale matching (Bayati on D recovered; hand-edited tables show CUSTOM and
stay unchanged), the packed scale record (29 bytes, `scale_cents` against the generator's exact
cents). EQUAL tables are shared by scales with the same slot mask, so a recovered label
there is the first scale in index order with that table.

## Keys test

`keys_test` (run from the repository root) drives the real `app/screen.c` through `sim_control`
(panel keys and buttons) and `sim_render` (time), and observes notes by recording them (REC, step
entry) and reading the sequence. It covers SNAP (C# to D up, D# to D down, 1 s reset, CHROM), the layout scopes and
that the root never moves keys, the layout default in the trailer, the OCT alt
gesture (measured E at the main versus alt pitch, recorded step with the alt bit and its playback),
double-tap octave (150 ms yes, slow no, key between no, clamp), drum tracks unchanged, and key lights.

## Chords test

`chords_test` (run from the repository root) covers chord mode (`app/maqam/chords.c`): the chord notes for WEST MIN
on A (all seven degrees), Rast TRIAD on C, the descending-only Bb key, DRN over A in Hüseyni, pentatonic TRIAD, snapping,
all eight types, inversions (3RD clamps), AUTO (C then F = C4 F4 A4), OPEN and BASS, and an exhaustive pass over all
73 scales x 24 roots x 7 types x every degree key (ascending, in range, on keys that carry a degree) and over every
inversion/voicing (at most 8 notes, in range). Through the real `screen.c` key loop it checks that a chord-mode key
records its chord on the step, that overlapping chords keep shared notes until the last holder releases (Goertzel
energy of the audio, and `maqam_chord_holders`), OCT alt on every chord note (`GROOVE_STEP_ALT`), the drum track
unaffected, and a full 16-slot chord table (edit error 2).

## Names test

`names_test` (run from the repository root) covers `app/maqam/names.c`: `Ed4` for E at -43, rounding to the
nearest quarter tone, `Cd5` for B4 +50, a zero offset equal to the stock 12-tone name for all 128 notes (the
stock formula is copied into the test), track names from the real tables (Rast TRAD: `Ed4`, `Bd4`; alt E with alt
0 reads `E4.` with the mark, plain E steps read `Ed4`; drum tracks keep stock names), and the real `screen.c`:
the step NOTE knob shows `Ed4` / `E4.`, and the step tiles differ for plain and alt steps and repaint when a table
changes. Scale-aware spelling (task 5.4): Bayati on D (`Bb4`, D Ed F G A Bb C), Saba `Gb`, Hijaz up and down,
Rast `Bd`/`Bb`, Major on A#/Bb and C#, pentatonic and WEST chromatic fall back to stock names, alt E at -100 reads
`Eb4`, and all 73 scales x 24 roots are either spelled with one letter and one accidental per degree or fall back
as a whole (the counts are printed).

## Pages test

`pages_test` (run from the repository root) drives the real `app/screen.c` scale pages through `sim_control`
and reads the result back with `groove_tuning_get` and the `maqam_*` getters: SCL opens the page on the last subpage and cycles
the subpages (ALGO wraps), HOME+SCL toggles with the stock scaling screen (level/rate SCL toggle kept), HOME leaves; page 1 (K1 root, K2 set, K3 scale, K4 tuning,
keys sound and leave the root alone (also with OCT held), subtitle `Ed arab sikah trad` with the arrow only for differing forms, the home
hint without moving the root); page 2 (override on/off, root, scale, CUSTOM kept, other tracks follow, drums
untouched); page 3 (project and track layouts); page 4 (key pick by panel key, which also sounds, +/-1 and +/-10 cents with clamp, the cents
readout and 24-tone name, reset, other tracks and drums untouched); a stored over-size sample bank, where a refused
save shows `REDUCE SAMPLE BANK` on the scale page and a good save clears it (also for a direct
`groove_project_save_slot` call, the SysEx path); the DX7-style frame (the header and hint rows hash identically across SCL cycling, the section row changes); undo (HOME+SAVE) re-derives the track labels from the restored tables; and painted pixels (title, subtitle,
home hint, alt readout; the subtitle repaints on a scale change). It writes 2x PNG screenshots of the real
framebuffer (`sim_pixels`) to `build/host/` (`scale_*.png`, `steps_alt_mark.png`, `project_reduce_bank.png`).

## Continuous integration

The `host-tests` job in `.github/workflows/firmware.yml` runs on macos-latest
and ubuntu-latest: `build.sh` (with `-Werror`), `pitch-test`, every
`build/host/*_test`, and `check_golden.sh --determinism` (two renders compared).
Golden hashes depend on compiler and CPU, so `check_golden.sh` against the
committed hashes runs only on macos-latest, with `continue-on-error`; treat a
mismatch there as a prompt to investigate, and re-record only on purpose with
`--record` on the machine that produced the change. Ubuntu is not compared with
the committed hashes.

The degree row and the spelled alt strip (task 5.5) are covered by `pages_test` too: the Bayati row cells, cents and
marks, the Rast row's `Bb` down and `Bd` up, one cell per degree for all 73 scales, the Bayati alt strip, and the
PNGs `scale_row_bayati`, `scale_row_rast`, `scale_alt_bayati` and `steps_bb4` in `build/host/`.

## Scale page screenshots

`tests/host/scale_shots.c` renders edge cases of the scale pages (Rast up/down, Hijaz, Sikah on Ed, Suznak's
9 degrees, pentatonic, chromatic, Bb major, a fallback root, quarter-tone roots, PSYCH, JINS, own-scale track,
CUSTOM, drums, TRACK page with FOLLOW/OWN layouts, alt extremes, REDUCE SAMPLE BANK, the CHORD page (Rast TRIAD, 7TH AUTO, DRN over A in Hüseyni), the stock DX7 scaling and curves screens for comparison) to `build/host/shots/*.png`. It is not a test, so
`build.sh` skips it; build it with the same command as a test, adding `tests/host/scale_shots.c` and
`-I tests/host`, then run `build/host/scale_shots` from the repository root.
