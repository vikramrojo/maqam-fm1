/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Scale pages (task 5.2 of maqam-scales), driving the real app/screen.c through sim_control (panel
 * controls) and sim_render (time). Every control is checked by reading the engine tables back
 * (groove_tuning_get) and the maqam_* state. The screen is also rendered to build/host/scale_*.png
 * (sim_pixels) and checked for non-empty text rows. Run from the repository root. */
#define TUNING_NO_MAIN
#include "tuning_test.c"
#include "../../app/maqam/maqam.h"
#include "../../app/maqam/scales.h"
#include <ctype.h>
#include <stdlib.h>

void sim_control(unsigned kind, unsigned index, int value);
const uint8_t *sim_state(void);
const uint16_t *sim_pixels(void);
uint8_t *sim_flash(void);
const void *sim_lcd_targets(void);
unsigned sim_lcd_count(void);
enum { OCT_DN = 0, OCT_UP = 1, SCL = 3, HOME = 8, SAVE = 9, SEQ = 11, REC = 13 };
enum { PAGE_OVERVIEW = 0, PAGE_PROJECT = 8, PAGE_SCALE = 19 };    /* screen.c page numbers */

static unsigned find(const char *code)
{
    for (unsigned i = 0; i < NSCALES; ++i)
        if (!strcmp(SCALE_CODE_STR[i], code)) return i;
    printf("FAIL no scale %s\n", code);
    ++failures;
    return 0;
}
static void ms(unsigned t) { for (unsigned n = 0; n < t * 44100 / 1000 / FM1_BLOCK_FRAMES + 1; ++n) sim_render(); }
static unsigned page(void) { return sim_state()[1]; }
static void btn(unsigned b) { sim_control(0, b, 1); ms(30); sim_control(0, b, 0); ms(30); }
static void key(unsigned k) { sim_control(1, k, 1); ms(30); sim_control(1, k, 0); ms(30); }
static void enc(unsigned i, int v) { sim_control(2, i, v); ms(30); }
static unsigned sub_of(void) { struct maqam_view v; maqam_page_view(0, &v); return (unsigned)v.index - 1; }
/* SCL opens the page on the last subpage; the tests then step to the SCALE subpage (1/3). */
static void open_scale(void)
{
    btn(SCL);
    for (unsigned i = 0; i < 3 && sub_of(); ++i) btn(SCL);
}
static void open_track(void) { open_scale(); btn(SCL); }
static void sel_track(unsigned t) { enc(0, (int)t - (int)sim_state()[0]); }
static void boot(void)
{
    sim_boot(1);
    maqam_init();
    ms(50);
    open_scale();
}
static void tables(unsigned t, struct groove_tuning *g) { groove_tuning_get(t, g); }
static void expect_main(unsigned t, unsigned scale, unsigned root, unsigned trad, const char *what)
{
    int8_t want[GROOVE_TUNING_KEYS];
    struct groove_tuning g;
    CHECK(maqam_main(scale, root, trad, want) == 0);
    tables(t, &g);
    int ok = !memcmp(g.main, want, sizeof want);
    printf("%-52s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}
/* The view text is mixed case; screen.c draws it uppercase, so the comparison ignores case. */
static int has_nocase(const char *got, const char *want)
{
    size_t n = strlen(want);
    for (; *got; ++got) {
        size_t i = 0;
        while (i < n && got[i] && toupper((unsigned char)got[i]) == toupper((unsigned char)want[i])) ++i;
        if (i == n) return 1;
    }
    return n == 0;
}
static void has(const char *what, const char *got, const char *want)
{
    int ok = has_nocase(got, want);
    printf("%-52s \"%s\" %s\n", what, got, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}

/* ---- PNG writer (stored deflate), the screen at 2x ---- */
static uint32_t crc_table[256];
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = v & 1 ? 0xedb88320U ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    c = ~c;
    while (n--) c = crc_table[(c ^ *p++) & 255] ^ (c >> 8);
    return ~c;
}
static void be(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n)
{
    uint8_t h[8], t[4];
    be(h, n); memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f); if (n) fwrite(d, 1, n, f);
    uint32_t c = crc(0, h + 4, 4);
    c = crc(c, d, n);
    be(t, c); fwrite(t, 1, 4, f);
}
static void shot(const char *name)
{
    enum { S = 2, W = 240 * S };
    char path[128];
    snprintf(path, sizeof path, "build/host/%s.png", name);
    size_t row = 1 + W * 3, raw = row * W;
    uint8_t *img = malloc(raw);
    const uint16_t *px = sim_pixels();
    for (unsigned y = 0; y < W; ++y) {
        uint8_t *r = img + y * row;
        *r++ = 0;
        for (unsigned x = 0; x < W; ++x) {
            uint16_t c = px[(y / S) * 240 + x / S];
            *r++ = (uint8_t)(((c >> 11) & 31) * 255 / 31);
            *r++ = (uint8_t)(((c >> 5) & 63) * 255 / 63);
            *r++ = (uint8_t)((c & 31) * 255 / 31);
        }
    }
    size_t blocks = (raw + 65534) / 65535, zlen = 2 + raw + blocks * 5 + 4;
    uint8_t *z = malloc(zlen), *q = z;
    *q++ = 0x78; *q++ = 1;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; ++i) { a = (a + img[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw; off += 65535) {
        size_t n = raw - off < 65535 ? raw - off : 65535;
        *q++ = off + n == raw; *q++ = (uint8_t)n; *q++ = (uint8_t)(n >> 8); *q++ = (uint8_t)~n; *q++ = (uint8_t)(~n >> 8);
        memcpy(q, img + off, n); q += n;
    }
    be(q, (b << 16) | a); q += 4;
    FILE *f = fopen(path, "wb");
    if (!f) { printf("cannot write %s\n", path); ++failures; free(img); free(z); return; }
    uint8_t ihdr[13];
    be(ihdr, W); be(ihdr + 4, W); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    chunk(f, "IHDR", ihdr, 13); chunk(f, "IDAT", z, (uint32_t)(q - z)); chunk(f, "IEND", NULL, 0);
    fclose(f); free(img); free(z);
}
/* Non-background pixels in a screen rectangle. */
static unsigned ink(unsigned x, unsigned y, unsigned w, unsigned h)
{
    unsigned n = 0;
    const uint16_t *p = sim_pixels();
    for (unsigned r = 0; r < h; ++r) for (unsigned c = 0; c < w; ++c) n += p[(y + r) * 240 + x + c] != 0;
    return n;
}
static unsigned hash_rect(unsigned x, unsigned y, unsigned w, unsigned h)
{
    unsigned v = 2166136261U;
    const uint16_t *p = sim_pixels();
    for (unsigned r = 0; r < h; ++r) for (unsigned c = 0; c < w; ++c) v = (v ^ p[(y + r) * 240 + x + c]) * 16777619U;
    return v;
}

static void hold_home_scl(void)
{
    sim_control(0, HOME, 1); ms(30);
    sim_control(0, SCL, 1); ms(30);
    sim_control(0, SCL, 0); sim_control(0, HOME, 0); ms(100);
}
static void navigation(void)
{
    struct maqam_view v;
    sim_boot(1);
    maqam_init();
    ms(50);
    CHECK(page() == PAGE_OVERVIEW);
    btn(SCL);                                    /* SCL opens the scale pages on the last subpage (first: SCALE) */
    CHECK(page() == PAGE_SCALE && sub_of() == 0);
    btn(SCL); CHECK(page() == PAGE_SCALE && sub_of() == 1);        /* and switches to TRACK */
    btn(SCL); CHECK(page() == PAGE_SCALE && sub_of() == 2);        /* then CHORD */
    maqam_page_view(0, &v);
    CHECK(!strcmp(v.title, "CHORD") && v.index == 3);
    btn(SCL); CHECK(sub_of() == 0 && page() == PAGE_SCALE);        /* and back to SCALE */
    printf("%-52s ok\n", "SCL opens, then switches 1/3 2/3 3/3 1/3");
    enc(1, -1); CHECK(sub_of() == 2);            /* ALGORITHM wraps both ways */
    enc(1, 1); CHECK(sub_of() == 0);
    enc(1, 1); CHECK(sub_of() == 1);
    enc(1, 1);                                   /* leave on CHORD */
    btn(HOME);                                   /* HOME leaves */
    CHECK(page() == PAGE_OVERVIEW);
    btn(SCL);                                    /* ... and SCL returns to the last subpage */
    CHECK(page() == PAGE_SCALE && sub_of() == 2);
    maqam_page_view(0, &v);
    CHECK(!strcmp(v.title, "CHORD") && v.index == 3);
    /* HOME+SCL from a scale page: the stock SCL screen (DX7 level scaling) with the stock SCL toggle */
    hold_home_scl();
    CHECK(page() == 2 /* FM_EDIT */ && sim_state()[6] == 4);
    btn(SCL); CHECK(page() == 2 && sim_state()[6] == 5);           /* rate scaling */
    btn(SCL); CHECK(page() == 2 && sim_state()[6] == 4);           /* and back */
    /* HOME+SCL from the stock screen: the scale pages on the last subpage */
    hold_home_scl();
    CHECK(page() == PAGE_SCALE && sub_of() == 2);
    /* the stock toggle still works after the round trip */
    hold_home_scl(); btn(SCL);
    CHECK(page() == 2 && sim_state()[6] == 5);
    printf("%-52s ok\n", "HOME+SCL toggles scale pages / stock scaling (level, rate)");
    /* HOME+SCL from another page opens the stock screen */
    btn(HOME); CHECK(page() == PAGE_OVERVIEW);
    hold_home_scl();
    CHECK(page() == 2 && sim_state()[6] == 4);
    btn(HOME); CHECK(page() == PAGE_OVERVIEW);
    /* HOME leaves either; stock HOME alone is the preset browser */
    btn(HOME);
    CHECK(page() != PAGE_SCALE && page() != PAGE_OVERVIEW);
    printf("navigation: SCL opens/cycles, ALGO wraps, HOME+SCL toggles with the stock screen, HOME leaves\n");
}

/* A key on a scale page sounds: with REC on, the note is recorded as a step. */
static void play_check(unsigned key_no, unsigned note, const char *what)
{
    struct groove_step c;
    btn(REC); key(key_no); btn(REC);
    btn(SEQ); btn(SEQ); ms(50);
    groove_sequence_read(0, 0, &c, 1);
    printf("%-52s %s\n", what, c.velocity && c.note == note ? "ok" : "FAIL");
    if (!(c.velocity && c.note == note)) ++failures;
    btn(HOME);
}

/* Cycling the subpages keeps the stock header and the hint row, and changes the section row. */
static void frame_cycling(void)
{
    boot();
    ms(300);
    unsigned head = hash_rect(0, 0, 240, 32), hint = hash_rect(0, 229, 240, 11), row = hash_rect(0, 33, 240, 11);
    CHECK(ink(0, 0, 240, 29) > 200 && ink(8, 35, 230, 7) > 50 && ink(6, 231, 228, 7) > 100);
    for (unsigned i = 1; i < 3; ++i) {
        btn(SCL); ms(300);
        CHECK(sub_of() == i);
        CHECK(hash_rect(0, 0, 240, 32) == head);            /* header: tempo, transport, mode box */
        CHECK(hash_rect(0, 229, 240, 11) == hint);
        CHECK(hash_rect(0, 33, 240, 11) != row);             /* section word and counter changed */
        row = hash_rect(0, 33, 240, 11);
    }
    shot("scale_frame_chord");
    btn(SCL); ms(300);
    CHECK(sub_of() == 0 && hash_rect(0, 0, 240, 32) == head);
    printf("frame: header and hint unchanged across SCL cycling, the section row changes\n");
}

static void header_tag(void)
{
    char t[12];
    sim_boot(1);
    maqam_init();
    ms(50);
    CHECK(!maqam_header_tag(0, t) && !t[0]);                      /* WEST chromatic: hidden */
    maqam_set_project(find("RAST"), 7);
    CHECK(maqam_header_tag(0, t));
    has("Rast on Ed", t, "E" MAQAM_HF " RAST");
    CHECK(!strcmp(t, "E" MAQAM_HF " Rast") || !strcmp(t, "E" MAQAM_HF " RAST"));
    CHECK(!maqam_header_tag(7, t));                                /* drums: hidden */
    maqam_set_project(find("SIKAH"), 0);
    CHECK(maqam_header_tag(0, t));
    has("scale name cut to 4", t, "C SIKA");
    CHECK(strlen(t) == 6);
    maqam_set_project(find("RAST"), 7);
    ms(300);
    shot("scale_overview_tag");
    CHECK(ink(190, 20, 42, 7) > 30);                               /* the tag is painted at the right of the header */
    unsigned rast = hash_rect(190, 20, 42, 7);
    maqam_set_project(find("BAYAT"), 4);
    ms(300);
    CHECK(hash_rect(190, 20, 42, 7) != rast);                      /* redraws when the scale changes */
    maqam_set_project(find("CHR"), 0);
    ms(300);
    CHECK(ink(190, 20, 42, 7) == 0);                               /* chromatic: cleared */
    maqam_set_project(find("RAST"), 7);
    btn(SEQ); ms(300);
    CHECK(page() == 1 && ink(190, 20, 42, 7) > 30);                /* the step page too */
    shot("scale_steps_tag");
    /* a CUSTOM table */
    struct groove_tuning g;
    tables(0, &g);
    g.main[1] = 17;
    CHECK(groove_tuning_set(0, &g) == 0);
    maqam_refresh();
    CHECK(maqam_header_tag(0, t));
    has("CUSTOM", t, "CUST");
    CHECK(!strcmp(t, "CUST"));
    btn(SCL); ms(300);                                             /* scale pages carry no tag */
    CHECK(page() == PAGE_SCALE);
    printf("header tag: Ed RAST, hidden for CHR and drums, SIKA cut, CUST, repaints on change\n");
}

static void scale_page(void)
{
    boot();
    CHECK(page() == PAGE_SCALE);
    struct maqam_view v;
    maqam_page_view(0, &v);
    CHECK(!strcmp(v.title, "SCALE") && v.index == 1);
    has("default subtitle", v.sub, "C west chromat trad");
    CHECK(maqam_project_scale() == 0 && maqam_project_root() == 0 && maqam_trad() == 1);
    unsigned sikah = find("SIKAH"), set = SCALES[sikah].set, first = 0;
    while (SCALES[first].set != set) ++first;
    enc(2, 7);                                   /* K1 ROOT: 7 quarter tones up = E half-flat */
    CHECK(maqam_project_root() == 7);
    enc(3, (int)set);                            /* K2 SET: WEST -> ARAB (jumps to its first scale) */
    CHECK(maqam_project_scale() == first && SCALES[maqam_project_scale()].set == set);
    enc(4, (int)(sikah - first));                /* K3 SCALE within the set */
    CHECK(maqam_project_scale() == sikah);
    maqam_page_view(0, &v);
    has("subtitle after K1/K2/K3", v.sub, "E" MAQAM_HF " arab sikah");
    has("  the two forms differ: arrow shown", v.sub, "\x18");
    has("  intonation in the subtitle", v.sub, " trad");
    has("home hint", v.note, "home: E" MAQAM_HF);
    CHECK(maqam_project_root() == 7);            /* a hint, it never moves the root */
    expect_main(0, sikah, 7, 1, "K1/K2/K3: track 0 table = Sikah on Ed (TRAD)");
    expect_main(3, sikah, 7, 1, "  and track 3 follows the project");
    struct groove_tuning g, g0;
    tables(0, &g0);
    {
        int8_t alt[GROOVE_TUNING_KEYS];
        maqam_alt_default(sikah, 7, alt);
        CHECK(!memcmp(g0.alt, alt, sizeof alt));
    }
    enc(5, -1);                                  /* K4 TUNING: EQUAL */
    CHECK(maqam_trad() == 0);
    expect_main(0, sikah, 7, 0, "K4 left: EQUAL tables");
    tables(0, &g);
    CHECK(!memcmp(g.alt, g0.alt, sizeof g.alt));
    maqam_page_view(0, &v);
    has("  subtitle says equal", v.sub, " equal");
    enc(5, 1);
    CHECK(maqam_trad() == 1);
    expect_main(0, sikah, 7, 1, "K4 right: TRAD tables");
    /* a scale with one form shows no arrow; a WEST scale has no home hint */
    enc(3, -(int)set);                           /* back to WEST */
    CHECK(SCALES[maqam_project_scale()].set == SET_WEST);
    maqam_page_view(0, &v);
    CHECK(v.note[0] == 0 && !strchr(v.sub, '\x18') && !strchr(v.sub, '\x19'));
    printf("%-52s ok\n", "WEST: no home hint, no arrow");
    /* panel keys play and never set the root, with or without OCT- / OCT+ held */
    {
        unsigned root = maqam_project_root();
        key(9);
        CHECK(maqam_project_root() == root);
        sim_control(0, OCT_DN, 1); ms(30); key(11); sim_control(0, OCT_DN, 0); ms(30);
        sim_control(0, OCT_UP, 1); ms(30); key(11); sim_control(0, OCT_UP, 0); ms(30);
        CHECK(maqam_project_root() == root && page() == PAGE_SCALE);
        play_check(9, 62, "SCALE page: D key sounds (step records D4)");   /* the page was left by HOME */
        CHECK(maqam_project_root() == root);
        open_scale();
    }
    /* root wraps */
    {
        unsigned root = maqam_project_root();
        enc(2, 20);
        CHECK(maqam_project_root() == (root + 20) % 24);
    }
    printf("scale page: K1 root, K2 set, K3 scale, K4 tuning, keys play and leave the root alone\n");
}

/* "Two scope-aware scale pages": edits apply to the scope of the selected track. */
static void scopes(void)
{
    boot();
    unsigned rast = find("RAST"), bayat = find("BAYAT");
    struct maqam_view v;
    maqam_set_project(rast, 0);
    maqam_page_view(0, &v);
    has("status: follows", v.status, "TRACK 1  FOLLOWS PROJECT");
    /* editing a following track edits the project */
    enc(4, (int)(bayat - rast));                 /* K3 SCALE: Rast -> Bayati (same set) */
    CHECK(maqam_project_scale() == bayat && !maqam_overridden(0));
    expect_main(0, bayat, 0, 1, "following track 1: scale change edits the project");
    expect_main(2, bayat, 0, 1, "  and track 3 follows");
    expect_main(6, bayat, 0, 1, "  and track 7 follows");
    enc(2, 4);                                   /* K1 ROOT */
    CHECK(maqam_project_root() == 4);
    expect_main(1, bayat, 4, 1, "  root edits the project too");
    /* OWN on track 2: its scale changes alone */
    sel_track(1);
    btn(SCL);
    maqam_page_view(1, &v);
    CHECK(!strcmp(v.title, "TRACK"));
    has("scope knob: FOLLOW", v.knob[0][1], "FOLLOW");
    enc(2, 1);                                   /* K1 SCOPE -> OWN: copies the project scale and root */
    CHECK(maqam_overridden(1) && maqam_track_scale(1) == bayat && maqam_track_root(1) == 4);
    maqam_page_view(1, &v);
    has("scope knob: OWN", v.knob[0][1], "OWN");
    has("status: own scale", v.status, "TRACK 2  OWN SCALE");
    btn(SCL); btn(SCL);  /* TRACK -> CHORD -> SCALE */
    enc(4, (int)rast - (int)bayat);              /* K3 SCALE on the SCALE page: only track 2 */
    CHECK(maqam_track_scale(1) == rast && maqam_track_root(1) == 4);
    CHECK(maqam_project_scale() == bayat && maqam_project_root() == 4);
    expect_main(1, rast, 4, 1, "OWN: track 2 = Rast on D");
    expect_main(0, bayat, 4, 1, "  track 1 keeps the project Bayati on D");
    expect_main(2, bayat, 4, 1, "  track 3 too");
    enc(2, 3);                                   /* K1 ROOT edits the track only */
    CHECK(maqam_track_root(1) == 7 && maqam_project_root() == 4);
    enc(5, -1);                                  /* K4 TUNING is always the project's */
    CHECK(maqam_trad() == 0);
    expect_main(1, rast, 7, 0, "  tuning is the project's: track 2 follows EQUAL");
    expect_main(0, bayat, 4, 0, "  and so does track 1");
    enc(5, 1);
    maqam_page_view(1, &v);
    has("subtitle of the own scale", v.sub, "E" MAQAM_HF " arab rast");
    /* back to FOLLOW */
    btn(SCL);
    enc(2, -1);
    CHECK(!maqam_overridden(1) && maqam_track_scale(1) == bayat && maqam_track_root(1) == 4);
    expect_main(1, bayat, 4, 1, "back to FOLLOW: track 2 takes the project scale");
    /* keys do not change the root */
    key(7);
    CHECK(maqam_project_root() == 4 && !maqam_overridden(1));
    printf("scopes: edit following = project; OWN edits only the track; FOLLOW restores; tuning is the project's\n");
}

/* CUSTOM tables are kept and replaced only on request; drums are untouched. */
static void custom_and_drums(void)
{
    boot();
    unsigned rast = find("RAST");
    maqam_set_project(rast, 0);
    struct maqam_view v;
    struct groove_tuning t;
    tables(0, &t);
    t.main[1] = 17; t.main[2] = -3;
    CHECK(groove_tuning_set(0, &t) == 0);
    maqam_refresh();
    CHECK(maqam_track_scale(0) == MAQAM_CUSTOM);
    ms(100);
    maqam_page_view(0, &v);
    has("CUSTOM status", v.status, "CUSTOM TABLE");
    has("CUSTOM subtitle", v.sub, "custom");
    has("CUSTOM root knob", v.knob[0][1], "--");
    has("CUSTOM scale knob", v.knob[2][1], "CUSTOM");
    CHECK(v.custom == 1 && v.ncell == 0);
    struct groove_tuning after;
    tables(0, &after);
    CHECK(!memcmp(&after, &t, sizeof t));
    shot("scale_custom");
    btn(SCL);                                    /* TRACK: reads OWN; the strip shows the keys the table moves */
    maqam_page_view(0, &v);
    has("CUSTOM scope knob", v.knob[0][1], "OWN");
    CHECK(v.ncell == 0 && (v.lit & 6) == 6);
    tables(0, &after);
    CHECK(!memcmp(&after, &t, sizeof t));
    shot("scale_track_custom");
    enc(2, -1);                                  /* K1 FOLLOW: the project tables again */
    CHECK(maqam_track_scale(0) == rast && !maqam_overridden(0));
    expect_main(0, rast, 0, 1, "CUSTOM -> FOLLOW: project tables");
    /* turning the SCALE knobs of a CUSTOM track replaces its table on request */
    tables(0, &t);
    t.main[1] = 17;
    CHECK(groove_tuning_set(0, &t) == 0);
    maqam_refresh();
    CHECK(maqam_track_scale(0) == MAQAM_CUSTOM);
    btn(SCL); btn(SCL);  /* TRACK -> CHORD -> SCALE */
    enc(4, 1);
    CHECK(maqam_track_scale(0) != MAQAM_CUSTOM && maqam_overridden(0));
    /* a drum track is not tuned and nothing changes */
    enc(0, 7);
    CHECK(sim_state()[0] == 7);
    open_track();
    maqam_page_view(7, &v);
    has("drum track status", v.status, "DRUMS - NOT TUNED");
    CHECK(!v.tuned);
    enc(2, 1); enc(4, 3); enc(5, 5);
    CHECK(!maqam_overridden(7));
    tables(7, &after);
    int8_t zero[24] = {0};
    CHECK(!memcmp(&after, zero, sizeof after));
    printf("custom and drums: CUSTOM kept, replaced on request, drums untouched\n");
}

/* Layout follows the scope of the selected track. */
static void layouts(void)
{
    boot();
    maqam_set_project(find("RAST"), 0);
    struct maqam_view v;
    btn(SCL);
    maqam_page_view(0, &v);
    CHECK(maqam_default_layout() == MAQAM_SNAP && maqam_layout_raw(0) == MAQAM_SNAP);
    has("KEYS knob", v.knob[1][0], "KEYS");
    enc(3, 1);                                   /* K2 on a following track: the project layout, SNAP -> CHROM */
    CHECK(maqam_default_layout() == MAQAM_CHROM && maqam_layout(0) == MAQAM_CHROM && maqam_layout(1) == MAQAM_CHROM);
    enc(3, 5);                                   /* clamps at CHROM */
    CHECK(maqam_default_layout() == MAQAM_CHROM);
    enc(3, -1);
    CHECK(maqam_default_layout() == MAQAM_SNAP);
    enc(3, 1);
    maqam_page_view(0, &v);
    has("layout value", v.knob[1][1], "CHROM");
    /* track 2 goes OWN: its layout starts as a copy of the project's */
    sel_track(1);
    enc(2, 1);
    CHECK(maqam_overridden(1) && maqam_layout_raw(1) == MAQAM_CHROM);
    enc(3, -1);                                  /* its own layout only */
    CHECK(maqam_layout(1) == MAQAM_SNAP && maqam_default_layout() == MAQAM_CHROM && maqam_layout(0) == MAQAM_CHROM);
    maqam_page_view(1, &v);
    has("own layout value", v.knob[1][1], "SNAP");
    sel_track(0);                                /* the project layout changes: the own layout stays */
    enc(3, -9);
    CHECK(maqam_default_layout() == MAQAM_SNAP && maqam_layout(1) == MAQAM_SNAP && maqam_layout(0) == MAQAM_SNAP);
    enc(3, 9);
    CHECK(maqam_default_layout() == MAQAM_CHROM && maqam_layout(1) == MAQAM_SNAP && maqam_layout(0) == MAQAM_CHROM);
    sel_track(1);
    enc(2, -1);                                  /* FOLLOW: the project layout again */
    CHECK(!maqam_overridden(1) && maqam_layout(1) == MAQAM_CHROM);
    enc(2, 1);                                   /* OWN again: a fresh copy */
    CHECK(maqam_layout_raw(1) == MAQAM_CHROM);
    printf("layouts: K2 edits the project layout when following, the track's own when OWN (copied on OWN)\n");
}

/* The cents row: every value -127..+127 fits its box and no two boxes on one line meet; painted ink of
   each non-zero entry lies inside its own box. */
static unsigned cents_text(int c, char *out) { return maqam_num(out, c, c > 0); }   /* as screen.c draws it */
static void maqam_cents_pos(unsigned key, unsigned len, int *x, unsigned *y)   /* top-left pixel, as screen.c places it */
{
    *x = 6 + (int)key * 19 + (19 - 6 * (int)len) / 2;
    *y = 117U + 9U * (key & 1U);
}
static void cents_layout(const struct maqam_view *v)
{
    char t[12];
    for (int c = -127; c <= 127; ++c) {
        unsigned n = cents_text(c, t);
        CHECK(n >= 1 && n <= 4 && (int)strlen(t) == (int)n);
        for (unsigned a = 0; a < 12; ++a) {
            int xa, xb;
            unsigned ya, yb;
            maqam_cents_pos(a, n, &xa, &ya);
            CHECK(xa >= 0 && xa + 6 * (int)n <= 240 && ya >= 115 && ya + 7 <= 134);
            for (unsigned b = a + 1; b < 12; ++b) {
                maqam_cents_pos(b, n, &xb, &yb);
                int apart_x = xa + 6 * (int)n + 1 <= xb || xb + 6 * (int)n + 1 <= xa;
                CHECK(apart_x || ya + 7 < yb || yb + 7 < ya);   /* one pixel of air either way */
            }
        }
    }
    for (unsigned k = 0; k < 12; ++k) {
        int x;
        unsigned y, n = cents_text(v->cents[k], t);
        maqam_cents_pos(k, n, &x, &y);
        unsigned inside = ink((unsigned)x, y, 6 * n, 7);
        unsigned band = ink(0, y, 240, 7);      /* the whole line: nothing outside the boxes on it */
        unsigned others = 0;
        for (unsigned j = (k & 1U); j < 12; j += 2) {
            int xj;
            unsigned yj, nj = cents_text(v->cents[j], t);
            maqam_cents_pos(j, nj, &xj, &yj);
            others += ink((unsigned)xj, yj, 6 * nj, 7);
        }
        CHECK(v->cents[k] == 0 || inside > 0);
        CHECK(band == others);
    }
    printf("alt page: every cents value -127..+127 fits its box; no two boxes on a line meet\n");
}
static void alt_page(void)
{
    boot();
    unsigned sikah = find("SIKAH");
    maqam_set_project(sikah, 7);
    struct maqam_view v;
    struct groove_tuning g, before;
    btn(SCL);
    maqam_page_view(0, &v);
    CHECK(!strcmp(v.title, "TRACK"));
    key(11);                                     /* E4: pitch class 4 */
    maqam_page_view(0, &v);
    CHECK(v.sel == (1U << 4));
    has("key knob", v.knob[2][1], "E");
    play_check(11, maqam_key_note(0, 11, 0, 0), "TRACK page: E key sounds (the step records its note)");
    btn(SCL);                                    /* reopens on the last subpage: TRACK */
    maqam_page_view(0, &v);
    CHECK(!strcmp(v.title, "TRACK") && v.sel == (1U << 4));
    tables(0, &before);
    enc(5, 1);                                   /* K4: one cent per detent */
    tables(0, &g);
    CHECK(g.alt[4] == before.alt[4] + 1);
    enc(5, -100 - before.alt[4] - 1);            /* alt E -100 cents */
    tables(0, &g);
    CHECK(g.alt[4] == -100);
    CHECK(!memcmp(g.main, before.main, 12));     /* main untouched */
    maqam_page_view(0, &v);
    has("readout: 24-TET deviation + spelled name", v.line[0], "ALT 0 E" MAQAM_FLAT "4");   /* -100 on E = Eb, 0 c off the grid */
    has("name of the resulting pitch (Eb)", v.line[0], "E" MAQAM_FLAT "4");
    has("main entry shown too", v.line[1], "MAIN ");
    has("alt knob value is signed", v.knob[3][1], "-100");
    has("alt knob label", v.knob[3][0], "ALT");
    enc(5, 1); tables(0, &g); CHECK(g.alt[4] == -99);     /* +/-1 per detent, no +/-10 */
    enc(5, -1); tables(0, &g); CHECK(g.alt[4] == -100);
    enc(5, 127); tables(0, &g); CHECK(g.alt[4] == 27);
    enc(5, 127); enc(5, 127); tables(0, &g); CHECK(g.alt[4] == 127);   /* clamps at +127 */
    maqam_page_view(0, &v);
    has("alt knob at the top", v.knob[3][1], "+127");
    enc(5, -127); enc(5, -127); enc(5, -127); tables(0, &g); CHECK(g.alt[4] == -127);
    enc(5, -50); tables(0, &g); CHECK(g.alt[4] == -127);
    printf("%-52s ok\n", "alt edits clamp at -127..+127");
    enc(5, 27);                                  /* -100 */
    /* K3 selects another key (E -> G); edits go to it only */
    enc(4, 3);
    maqam_page_view(0, &v);
    CHECK(v.sel == (1U << 7));
    has("K3 key", v.knob[2][1], "G");
    enc(5, 5);
    tables(0, &g);
    CHECK(g.alt[7] == before.alt[7] + 5 && g.alt[4] == -100);
    enc(4, 9);                                   /* wraps: G + 9 = E */
    maqam_page_view(0, &v);
    CHECK(v.sel == (1U << 4));
    /* the alt row under the keys is the alt table, shown as cents from the nearest quarter tone */
    key(12); enc(5, 127); enc(5, 127);           /* F: +127 right next to E -100 ... */
    key(13); enc(5, -127); enc(5, -127);         /* ... and F#: -127 */
    key(11);                                     /* E selected again */
    maqam_page_view(0, &v);
    tables(0, &g);
    CHECK(g.alt[5] == 127 && g.alt[6] == -127);
    CHECK(v.cents[4] == 0 && v.cents[5] == -23 && v.cents[6] == 23);   /* -100, +127, -127 vs 24-TET */
    ms(300);
    cents_layout(&v);
    shot("scale_track_alt_extremes");     /* E -100, F +127, F# -127 side by side */
    key(12); enc(5, before.alt[5] - 127);        /* put F and F# back */
    key(13); enc(5, before.alt[6] + 127);
    key(11);
    ms(300);
    /* another track keeps its own alt table */
    tables(1, &g);
    CHECK(!memcmp(g.alt, before.alt, 12));
    /* a scale change resets the alt table (spec) */
    enc(5, 20);
    btn(SCL); btn(SCL);  /* TRACK -> CHORD -> SCALE */
    enc(4, 1);
    int8_t def[GROOVE_TUNING_KEYS];
    tables(0, &g);
    maqam_alt_default(maqam_project_scale(), 7, def);
    CHECK(!memcmp(g.alt, def, 12));
    /* a drum track: alt cannot be edited */
    btn(SCL);
    enc(0, 7);
    enc(5, 10);
    tables(7, &g);
    int8_t zero[24] = {0};
    CHECK(!memcmp(&g, zero, sizeof g));
    printf("alt: keys play and select (K3 too), K4 +/-1 cents clamped at -127..+127, readout and name\n");
}

/* ---- the sample-bank hint ---- */
static uint8_t *make_bank(unsigned chunks, unsigned *bytes_out)
{
    unsigned bytes = 512 + chunks * 228;
    uint8_t *b = calloc(1, bytes);
    uint32_t *h = (uint32_t *)(void *)b;
    h[0] = 0x53314d46U; h[1] = 1; h[2] = bytes; h[5] = 32000;
    unsigned next = 512, left = chunks;
    for (unsigned i = 0; i < 16; ++i) {
        uint32_t *e = (uint32_t *)(void *)(b + 32 + i * 28);
        unsigned n = i < 8 ? (i == 7 ? left : chunks / 8) : 0;
        left -= n;
        e[0] = next; e[1] = n * 320; e[2] = n * 228;
        if (n) { b[32 + i * 28 + 12] = 'P'; b[32 + i * 28 + 13] = (uint8_t)('0' + i); }
        next += n * 228;
    }
    for (unsigned p = 512; p < bytes; p += 228) { b[p] = 1; b[p + 1] = 0x45; b[p + 2] = 20; b[p + 3] = 0x5a; }
    uint32_t c = ~groove_crc32(~0U, b, bytes);
    memcpy(b + 12, &c, 4);
    *bytes_out = bytes;
    return b;
}
static void reduce_bank(void)
{
    sim_boot(1);
    maqam_init();
    unsigned bytes;
    uint8_t *b = make_bank(2028, &bytes);
    memcpy(sim_flash() + GROOVE_SAMPLE_OFFSET, b, bytes);
    free(b);
    groove_samples_init();
    for (unsigned i = 0; i < 4096 && groove_samples_status() == 1; ++i) groove_samples_poll();
    CHECK(bytes > GROOVE_SAMPLE_LIMIT);
    ms(50);
    open_scale();
    CHECK(page() == PAGE_SCALE);
    /* a WEST project has all-zero tables, so it saves fine next to the over-size bank */
    maqam_set_project(find("MAJ"), 0);
    ms(400);
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        struct groove_tuning z, zero;
        memset(&zero, 0, sizeof zero);
        tables(t, &z);
        CHECK(!memcmp(&z, &zero, sizeof z));
    }
    btn(SAVE); btn(SAVE); ms(300);
    CHECK(page() == PAGE_SCALE);                 /* a good save returns to the page it came from */
    ms(300);
    CHECK(ink(8, 182, 224, 8) == 0);
    /* a non-WEST scale makes the tables non-zero: the save is refused */
    maqam_set_project(find("RAST"), 0);
    ms(400);
    unsigned quiet = ink(8, 182, 224, 8);        /* the warning row is empty before a failed save */
    CHECK(quiet == 0);
    btn(SAVE);                                   /* the project page */
    CHECK(page() == PAGE_PROJECT);
    btn(SAVE);                                   /* save: nonzero tables + the over-size bank */
    ms(300);
    CHECK(page() == PAGE_PROJECT);
    shot("project_reduce_bank");
    btn(HOME);
    open_scale();
    ms(500);
    CHECK(page() == PAGE_SCALE);
    CHECK(ink(8, 182, 224, 8) > 100);            /* "SAVE FAILED: REDUCE SAMPLE BANK" */
    struct maqam_view v;
    maqam_page_view(0, &v);
    has("warn text (view)", v.warn, "REDUCE SAMPLE BANK");
    shot("scale_reduce_bank");
    /* back to a WEST scale: zero tables save, and the hint goes away once a save succeeds */
    maqam_set_project(find("MAJ"), 0);
    ms(300);
    btn(SAVE); btn(SAVE); ms(300);
    CHECK(page() == PAGE_SCALE);                 /* a good save returns to the page it came from */
    ms(300);
    CHECK(ink(8, 182, 224, 8) == 0);
    /* a save by any path updates it: groove_project_save_slot itself (SysEx saves, transfer.c) sets and clears the warning */
    maqam_init();
    maqam_page_view(0, &v);
    CHECK(!v.warn[0]);
    maqam_set_project(find("RAST"), 0);
    CHECK(groove_project_save_slot(0) == GROOVE_PROJECT_TUNING_SPACE);
    maqam_page_view(0, &v);
    has("direct refused save sets the warning", v.warn, "REDUCE SAMPLE BANK");
    maqam_set_project(find("MAJ"), 0);
    CHECK(groove_project_save_slot(0) == 0);
    maqam_page_view(0, &v);
    CHECK(!v.warn[0]);
    printf("sample bank: a refused save shows REDUCE SAMPLE BANK on the scale page; a good save clears it\n");
}

/* Undo restores the engine tables: the track labels follow them (both undo call sites call maqam_refresh). */
static void undo_labels(void)
{
    boot();
    unsigned rast = find("RAST"), bayat = find("BAYAT");
    maqam_set_project(rast, 0);
    ms(50);
    groove_edit_checkpoint(0x1234);              /* the state undo returns to: Rast on C everywhere */
    maqam_set_project(bayat, 4);
    CHECK(maqam_track_scale(0) == bayat && maqam_track_root(0) == 4);
    sim_control(0, HOME, 1); ms(30);             /* HOME + SAVE: undo */
    sim_control(0, SAVE, 1); ms(30);
    sim_control(0, SAVE, 0); sim_control(0, HOME, 0); ms(100);
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        if (!maqam_melodic(t)) continue;
        struct groove_tuning g;
        int8_t want[GROOVE_TUNING_KEYS];
        tables(t, &g);
        CHECK(maqam_main(maqam_track_scale(t), maqam_track_root(t), maqam_trad(), want) == 0 && !memcmp(g.main, want, sizeof want));
        CHECK(maqam_track_scale(t) == rast && maqam_track_root(t) == 0);
    }
    printf("undo (HOME+SAVE): the track labels match the restored tables\n");
}

static void pixels(void)
{
    boot();
    ms(400);
    shot("scale_default");
    CHECK(ink(8, 35, 230, 7) > 50);                  /* the section row: T1 DX7  SCALE  1/2 */
    unsigned base_sub = hash_rect(8, 46, 224, 16);
    CHECK(ink(8, 46, 224, 16) > 100);                /* subtitle text is painted */
    maqam_set_project(find("SIKAH"), 7);
    ms(300);
    CHECK(hash_rect(8, 46, 224, 16) != base_sub);    /* and repaints when the scale changes */
    CHECK(ink(8, 46, 224, 16) > 100);
    CHECK(ink(8, 78, 224, 8) > 20);                  /* the home hint row */
    shot("scale_sikah");
    maqam_set_project(find("RAST"), 0);
    ms(300);
    shot("scale_rast");
    enc(1, 1);
    ms(300);
    shot("track_follow");
    enc(2, 1); enc(3, 1);                            /* OWN, its own layout CHROM */
    key(11);
    enc(5, -100);
    ms(300);
    CHECK(ink(8, 134, 224, 16) > 100);               /* the large ALT readout */
    shot("track_own");
    /* a drum track on the SCALE page */
    enc(0, 7);
    enc(1, 1);
    ms(300);
    shot("scale_drums");
    enc(1, 1);
    ms(300);
    shot("track_drums");
    /* the step page with a plain and an alt step (the alt mark) */
    sim_boot(1);
    maqam_init();
    maqam_set_project(find("RAST"), 0);
    ms(50);
    btn(REC); key(11);
    sim_control(0, OCT_DN, 1); ms(20); key(11); sim_control(0, OCT_DN, 0); ms(20);
    key(12);
    btn(REC);
    btn(SEQ);
    ms(300);
    shot("steps_alt_mark");
    CHECK(page() == 1);
    /* the browser simulator asks for hit targets on every page: no crash on the new one */
    (void)sim_lcd_targets();
    CHECK(sim_lcd_count() <= 32);
    printf("pixels: title, subtitle, home hint, alt readout painted; PNGs in build/host/\n");
}

/* ---- the degree row and the spelled key strip (task 5.5) ---- */
static void expect_row(const char *what, const struct maqam_view *v, const char *const *want, unsigned n)
{
    int ok = v->ncell == n;
    for (unsigned i = 0; ok && i < n; ++i) ok = !strcmp(v->lab[i], want[i]);
    printf("%-52s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}
static void degree_row(void)
{
    struct maqam_view v;
    struct groove_tuning g;
    boot();
    maqam_set_project(find("BAYAT"), 4);          /* Bayati on D, TRAD */
    ms(300);
    maqam_page_view(0, &v);
    {   static const char *const want[] = {"D", "E" MAQAM_HF, "F", "G", "A", "B" MAQAM_FLAT, "C"};
        expect_row("Bayati on D: seven cells D Ed F G A Bb C", &v, want, 7); }
    CHECK(v.lit == 0x7f && v.root == 1);
    tables(0, &g);
    CHECK(g.main[4] == -47 && g.main[10] == 16);  /* the engine table stays relative to 12-TET */
    {   static const int8_t want[7] = {0, 3, -6, -2, 2, 16, -4};       /* shown vs 24-TET: Ed is -47 from E, +3 from E half-flat */
        CHECK(!memcmp(v.cents, want, 7)); }
    printf("%-52s ok\n", "Bayati row cents vs 24-TET: D 0, Ed +3, F -6, G -2, A +2, Bb +16, C -4");
    has("subtitle (spelled root)", v.sub, "D arab bayati");
    CHECK(ink(6, 92, 228, 22) > 500);
    shot("scale_row_bayati");
    maqam_set_project(find("RAST"), 0);
    ms(300);
    maqam_page_view(0, &v);
    {   static const char *const want[] = {"C", "D", "E" MAQAM_HF, "F", "G", "A", "B" MAQAM_FLAT "\x19", "B" MAQAM_HF "\x18"};
        expect_row("Rast on C: Bb marked down, Bd marked up", &v, want, 8); }
    shot("scale_row_rast");
    /* the SCALE page shows the row of the selected track's own scale */
    btn(SCL);
    enc(2, 1);                                    /* OWN */
    btn(SCL); btn(SCL);                           /* TRACK -> CHORD -> SCALE */
    enc(2, 4);                                    /* root D, this track only */
    maqam_page_view(0, &v);
    has("scale page: row of the track's own scale", v.lab[0], "D");
    CHECK(v.ncell >= 5);
    CHECK(maqam_project_root() == 0);
    /* every scale: the row has one cell per degree of the union, 5..9 cells (12 for WEST CHR) */
    unsigned bad = 0;
    btn(SCL); enc(2, -1); btn(SCL); btn(SCL);     /* FOLLOW again: the project is edited below */
    for (unsigned sc = 0; sc < NSCALES; ++sc) {
        maqam_set_project(sc, 0);
        maqam_page_view(0, &v);
        unsigned u = 0, un = scale_up(&SCALES[sc]) | scale_down(&SCALES[sc]);
        for (unsigned i = 0; i < 24; ++i) u += un >> i & 1U;
        bad += v.ncell != u || v.ncell < 5 || (v.ncell > 9 && sc != 0);
    }
    CHECK(bad == 0);
    printf("degree row: one cell per degree of the scale (5-9; 12 for chromatic)\n");
    /* a CUSTOM table keeps the 12-key strip */
    maqam_set_project(find("RAST"), 0);
    tables(0, &g);
    g.main[1] = 17;
    CHECK(groove_tuning_set(0, &g) == 0);
    maqam_refresh();
    maqam_page_view(0, &v);
    CHECK(v.custom && v.ncell == 0);
    /* ALT strip: all 12 keys, degree keys spelled, other keys dimmed with the fixed name */
    boot();
    maqam_set_project(find("BAYAT"), 4);
    btn(SCL);
    ms(300);
    maqam_page_view(0, &v);
    CHECK(v.ncell == 0);
    has("alt strip: A# key is labelled Bb", v.lab[10], "B" MAQAM_FLAT);
    {   static const char *const want[12] = {"C", "C#", "D", "D#", "E" MAQAM_HF, "F", "F#", "G", "G#", "A", "B" MAQAM_FLAT, "B"};
        unsigned ok = 1;
        for (unsigned k = 0; k < 12; ++k) if (k != 4 && k != 10 && strcmp(v.lab[k], want[k])) ok = 0;
        has("alt strip: E key reads Ed", v.lab[4], "E" MAQAM_HF);
        CHECK(ok);
    }
    CHECK(v.lit == ((1U << 2) | (1U << 4) | (1U << 5) | (1U << 7) | (1U << 9) | (1U << 10) | (1U << 0)));
    CHECK(!(v.lit & ((1U << 1) | (1U << 3) | (1U << 6) | (1U << 8))));    /* C# D# F# G# are dimmed */
    shot("track_bayati");
    printf("alt strip: degree keys spelled (Bb), C# D# F# G# dimmed with their fixed names\n");
    /* a step tile showing Bb4 */
    sim_boot(1);
    maqam_init();
    maqam_set_project(find("BAYAT"), 4);
    ms(50);
    btn(REC); key(17); key(11); btn(REC);
    btn(SEQ);
    ms(300);
    struct groove_step c;
    groove_sequence_read(0, 0, &c, 1);
    CHECK(c.note == 70);
    char nm[12];
    maqam_note_name(0, c.note, 0, nm);
    has("step 1 tile name", nm, "B" MAQAM_FLAT "4");
    shot("steps_bb4");
}

/* The CHORD page (task 3.2): K1 MODE, K2 TYPE, K3 INV, K4 VOICE of the selected track; the last chord, spelled, with its name. */
static void chord_page(void)
{
    struct maqam_view v;
    boot();
    maqam_set_project(find("RAST"), 0);
    btn(SCL); btn(SCL);                                  /* SCALE -> TRACK -> CHORD */
    maqam_page_view(0, &v);
    CHECK(sub_of() == 2 && !strcmp(v.title, "CHORD") && v.index == 3);
    has("MODE label", v.knob[0][0], "MODE");  has("MODE value", v.knob[0][1], "OFF");
    has("TYPE value", v.knob[1][1], "TRI");   has("INV value", v.knob[2][1], "ROOT");
    has("VOICE label", v.knob[3][0], "VOICE"); has("VOICE value", v.knob[3][1], "CLOS");
    CHECK(!v.sub[0] && !v.status[0]);                    /* no chord played yet */
    enc(2, 1);  CHECK(maqam_chord_get(0, MAQAM_CH_MODE) == 1);
    enc(3, 1);  CHECK(maqam_chord_get(0, MAQAM_CH_TYPE) == MAQAM_CHORD_7TH);
    enc(4, 1);  CHECK(maqam_chord_get(0, MAQAM_CH_INV) == MAQAM_INV_1ST);
    enc(5, 1);  CHECK(maqam_chord_get(0, MAQAM_CH_VOICE) == MAQAM_VOICE_OPEN);
    maqam_page_view(0, &v);
    has("MODE ON", v.knob[0][1], "ON");  has("TYPE 7TH", v.knob[1][1], "7TH");
    has("INV 1ST", v.knob[2][1], "1ST");  has("VOICE OPEN", v.knob[3][1], "OPEN");
    for (unsigned i = 0; i < 8; ++i) { enc(3, 1); enc(4, 1); enc(5, 1); }   /* each stops at its last value */
    maqam_page_view(0, &v);
    has("TYPE clamps at JINS", v.knob[1][1], "JINS");  has("INV clamps at AUTO", v.knob[2][1], "AUTO");
    has("VOICE clamps at BASS", v.knob[3][1], "BASS");
    for (unsigned i = 0; i < 8; ++i) { enc(2, -1); enc(3, -1); enc(4, -1); enc(5, -1); }
    CHECK(maqam_chord_get(0, MAQAM_CH_MODE) == 0 && maqam_chord_get(0, MAQAM_CH_TYPE) == 0 &&
          maqam_chord_get(0, MAQAM_CH_INV) == 0 && maqam_chord_get(0, MAQAM_CH_VOICE) == 0);
    /* settings belong to the selected track */
    sel_track(2);  enc(2, 1);
    CHECK(maqam_chord_get(2, MAQAM_CH_MODE) == 1 && maqam_chord_get(0, MAQAM_CH_MODE) == 0);
    maqam_page_view(2, &v);  has("track 3 MODE ON", v.knob[0][1], "ON");
    sel_track(0);
    enc(2, 1);                                           /* chord mode on track 1, TRIAD / ROOT / CLOSE */
    /* the C key on a Rast track plays C Ed G; the page shows it spelled with its name */
    key(7);
    maqam_page_view(0, &v);
    has("last chord spelled", v.sub, "C E" MAQAM_HF " G");
    has("last chord name", v.status, "NEU");
    ms(300);
    CHECK(ink(8, 46, 224, 16) > 100 && ink(8, 66, 224, 8) > 20);   /* the chord (large) and its name are painted */
    shot("scale_chord_rast");
    printf("%-52s ok\n", "CHORD page: C Ed G / NEU after the C key in Rast");
    /* a drum track has no chord mode: the knobs do not move */
    sel_track(7);
    enc(2, 1);
    CHECK(maqam_chord_get(7, MAQAM_CH_MODE) == 0);
    sel_track(0);
    printf("CHORD page: knobs MODE TYPE INV VOICE per track, clamped, drum tracks refuse\n");
}

int main(void)
{
    navigation();
    header_tag();
    frame_cycling();
    scale_page();
    scopes();
    custom_and_drums();
    layouts();
    alt_page();
    reduce_bank();
    degree_row();
    undo_labels();
    pixels();
    chord_page();
    printf(failures ? "FAILED: %d\n" : "pages_test: all ok\n", failures);
    return failures != 0;
}
