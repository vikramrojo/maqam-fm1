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
enum { SECTOR = 4096, TAIL = 4088, STAGING = GROOVE_PROJECT_SLOTS };

static unsigned find(const char *code)
{
    for (unsigned i = 0; i < NSCALES; ++i)
        if (!strcmp(SCALES[i].code, code)) return i;
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
        snprintf(cmd, sizeof cmd, "python3 tools/gen_scales.py --assign %s", SCALES[s].code);
        FILE *p = popen(cmd, "r");
        if (!p) { CHECK(!"popen"); return; }
        while (fgets(line, sizeof line, p)) {
            char code[16];
            unsigned root;
            int n;
            int8_t ref[24], got[24];
            memset(ref, -1, sizeof ref);
            if (sscanf(line, "%15s root %u:%n", code, &root, &n) != 2 || strcmp(code, SCALES[s].code) || root > 23) {
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
                    int d = t ? wrap((int)root * 500 + SCALES[s].cents_x10[sl] - ref[sl] * 1000, 12000)
                              : wrap(((int)root + (int)sl) * 50 - ref[sl] * 100, 1200) * 10;
                    d = d >= 0 ? (d + 5) / 10 : -((-d + 5) / 10);
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

/* ---- persistence ---- */
static uint32_t record_crc(const uint8_t *s) /* the V13 formula: header up to crc, settings, project */
{
    uint32_t crc = groove_crc32(~0U, s, 12);
    crc = groove_crc32(crc, s + 4084, 4);
    return ~groove_crc32(crc, s + 20, 4064);
}
static uint8_t *sector(unsigned n) { return sim_flash() + n * SECTOR; }
static void wipe_tail(unsigned slot) { memset(sector(slot) + TAIL, 0xff, 8); memset(sector(STAGING) + TAIL, 0xff, 8); }
static void trailer(void)
{
    unsigned t7 = find("T.RST"), by = find("BAYAT");
    fresh();
    CHECK(maqam_set_project(t7, 4) == 0 && maqam_set_override(2, by, 4) == 0);
    CHECK(maqam_set_alt(3, G, 17) == 0);
    tun8 want;
    CHECK(groove_tuning_capture(want) == 0);
    CHECK(groove_project_save_slot(3) == 0);
    const uint8_t *s = sector(3);
    uint8_t x = 0;
    for (unsigned i = 0; i < 7; ++i) x ^= s[TAIL + i];
    CHECK(s[TAIL] == 0xA1 && s[TAIL + 1] == t7 && s[TAIL + 2] == 4 && s[TAIL + 3] == 1 && s[TAIL + 4] == 4 &&
          !s[TAIL + 5] && !s[TAIL + 6] && s[TAIL + 7] == (uint8_t)~x);
    /* stock V13 compatibility: the 4088 B record and its CRC are what V13 writes */
    uint32_t crc;
    memcpy(&crc, s + 12, 4);
    CHECK(crc == record_crc(s));
    struct groove_project *p = groove_project_scratch();
    CHECK(groove_project_capture(p) == 0 && !memcmp(s + 20, p, 4064));
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
    CHECK((sector(3)[TAIL + 3] & 1) == 0);
    maqam_init(); CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(3) == 0 && is_tables(want) && maqam_trad() == 0 && maqam_overridden(2));
    CHECK(track_is(2, maqam_track_scale(2), maqam_track_root(2), 0, 0));
    /* the staging copy carries the trailer: a trailer-only change rewrites the record, tables unchanged */
    maqam_init();
    CHECK(groove_project_save_slot(3) == 0);
    CHECK(!memcmp(sector(3) + TAIL, sector(STAGING) + TAIL, 8) && sector(3)[TAIL + 4] == 0);
    CHECK(!memcmp(sector(3), sector(STAGING), TAIL) && record_crc(sector(STAGING)) == record_crc(sector(3)));
    CHECK(groove_project_load_slot(3) == 0 && is_tables(want));
    /* the settings-only rewrite of slot 15 keeps the trailer of the project stored there */
    CHECK(groove_project_save_slot(GROOVE_PROJECT_SLOTS - 1) == 0);
    uint8_t before[8];
    memcpy(before, sector(GROOVE_PROJECT_SLOTS - 1) + TAIL, 8);
    CHECK(before[0] == 0xA1);
    groove_settings_clock(GROOVE_CLOCK_MIDI);
    settle(); settle();
    CHECK(groove_settings_status() == 0);
    CHECK(!memcmp(sector(GROOVE_PROJECT_SLOTS - 1) + TAIL, before, 8));
    CHECK(record_crc(sector(GROOVE_PROJECT_SLOTS - 1)) == *(const uint32_t *)(const void *)(sector(GROOVE_PROJECT_SLOTS - 1) + 12));
    groove_settings_clock(GROOVE_CLOCK_INTERNAL);
    settle(); settle();

    /* a stock project (no trailer, no tables): WEST / C / TRAD, zero tables */
    fresh();
    CHECK(groove_project_save_slot(4) == 0);
    wipe_tail(4);
    CHECK(maqam_set_project(t7, 4) == 0);
    CHECK(groove_project_load_slot(4) == 0 && is_tables(z));
    CHECK(maqam_project_scale() == 0 && maqam_project_root() == 0 && maqam_trad() == 1);
    for (unsigned t = 0; t < 7; ++t) CHECK(!maqam_overridden(t) && maqam_track_scale(t) == 0);

    /* a stock V13 project with tables: tables kept, scale derived (BAYAT on D, TRAD), no trailer */
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
    /* a damaged check byte or tag is treated as absent */
    CHECK(groove_project_save_slot(5) == 0);
    sector(5)[TAIL + 7] ^= 1; sector(STAGING)[TAIL + 7] ^= 1;
    maqam_init(); CHECK(groove_tuning_apply(z) == 0);
    CHECK(groove_project_load_slot(5) == 0 && is_tables(stock) && maqam_project_scale() == 0 && maqam_track_scale(1) == by);
    /* a save over a stock record adds the trailer, and the unchanged project keeps its record body */
    CHECK(groove_project_save_slot(5) == 0 && sector(5)[TAIL] == 0xA1 && record_crc(sector(5)) == *(const uint32_t *)(const void *)(sector(5) + 12));
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
    cross_check();
    state_push();
    undo();
    trailer();
    matching();
    if (failures) { printf("maqam_test: %d failure(s)\n", failures); return 1; }
    printf("maqam_test: ok\n");
    return 0;
}
