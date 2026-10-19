/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Chord mode (tasks 1.1, 1.2, 2.1, 2.2 of maqam-chord-mode): the chord notes app/maqam/chords.c stacks from the
 * scale (types, snapping, descending-only degrees, inversions, AUTO, voicing), and the real app/screen.c key loop:
 * a chord-mode key posts the chord, overlapping chords keep shared notes until the last holder lets go (observed
 * in the audio and through maqam_chord_holders), OCT alt covers every chord note, drum tracks are unaffected, and
 * recording stores the chord on the step (a full 16-slot table gives the stock edit error 2).
 * Run from the repository root. */
#define TUNING_NO_MAIN
#include "tuning_test.c"
#include "../../app/maqam/maqam.h"
#include "../../app/maqam/scales.h"

#ifndef GROOVE_STEP_ALT
#define GROOVE_STEP_ALT (1U << 13)
#endif
void sim_control(unsigned kind, unsigned index, int value);
int sim_select(unsigned track, unsigned source);
enum { OCT_DN = 0, REC = 13 };

static unsigned find(const char *code)
{
    for (unsigned i = 0; i < NSCALES; ++i)
        if (!strcmp(SCALE_CODE_STR[i], code)) return i;
    printf("FAIL no scale %s\n", code);
    ++failures;
    return 0;
}
static void ms(unsigned t) { for (unsigned n = 0; n < t * 44100 / 1000 / FM1_BLOCK_FRAMES + 1; ++n) sim_render(); }
static void boot(const char *scale, unsigned root)
{
    sim_boot(1);
    maqam_init();
    CHECK(maqam_set_project(find(scale), root) == 0);
    ms(50);
}
static void conf(unsigned type, unsigned inv, unsigned voice)
{
    CHECK(maqam_chord_set(0, MAQAM_CH_MODE, 1) == 0 && maqam_chord_set(0, MAQAM_CH_TYPE, type) == 0);
    CHECK(maqam_chord_set(0, MAQAM_CH_INV, inv) == 0 && maqam_chord_set(0, MAQAM_CH_VOICE, voice) == 0);
}
/* The chord of the played MIDI note on track 0 as text "60 64 67". */
static const char *chord(unsigned note, unsigned desc)
{
    static char text[64];
    uint8_t n[8];
    unsigned c = maqam_chord_notes(0, note, desc, n), len = 0;
    text[0] = 0;
    for (unsigned i = 0; i < c; ++i) len += (unsigned)snprintf(text + len, sizeof text - len, i ? " %u" : "%u", n[i]);
    return text;
}
#define SAYS(note, desc, want) do { const char *g_ = chord(note, desc); \
    if (strcmp(g_, want)) { printf("FAIL %s:%d: note %u: '%s' want '%s'\n", __FILE__, __LINE__, (unsigned)(note), g_, want); ++failures; } } while (0)

static void types(void)
{
    /* natural minor on A: the chord on each degree (min dim maj min min maj maj) */
    boot("MIN", 18);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    SAYS(69, 0, "69 72 76");   /* A C E */
    SAYS(71, 0, "71 74 77");   /* B D F */
    SAYS(72, 0, "72 76 79");   /* C E G */
    SAYS(74, 0, "74 77 81");   /* D F A */
    SAYS(76, 0, "76 79 83");   /* E G B */
    SAYS(77, 0, "77 81 84");   /* F A C */
    SAYS(79, 0, "79 83 86");   /* G B D */
    /* Rast on C: C, the E key (Ed), G */
    boot("RAST", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    SAYS(60, 0, "60 64 67");
    int8_t main[GROOVE_TUNING_KEYS];
    CHECK(maqam_main(find("RAST"), 0, 1, main) == 0 && main[4] != 0);              /* the E key is the quarter tone */
    SAYS(70, 0, "70 74 77");   /* the A# key is B-flat, descending only: Bb D F (the descending form) */
    SAYS(70, 1, "70 74 77");
    /* the other types on C (Rast): 7TH, SUS2, SUS4, 5TH, 5+8 */
    conf(MAQAM_CHORD_7TH, 0, 0);  SAYS(60, 0, "60 64 67 71");
    conf(MAQAM_CHORD_SUS2, 0, 0); SAYS(60, 0, "60 62 67");
    conf(MAQAM_CHORD_SUS4, 0, 0); SAYS(60, 0, "60 65 67");
    conf(MAQAM_CHORD_5TH, 0, 0);  SAYS(60, 0, "60 67");
    conf(MAQAM_CHORD_5P8, 0, 0);  SAYS(60, 0, "60 67 72");
    /* pentatonic C D E G A: degrees 1 3 5 are C E A */
    boot("PEN", 0);
    conf(MAQAM_CHORD_TRIAD, 0, 0);
    SAYS(60, 0, "60 64 69");
    /* snapping: C# in C major plays the D chord going up, the C chord going down */
    boot("MAJ", 0);
    conf(MAQAM_CHORD_TRIAD, 0, 0);
    SAYS(61, 0, "62 65 69");
    SAYS(61, 1, "60 64 67");
    /* no chords for a drum track, a CUSTOM table or a chromatic scale */
    boot("CHR", 0);
    CHECK(maqam_chord_notes(0, 60, 0, (uint8_t[8]){0}) == 0);
    boot("MAJ", 0);
    CHECK(sim_select(0, GROOVE_808) == 0 && maqam_chord_notes(0, 60, 0, (uint8_t[8]){0}) == 0);
    CHECK(maqam_chord_set(0, MAQAM_CH_TYPE, 8) < 0 && maqam_chord_set(0, MAQAM_CH_INV, 5) < 0 &&
          maqam_chord_set(0, MAQAM_CH_VOICE, 3) < 0 && maqam_chord_set(0, MAQAM_CH_MODE, 2) < 0 &&
          maqam_chord_set(8, MAQAM_CH_MODE, 1) < 0 && maqam_chord_set(0, 4, 0) < 0);
    printf("types: WEST MIN degrees, Rast neutral triad, descending-only Bb, pentatonic, snapping\n");
}

/* Every scale x root x type x degree key: ascending, in range, each note on a key that carries a degree. */
/* Every scale and root, DRN and DRN5, every pedal placement and voicing, on every degree key: ascending, in range, at
   most 8, the pedal on the tonic's assigned key, the played note last, DRN5's fifth between them. */
static void drones_all(void)
{
    unsigned n_ok = 0;
    for (unsigned s = 0; s < NSCALES; ++s) {
        CHECK(__builtin_popcount(scale_up(&SCALES[s])) >= 5);   /* DRN5's degree 5 exists */
        for (unsigned r = 0; r < 24; ++r) {
            if (maqam_set_project(s, r) < 0) continue;
            unsigned sc, rt;
            const int8_t *key = maqam_keys_assignment(0, &sc, &rt);
            if (!key) continue;
            int tonic = key[0];
            for (unsigned type = MAQAM_CHORD_DRN; type < MAQAM_CHORD_TYPES; ++type)
                for (unsigned inv = 0; inv <= MAQAM_INV_AUTO; ++inv)
                    for (unsigned v = 0; v < 3; v += 2) {
                        conf(type, inv, v);   /* no reset between cases: a root change forgets the pedal itself */
                        for (unsigned slot = 0; slot < 24; ++slot) {
                            if (key[slot] < 0) continue;
                            for (unsigned oct = 0; oct < 2; ++oct) {
                                unsigned note = (oct ? 84 : 36) + (unsigned)key[slot];
                                uint8_t n[8];
                                unsigned c = maqam_chord_notes(0, note, 0, n);
                                maqam_chord_commit(0, n, c);
                                unsigned want = (type == MAQAM_CHORD_DRN5 ? 3U : 2U) + (v == MAQAM_VOICE_BASS);
                                unsigned has = 0, ped = 0;
                                int bad = c > want || c + 1 < want || c < 2;   /* a duplicate (the fifth, or the BASS, on the played note) is dropped */
                                for (unsigned i = 0; i < c && !bad; ++i) {
                                    has |= n[i] == note;
                                    ped += n[i] % 12 == (unsigned)tonic;
                                    bad |= n[i] > 127 || (i && n[i] <= n[i - 1]);
                                }
                                bad |= !has || !ped || (inv < 2 && n[c - 1] != note && type == MAQAM_CHORD_DRN);   /* the pedal is on the tonic key */
                                if (bad) {
                                    if (failures < 10) printf("FAIL drone scale %s root %u type %u inv %u slot %u: %s\n", SCALE_CODE_STR[s], r, type, inv, slot, chord(note, 0));
                                    ++failures;
                                }
                                ++n_ok;
                            }
                        }
                    }
        }
    }
    CHECK(n_ok > 100000);
    printf("exhaustive drones: %u chords, every scale and root, DRN/DRN5, ROOT..AUTO, CLOSE/BASS\n", n_ok);
}

static void exhaustive(void)
{
    unsigned combos = 0, chords = 0, none = 0, skipped = 0;
    sim_boot(1);
    maqam_init();
    for (unsigned s = 0; s < NSCALES; ++s)
        for (unsigned r = 0; r < 24; ++r) {
            if (maqam_set_project(s, r) < 0) { ++skipped; continue; }
            unsigned sc, rt;
            const int8_t *key = maqam_keys_assignment(0, &sc, &rt);
            int8_t k[24];
            if (key) memcpy(k, key, 24);
            ++combos;
            for (unsigned type = 0; type < MAQAM_CHORD_DRN; ++type) {
                maqam_chord_set(0, MAQAM_CH_TYPE, type);
                unsigned want = (unsigned[]){3, 4, 3, 3, 2, 3}[type];
                for (unsigned slot = 0; slot < 24; ++slot) {
                    if (!key) {
                        none += slot == 0;
                        uint8_t n[8];
                        if (!maqam_chord_notes(0, 60 + slot % 12, 0, n)) continue;
                        printf("FAIL scale %u: chord on a track with no degrees\n", s); ++failures;
                        continue;
                    }
                    if (k[slot] < 0) continue;
                    unsigned note = 60 + (unsigned)k[slot];
                    uint8_t n[8];
                    unsigned c = maqam_chord_notes(0, note, 0, n);
                    ++chords;
                    int bad = c != want || n[0] != note;
                    for (unsigned i = 0; i < c && !bad; ++i) {
                        unsigned has = 0;
                        for (unsigned q = 0; q < 24; ++q) has |= k[q] == (int)(n[i] % 12U);
                        bad |= n[i] > 127 || !has || (i && n[i] <= n[i - 1]);
                    }
                    if (bad) {
                        if (failures < 10) printf("FAIL scale %s root %u type %u slot %u: %s\n", SCALE_CODE_STR[s], r, type, slot, chord(note, 0));
                        ++failures;
                    }
                }
            }
        }
    CHECK(combos > 1500);
    drones_all();
    printf("exhaustive: %u scale/root pairs (%u skipped, %u without degrees), %u chords ascending, in range, on degree keys\n",
           combos, skipped, none, chords);
}

/* The drone scenarios of the chord-mode spec (tasks 5.1). Hüseyni on A: the tonic A, the fifth E, the B key (B-half-flat). */
static void drones(void)
{
    boot("P.HUS", 18);
    conf(MAQAM_CHORD_DRN, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    SAYS(59, 0, "45 59");                          /* A2 under B-half-flat 3 */
    conf(MAQAM_CHORD_DRN5, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    SAYS(59, 0, "45 52 59");                       /* A2 E3 B-half-flat 3 */
    conf(MAQAM_CHORD_DRN, MAQAM_INV_1ST, MAQAM_VOICE_CLOSE);
    SAYS(59, 0, "57 59");                          /* the tight drone */
    conf(MAQAM_CHORD_DRN, MAQAM_INV_2ND, MAQAM_VOICE_CLOSE);
    SAYS(59, 0, "59 69");                          /* the high drone */
    conf(MAQAM_CHORD_DRN5, MAQAM_INV_3RD, MAQAM_VOICE_OPEN);
    SAYS(59, 0, "59 69 76");                       /* OPEN has no effect; the fifth sits above the pedal */
    conf(MAQAM_CHORD_DRN, MAQAM_INV_1ST, MAQAM_VOICE_CLOSE);
    SAYS(57, 0, "45 57");                          /* the tonic itself: 1ST puts the pedal an octave below */
    conf(MAQAM_CHORD_DRN, MAQAM_INV_2ND, MAQAM_VOICE_CLOSE);
    SAYS(57, 0, "57 69");                          /* and 2ND an octave above */
    /* AUTO keeps the previous pedal while it stays below the note; the first drone chord is ROOT */
    boot("MAJ", 0);
    conf(MAQAM_CHORD_DRN, MAQAM_INV_AUTO, MAQAM_VOICE_CLOSE);
    uint8_t c[8];
    unsigned n = maqam_chord_notes(0, 60, 0, c);
    CHECK(n == 2 && c[0] == 48 && c[1] == 60);
    maqam_chord_commit(0, c, n);
    n = maqam_chord_notes(0, 72, 0, c);
    CHECK(n == 2 && c[0] == 48 && c[1] == 72);     /* C3 again, not C4 */
    maqam_chord_commit(0, c, n);
    n = maqam_chord_notes(0, 48, 0, c);            /* a note the pedal no longer sits below: ROOT again */
    CHECK(n == 2 && c[0] == 36 && c[1] == 48);
    /* BASS adds the pedal an octave lower */
    conf(MAQAM_CHORD_DRN, MAQAM_INV_ROOT, MAQAM_VOICE_BASS);
    SAYS(64, 0, "36 48 64");
    /* a new root, a new pedal: the previous drone chord is forgotten */
    boot("MAJ", 0);
    conf(MAQAM_CHORD_DRN, MAQAM_INV_AUTO, MAQAM_VOICE_CLOSE);
    n = maqam_chord_notes(0, 60, 0, c);
    CHECK(n == 2 && c[0] == 48 && c[1] == 60);
    maqam_chord_commit(0, c, n);
    CHECK(maqam_set_project(find("MAJ"), 4) == 0);   /* the root becomes D (quarter tone 4) */
    n = maqam_chord_notes(0, 64, 0, c);
    CHECK(n == 2 && c[0] == 50 && c[1] == 64);       /* E4 over D3, not over the stale C3 */
    /* a track's own scale or root forgets it too */
    boot("MAJ", 0);
    conf(MAQAM_CHORD_DRN, MAQAM_INV_AUTO, MAQAM_VOICE_CLOSE);
    n = maqam_chord_notes(0, 60, 0, c);
    maqam_chord_commit(0, c, n);
    CHECK(maqam_set_override(0, find("MAJ"), 14) == 0);   /* G */
    n = maqam_chord_notes(0, 72, 0, c);
    CHECK(n == 2 && c[0] == 55 && c[1] == 72);            /* G3, not the stale C3 */
    printf("drones: DRN/DRN5 over A in Huseyni, ROOT/1ST/2ND placement, AUTO steady pedal, BASS, OPEN ignored\n");
}

/* Inversions, AUTO, voicing and their limits. */
static void voicing(void)
{
    boot("MAJ", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_1ST, MAQAM_VOICE_CLOSE);
    SAYS(60, 0, "64 67 72");                       /* E G C */
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_2ND, MAQAM_VOICE_CLOSE);
    SAYS(60, 0, "67 72 76");
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_3RD, MAQAM_VOICE_CLOSE);
    SAYS(60, 0, "67 72 76");                       /* a triad clamps 3RD to its highest inversion */
    conf(MAQAM_CHORD_7TH, MAQAM_INV_3RD, MAQAM_VOICE_CLOSE);
    SAYS(60, 0, "71 72 76 79");
    conf(MAQAM_CHORD_5TH, MAQAM_INV_3RD, MAQAM_VOICE_CLOSE);
    SAYS(60, 0, "67 72");
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_BASS);
    SAYS(60, 0, "48 60 64 67");
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_OPEN);
    SAYS(60, 0, "60 67 76");
    conf(MAQAM_CHORD_7TH, MAQAM_INV_ROOT, MAQAM_VOICE_OPEN);
    SAYS(60, 0, "60 67 71 76");
    conf(MAQAM_CHORD_5TH, MAQAM_INV_ROOT, MAQAM_VOICE_OPEN);
    SAYS(60, 0, "60 67");                          /* OPEN needs 3 notes */
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_1ST, MAQAM_VOICE_BASS);
    SAYS(60, 0, "52 64 67 72");                    /* the inversion first, the bass below its lowest note */
    /* out of range notes are dropped, never wrapped */
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_BASS);
    SAYS(12, 0, "0 12 16 19");
    SAYS(0, 0, "0 4 7");
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    SAYS(124, 0, "124 127");                       /* E G B: the B above 127 is dropped */
    /* AUTO on a stacked chord acts as ROOT: no voice leading */
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_AUTO, MAQAM_VOICE_CLOSE);
    uint8_t c[8];
    unsigned n = maqam_chord_notes(0, 60, 0, c);
    CHECK(n == 3 && c[0] == 60 && c[1] == 64 && c[2] == 67);
    maqam_chord_commit(0, c, n);
    SAYS(65, 0, "65 69 72");                       /* the F chord in root position: F A C */
    n = maqam_chord_notes(0, 65, 0, c);
    maqam_chord_commit(0, c, n);
    SAYS(67, 0, "67 71 74");                       /* G B D, whatever came before */
    conf(MAQAM_CHORD_7TH, MAQAM_INV_AUTO, MAQAM_VOICE_BASS);
    SAYS(65, 0, "53 65 69 72 76");                 /* and the voicing still applies */
    /* never more than 8 notes, in range, ascending, for every setting on every scale */
    unsigned bad = 0, total = 0;
    for (unsigned s = 0; s < NSCALES; ++s) {
        sim_boot(1);
        maqam_init();
        if (maqam_set_project(s, (s * 7) % 24) < 0) continue;
        for (unsigned type = 0; type < MAQAM_CHORD_TYPES; ++type)
            for (unsigned inv = 0; inv < 5; ++inv)
                for (unsigned v = 0; v < 3; ++v) {
                    conf(type, inv, v);
                    for (unsigned note = 0; note < 128; note += 5) {
                        uint8_t o[8];
                        unsigned m = maqam_chord_notes(0, note, note & 1, o);
                        maqam_chord_commit(0, o, m);
                        ++total;
                        for (unsigned i = 0; i < m; ++i) bad += o[i] > 127 || (i && o[i] <= o[i - 1]);
                        bad += m > 8;
                    }
                }
    }
    CHECK(bad == 0 && total > 1000);
    printf("voicing: 1ST/2ND/3RD, AUTO on a stacked chord = ROOT, BASS, OPEN, clamps, <=8 notes in range (%u chords)\n", total);
}

/* ---- keys: the real screen.c ---- */
static void rec_on(void) { sim_control(0, REC, 1); sim_control(0, REC, 0); ms(30); }
static void key(unsigned k, unsigned hold) { sim_control(1, k, 1); ms(hold); sim_control(1, k, 0); ms(20); }
/* The notes of a recorded step as "60 64 67" (root, then the chord table's offsets). */
static const char *step_chord(unsigned track, unsigned step)
{
    static char text[64];
    struct groove_step c;
    struct groove_sequence_options o;
    unsigned len = 0, id;
    groove_sequence_read(track, step, &c, 1);
    groove_sequence_options_copy(&o);
    id = c.drums & 31U;
    len = (unsigned)snprintf(text, sizeof text, "%u", c.note);
    for (unsigned i = 1; id && i < o.chord[id - 1][0]; ++i)
        len += (unsigned)snprintf(text + len, sizeof text - len, " %u", c.note + o.chord[id - 1][i]);
    return text;
}
#define STEP(t, s, want) do { const char *g_ = step_chord(t, s); \
    if (strcmp(g_, want)) { printf("FAIL %s:%d: step %u: '%s' want '%s'\n", __FILE__, __LINE__, (unsigned)(s), g_, want); ++failures; } } while (0)

/* Energy of one note's frequency in the last `blocks` rendered blocks (Goertzel, Hann window). */
static double energy(double note, unsigned blocks)
{
    unsigned n = blocks * FM1_BLOCK_FRAMES;
    const float *w = pcm + (filled - blocks) * FM1_BLOCK_FRAMES;
    double k = 2 * cos(2 * M_PI * note_hz(note) / RATE), s1 = 0, s2 = 0;
    for (unsigned i = 0; i < n; ++i) {
        double s = w[i] * (0.5 - 0.5 * cos(2 * M_PI * i / (n - 1))) + k * s1 - s2;
        s2 = s1; s1 = s;
    }
    return (s1 * s1 + s2 * s2 - k * s1 * s2) / ((double)n * n);
}
static void listen(unsigned blocks) { filled = 0; run(blocks); }

static void sounding(void)
{
    boot("MAJ", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    rec_on();
    key(7, 40);                                    /* C4 key: C E G */
    STEP(0, 0, "60 64 67");
    key(12, 40);                                   /* F4 key: F A C (chord 2 steps along) */
    STEP(0, 1, "65 69 72");
    /* chord mode off: one note */
    maqam_chord_set(0, MAQAM_CH_MODE, 0);
    key(7, 40);
    STEP(0, 2, "60");
    maqam_chord_set(0, MAQAM_CH_MODE, 1);
    /* the key owns the chord while held, and lets go of it */
    CHECK(!maqam_chord_owned(7));
    sim_control(1, 7, 1); ms(40);
    CHECK(maqam_chord_owned(7) && maqam_chord_holders(0, 60, 27) == 1 && maqam_chord_holders(0, 64, 27) == 1 &&
          maqam_chord_holders(0, 67, 27) == 1 && maqam_chord_holders(0, 62, 27) == 0);
    uint8_t last[8];
    CHECK(maqam_chord_last(0, last) == 3 && last[0] == 60 && last[2] == 67);
    sim_control(1, 7, 0); ms(40);
    CHECK(!maqam_chord_owned(7) && maqam_chord_holders(0, 60, 27) == 0);
    /* audio: all three notes sound while held, and are silent after the release */
    boot("MAJ", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    sim_control(1, 7, 1); ms(300);
    listen(120);
    double c4 = energy(60, 120), e4 = energy(64, 120), g4 = energy(67, 120), d4 = energy(62, 120);
    CHECK(c4 > 20 * d4 && e4 > 20 * d4 && g4 > 20 * d4);
    sim_control(1, 7, 0); ms(2500);
    listen(120);
    CHECK(energy(60, 120) < 1e-3 * c4 && energy(67, 120) < 1e-3 * g4);
    printf("sounding: key plays C E G / F A C, chord mode off plays one note, owned while held, silent after\n");
}

/* V14 routing: chord notes follow the track's MIDI input channel; an OFF track plays nothing. */
static void routing(void)
{
    boot("MAJ", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    CHECK(groove_midi_input_set(0, 4) == 0 && groove_midi_input_channel(0) == 4);
    sim_control(1, 7, 1); ms(300);
    listen(120);
    double c4 = energy(60, 120), e4 = energy(64, 120), g4 = energy(67, 120), d4 = energy(62, 120);
    CHECK(c4 > 20 * d4 && e4 > 20 * d4 && g4 > 20 * d4);
    sim_control(1, 7, 0); ms(2500);
    listen(120);
    CHECK(energy(60, 120) < 1e-3 * c4 && energy(67, 120) < 1e-3 * g4);
    boot("MAJ", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    CHECK(groove_track_role_set(0, GROOVE_TRACK_OFF) == 0);
    sim_control(1, 7, 1); ms(300);
    listen(120);
    CHECK(energy(60, 120) < 1e-6 && energy(64, 120) < 1e-6);
    sim_control(1, 7, 0); ms(40);
    printf("routing: chord on a remapped input channel sounds and releases; an OFF track is silent\n");
}
static void overlap(void)
{
    /* A3 key (key 4) = A C E (57 60 64), C4 key (key 7) = C E G (60 64 67): C and E are shared */
    boot("MAJ", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    sim_control(1, 7, 1); ms(100);
    sim_control(1, 4, 1); ms(300);
    CHECK(maqam_chord_holders(0, 60, 27) == 2 && maqam_chord_holders(0, 64, 27) == 2 &&
          maqam_chord_holders(0, 67, 27) == 1 && maqam_chord_holders(0, 57, 27) == 1);
    listen(120);
    double a3 = energy(57, 120), c4 = energy(60, 120), e4 = energy(64, 120), g4 = energy(67, 120), f4 = energy(65, 120);
    CHECK(a3 > 20 * f4 && c4 > 20 * f4 && e4 > 20 * f4 && g4 > 20 * f4);
    sim_control(1, 7, 0); ms(300);                 /* the C chord goes: G stops, A C E keep sounding */
    CHECK(maqam_chord_holders(0, 60, 27) == 1 && maqam_chord_holders(0, 64, 27) == 1 &&
          maqam_chord_holders(0, 67, 27) == 0 && maqam_chord_holders(0, 57, 27) == 1);
    listen(120);
    CHECK(energy(67, 120) < 1e-3 * g4);
    CHECK(energy(57, 120) > 0.02 * a3 && energy(60, 120) > 0.02 * c4 && energy(64, 120) > 0.02 * e4);
    sim_control(1, 4, 0); ms(2500);                /* the last holder goes: all silent */
    listen(120);
    CHECK(energy(57, 120) < 1e-3 * a3 && energy(60, 120) < 1e-3 * c4 && energy(64, 120) < 1e-3 * e4);
    CHECK(maqam_chord_holders(0, 60, 27) == 0);
    /* the same with the A chord released first */
    sim_control(1, 7, 1); ms(100);
    sim_control(1, 4, 1); ms(100);
    sim_control(1, 4, 0); ms(300);
    CHECK(maqam_chord_holders(0, 60, 27) == 1 && maqam_chord_holders(0, 57, 27) == 0);
    listen(120);
    CHECK(energy(60, 120) > 0.02 * c4 && energy(67, 120) > 0.02 * g4 && energy(57, 120) < 1e-3 * a3);
    sim_control(1, 7, 0); ms(100);
    printf("overlap: shared C and E held by two chords until the last release (audio and holders)\n");
}

static void alt_and_drums(void)
{
    boot("RAST", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    rec_on();
    sim_control(0, OCT_DN, 1); ms(20);
    CHECK(groove_alt_held(0));
    key(7, 40);
    sim_control(0, OCT_DN, 0); ms(20);
    key(7, 40);
    struct groove_step c;
    groove_sequence_read(0, 0, &c, 1);
    CHECK((c.drums & GROOVE_STEP_ALT) && (c.drums & 31U));
    STEP(0, 0, "60 64 67");
    groove_sequence_read(0, 1, &c, 1);
    CHECK(!(c.drums & GROOVE_STEP_ALT) && (c.drums & 31U));
    /* a chord started under alt keeps its alt notes when OCT is let go before the key (the note-offs need no alt) */
    boot("RAST", 0);
    conf(MAQAM_CHORD_TRIAD, 0, 0);
    sim_control(0, OCT_DN, 1); ms(20);
    sim_control(1, 7, 1); ms(40);
    sim_control(0, OCT_DN, 0); ms(20);
    CHECK(maqam_chord_owned(7) && maqam_chord_holders(0, 64, 27) == 1);
    sim_control(1, 7, 0); ms(40);
    CHECK(maqam_chord_holders(0, 64, 27) == 0);
    /* drum track: no chord, stock single note, owned by nobody */
    boot("MAJ", 0);
    conf(MAQAM_CHORD_TRIAD, 0, 0);
    CHECK(sim_select(0, GROOVE_808) == 0);
    sim_control(1, 7, 1); ms(40);
    CHECK(!maqam_chord_owned(7) && maqam_chord_holders(0, 60, 27) == 0);
    sim_control(1, 7, 0); ms(40);
    printf("OCT alt: whole chord recorded with GROOVE_STEP_ALT; drum track plays a single stock note\n");
}

static void recording(void)
{
    boot("RAST", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    rec_on();
    key(7, 40);
    STEP(0, 0, "60 64 67");
    struct groove_step c;
    struct groove_sequence_options o;
    groove_sequence_read(0, 0, &c, 1);
    groove_sequence_options_copy(&o);
    CHECK((c.drums & 31U) && o.chord[(c.drums & 31U) - 1][0] == 3 && o.chord[(c.drums & 31U) - 1][1] == 4 &&
          o.chord[(c.drums & 31U) - 1][2] == 7);
    CHECK(groove_edit_error() == 0);
    key(7, 40);                                    /* the same chord again: the shape is shared, no new slot */
    STEP(0, 1, "60 64 67");
    CHECK((c.drums & 31U) == (unsigned)(groove_sequence_read(0, 1, &c, 1), c.drums & 31U));
    /* the whole chord on playback: the sequencer plays root + offsets of the stored shape */
    CHECK(c.note == 60);
    /* a full table: 16 other steps hold 16 distinct shapes, so a 17th shape cannot be stored */
    boot("RAST", 0);
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    groove_sequence_length_set(0, 32);
    unsigned used = 0;
    for (unsigned a = 1; a < 8 && used < 16; ++a)
        for (unsigned b = a + 1; b < 12 && used < 16; ++b) {
            if (a == 4 && b == 7) continue;
            uint8_t notes[3] = {60, (uint8_t)(60 + a), (uint8_t)(60 + b)};
            CHECK(groove_sequence_chord(0, 1 + used, notes, 3, 100) == 0);
            ++used;
        }
    CHECK(used == 16);
    uint8_t keep[3] = {60, 61, 62};                /* step 0 holds the shape of step 1: nothing of its own to reuse */
    CHECK(groove_sequence_chord(0, 0, keep, 3, 100) == 0);
    STEP(0, 0, "60 61 62");
    groove_edit_record(0, 0, 0);
    rec_on();
    CHECK(groove_edit_error() == 0);
    key(7, 40);
    CHECK(groove_edit_error() == 2);               /* the stock "CHORD LIMIT - LAST NOTES KEPT" */
    STEP(0, 0, "60");                              /* what the engine accepted before the table ran out */
    for (unsigned s = 1; s <= 16; ++s) CHECK(strlen(step_chord(0, s)) > 5);   /* the other steps keep their chords */
    printf("recording: C E G stored as one chord step (root + offsets 4 7); full table gives edit error 2\n");
}

/* Interval names (task 3.1): from the tuned pitches, 50 c steps from the lowest note. */
static const char *named(unsigned alt, unsigned n, const uint8_t *notes)
{
    static char out[12];
    maqam_chord_name(0, alt, notes, n, out);
    return out;
}
#define NAMED(alt, want, ...) do { const uint8_t nn_[] = {__VA_ARGS__}; const char *g_ = named(alt, sizeof nn_, nn_); \
    if (strcmp(g_, want)) { printf("FAIL %s:%d: '%s' want '%s'\n", __FILE__, __LINE__, g_, want); ++failures; } } while (0)

static void names(void)
{
    /* the same semitone stack C E G: neutral in Rast, major in WEST */
    boot("RAST", 0);
    NAMED(0, "NEU", 60, 64, 67);
    boot("MAJ", 0);
    NAMED(0, "MAJ", 60, 64, 67);
    NAMED(0, "MIN-", 59, 62, 65);              /* B D F */
    NAMED(0, "MAJ", 64, 67, 72);               /* E G C: an inversion keeps its name */
    NAMED(0, "MIN", 60, 63, 67);
    NAMED(0, "MAJ+", 60, 64, 68);
    NAMED(0, "MAJ7", 60, 64, 67, 71);
    NAMED(0, "MIN7", 60, 63, 67, 70);
    NAMED(0, "DOM7", 60, 64, 67, 70);
    NAMED(0, "DOM7", 64, 67, 70, 72);          /* an inversion of it */
    NAMED(0, "MAJ", 48, 60, 64, 67);           /* a doubled root (BASS voicing) is still the triad */
    NAMED(0, "SINGLE", 60);
    NAMED(0, "5TH", 60, 67);  NAMED(0, "5TH", 67, 72);   /* root position and inverted */
    NAMED(0, "8VA", 60, 72);
    NAMED(0, "5+8", 60, 67, 72);  NAMED(0, "5+8", 67, 72, 79);
    NAMED(0, "SUS2", 60, 62, 67);
    NAMED(0, "SUS4", 60, 65, 67);
    NAMED(0, "ADD9", 60, 64, 67, 74);
    NAMED(0, "CUST", 60, 61, 62, 63, 64);
    NAMED(0, "+400", 60, 64);                  /* any other two notes read as a drone */
    /* tunings the semitone stack does not show: neutral seventh */
    struct groove_tuning g;
    memset(&g, 0, sizeof g);
    g.main[4] = -50; g.main[11] = -50;         /* E 350 c, B 1050 c */
    CHECK(groove_tuning_set(0, &g) == 0);
    NAMED(0, "NEU7", 60, 64, 67, 71);
    memset(&g, 0, sizeof g);
    g.main[1] = 50;                            /* C# 150 c */
    CHECK(groove_tuning_set(0, &g) == 0);
    NAMED(0, "CUST", 60, 61, 63, 65);          /* the four-note cluster is no longer a named jins */
    memset(&g, 0, sizeof g);
    g.main[4] = -5; g.main[7] = 20; g.alt[4] = -50;   /* rounding to 50 c; the alt table for an alt step */
    CHECK(groove_tuning_set(0, &g) == 0);
    NAMED(0, "MAJ", 60, 64, 67);
    g.main[4] = 0;
    CHECK(groove_tuning_set(0, &g) == 0);
    NAMED(1, "NEU", 60, 64, 67);               /* an alt step reads the alt table */
    NAMED(0, "MAJ", 60, 64, 67);
    /* drone names: +NNN for a two-note chord, or three notes whose lowest two are a fifth apart */
    boot("RAST", 0);
    {
        struct groove_tuning rt;
        maqam_engine_get(0, &rt);
        CHECK(rt.main[4] == -43);               /* E entry -43 (TRAD) */
    }
    NAMED(0, "+350", 36, 64);                  /* Rast C3 under E-half-flat 4 */
    boot("P.HUS", 18);
    NAMED(0, "+150", 45, 52, 59);              /* DRN5 on A: A2 E3 B-half-flat 3 */
    NAMED(0, "+150", 59, 69, 76);              /* a high drone: B-half-flat 3, A4, E5 (DRN5 in 2ND) */
    boot("MAJ", 0);
    NAMED(0, "+400", 36, 48, 64);              /* a doubled pedal (BASS): C2 C3 E4 */
    NAMED(0, "CUST", 62, 64);                  /* no tonic among them: no drone */
    NAMED(0, "+700", 36, 48, 79);              /* the odd note names itself over the tonic, reduced */
    {   /* what the drone types actually play reads the same */
        uint8_t n[8];
        conf(MAQAM_CHORD_DRN, MAQAM_INV_ROOT, MAQAM_VOICE_BASS);
        unsigned c = maqam_chord_notes(0, 64, 0, n);
        CHECK(c == 3 && !strcmp(named(0, c, n), "+400"));
        conf(MAQAM_CHORD_DRN5, MAQAM_INV_2ND, MAQAM_VOICE_CLOSE);
        c = maqam_chord_notes(0, 62, 0, n);
        CHECK(c == 3 && !strcmp(named(0, c, n), "+200"));
    }
    boot("MAJ", 0);
    NAMED(0, "MAJ", 60, 67, 76);               /* an OPEN triad is not a drone */
    NAMED(0, "+0", 60, 84);
    NAMED(0, "+1100", 60, 83);
    /* the 12 stock chords in WEST major: none CUST */
    boot("MAJ", 0);
    static const char *const stock[12] = {"MAJ", "MIN", "MAJ7", "MIN7", "DOM7", "SUS2", "SUS4", "MIN-", "MAJ+", "5TH", "8VA", "ADD9"};
    struct groove_sequence_options o;
    groove_sequence_options_copy(&o);
    for (unsigned id = 0; id < 12; ++id) {
        uint8_t n[8];
        unsigned c = o.chord[id][0];
        for (unsigned i = 0; i < c; ++i) n[i] = (uint8_t)(60 + (i ? o.chord[id][i] : 0));
        const char *got = named(0, c, n);
        if (strcmp(got, stock[id])) { printf("FAIL stock chord %u: '%s' want '%s'\n", id, got, stock[id]); ++failures; }
    }
    printf("names: NEU/MAJ by tuning, MIN-, inversions, sevenths, fixed shapes, drones, 12 stock chords\n");
}

/* V14 keeps a PLAY track's held keys across a project load: a held chord must still release every note. */
static unsigned offs, ons;
static int count_out(unsigned t, unsigned note, unsigned on) { (void)t; (void)note; if (on) ++ons; else ++offs; return 1; }
static void held_across_load(void)
{
    boot("RAST", 0);
    conf(0, 0, 0);
    ons = offs = 0;
    unsigned n = maqam_chord_start(3, 0, 60, 0);
    CHECK(n >= 3 && maqam_chord_play(3, count_out) && ons == n);
    maqam_project_loaded(NULL);                    /* a load while the key is held */
    CHECK(maqam_chord_owned(3));
    CHECK(maqam_chord_stop(3, count_out) && offs == n && !maqam_chord_owned(3));
}
int main(void)
{
    held_across_load();
    routing();
    types();
    drones();
    exhaustive();
    voicing();
    sounding();
    overlap();
    alt_and_drums();
    recording();
    names();
    printf(failures ? "FAILED: %d\n" : "chords_test: all ok\n", failures);
    return failures != 0;
}
