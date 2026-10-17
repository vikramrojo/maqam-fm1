/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Maqam keyboard (tasks 4.1-4.3 of maqam-scales), driving the real app/screen.c through the simulator's
 * panel-control path (sim_control, sim_render for time): SNAP and WHITE note choice with direction and the
 * 1 s reset, the OCT alt gesture, double-tap octave, recorded alt steps, and the key lights.
 * Notes are observed by recording them (REC, step entry) and reading the sequence back.
 * Run from the repository root. */
#define TUNING_NO_MAIN
#include "tuning_test.c"
#include "../../app/maqam/maqam.h"
#include "../../app/maqam/scales.h"

#ifndef GROOVE_STEP_ALT
#define GROOVE_STEP_ALT (1U << 13)
#endif
void sim_control(unsigned kind, unsigned index, int value);
const uint8_t *sim_state(void);
int sim_select(unsigned track, unsigned source);
enum { OCT_DN = 0, OCT_UP = 1, REC = 13 };
enum { C = 0, CS, D, DS, E, F, FS, G, GS, A, AS, B };

static unsigned find(const char *code)
{
    for (unsigned i = 0; i < NSCALES; ++i)
        if (!strcmp(SCALES[i].code, code)) return i;
    printf("FAIL no scale %s\n", code);
    ++failures;
    return 0;
}
static void ms(unsigned t) { for (unsigned n = 0; n < t * 44100 / 1000 / FM1_BLOCK_FRAMES + 1; ++n) sim_render(); }
static void boot(const char *scale, unsigned root, int layout)
{
    sim_boot(1);
    maqam_init();
    CHECK(maqam_set_project(find(scale), root) == 0);
    if (layout >= 0) CHECK(maqam_set_default_layout((unsigned)layout) == 0);
    ms(50);
}
static void rec_on(void) { sim_control(0, REC, 1); sim_control(0, REC, 0); ms(30); }
/* Panel key k (0 = F3, 7 = C4) pressed and released; hold = ms before release. */
static void key(unsigned k, unsigned hold)
{
    sim_control(1, k, 1); ms(hold); sim_control(1, k, 0); ms(20);
}
static void tapb(unsigned b) { sim_control(0, b, 1); ms(20); sim_control(0, b, 0); ms(20); }
static unsigned step_note(unsigned s)
{
    struct groove_step c;
    groove_sequence_read(0, s, &c, 1);
    return c.note;
}
static unsigned step_alt(unsigned s)
{
    struct groove_step c;
    groove_sequence_read(0, s, &c, 1);
    return (c.drums & GROOVE_STEP_ALT) != 0;
}
static int octave(void) { return (int)sim_state()[7] - 24; }
#define WHITE_KEYS_C4_C5 { 7, 9, 11, 12, 14, 16, 18, 19 }

static void snap(void)
{
    boot("MAJ", 0, -1);                       /* default layout is SNAP */
    CHECK(maqam_default_layout() == MAQAM_SNAP && maqam_layout(0) == MAQAM_SNAP);
    rec_on();
    key(7, 40);                               /* C4 */
    key(8, 40);                               /* C#4 going up -> D4 */
    key(7, 40);                               /* back down to C4 */
    key(8, 40);                               /* C#4: lower than the last? no, higher: up -> D4 */
    CHECK(step_note(0) == 60 && step_note(1) == 62 && step_note(2) == 60 && step_note(3) == 62);
    /* descending: D#4 (key 10) after E4 (key 11) -> D4 (62), next lower degree */
    boot("MAJ", 0, -1);
    rec_on();
    key(11, 40);                              /* E4 */
    key(10, 40);                              /* D#4 going down -> D4 */
    CHECK(step_note(0) == 64 && step_note(1) == 62);
    /* a pause over 1 s makes the same key ascending again: D#4 -> E4 */
    boot("MAJ", 0, -1);
    rec_on();
    key(11, 40);
    ms(1100);
    key(10, 40);
    CHECK(step_note(1) == 64);
    /* CHROM plays every key */
    boot("MAJ", 0, MAQAM_CHROM);
    rec_on();
    key(8, 40);
    CHECK(step_note(0) == 61);
    printf("snap: C#->D up, D#->D down, pause resets, CHROM own note\n");
}
static void white(void)
{
    static const unsigned k[8] = WHITE_KEYS_C4_C5;
    static const unsigned up[8] = {60, 62, 64, 65, 67, 69, 71, 72};   /* C D Ed(E key) F G A Bd(B key) C */
    unsigned rast = find("RAST");
    boot("RAST", 0, MAQAM_WHITE);
    CHECK(maqam_layout(0) == MAQAM_WHITE);
    rec_on();
    for (unsigned i = 0; i < 8; ++i) key(k[i], 30);
    for (unsigned i = 0; i < 8; ++i) CHECK(step_note(i) == up[i]);
    /* descending from C5: the B key plays B-flat (A# key) */
    boot("RAST", 0, MAQAM_WHITE);
    rec_on();
    key(19, 30);                              /* C5 */
    key(18, 30);                              /* B4 key, lower: descending form */
    key(16, 30);                              /* A */
    CHECK(step_note(0) == 72 && step_note(1) == 70 && step_note(2) == 69);
    /* black keys are silent: C#4 (key 8) records nothing */
    boot("RAST", 0, MAQAM_WHITE);
    rec_on();
    key(8, 30);
    key(7, 30);
    CHECK(step_note(0) == 60);
    /* ascending after a pause goes back to B-half-flat */
    boot("RAST", 0, MAQAM_WHITE);
    rec_on();
    key(19, 30); key(18, 30); ms(1100); key(18, 30);
    CHECK(step_note(1) == 70 && step_note(2) == 71);
    /* the per-track layout overrides the project default */
    boot("RAST", 0, MAQAM_WHITE);
    CHECK(maqam_set_layout(0, MAQAM_CHROM) == 0 && maqam_layout(0) == MAQAM_CHROM);
    rec_on();
    key(8, 30);
    CHECK(step_note(0) == 61);
    (void)rast;
    printf("white: Rast C..C ascending, B-flat descending, black keys silent\n");
}
static void persistence(void)
{
    uint8_t t[MAQAM_TRAILER_BYTES];
    boot("RAST", 0, MAQAM_WHITE);
    maqam_project_save(t);
    maqam_init();
    CHECK(maqam_default_layout() == MAQAM_SNAP);
    maqam_project_loaded(t);
    CHECK(maqam_default_layout() == MAQAM_WHITE);
    printf("layout default persists through the trailer\n");
}
static void alt(void)
{
    boot("RAST", 0, MAQAM_CHROM);
    CHECK(groove_source_type(0) == GROOVE_DX7);
    /* live: E4 plain -> main table (-43 c); with OCT- held -> alt table (0 c) */
    double base = measure(GROOVE_DX7, 64, NULL, 0);
    boot("RAST", 0, MAQAM_CHROM);
    filled = 0;
    sim_control(1, 11, 1); run(SETTLE + WINDOW);
    double plain = tail_cents(64) - base;
    sim_control(1, 11, 0); ms(100);
    boot("RAST", 0, MAQAM_CHROM);
    filled = 0;
    sim_control(0, OCT_DN, 1); ms(20);
    CHECK(groove_alt_held(0));
    sim_control(1, 11, 1); run(SETTLE + WINDOW);
    double altc = tail_cents(64) - base;
    sim_control(1, 11, 0);
    sim_control(0, OCT_DN, 0); ms(20);
    CHECK(!groove_alt_held(0));
    NEAR("E key plain (Rast TRAD)", plain, -43, 1.0);
    NEAR("E key with OCT- held (alt)", altc, 0, 1.0);
    CHECK(octave() == 0);                     /* hold never shifts the octave */
    /* the same with OCT+ */
    sim_control(0, OCT_UP, 1); ms(20);
    CHECK(groove_alt_held(0));
    sim_control(0, OCT_UP, 0); ms(20);
    CHECK(!groove_alt_held(0) && octave() == 0);

    /* recorded: OCT- held while recording marks the step alt; others stay plain */
    boot("RAST", 0, MAQAM_CHROM);
    rec_on();
    sim_control(0, OCT_DN, 1); ms(20);
    key(11, 40);
    sim_control(0, OCT_DN, 0); ms(20);
    key(11, 40);
    CHECK(step_note(0) == 64 && step_alt(0) == 1);
    CHECK(step_note(1) == 64 && step_alt(1) == 0);
    /* played back: the alt step sounds at the alt pitch, the plain step at the main one */
    sim_control(0, REC, 1); sim_control(0, REC, 0); ms(30);       /* stop recording */
    struct groove_step c;
    groove_sequence_read(0, 0, &c, 1);
    CHECK(c.drums & GROOVE_STEP_ALT);
    printf("alt: live and recorded steps marked; alt note %+0.1f c vs plain %+0.1f c\n", altc, plain);
}
static void playback_alt(void)
{
    /* a recorded alt step plays back as alt (engine playback of the cell written by the panel) */
    boot("RAST", 0, MAQAM_CHROM);
    rec_on();
    sim_control(0, OCT_DN, 1); ms(20);
    key(11, 40);
    sim_control(0, OCT_DN, 0); ms(20);
    sim_control(0, REC, 1); sim_control(0, REC, 0); ms(30);
    struct groove_step c;
    groove_sequence_read(0, 0, &c, 1);
    CHECK(c.note == 64 && (c.drums & GROOVE_STEP_ALT));
    double base = measure(GROOVE_DX7, 64, NULL, 0);
    struct groove_tuning t;
    boot("RAST", 0, MAQAM_CHROM);
    groove_tuning_get(0, &t);
    setup(GROOVE_DX7, &t, 0);
    groove_sequence_length_set(0, 16);
    c.drums |= 1U << 12;                       /* tie */
    for (unsigned s = 0; s < 16; ++s) groove_sequence_replace(0, s, &c, 1);
    const uint8_t start = 0xfa;
    fm1_web_midi(&start, 1);
    run(SETTLE + WINDOW);
    NEAR("recorded alt step plays back at alt pitch", tail_cents(64) - base, 0, 1.0);
}
static void octaves(void)
{
    boot("RAST", 0, -1);
    CHECK(octave() == 0);
    tapb(OCT_UP);                             /* single tap */
    ms(500);
    CHECK(octave() == 0);
    tapb(OCT_UP); ms(150); tapb(OCT_UP);      /* two presses 170 ms apart */
    CHECK(octave() == 1);
    tapb(OCT_DN); ms(500); tapb(OCT_DN);      /* too slow */
    CHECK(octave() == 1);
    ms(500);
    tapb(OCT_DN); ms(100); tapb(OCT_DN);      /* double tap down */
    CHECK(octave() == 0);
    /* a key between the taps cancels the double tap */
    tapb(OCT_UP); key(7, 20); tapb(OCT_UP);
    CHECK(octave() == 0);
    /* different buttons are not a double tap */
    ms(500);
    tapb(OCT_UP); ms(50); tapb(OCT_DN);
    CHECK(octave() == 0);
    /* clamps at +2 */
    for (unsigned i = 0; i < 5; ++i) { ms(500); tapb(OCT_UP); ms(50); tapb(OCT_UP); }
    CHECK(octave() == 2);
    /* the shifted octave applies to the notes */
    rec_on();
    key(7, 30);
    CHECK(step_note(0) == 84);
    /* a held key keeps sounding when the octave moves (no re-attack): octave already clamped here */
    printf("octave: single tap and slow taps ignored, double tap shifts, clamped\n");
}
static void drums_unchanged(void)
{
    /* a drum track keeps the stock OCT: one tap moves the octave, holding does not turn on alt */
    sim_boot(1);
    maqam_init();
    CHECK(sim_select(0, GROOVE_808) == 0 && !maqam_melodic(0));
    sim_control(0, OCT_UP, 1); ms(20);
    CHECK(!groove_alt_held(0) && octave() == 1);
    sim_control(0, OCT_UP, 0); ms(20);
    tapb(OCT_DN);
    CHECK(octave() == 0);
    printf("drum track: single tap moves the octave, no alt hold\n");
}
static unsigned lit(unsigned k) { return sim_state()[168 + 14 + k]; }
static void lights(void)
{
    boot("RAST", 0, -1);
    ms(100);
    /* keys 0..26 = F3.. : pitch class (5 + k) % 12 */
    static const unsigned want[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1}; /* C Db D Eb E F Gb G Ab A Bb B */
    unsigned bad = 0;
    for (unsigned k = 0; k < 27; ++k)
        if (lit(k) != want[(5 + k) % 12]) { ++bad; printf("key %u lit=%u\n", k, lit(k)); }
    CHECK(bad == 0);
    boot("CHR", 0, -1);
    ms(100);
    for (unsigned k = 0; k < 27; ++k) CHECK(!lit(k));   /* chromatic: stock lights */
    boot("MAJ", 0, -1);
    ms(100);
    CHECK(lit(7) && !lit(8) && lit(9) && !lit(10) && lit(11) && lit(12) && !lit(13));
    printf("lights: Rast on C lit C D E F G A A# B; unlit C# D# F# G#\n");
}

int main(void)
{
    snap();
    white();
    persistence();
    alt();
    playback_alt();
    octaves();
    drums_unchanged();
    lights();
    printf(failures ? "FAILED: %d\n" : "keys_test: all ok\n", failures);
    return failures != 0;
}
