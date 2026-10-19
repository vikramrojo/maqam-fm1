/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Maqam scale layer (tasks 3.2-3.5 of maqam-scales): table maths and key assignment (cross-checked
 * against tools/gen_scales.py for every scale and root), pushing through engine.c, undo, the scale
 * trailer in the project record, stock V13 compatibility, and table -> scale matching.
 * Run from the repository root (the cross-check runs python3 tools/gen_scales.py). */
#define _POSIX_C_SOURCE 200809L /* popen/pclose under strict C11 libcs */
#include "internal.h"
#include "../../app/maqam/maqam.h"
#include "../../app/maqam/scales.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void sim_boot(unsigned clear);
const float *sim_render(void);
uint8_t *sim_flash(void);
uint32_t sim_flash_revision(void);

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

typedef struct groove_tuning tun8[GROOVE_TRACKS];
enum { C = 0, CS, D, DS, E, F, FS, G, GS, A, AS, B };
enum { SECTOR = 4096, STAGING = GROOVE_PROJECT_SLOTS };

static unsigned find(const char *code)
{
    for (unsigned i = 0; i < NSCALES; ++i)
        if (!strcmp(SCALE_CODE_STR[i], code)) return i;
    printf("FAIL no scale %s\n", code);
    ++failures;
    return 0;
}
static int wrap(int v, int m) { v %= m; if (v < 0) v += m; return v >= m / 2 ? v - m : v; }

/* ---- 3.2: pure maths ---- */
static void expect(const int8_t *got, const int8_t *want) { CHECK(!memcmp(got, want, GROOVE_TUNING_KEYS)); }
static void rast(void)
{
    unsigned rs = find("RAST");
    int8_t m[12];
    int8_t eq[12] = {0}, tr[12] = {0}, al[12] = {0};
    eq[E] = -50; eq[B] = -50;
    CHECK(maqam_main(rs, 0, 0, m) == 0); expect(m, eq);
    tr[E] = -43; tr[B] = -41; tr[AS] = 20; tr[D] = 4; tr[F] = -2; tr[G] = 2; tr[A] = 6;
    CHECK(maqam_main(rs, 0, 1, m) == 0); expect(m, tr);
    /* alt defaults: quarter-tone keys 0, semitone-degree keys -50, absent keys 0 */
    al[C] = al[D] = al[F] = al[G] = al[A] = al[AS] = -50;
    maqam_alt_default(rs, 0, m); expect(m, al);
    /* all WEST scales are zero under both intonations, at every semitone root */
    for (unsigned s = 0; s < NSCALES; ++s) {
        if (SCALES[s].set != SET_WEST) continue;
        for (unsigned r = 0; r < 24; r += 2)
            for (unsigned t = 0; t < 2; ++t) {
                int8_t z[12] = {0};
                CHECK(maqam_main(s, r, t, m) == 0); expect(m, z);
            }
    }
    /* WEST alt defaults are all zero at every root, including quarter-tone roots (where main is +/-50) */
    for (unsigned s = 0; s < NSCALES; ++s) {
        if (SCALES[s].set != SET_WEST) continue;
        for (unsigned r = 0; r < 24; ++r) {
            int8_t z[12] = {0};
            memset(m, 0x55, sizeof m);
            maqam_alt_default(s, r, m); expect(m, z);
        }
    }
    { int8_t z[12] = {0}; CHECK(maqam_main(find("CHR"), 1, 0, m) == 0 && m[C] != 0); maqam_alt_default(find("CHR"), 1, m); expect(m, z); }
    CHECK(maqam_main(NSCALES, 0, 1, m) < 0 && maqam_main(rs, 24, 1, m) < 0);
}
/* Key assignment equals the generator's for every scale x root; tables follow from it. */
static void cross_check(void)
{
    static const char *const names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    unsigned lines = 0, fits = 0;
    for (unsigned s = 0; s < NSCALES; ++s) {
        char cmd[96], line[256];
        snprintf(cmd, sizeof cmd, "python3 tools/gen_scales.py --assign %s", SCALE_CODE_STR[s]);
        FILE *p = popen(cmd, "r");
        if (!p) { CHECK(!"popen"); return; }
        while (fgets(line, sizeof line, p)) {
            char code[16];
            unsigned root;
            int n;
            int8_t ref[24], got[24];
            memset(ref, -1, sizeof ref);
            if (sscanf(line, "%15s root %u:%n", code, &root, &n) != 2 || strcmp(code, SCALE_CODE_STR[s]) || root > 23) {
                CHECK(!"parse"); continue;
            }
            for (const char *q = line + n; *q;) {
                unsigned slot; char key[4]; int used;
                if (sscanf(q, " %u->%3s%n", &slot, key, &used) != 2) break;
                for (unsigned k = 0; k < 12; ++k) if (!strcmp(key, names[k])) ref[slot] = (int8_t)k;
                q += used;
            }
            ++lines;
            CHECK(maqam_assign(s, root, got) == 0 && !memcmp(got, ref, sizeof ref));
            for (unsigned t = 0; t < 2; ++t) {   /* tables from the reference keys, independently */
                int8_t want[12] = {0}, m[12];
                for (unsigned sl = 0; sl < 24; ++sl) {
                    if (ref[sl] < 0) continue;
                    int d = wrap((int)root * 50 + (t ? scale_cents(&SCALES[s], sl) : (int)sl * 50) - ref[sl] * 100, 1200);
                    want[ref[sl]] = (int8_t)d;
                }
                if (maqam_main(s, root, t, m) == 0) { ++fits; expect(m, want); } else CHECK(!"fits");
            }
        }
        pclose(p);
    }
    CHECK(lines == NSCALES * 24U && fits == NSCALES * 48U);
    /* Rast on C+ (root 1): every degree sits 50 c higher than on C. */
    int8_t k0[24], k1[24], m[12];
    unsigned rs = find("RAST");
    CHECK(maqam_assign(rs, 1, k1) == 0 && maqam_assign(rs, 0, k0) == 0);
    CHECK(maqam_main(rs, 1, 0, m) == 0);
    for (unsigned s = 0; s < 24; ++s)
        if (k1[s] >= 0) CHECK(m[k1[s]] == wrap(((int)s + 1) * 50 - k1[s] * 100, 1200));
    CHECK(k1[7] == E && k1[0] == CS && k1[21] == B);     /* matches the generator: 7->E, 0->C#, 21->B */
}

/* The packed record has no padding: 29 bytes (off[] is indexed by the rank of the slot among the used ones), the
   accessors read the little-endian masks, and scale_cents() agrees with the exact
   cents the generator prints (--print, 0.1 c) to within the half cent of the whole-cent rounding. */
static void packing(void)
{
    CHECK(sizeof(scale_def_t) == 29 && sizeof SCALES == 29U * NSCALES);
    for (unsigned s = 0; s < NSCALES; ++s) {
        CHECK(SCALES[s].set < NSETS);
        CHECK(scale_popcount(scale_up(&SCALES[s]) | scale_down(&SCALES[s])) <= 12);
    }
    FILE *p = popen("python3 tools/gen_scales.py --print", "r");
    char line[512];
    unsigned rows = 0;
    CHECK(p != NULL);
    while (p && fgets(line, sizeof line, p)) {
        char code[16], set[16];
        int n;
        if (sscanf(line, "%15s %15s %*s %*s%n", code, set, &n) < 2) { CHECK(!"print row"); continue; }
        unsigned s = find(code), used = 0;
        CHECK(!strcmp(SET_NAMES[SCALES[s].set], set));
        for (const char *q = line + n; *q;) {
            unsigned slot; double c; int k;
            if (sscanf(q, " %u:%lf%n", &slot, &c, &k) != 2) break;
            q += k;
            CHECK(slot < 24 && ((scale_up(&SCALES[s]) | scale_down(&SCALES[s])) >> slot & 1U));
            CHECK(fabs(scale_cents(&SCALES[s], slot) - c) <= 0.55);
            ++used;
        }
        CHECK(used == scale_popcount(scale_up(&SCALES[s]) | scale_down(&SCALES[s])));
        ++rows;
    }
    if (p) pclose(p);
    CHECK(rows == NSCALES);
    unsigned rs = find("RAST");
    CHECK(scale_up(&SCALES[rs]) == 0x244491u && scale_down(&SCALES[rs]) == 0x144491u);
    CHECK(scale_cents(&SCALES[rs], 7) == 357 && scale_cents(&SCALES[rs], 21) == 1059);   /* 356.9 c, 1058.8 c */
}

/* ---- engine fixture ---- */
static void fresh(void)
{
    sim_boot(3);
    for (unsigned t = 0; t < 7; ++t) CHECK(groove_source_set(t, GROOVE_PLUCK) == 0);
    maqam_init();
    tun8 z; memset(z, 0, sizeof z);
    CHECK(groove_tuning_apply(z) == 0);
}
static void settle(void) { for (unsigned i = 0; i < 800; ++i) sim_render(); }
static void get(unsigned t, struct groove_tuning *g) { memset(g, 0x55, sizeof *g); groove_tuning_get(t, g); }
static int is_tables(const tun8 want)
{
    tun8 got;
    CHECK(groove_tuning_capture(got) == 0);
    return !memcmp(got, want, sizeof got);
}
static int track_is(unsigned t, unsigned scale, unsigned root, unsigned trad, int alt_default)
{
    struct groove_tuning g, w;
    memset(&w, 0, sizeof w);
    get(t, &g);
    if (maqam_main(scale, root, trad, w.main)) return 0;
    if (alt_default) maqam_alt_default(scale, root, w.alt); else memcpy(w.alt, g.alt, sizeof w.alt);
    return !memcmp(&g, &w, sizeof g);
}

/* ---- 3.3 ---- */
static void state_push(void)
{
    unsigned rs = find("RAST"), by = find("BAYAT");
    fresh();
    CHECK(maqam_project_scale() == 0 && maqam_project_root() == 0 && maqam_trad() == 1);
    CHECK(maqam_set_project(rs, 0) == 0);
    for (unsigned t = 0; t < 7; ++t) CHECK(track_is(t, rs, 0, 1, 1));
    struct groove_tuning g, z;
    memset(&z, 0, sizeof z);
    get(7, &g); CHECK(!memcmp(&g, &z, sizeof g));                    /* drum track untouched */
    /* dirty: a tuning-only change marks the project changed */
    CHECK(groove_project_save_slot(1) == 0);
    settle(); CHECK(groove_project_dirty() == 0);
    CHECK(maqam_set_project(by, 4) == 0);
    settle(); CHECK(groove_project_dirty() != 0);
    CHECK(maqam_set_project(rs, 0) == 0);
    /* alt edits survive an intonation toggle, main changes */
    CHECK(maqam_set_alt(1, E, -100) == 0 && maqam_set_alt(1, E, 128) < 0 && maqam_set_alt(7, E, 1) < 0);
    get(1, &g); CHECK(g.alt[E] == -100 && g.main[E] == -43);
    CHECK(maqam_set_intonation(0) == 0 && maqam_trad() == 0);
    get(1, &g); CHECK(g.alt[E] == -100 && g.main[E] == -50 && g.main[B] == -50 && g.alt[C] == -50);
    CHECK(track_is(0, rs, 0, 0, 1));
    CHECK(maqam_set_intonation(1) == 0);
    /* a project-scale change to the same scale/root keeps alt edits; a different one resets them */
    CHECK(maqam_set_project(rs, 0) == 0);
    get(1, &g); CHECK(g.alt[E] == -100);
    CHECK(maqam_set_project(rs, 2) == 0);
    get(1, &g); CHECK(g.alt[E] == 0 && track_is(1, rs, 2, 1, 1));
    CHECK(maqam_set_project(rs, 0) == 0);
    /* per-track override: two maqamat at once */
    CHECK(maqam_set_override(2, by, 4) == 0 && maqam_overridden(2));
    CHECK(track_is(2, by, 4, 1, 1));
    for (unsigned t = 0; t < 7; ++t) if (t != 2) CHECK(track_is(t, rs, 0, 1, 1));
    CHECK(maqam_set_override(7, by, 4) < 0 && maqam_set_override(2, NSCALES, 0) < 0 && maqam_set_override(2, by, 24) < 0);
    CHECK(maqam_set_project(find("HICAZ"), 0) == 0);                  /* override stays */
    CHECK(track_is(2, by, 4, 1, 1) && maqam_track_scale(2) == by && maqam_track_scale(3) == find("HICAZ"));
    CHECK(maqam_clear_override(2) == 0 && !maqam_overridden(2));
    CHECK(track_is(2, find("HICAZ"), 0, 1, 1));
    CHECK(maqam_set_project(rs, 0) == 0);
    /* non-melodic: an 808 track is not retuned */
    CHECK(groove_source_set(5, GROOVE_808) == 0);
    CHECK(maqam_melodic(5) == 0 && maqam_melodic(7) == 0 && maqam_melodic(4));
    get(5, &g);
    CHECK(maqam_set_project(by, 4) == 0);
    get(5, &z); CHECK(!memcmp(&g, &z, sizeof g));                     /* no push to the 808 track */
    CHECK(maqam_set_project(rs, 0) == 0);
}
static void undo(void)
{
    unsigned rs = find("RAST"), by = find("BAYAT");
    fresh();
    CHECK(maqam_set_project(rs, 0) == 0);
    tun8 a, b;
    CHECK(groove_tuning_capture(a) == 0);
    CHECK(groove_project_save_slot(1) == 0);
    CHECK(maqam_set_project(by, 4) == 0);
    CHECK(groove_tuning_capture(b) == 0);
    CHECK(groove_project_load_slot(1) == 0 && is_tables(a));          /* load checkpoints first */
    CHECK(maqam_project_scale() == rs);
    CHECK(groove_edit_undo() == 0 && is_tables(b));                   /* V13 restores the tables */
    maqam_refresh();
    CHECK(is_tables(b));                                              /* refresh never writes tables */
    CHECK(maqam_track_scale(0) == by && maqam_track_root(0) == 4 && maqam_overridden(0));
    CHECK(groove_edit_undo() == 0 && is_tables(a));                   /* redo */
    maqam_refresh();
    CHECK(maqam_track_scale(0) == rs && maqam_track_root(0) == 0);   /* relabelled by matching */
}

/* ---- persistence (V14: maqam settings in their own sector, app/maqam/storage.c) ---- */
static uint8_t *sector(unsigned n) { return sim_flash() + n * SECTOR; }
/* A slot's entry in the maqam sector: key (4), settings (8), crc (4) after an 8-byte header. */
static uint8_t *entry(unsigned slot) { return sim_flash() + GROOVE_MAQAM_OFFSET + 8 + slot * 16; }
static uint8_t *ms(unsigned slot) { return entry(slot) + 4; }
static void entry_fix(unsigned slot)
{
    uint32_t crc = ~groove_crc32(~0U, entry(slot), 12);
    memcpy(entry(slot) + 12, &crc, 4);
}
static void wipe_tail(unsigned slot) { memset(entry(slot), 0xff, 16); }
/* V14 record checksum: header up to crc, the 12 settings bytes, the project. */
static uint32_t v6_crc(const uint8_t *s)
{
    uint32_t crc = groove_crc32(~0U, s, 12);
    crc = groove_crc32(crc, s + 4084, 12);
    return ~groove_crc32(crc, s + 20, 4064);
}
/* The V5 formula (V13 and the V13 fork): header up to crc, 4 settings bytes, the project. */
static uint32_t v5_crc(const uint8_t *s)
{
    uint32_t crc = groove_crc32(~0U, s, 12);
    crc = groove_crc32(crc, s + 4084, 4);
    return ~groove_crc32(crc, s + 20, 4064);
}
static void trailer(void)
{
    unsigned t7 = find("T.RST"), by = find("BAYAT");
    fresh();
    CHECK(maqam_set_project(t7, 4) == 0 && maqam_set_override(2, by, 4) == 0);
    CHECK(maqam_set_alt(3, G, 17) == 0);
    tun8 want;
    CHECK(groove_tuning_capture(want) == 0);
    CHECK(groove_project_save_slot(3) == 0);
    const uint8_t *s = ms(3);
    uint8_t x = 0;
    for (unsigned i = 0; i < 7; ++i) x ^= s[i];
    CHECK(s[0] == 0xA1 && s[1] == t7 && s[2] == 4 && s[3] == 1 && s[4] == 4 && !s[5] && !s[6] && s[7] == (uint8_t)~x);
    /* the project record is V14's own, its checksum unchanged by maqam */
    uint32_t crc;
    memcpy(&crc, sector(3) + 12, 4);
    CHECK(crc == v6_crc(sector(3)));
    CHECK(sector(STAGING)[0] == 0xff);                               /* the staged write finished */
    /* an unchanged save writes nothing */
    uint32_t rev = sim_flash_revision();
    CHECK(groove_project_save_slot(3) == 0 && sim_flash_revision() == rev);
    /* round trip */
    maqam_init();
    tun8 z; memset(z, 0, sizeof z);
    CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(3) == 0 && is_tables(want));
    CHECK(maqam_project_scale() == t7 && maqam_project_root() == 4 && maqam_trad() == 1);
    CHECK(maqam_overridden(2) && maqam_track_scale(2) == by && maqam_track_root(2) == 4);
    for (unsigned t = 0; t < 7; ++t) if (t != 2) CHECK(!maqam_overridden(t) && maqam_track_scale(t) == t7);
    /* EQUAL is saved in flag bit 0; the label of a recovered track may be an earlier scale with the same table */
    CHECK(maqam_set_intonation(0) == 0 && groove_tuning_capture(want) == 0 && groove_project_save_slot(3) == 0);
    CHECK((ms(3)[3] & 1) == 0);
    maqam_init(); CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(3) == 0 && is_tables(want) && maqam_trad() == 0 && maqam_overridden(2));
    CHECK(track_is(2, maqam_track_scale(2), maqam_track_root(2), 0, 0));
    /* a settings-only change rewrites the entry; the record and tables are unchanged */
    maqam_init();
    CHECK(groove_project_save_slot(3) == 0 && ms(3)[4] == 0 && sector(STAGING)[0] == 0xff);
    CHECK(groove_project_load_slot(3) == 0 && is_tables(want));
    /* the device-settings rewrite of slot 15 leaves its maqam entry alone */
    CHECK(groove_project_save_slot(GROOVE_PROJECT_SLOTS - 1) == 0);
    uint8_t before[8];
    memcpy(before, ms(GROOVE_PROJECT_SLOTS - 1), 8);
    CHECK(before[0] == 0xA1);
    groove_settings_clock(GROOVE_CLOCK_MIDI);
    settle(); settle();
    CHECK(groove_settings_status() == 0);
    CHECK(!memcmp(ms(GROOVE_PROJECT_SLOTS - 1), before, 8));
    groove_settings_clock(GROOVE_CLOCK_INTERNAL);
    settle(); settle();

    /* a stock project (no entry, no tables): WEST / C / TRAD, zero tables */
    fresh();
    CHECK(groove_project_save_slot(4) == 0);
    wipe_tail(4);
    CHECK(maqam_set_project(t7, 4) == 0);
    CHECK(groove_project_load_slot(4) == 0 && is_tables(z));
    CHECK(maqam_project_scale() == 0 && maqam_project_root() == 0 && maqam_trad() == 1);
    for (unsigned t = 0; t < 7; ++t) CHECK(!maqam_overridden(t) && maqam_track_scale(t) == 0);

    /* a stock project with tables: tables kept, scale derived (BAYAT on D, TRAD), no entry */
    fresh();
    struct groove_tuning g;
    memset(&g, 0, sizeof g);
    CHECK(maqam_main(by, 4, 1, g.main) == 0);
    CHECK(groove_tuning_set(1, &g) == 0);
    tun8 stock;
    CHECK(groove_tuning_capture(stock) == 0);
    CHECK(groove_project_save_slot(5) == 0);
    wipe_tail(5);
    maqam_init(); CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(5) == 0 && is_tables(stock));
    CHECK(maqam_overridden(1) && maqam_track_scale(1) == by && maqam_track_root(1) == 4);
    CHECK(!maqam_overridden(0));
    /* a damaged entry (settings check byte, or the entry crc) is treated as absent */
    CHECK(groove_project_save_slot(5) == 0);
    ms(5)[7] ^= 1; entry_fix(5);
    maqam_init(); CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(5) == 0 && is_tables(stock) && maqam_project_scale() == 0 && maqam_track_scale(1) == by);
    CHECK(groove_project_save_slot(5) == 0 && ms(5)[0] == 0xA1);
    entry(5)[13] ^= 1;
    maqam_init(); CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(5) == 0 && maqam_project_scale() == 0);
    CHECK(groove_project_save_slot(5) == 0 && ms(5)[0] == 0xA1);   /* a save restores the entry */
}

/* Settings belong to the exact project: a slot replaced elsewhere (here: its tempo edited and
   the record checksum fixed, as another firmware's save would) loads the defaults. */
static void replaced(void)
{
    unsigned rs = find("RAST");
    fresh();
    CHECK(maqam_set_project(rs, 0) == 0);
    CHECK(groove_project_save_slot(8) == 0 && ms(8)[0] == 0xA1);
    sector(8)[20 + 3832] ^= 1;                                       /* the project's tempo (120 -> 121) */
    uint32_t crc = v6_crc(sector(8));
    memcpy(sector(8) + 12, &crc, 4);
    tun8 z; memset(z, 0, sizeof z);
    maqam_init(); CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(8) == 0 && maqam_project_scale() == 0);
    printf("replaced: a slot changed elsewhere loads default maqam settings\n");
}

/* A project saved by the V13 fork: a V5 record with the 8 settings bytes right after it. It loads with
   them; the next save stores an entry and V14 rewrites the record as V6. */
static void migrate(void)
{
    unsigned t7 = find("T.RST");
    fresh();
    CHECK(maqam_set_project(t7, 4) == 0);
    CHECK(groove_project_save_slot(9) == 0);
    uint8_t old[8];
    memcpy(old, ms(9), 8);
    wipe_tail(9);
    uint8_t *s = sector(9);
    uint32_t magic = 0x35504d46U;                                    /* V5_PROJECT_MAGIC */
    memcpy(s, &magic, 4);
    memset(s + 4084, 0, 4);                                          /* V5 settings: 4 bytes */
    memcpy(s + 4088, old, 8);                                        /* the V13 fork's trailer */
    uint32_t crc = v5_crc(s);
    memcpy(s + 12, &crc, 4);
    tun8 z; memset(z, 0, sizeof z);
    maqam_init(); CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(9) == 0 && maqam_project_scale() == t7 && maqam_project_root() == 4);
    CHECK(groove_project_save_slot(9) == 0 && !memcmp(ms(9), old, 8));
    memcpy(&magic, sector(9), 4);
    CHECK(magic == 0x36504d46U);                                     /* PROJECT_MAGIC (V6) */
    maqam_init(); CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(9) == 0 && maqam_project_scale() == t7);
    printf("migrate: a V13-fork V5 record keeps its settings and is saved as V6 with an entry\n");
}

/* An older sample bank reaching into the maqam sector: non-default settings are not saved (the stock
   REDUCE SAMPLE BANK result), default settings save without a warning; the project itself is saved. */
static void covered(void)
{
    unsigned rs = find("RAST");
    fresh();
    CHECK(GROOVE_SAMPLE_LIMIT == 442368 && GROOVE_MAQAM_OFFSET + 4096 == GROOVE_LIBRARY_OFFSET);
    uint32_t h[3] = {0x53314d46U, 1, 445000};
    memcpy(sim_flash() + GROOVE_SAMPLE_OFFSET, h, sizeof h);
    CHECK(groove_project_save_slot(10) == 0);                       /* defaults: nothing to keep */
    CHECK(maqam_set_project(rs, 0) == 0);
    CHECK(groove_project_save_slot(10) == GROOVE_PROJECT_TUNING_SPACE && ms(10)[0] == 0xff);
    memset(sim_flash() + GROOVE_SAMPLE_OFFSET, 0xff, sizeof h);
    CHECK(groove_project_save_slot(10) == 0 && ms(10)[0] == 0xA1);
    printf("covered: an old sample bank over the sector blocks non-default settings only\n");
}

/* Power loss around the staged write: a committed stage is completed on the next storage access,
   an uncommitted one is dropped and the old sector stands. Other slots keep their entries. */
static void staged(void)
{
    unsigned rs = find("RAST"), t7 = find("T.RST");
    fresh();
    CHECK(maqam_set_project(rs, 0) == 0 && groove_project_save_slot(11) == 0);
    CHECK(maqam_set_project(t7, 0) == 0 && groove_project_save_slot(12) == 0);
    uint8_t keep11[16], body[4096];
    memcpy(keep11, entry(11), 16);
    /* build the stage of a write that changed slot 12, as storage.c does */
    enum { HDR = 12 };
    memcpy(body, sim_flash() + GROOVE_MAQAM_OFFSET, 8 + 16 * GROOVE_PROJECT_SLOTS);
    body[8 + 12 * 16 + 4 + 2] = 7;                                   /* slot 12: root 7 */
    uint8_t *e12 = body + 8 + 12 * 16, x = 0;
    for (unsigned i = 0; i < 7; ++i) x ^= e12[4 + i];
    e12[4 + 7] = (uint8_t)~x;
    uint32_t ecrc = ~groove_crc32(~0U, e12, 12);
    memcpy(e12 + 12, &ecrc, 4);
    uint32_t bcrc = ~groove_crc32(~0U, body, 8 + 16 * GROOVE_PROJECT_SLOTS);
    for (unsigned committed = 0; committed < 2; ++committed) {
        uint8_t *st = sector(STAGING);
        uint32_t hdr[3] = {0x31514d46U, bcrc, committed ? 0U : ~0U};
        memset(st, 0xff, SECTOR);
        memcpy(st, hdr, HDR);
        memcpy(st + HDR, body, 8 + 16 * GROOVE_PROJECT_SLOTS);
        CHECK(groove_storage_prepare() == 0 && st[0] == 0xff);       /* the stage is consumed */
        CHECK(ms(12)[2] == (committed ? 7 : 0));
        CHECK(!memcmp(entry(11), keep11, 16));
    }
    /* power lost mid-copy: the stage is committed and the sector already erased */
    {
        uint8_t *st = sector(STAGING);
        uint32_t hdr[3] = {0x31514d46U, bcrc, 0U};
        body[8 + 12 * 16 + 4 + 2] = 9; x = 0;
        for (unsigned i = 0; i < 7; ++i) x ^= e12[4 + i];
        e12[4 + 7] = (uint8_t)~x;
        ecrc = ~groove_crc32(~0U, e12, 12); memcpy(e12 + 12, &ecrc, 4);
        hdr[1] = ~groove_crc32(~0U, body, 8 + 16 * GROOVE_PROJECT_SLOTS);
        memset(st, 0xff, SECTOR);
        memcpy(st, hdr, HDR);
        memcpy(st + HDR, body, 8 + 16 * GROOVE_PROJECT_SLOTS);
        memset(sim_flash() + GROOVE_MAQAM_OFFSET, 0xff, SECTOR);
        CHECK(groove_storage_prepare() == 0 && st[0] == 0xff && ms(12)[2] == 9 && !memcmp(entry(11), keep11, 16));
    }
    printf("staged: a committed stage completes (also over an erased sector), an uncommitted one is dropped, other slots unaffected\n");
}
static void chords(unsigned track, unsigned type, unsigned inv, unsigned voice)
{
    CHECK(maqam_chord_set(track, MAQAM_CH_MODE, 1) == 0 && maqam_chord_set(track, MAQAM_CH_TYPE, type) == 0);
    CHECK(maqam_chord_set(track, MAQAM_CH_INV, inv) == 0 && maqam_chord_set(track, MAQAM_CH_VOICE, voice) == 0);
}
static void patch(unsigned slot, unsigned b5, unsigned b6)   /* new [5], [6] and check byte in the slot's entry */
{
    uint8_t *t = ms(slot), x = 0;
    t[5] = (uint8_t)b5;
    t[6] = (uint8_t)b6;
    for (unsigned i = 0; i < 7; ++i) x ^= t[i];
    t[7] = (uint8_t)~x;
    entry_fix(slot);
}
static void reload(unsigned slot)
{
    tun8 z; memset(z, 0, sizeof z);
    maqam_init();
    CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(slot) == 0);
}
static unsigned chord_is(unsigned t, unsigned mode, unsigned type, unsigned inv, unsigned voice)
{
    return maqam_chord_get(t, MAQAM_CH_MODE) == mode && maqam_chord_get(t, MAQAM_CH_TYPE) == type &&
           maqam_chord_get(t, MAQAM_CH_INV) == inv && maqam_chord_get(t, MAQAM_CH_VOICE) == voice;
}
static void chord_trailer(void)
{
    unsigned rs = find("RAST");
    /* round trip: track 2 (index 1... the third track) in chord mode with 7TH / AUTO / OPEN */
    fresh();
    CHECK(maqam_set_project(rs, 0) == 0);
    chords(2, MAQAM_CHORD_7TH, MAQAM_INV_AUTO, MAQAM_VOICE_OPEN);
    CHECK(groove_project_save_slot(6) == 0);
    CHECK(ms(6)[5] == 4 && ms(6)[6] == (1 | 4 << 3 | 1 << 6));
    reload(6);
    CHECK(maqam_project_scale() == rs && chord_is(2, 1, MAQAM_CHORD_7TH, MAQAM_INV_AUTO, MAQAM_VOICE_OPEN));
    CHECK(chord_is(0, 0, MAQAM_CHORD_7TH, MAQAM_INV_AUTO, MAQAM_VOICE_OPEN));     /* every track gets the one set; only the mask differs */
    /* type 7 (DRN5) round trips with the largest inversion and voicing */
    fresh();
    CHECK(maqam_set_project(rs, 0) == 0);
    chords(0, MAQAM_CHORD_DRN5, MAQAM_INV_AUTO, MAQAM_VOICE_BASS);
    CHECK(groove_project_save_slot(6) == 0);
    CHECK(ms(6)[6] == (7 | 4 << 3 | 2 << 6));
    reload(6);
    CHECK(chord_is(0, 1, MAQAM_CHORD_DRN5, MAQAM_INV_AUTO, MAQAM_VOICE_BASS));
    /* lossy: tracks 2 and 4 (indexes 1 and 3) differ, both load with the lowest-numbered track's set */
    fresh();
    CHECK(maqam_set_project(rs, 0) == 0);
    chords(3, MAQAM_CHORD_DRN, MAQAM_INV_1ST, MAQAM_VOICE_BASS);
    chords(1, MAQAM_CHORD_7TH, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE);
    CHECK(groove_project_save_slot(6) == 0);
    CHECK(ms(6)[5] == (2 | 8) && ms(6)[6] == MAQAM_CHORD_7TH);
    reload(6);
    CHECK(chord_is(1, 1, MAQAM_CHORD_7TH, 0, 0) && chord_is(3, 1, MAQAM_CHORD_7TH, 0, 0) && chord_is(2, 0, MAQAM_CHORD_7TH, 0, 0));
    /* no track in chord mode: the defaults are saved whatever the other fields hold */
    fresh();
    CHECK(maqam_chord_set(2, MAQAM_CH_TYPE, MAQAM_CHORD_DRN) == 0 && groove_project_save_slot(7) == 0);
    CHECK(!ms(7)[5] && !ms(7)[6]);
    /* an old trailer ([5] = [6] = 0): chord mode off, TRIAD / ROOT / CLOSE; a chord-less project leaves the scale alone */
    fresh();
    CHECK(maqam_set_project(rs, 0) == 0);
    chords(2, MAQAM_CHORD_5P8, MAQAM_INV_2ND, MAQAM_VOICE_BASS);
    CHECK(groove_project_save_slot(6) == 0);
    patch(6, 0, 0);
    reload(6);
    CHECK(maqam_project_scale() == rs);
    for (unsigned t = 0; t < 8; ++t) CHECK(chord_is(t, 0, MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE));
    /* out-of-range values (inversion 5, voice 3; every 3-bit type is valid) make the whole trailer invalid: defaults, the tables decide */
    static const unsigned bad[2] = {5 << 3, 3 << 6};
    for (unsigned k = 0; k < 2; ++k) {
        patch(6, 1, bad[k]);
        reload(6);
        CHECK(maqam_project_scale() == 0);
        for (unsigned t = 0; t < 8; ++t) CHECK(chord_is(t, 0, MAQAM_CHORD_TRIAD, MAQAM_INV_ROOT, MAQAM_VOICE_CLOSE));
    }
    patch(6, 1, MAQAM_CHORD_DRN5 | MAQAM_INV_AUTO << 3 | MAQAM_VOICE_BASS << 6);   /* the largest valid values load */
    reload(6);
    CHECK(maqam_project_scale() == rs && chord_is(0, 1, MAQAM_CHORD_DRN5, MAQAM_INV_AUTO, MAQAM_VOICE_BASS));
    printf("chord trailer: round trip, lossy for several tracks, old trailer defaults, out-of-range rejected\n");
}

/* ---- 3.5 ---- */
static void matching(void)
{
    unsigned rs = find("RAST"), by = find("BAYAT");
    unsigned s, r;
    int8_t m[12];
    CHECK(maqam_main(by, 4, 1, m) == 0 && maqam_match(m, 1, &s, &r) == 0 && s == by && r == 4);
    CHECK(maqam_main(by, 4, 0, m) == 0 && maqam_match(m, 0, &s, &r) == 0);   /* EQUAL tables are shared: first hit */
    { int8_t q[12]; CHECK(maqam_main(s, r, 0, q) == 0 && !memcmp(q, m, 12) && s <= by); }
    memset(m, 0, 12);
    CHECK(maqam_match(m, 1, &s, &r) == 0 && s == 0 && r == 0);          /* zero = CHR on C */
    for (unsigned i = 0; i < 12; ++i) m[i] = (int8_t)(7 * (int)i - 31);
    CHECK(maqam_match(m, 1, &s, &r) < 0);

    fresh();
    CHECK(maqam_set_project(rs, 0) == 0 && maqam_set_override(2, by, 4) == 0);
    struct groove_tuning h;
    for (unsigned i = 0; i < 12; ++i) { h.main[i] = (int8_t)((i * 37 + 11) % 61 - 30); h.alt[i] = (int8_t)(i * 5 - 20); }
    CHECK(groove_tuning_set(3, &h) == 0 && groove_project_save_slot(6) == 0);   /* hand-edited track 3 */
    tun8 want;
    CHECK(groove_tuning_capture(want) == 0);
    maqam_init();
    tun8 z; memset(z, 0, sizeof z);
    CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(6) == 0 && is_tables(want));
    CHECK(maqam_project_scale() == rs && maqam_track_scale(2) == by && maqam_track_root(2) == 4);
    CHECK(maqam_overridden(3) && maqam_track_scale(3) == MAQAM_CUSTOM);   /* shown as CUSTOM */
    CHECK(!maqam_overridden(0) && maqam_track_scale(0) == rs);
    CHECK(is_tables(want));                                             /* tables not changed by loading */
    CHECK(maqam_set_project(find("HICAZ"), 0) == 0);                    /* nor by project changes */
    struct groove_tuning g;
    get(3, &g); CHECK(!memcmp(&g, &h, sizeof g));
    CHECK(maqam_set_intonation(0) == 0);
    get(3, &g); CHECK(!memcmp(&g, &h, sizeof g));
    CHECK(maqam_clear_override(3) == 0);                                /* clearing re-follows the project */
    CHECK(track_is(3, find("HICAZ"), 0, 0, 1));
}

int main(void)
{
    rast();
    packing();
    cross_check();
    state_push();
    undo();
    trailer();
    replaced();
    migrate();
    covered();
    staged();
    chord_trailer();
    matching();
    if (failures) { printf("maqam_test: %d failure(s)\n", failures); return 1; }
    printf("maqam_test: ok\n");
    return 0;
}
