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
#define FL MAQAM_FLAT
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
/* The glyph codes print as letters: 0x1a d, 0x1b b, 0x18 ^, 0x19 v. */
static const char *show(const char *s, char *buf)
{
    char *p = buf;
    for (; *s; ++s) *p++ = *s == 0x1a ? 'd' : *s == 0x1b ? 'b' : *s == 0x18 ? '^' : *s == 0x19 ? 'v' : *s;
    *p = 0;
    return buf;
}
static void is(const char *what, const char *got, const char *want)
{
    char a[32], b[32];
    int ok = !strcmp(got, want);
    printf("%-34s %-8s (want %s) %s\n", what, show(got, a), show(want, b), ok ? "ok" : "FAIL");
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
    maqam_note_name(0, 64, 1, n); is("alt edited to -100 (E key keeps its letter)", n, "E" FL "4.");
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

/* ---- scale-aware spelling (task 5.4) ---- */
static const char *nm(const char *scale, unsigned root, unsigned note, unsigned alt)
{
    static char out[12];
    boot(scale);
    CHECK(maqam_set_project(find(scale), root) == 0);
    maqam_note_name(0, note, alt, out);
    return out;
}
static void list(const char *what, const char *scale, unsigned root, const unsigned *notes, unsigned n, const char *const *want)
{
    for (unsigned i = 0; i < n; ++i) { char w[40]; snprintf(w, sizeof w, "%s %u", what, i + 1); is(w, nm(scale, root, notes[i], 0), want[i]); }
}
enum { RC = 0, RD = 4, RBB = 20 };
static void spelling(void)
{
    char n[12];
    is("Rast C: E4 stored, E at -43", nm("RAST", RC, 64, 0), "E" HF "4");
    is("Bayati D: A#4 key is Bb4", nm("BAYAT", RD, 70, 0), "B" FL "4");
    {   static const unsigned notes[] = {62, 64, 65, 67, 69, 70, 72};
        static const char *const want[] = {"D4", "E" HF "4", "F4", "G4", "A4", "B" FL "4", "C5"};
        list("Bayati D degree", "BAYAT", RD, notes, 7, want); }
    is("Saba D: fourth is Gb, not F#", nm("SABA", RD, 66, 0), "G" FL "4");
    {   static const unsigned notes[] = {62, 63, 66, 67, 69, 71, 72};
        static const char *const want[] = {"D4", "E" FL "4", "F#4", "G4", "A4", "B" HF "4", "C5"};
        list("Hijaz D up degree", "HIJAZ", RD, notes, 7, want); }
    is("Hijaz D: descending seventh is Bb", nm("HIJAZ", RD, 70, 0), "B" FL "4");
    is("Rast C: ascending seventh is Bd", nm("RAST", RC, 71, 0), "B" HF "4");
    is("Rast C: descending seventh is Bb", nm("RAST", RC, 70, 0), "B" FL "4");
    {   static const unsigned notes[] = {70, 72, 74, 75, 77, 79, 81};
        static const char *const want[] = {"B" FL "4", "C5", "D5", "E" FL "5", "F5", "G5", "A5"};
        list("MAJ on A#/Bb degree", "MAJ", RBB, notes, 7, want); }
    is("MAJ on C#: flat spelling wins (Db)", nm("MAJ", 2, 61, 0), "D" FL "4");
    is("PEN on C, D# key unbent: stock name", nm("PEN", RC, 63, 0), "D#4");
    is("Rast C: C# key carries no degree", nm("RAST", RC, 61, 0), "C#4");
    boot("RAST");
    CHECK(maqam_set_alt(0, 4, -100) == 0);
    maqam_note_name(0, 64, 1, n); is("Rast C alt E at -100 keeps E: Eb", n, "E" FL "4.");
    CHECK(maqam_set_alt(0, 4, -43) == 0);
    maqam_note_name(0, 64, 1, n); is("Rast C alt E at -43 (alt mark)", n, "E" HF "4.");
    CHECK(maqam_set_alt(0, 4, 100) == 0);
    maqam_note_name(0, 64, 1, n); is("Rast C alt E at +100 is the degree F", n, "F4.");
    boot("CHR");
    unsigned bad = 0;
    for (unsigned note = 0; note < 128; ++note) {
        char s[12];
        maqam_note_name(0, note, 0, n);
        stock(note, s);
        bad += strcmp(n, s) != 0;
    }
    CHECK(bad == 0);
    printf("WEST CHR: stock names for all 128 notes on a track\n");
    /* the Cd/B+ edge: the octave is the spelled letter's */
    maqam_name_scale(find("RAST"), RC, 60, -50, n); is("C4 at -50 on Rast C: Cd4", n, "C" HF "4");
}

/* All 73 scales x 24 roots: spelled with one letter per degree and a single accidental, or fixed names. */
static void exhaustive(void)
{
    static const uint8_t nat[7] = {0, 4, 8, 10, 14, 18, 22};
    unsigned spelled = 0, fallback = 0, bad = 0, unfit = 0;
    for (unsigned s = 0; s < NSCALES; ++s)
        for (unsigned r = 0; r < 24; ++r) {
            uint32_t m[2] = {scale_up(&SCALES[s]), scale_down(&SCALES[s])};
            char nme[4], first[4];
            int ok = 1, hep = 1, L0 = -1;
            for (unsigned f = 0; f < 2; ++f) hep &= __builtin_popcount(m[f]) == 7;
            for (unsigned f = 0; f < 2 && hep; ++f) {
                int seen = 0, L = -1, prev = -1;
                for (unsigned sl = 0; sl < 24; ++sl) {
                    if (!(m[f] >> sl & 1U)) continue;
                    unsigned sp = maqam_degree_name(s, r, r + sl, nme);
                    ok &= (int)sp;
                    if (!sp) continue;
                    const char *letters = "CDEFGAB";
                    L = (int)(strchr(letters, nme[0]) - letters);
                    int a = nme[1] == 0 ? 0 : nme[1] == FL[0] ? -2 : nme[1] == HF[0] ? -1 : nme[1] == '+' ? 1 : nme[1] == '#' ? 2 : 99;
                    ok &= a != 99 && nme[1 + (a != 0)] == 0;                               /* one accidental at most */
                    ok &= ((int)(nat[L] + a + 48) % 24) == (int)((r + sl) % 24);          /* it is that pitch */
                    if (prev >= 0) ok &= L == (prev + 1) % 7;                              /* consecutive letters */
                    prev = L;
                    if (!seen++) L0 = L;
                    ok &= (L - L0 + 7) % 7 == seen - 1;
                }
            }
            (void)first;
            if (!hep) { ++fallback; for (unsigned sl = 0; sl < 24; ++sl) bad += maqam_degree_name(s, r, r + sl, nme) != 0; }
            else if (ok) ++spelled;
            else {                                    /* all or nothing: no degree is half spelled */
                ++fallback; ++unfit;
                for (unsigned sl = 0; sl < 24; ++sl) bad += maqam_degree_name(s, r, r + sl, nme) != 0;
            }
        }
    printf("spelling: %u of %u scale/root pairs spelled, %u fall back (%u heptatonic but needing a double accidental)\n",
           spelled, NSCALES * 24, fallback, unfit);
    CHECK(bad == 0);
    CHECK(spelled + fallback == NSCALES * 24);
    printf("exhaustive: no crash; every spelled degree is one letter, one accidental, the right pitch\n");
}

int main(void)
{
    pure();
    glyph();
    tracks();
    spelling();
    exhaustive();
    screen();
    printf(failures ? "FAILED: %d\n" : "names_test: all ok\n", failures);
    return failures != 0;
}
