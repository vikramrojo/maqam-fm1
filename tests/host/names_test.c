/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* 24-tone names (task 5.1 of maqam-scales): app/maqam/names.c, and its use on the step pages of the real
 * app/screen.c. Run from the repository root. */
#define TUNING_NO_MAIN
#include "tuning_test.c"
#include "../../app/maqam/maqam.h"
#include "../../app/maqam/scales.h"

void sim_control(unsigned kind, unsigned index, int value);
int sim_open(unsigned target);
const char *sim_knobs(void);
const uint16_t *sim_pixels(void);
#define HF MAQAM_HF
enum { REC = 13, SEQ = 11, OCT_DN = 0, STEP_EDIT = 13 };

static unsigned find(const char *code)
{
    for (unsigned i = 0; i < NSCALES; ++i)
        if (!strcmp(SCALES[i].code, code)) return i;
    printf("FAIL no scale %s\n", code);
    ++failures;
    return 0;
}
static void ms(unsigned t) { for (unsigned n = 0; n < t * 44100 / 1000 / FM1_BLOCK_FRAMES + 1; ++n) sim_render(); }
static void boot(const char *scale)
{
    sim_boot(1);
    maqam_init();
    CHECK(maqam_set_project(find(scale), 0) == 0);
    ms(50);
}
static void key(unsigned k) { sim_control(1, k, 1); ms(40); sim_control(1, k, 0); ms(20); }
static void is(const char *what, const char *got, const char *want)
{
    int ok = !strcmp(got, want);
    printf("%-34s %-8s (want %s) %s\n", what, got, want, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}
/* A copy of screen.c's 12-tone note_name (the stock formula) to compare against. */
static void stock(unsigned note, char out[12])
{
    static const char names[] = "CCDDEFFGGAAB";
    static const uint16_t sharps = (1U << 1) | (1U << 3) | (1U << 6) | (1U << 8) | (1U << 10);
    unsigned pitch = note % 12, n = 1;
    out[0] = names[pitch];
    if (sharps & (1U << pitch)) out[n++] = '#';
    snprintf(out + n, 8, "%d", (int)(note / 12) - 1);
}

static void pure(void)
{
    char n[12], s[12];
    maqam_name24(64, -43, n); is("E4 at -43", n, "E" HF "4");
    maqam_name24(64, -50, n); is("E4 at -50", n, "E" HF "4");
    maqam_name24(64, -100, n); is("E4 at -100", n, "D#4");
    maqam_name24(64, 50, n); is("E4 at +50", n, "F" HF "4");
    maqam_name24(71, 50, n); is("B4 at +50 (C half-flat above)", n, "C" HF "5");
    maqam_name24(60, -50, n); is("C4 at -50", n, "C" HF "4");
    maqam_name24(60, 25, n); is("C4 at +25 (rounds up)", n, "C+4");
    maqam_name24(60, 24, n); is("C4 at +24", n, "C4");
    maqam_name24(0, -50, n); is("note 0 at -50", n, "C" HF "-1");
    maqam_name24(127, 127, n); is("note 127 at +127", n, "A" HF "9");
    unsigned bad = 0;
    for (unsigned note = 0; note < 128; ++note) {
        maqam_name24(note, 0, n);
        stock(note, s);
        if (strcmp(n, s)) { ++bad; printf("note %u: %s vs stock %s\n", note, n, s); }
    }
    CHECK(bad == 0);
    printf("zero offset equals the stock 12-tone name for all 128 notes\n");
    /* screen: the half-flat code draws a lowercase d, distinct from D, and stock lowercase text is unchanged */
    CHECK(strlen(HF) == 1 && HF[0] == 0x1a);
    CHECK(!strcmp(maqam_slot_name(7), "E" HF) && !strcmp(maqam_slot_name(23), "C" HF));
}

/* The scale page subtitle (2x text, 12 px per character at x=8, y=34): root "Dd" must draw a glyph in
 * the second cell that is neither blank nor the capital D drawn for root "D". */
static unsigned cell_hash(unsigned cell)
{
    unsigned h = 2166136261U, ink = 0;
    const uint16_t *p = sim_pixels();
    for (unsigned r = 0; r < 14; ++r)
        for (unsigned c = 0; c < 12; ++c) { uint16_t v = p[(34 + r) * 240 + 8 + cell * 12 + c]; ink += v != 0; h = (h ^ v) * 16777619U; }
    return ink ? h : 0;
}
static void glyph(void)
{
    boot("RAST");
    sim_control(0, 8, 1); ms(30); sim_control(0, 3, 1); ms(30);        /* HOME + SCL: the scale pages */
    sim_control(0, 3, 0); sim_control(0, 8, 0); ms(100);
    maqam_set_project(find("RAST"), 4); ms(300);                      /* D root */
    unsigned big_d = cell_hash(0), after_d = cell_hash(1);
    maqam_set_project(find("RAST"), 3); ms(300);                      /* D-half-flat root */
    unsigned hf_first = cell_hash(0), hf_second = cell_hash(1);
    CHECK(big_d && hf_second);
    CHECK(hf_first == big_d);                                          /* first cell is the plain D */
    CHECK(hf_second != big_d);                                         /* the half-flat is not a capital D */
    CHECK(hf_second != after_d);                                       /* and not a blank */
    printf("screen: half-flat draws a lowercase d glyph (differs from the capital D)\n");
}

static void tracks(void)
{
    char n[12];
    boot("RAST");
    struct groove_tuning g;
    groove_tuning_get(0, &g);
    CHECK(g.main[4] == -43 && g.alt[4] == 0 && g.main[11] == -41);
    maqam_note_name(0, 64, 0, n); is("Rast TRAD, E4 plain", n, "E" HF "4");
    maqam_note_name(0, 64, 1, n); is("Rast TRAD, E4 alt (alt 0)", n, "E4.");
    maqam_note_name(0, 71, 0, n); is("Rast TRAD, B4 plain", n, "B" HF "4");
    maqam_note_name(0, 60, 0, n); is("Rast TRAD, C4 plain", n, "C4");
    maqam_note_name(0, 60, 1, n); is("Rast TRAD, C4 alt (-50)", n, "C" HF "4.");
    maqam_note_name(0, 76, 0, n); is("E5 plain (any octave)", n, "E" HF "5");
    maqam_note_name(7, 64, 0, n); is("drum track keeps the stock name", n, "E4");
    CHECK(maqam_set_alt(0, 4, -100) == 0);
    maqam_note_name(0, 64, 1, n); is("alt edited to -100", n, "D#4.");
    boot("CHR");
    maqam_note_name(0, 64, 0, n); is("WEST chromatic, E4", n, "E4");
    maqam_note_name(0, 61, 0, n); is("WEST chromatic, C#4", n, "C#4");
}

/* The step pages of the real screen.c use the names: the step NOTE knob reads the track's table (and the
 * alt table with a mark for alt steps), and the step tiles repaint when a table changes. */
static const char *knob_value(unsigned col) { return sim_knobs() + col * 32 + 16; }
static unsigned tile_hash(unsigned i)
{
    unsigned x = 8 + (i % 4) * 57, y = 72 + (i / 4) * 29, h = 2166136261U;
    const uint16_t *p = sim_pixels();
    for (unsigned r = 0; r < 25; ++r)
        for (unsigned c = 0; c < 53; ++c) h = (h ^ p[(y + r) * 240 + x + c]) * 16777619U;
    return h;
}
static void screen(void)
{
    boot("RAST");
    sim_control(0, REC, 1); sim_control(0, REC, 0); ms(30);
    key(11);                                         /* E4 -> step 1 (plain) */
    sim_control(0, OCT_DN, 1); ms(20);
    key(11);                                         /* E4 alt -> step 2 */
    sim_control(0, OCT_DN, 0); ms(20);
    sim_control(0, REC, 1); sim_control(0, REC, 0); ms(30);
    struct groove_step c;
    groove_sequence_read(0, 0, &c, 1); CHECK(c.note == 64 && !(c.drums & GROOVE_STEP_ALT));
    groove_sequence_read(0, 1, &c, 1); CHECK(c.note == 64 && (c.drums & GROOVE_STEP_ALT));
    /* STEP page 4/5 (LOCKS) shows the step NOTE as knob 4 */
    CHECK(sim_open(STEP_EDIT) == 0);
    sim_control(2, 1, 2); ms(100);                   /* page 3 = LOCKS */
    sim_control(2, 6, -8); ms(100);                  /* PRESETS: back to step 1 */
    is("step 1 NOTE knob (non-alt E)", knob_value(3), "E" HF "4");
    sim_control(2, 6, 1); ms(100);                   /* PRESETS: next step */
    is("step 2 NOTE knob (alt E)", knob_value(3), "E4.");
    /* tiles: plain and alt differ, and a table change repaints them */
    CHECK(sim_open(0) == 0);
    sim_control(0, SEQ, 1); sim_control(0, SEQ, 0); ms(300);
    unsigned plain = tile_hash(0), alt = tile_hash(1);
    CHECK(plain != alt);
    maqam_set_project(find("CHR"), 0);               /* E is now plain E4 */
    ms(300);
    CHECK(tile_hash(0) != plain);
    printf("step tiles: non-alt Ed4 differs from alt E4., and repaint when the table changes\n");
}

int main(void)
{
    pure();
    glyph();
    tracks();
    screen();
    printf(failures ? "FAILED: %d\n" : "names_test: all ok\n", failures);
    return failures != 0;
}
