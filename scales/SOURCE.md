# Scale files source

`west.txt` and `arab.txt` are owned by this app and edited here. There is no firmware sync and no frozen index:
a scale's index is its position in load order (`west.txt`, then `arab.txt`, then the custom set).

## Origin

The files began as a copy of the Maqam FM firmware scales.

- Repository: `vikramrojo/maqam-fm1`
- Branch: `vikramrojo/feat-maqam-scales`
- Commit: `b7519b16e76b7e22f847e096b88a433b86b92066`
- Files: `scales/west.txt turk.txt arab.txt india.txt psych.txt jins.txt` (73 scales)

## Changes since the origin commit

- **Removed sets.** `turk.txt` and `india.txt` are gone. Turkish and Indian names survive only as aliases (below);
  the four ragas with no Western or Arabic home (Todi, Purvi, Marwa, Malkauns) are dropped. `psych.txt` (16) and
  `jins.txt` (5) are the maintainer's personal scales: they live outside the bundle as one importable custom set,
  `docs/custom-scales/psych-jins.txt`, unchanged apart from a header comment.
- **Grammar.** An `alias "Name"` line (any number per scale); names and aliases up to 24 UTF-8 bytes (the 8-byte ASCII
  limit was the firmware display's); codes stay at 5 characters. All build rules are unchanged.
- **west.txt.** Added `PHDOM` "Phrygian dominant" (12-TET Hijaz: C D♭ E F G A♭ B♭). Aliases: Bilawal (MAJ), Kafi (DOR),
  Bhairavi (PHRY), Asavari (MIN), Yaman (LYD), Khamaj (MIX), Bhupali (PEN), Hicaz and Hijaz 12-TET (PHDOM).
- **arab.txt.**
  - **Ajam's fifth.** Ajam is defined from Rast's `ajam` note (883/490); with the plain chahargah 4/3 its fifth was
    678 c. The fifth is now `ajam × 3/2` (2649/1960), 702 c. Its other degrees are unchanged.
  - **Seven maqamat added** (degree structure from maqamworld.com's maqam pages; intonation from this file's own
    al-Sabbagh ratios, with a half-flat that the list lacks written as the pure fourth or fifth above a named one;
    each derivation is in a comment in the file): Nairuz, Mahur, Husayni, Bayati Shuri, 'Iraq, Bastanikar, Jiharkah.
  - **Aliases** (names only, never codes): Uşşak (BAYAT), Hüseyni (HUSNI), Rast (Turkish) (MAHUR), Karcığar (BSHRI),
    Segah (SIKAH), Hüzzam (HUZAM), Nihavend (NAHAW), Kürdi (KURD), Hicazkar and Bhairav (HJKAR), Suzinak (SUZNK).

## Sources for the additions and aliases

- maqamworld.com maqam pages: Nairuz, Mahur, Husayni, Bayati Shuri, 'Iraq, Bastanikar (page text: "effectively Jins
  Sikah followed by Maqam Saba"; its map runs two octaves, so BSTNK is the one-octave form), Jiharkah (page text:
  Jins Jiharkah on the tonic and Jins Upper Rast on the 5th; the page does not name the tonic).
- Second sources, checked for each new maqam's family and structure: Wikipedia, "Arabic maqam" (Nairuz and Mahur in
  the Rast family, Husayni and Bayati Shuri in the Bayati family, 'Iraq in the Sikah family, Jiharkah in the 'Ajam
  family); Wikipedia, "Bayati (maqam)" (Bayati Shuri: emphasis on Jins Hijaz on the 4th degree above the Bayati
  tonic); pianoscales.org/maqams.html and tuning.ableton.com/arabic-maqam/jiharkah (Jiharkah on F: F G A B♭ C D, with the
  7th written E♭ in 12-TET). maqamlessons.com (Marcus/Touma-based) and the Leiden "Inside Arabic Music" text carry no
  per-maqam note lists for the others, so their notes rest on maqamworld plus the family and jins structure above.
- **Jiharkah's tonic is F** (Jins Jiharkah F G A B♭, upper Rast on C: C D E𝄳 F). Both second sources give F; none gives
  E half-flat. The 7th is E half-flat per maqamworld's map and the Rast jins on C; 12-TET sources write E♭.
- **Turkish placements by performance** (Uşşak, Hüseyni, Rast (Turkish), and Karcığar by inference): Bozkurt, Yarman,
  Karaosmanoğlu & Akkoç, "Weighing Diverse Theoretical Models on Turkish Maqam Music Against Pitch Measurements",
  Journal of New Music Research 38(1), 2009. Their measurements of 128 master recordings put Uşşak's 2nd at 130-142 c,
  Hüseyni's at 165 c and Rast's segah at 365 c, near the Arabic neutral positions, so those makams alias Bayati,
  Husayni and Mahur rather than Minor, Dorian and Major. Karcığar has no measurements: it aliases Bayati Shuri by its
  ajnas (Bayati + Hijaz on the 4th). Segah and Hüzzam alias Sikah and Huzam (same ajnas; AEU's Segah measures
  118 / 320 c, between the two grids). Nihavend, Kürdi and Hicazkar alias scales with the same notes. Suzinak aliases
  Suznak by its ajnas (Rast + Hijaz on the 5th): AEU's 3rd is 384 c, a major third, where Suznak's is the neutral
  segah (357 c), as measured for Turkish Rast, whose lower tetrachord Suzinak shares.
- Indian aliases go by thaat (Bilawal, Kafi, Bhairavi, Asavari, Khamaj) or by the scale's usual Western reading
  (Yaman as Lydian, Bhupali as major pentatonic, Bhairav as Hijazkar).
