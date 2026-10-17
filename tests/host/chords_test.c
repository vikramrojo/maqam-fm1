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
    /* a jins on D (Bayati): D Ed F G */
    boot("BAYAT", 4);
    conf(MAQAM_CHORD_JINS, 0, 0);
    SAYS(62, 0, "62 64 65 67");
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
    CHECK(maqam_chord_set(0, MAQAM_CH_TYPE, 7) < 0 && maqam_chord_set(0, MAQAM_CH_INV, 5) < 0 &&
          maqam_chord_set(0, MAQAM_CH_VOICE, 3) < 0 && maqam_chord_set(0, MAQAM_CH_MODE, 2) < 0 &&
          maqam_chord_set(8, MAQAM_CH_MODE, 1) < 0 && maqam_chord_set(0, 4, 0) < 0);
    printf("types: WEST MIN degrees, Rast neutral triad, descending-only Bb, Bayati jins, pentatonic, snapping\n");
}

/* Every scale x root x type x degree key: ascending, in range, each note on a key that carries a degree. */
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
            for (unsigned type = 0; type < MAQAM_CHORD_TYPES; ++type) {
                maqam_chord_set(0, MAQAM_CH_TYPE, type);
                unsigned want = (unsigned[]){3, 4, 3, 3, 2, 3, 4}[type];
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
    printf("exhaustive: %u scale/root pairs (%u skipped, %u without degrees), %u chords ascending, in range, on degree keys\n",
           combos, skipped, none, chords);
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
    /* AUTO: the first chord is in root position, then the inversion and octave nearest the last chord */
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_AUTO, MAQAM_VOICE_CLOSE);
    maqam_chords_reset();
    conf(MAQAM_CHORD_TRIAD, MAQAM_INV_AUTO, MAQAM_VOICE_CLOSE);
    uint8_t c[8];
    unsigned n = maqam_chord_notes(0, 60, 0, c);
    CHECK(n == 3 && c[0] == 60 && c[1] == 64 && c[2] == 67);
    maqam_chord_commit(0, c, n);
    SAYS(65, 0, "60 65 69");                       /* F chord as C F A, not F A C */
    n = maqam_chord_notes(0, 65, 0, c);
    maqam_chord_commit(0, c, n);
    SAYS(67, 0, "59 62 67");                       /* then G: B D G, nearest C F A */
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
    printf("voicing: 1ST/2ND/3RD, AUTO C->F = C4 F4 A4, BASS, OPEN, clamps, <=8 notes in range (%u chords)\n", total);
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

int main(void)
{
    types();
    exhaustive();
    voicing();
    sounding();
    overlap();
    alt_and_drums();
    recording();
    printf(failures ? "FAILED: %d\n" : "chords_test: all ok\n", failures);
    return failures != 0;
}
