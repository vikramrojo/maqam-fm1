/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The scale pages: project scale, per-track override, key layouts and the alt table editor. All state is
   in the maqam_* API and the engine tables; this file keeps only the page cursor. screen.c draws the view. */
#include "maqam.h"
#include "scales.h"
#include <string.h>

enum { KEY_BASE = 53, PC_KEYS = 27 };
enum { SUB_PROJECT, SUB_TRACK, SUB_KEYS, SUB_ALT };
static unsigned sub, sel;                                   /* sub-page; selected alt key (pitch class) */

static unsigned no_space;                                   /* the last save could not store tuning */

MAQAM_SMALL void maqam_page_next(void) { sub = (sub + 1) % MAQAM_SUBPAGES; }
MAQAM_SMALL void maqam_page_save_result(int r)
{
    if (r == 0 || r == GROOVE_PROJECT_TUNING_SPACE) no_space = r != 0;
}

/* ---- small helpers ---- */
MAQAM_SMALL static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
/* The k-th scale of a set (by index order), and a scale's place in its set. */
MAQAM_SMALL static unsigned set_nth(unsigned set, unsigned k)
{
    for (unsigned i = 0; i < NSCALES; ++i) if (SCALES[i].set == set && !k--) return i;
    return 0;
}
MAQAM_SMALL static unsigned set_rank(unsigned s, unsigned *len)
{
    unsigned before = 0, n = 0;
    for (unsigned i = 0; i < NSCALES; ++i) if (SCALES[i].set == SCALES[s].set) { before += i < s; ++n; }
    *len = n;
    return before;
}
/* Move to another set (jumps to that set's first scale; clamped at the ends) and/or around the scales of the
   current set (wraps). */
MAQAM_SMALL static unsigned step_scale(unsigned s, int dset, int dscale)
{
    if (dset) {
        unsigned set = (unsigned)clampi((int)SCALES[s].set + dset, 0, NSETS - 1);
        if (set != SCALES[s].set) s = set_nth(set, 0);
    }
    if (dscale) {
        unsigned n, at = set_rank(s, &n);
        s = set_nth(SCALES[s].set, maqam_mod((int)at + dscale, (int)n));
    }
    return s;
}
MAQAM_SMALL static unsigned key_pc(uint32_t keys)
{
    for (unsigned k = 0; k < PC_KEYS; ++k)
        if (keys >> k & 1U) return (KEY_BASE + k) % 12U;
    return 12;
}
static const char *const layout_name[3] = {"SNAP", "WHITE", "CHROM"};            /* by MAQAM_* value */

/* The track's scale and root for editing: its own, or the project's when its table is CUSTOM. */
MAQAM_SMALL static void track_base(unsigned t, unsigned *s, unsigned *r)
{
    *s = maqam_track_scale(t); *r = maqam_track_root(t);
    if (*s == MAQAM_CUSTOM) { *s = maqam_project_scale(); *r = maqam_project_root(); }
}

/* ---- control ---- */
MAQAM_SMALL void maqam_page_control(unsigned t, const int16_t enc[7], uint32_t key_down)
{
    int d[4] = {enc[2], enc[3], enc[4], enc[5]};
    sub = maqam_mod((int)sub + enc[1], MAQAM_SUBPAGES);
    unsigned pc = key_pc(key_down);
    if (sub == SUB_PROJECT) {
        unsigned s = maqam_project_scale(), r = maqam_project_root();
        if (d[0] || d[1] || d[2]) {
            maqam_set_project(step_scale(s, d[1], d[2]), maqam_mod((int)r + d[0], 24));
        }
        if (d[3]) maqam_set_intonation(d[3] > 0);
    } else if (sub == SUB_TRACK) {
        if (!maqam_melodic(t)) return;
        unsigned s, r;
        track_base(t, &s, &r);
        if (d[0] > 0 && !maqam_overridden(t)) maqam_set_override(t, s, r);
        else if (d[0] < 0) maqam_clear_override(t);
        if (d[1] || d[2] || d[3]) maqam_set_override(t, step_scale(s, d[2], d[3]), maqam_mod((int)r + d[1], 24));
    } else if (sub == SUB_KEYS) {                          /* the layouts are numbered in display order */
        if (d[0]) maqam_set_default_layout((unsigned)clampi((int)maqam_default_layout() + d[0], 0, 2));
        if (d[1] && maqam_melodic(t)) maqam_set_layout(t, clampi(maqam_layout_raw(t) + d[1], -1, 2));
    } else {
        sel = pc < 12 ? pc : maqam_mod((int)sel + d[0], 12);
        if (!maqam_melodic(t)) return;
        struct groove_tuning g;
        maqam_engine_get(t, &g);
        if (d[1] || d[2]) maqam_set_alt(t, sel, clampi(g.alt[sel] + d[1] + 10 * d[2], -127, 127));
        if (d[3]) {                                        /* RESET: the scale's default alt table */
            unsigned s = maqam_track_scale(t);
            if (s == MAQAM_CUSTOM) memset(g.alt, 0, sizeof g.alt);
            else maqam_alt_default(s, maqam_track_root(t), g.alt);
            maqam_engine_set(t, &g);
        }
    }
}

/* The header tag on the overview and step pages: "Ed RAST" (spelled root, scale name cut to 4). 0 (nothing
   written) for a drum track or WEST chromatic; CUSTOM gives "CUST". out holds 12 bytes. */
MAQAM_SMALL unsigned maqam_header_tag(unsigned t, char out[12])
{
    unsigned s = maqam_track_scale(t);
    memset(out, 0, 12);
    if (!maqam_melodic(t) || !s) return 0;
    if (s == MAQAM_CUSTOM) { memcpy(out, "CUST", 4); return 1; }
    maqam_degree_name(s, maqam_track_root(t), maqam_track_root(t), out);
    strcat(out, " ");
    strncat(out, SCALES[s].name, 4);
    return 1;
}

/* ---- view ---- */
MAQAM_SMALL static void put(char *dst, unsigned n, const char *s) { strncpy(dst, s, n - 1); dst[n - 1] = 0; }
MAQAM_SMALL static void cat(char *dst, unsigned n, const char *s)
{
    unsigned l = (unsigned)strlen(dst);
    if (l < n - 1) put(dst + l, n - l, s);
}
/* The subtitle: tonic, set, scale and intonation (screen.c draws text uppercase, so no case folding here). */
/* Cents from the nearest quarter tone (24-TET), rounded half away from zero like the names: what the
   scale pages show, since every scale is read as 24-TET plus a deviation. */
MAQAM_SMALL static int dev24(int c) { return c - 50 * (c >= 0 ? (c + 25) / 50 : -((25 - c) / 50)); }
MAQAM_SMALL static void subtitle(char *out, unsigned n, unsigned s, const char *tonic, unsigned desc)
{
    for (unsigned set = 1;; set = 0) {          /* drop the set name when the line won't fit the large font */
        out[0] = 0;
        if (s == MAQAM_CUSTOM) { put(out, n, "custom"); }
        else {
            put(out, n, tonic);
            if (set) { cat(out, n, " "); cat(out, n, SET_NAMES[SCALES[s].set]); }
            cat(out, n, " ");
            cat(out, n, SCALES[s].name);
            if (scale_up(&SCALES[s]) != scale_down(&SCALES[s])) cat(out, n, desc ? "\x19" : "\x18");
        }
        cat(out, n, maqam_trad() ? " trad" : " equal");
        if (!set || strlen(out) <= MAQAM_SUB_BIG) return;
    }
}
/* The key strip, or (row) the degree row: the scale's degrees in pitch order, spelled, with the table cents of
   the key that plays each and a mark for a degree of one form only. A CUSTOM table has no degrees: the strip. */
MAQAM_SMALL static void strip(unsigned s, unsigned r, const struct groove_tuning *g, struct maqam_view *v, unsigned row)
{
    int8_t key[24];
    char nm[4];
    for (unsigned i = 0; i < 12; ++i) put(v->lab[i], sizeof v->lab[i], maqam_slot_name(2 * i));
    v->lit = 0xfff;
    if (s == MAQAM_CUSTOM) {                              /* no scale: show the keys the table moves */
        v->lit = 0;
        for (unsigned i = 0; i < 12; ++i) if (g->main[i]) v->lit |= (uint16_t)(1U << i);
        return;
    }
    if (maqam_assign(s, r, key)) return;
    v->lit = 0;
    for (unsigned sl = 0, n = 0; sl < 24; ++sl) {
        if (key[sl] < 0) continue;
        unsigned up = scale_up(&SCALES[s]) >> sl & 1U, down = scale_down(&SCALES[s]) >> sl & 1U, k = (unsigned)key[sl];
        if (maqam_degree_name(s, r, r + sl, nm) || row) {
            if (row) {
                if (up != down) strcat(nm, up ? "\x18" : "\x19");
                v->cents[n] = g->main[k];
                k = n++;
                v->ncell = (uint8_t)n;
            }
            put(v->lab[k], sizeof v->lab[k], nm);
        }
        v->lit |= (uint16_t)(1U << k);
        if (!sl) v->root = (uint16_t)(1U << k);
    }
}
MAQAM_SMALL void maqam_page_view(unsigned t, struct maqam_view *v)
{
    static const char *const title[MAQAM_SUBPAGES] = {"PROJECT", "TRACK", "KEYS", "ALT"};
    struct groove_tuning g;
    memset(v, 0, sizeof *v);
    memset(&g, 0, sizeof g);
    v->title = title[sub];
    v->index = (uint8_t)(sub + 1);
    v->tuned = (uint8_t)maqam_melodic(t);
    if (v->tuned) maqam_engine_get(t, &g);
    unsigned s = maqam_project_scale(), r = maqam_project_root();
    if (v->tuned && sub != SUB_PROJECT) { s = maqam_track_scale(t); r = maqam_track_root(t); }
    unsigned custom = s == MAQAM_CUSTOM, set = custom ? 0 : SCALES[s].set;
    v->custom = (uint8_t)custom;
    maqam_degree_name(s, r, r, v->rname);                 /* a CUSTOM scale gives the fixed name */
    subtitle(v->sub, sizeof v->sub, s, v->rname, maqam_last_descending(t));
    char *p = v->status;
    memcpy(p, "TRACK ", 6);
    p += 6 + maqam_num(p + 6, (int)t + 1, 0);
    put(p, sizeof v->status - (unsigned)(p - v->status), !v->tuned ? "  DRUMS - NOT TUNED"
        : maqam_track_scale(t) == MAQAM_CUSTOM ? "  CUSTOM TABLE"
        : maqam_overridden(t) ? "  OVERRIDE" : "  FOLLOWS PROJECT");
    if (!custom && set != SET_WEST) {                     /* a hint: it never moves the root */
        put(v->note, sizeof v->note, "home: ");
        cat(v->note, sizeof v->note, maqam_slot_name(SCALES[s].home));
    }
    memcpy(v->cents, g.main, 12);
    strip(s, r, &g, v, sub <= SUB_TRACK && !custom);
    if (no_space) put(v->warn, sizeof v->warn, "SAVE FAILED: REDUCE SAMPLE BANK");

    if (sub <= SUB_TRACK) {                               /* PROJECT: ROOT SET SCALE TUNING; TRACK: OVERRIDE ROOT SET SCALE */
        const char *const k[3][2] = {{"ROOT", custom ? "--" : v->rname}, {"SET", custom ? "--" : SET_NAMES[set]},
                                     {"SCALE", custom ? "CUSTOM" : SCALES[s].name}};
        memcpy(v->knob + sub, k, sizeof k);
        v->knob[sub ? 0 : 3][0] = sub ? "OVERRIDE" : "TUNING";
        v->knob[sub ? 0 : 3][1] = sub ? (maqam_overridden(t) ? "ON" : "OFF") : (maqam_trad() ? "TRAD" : "EQUAL");
    } else if (sub == SUB_KEYS) {
        int raw = maqam_layout_raw(t);
        v->knob[0][0] = "PROJECT"; v->knob[0][1] = layout_name[maqam_default_layout()];
        v->knob[1][0] = "TRACK"; v->knob[1][1] = raw < 0 ? "FOLLOW" : layout_name[raw];
        if (v->tuned) {
            put(v->note, sizeof v->note, "plays: ");
            cat(v->note, sizeof v->note, layout_name[maqam_layout(t)]);
        }
    } else {
        memcpy(v->cents, g.alt, 12);
        v->sel = (uint16_t)(1U << sel);
        maqam_num(v->atext, g.alt[sel], 1);
        v->knob[0][0] = "KEY"; v->knob[0][1] = maqam_slot_name(2 * sel);
        v->knob[1][0] = "ALT +/-1"; v->knob[1][1] = v->atext;
        v->knob[2][0] = "ALT +/-10"; v->knob[2][1] = v->atext;
        v->knob[3][0] = "RESET"; v->knob[3][1] = "TURN";
        for (unsigned i = 0; i < 2; ++i) {                /* "ALT -100 Eb4", "MAIN -47 Ed4" */
            char *q = v->line[i];
            memcpy(q, i ? "MAIN " : "ALT ", 5);
            int c = i ? g.main[sel] : g.alt[sel];
            q += (i ? 5 : 4) + maqam_num(q + (i ? 5 : 4), dev24(c), 0);
            *q++ = ' ';
            maqam_name_scale(s, r, 60 + sel, c, q);
        }
    }
    for (unsigned i = 0; i < 12; ++i) v->cents[i] = (int8_t)dev24(v->cents[i]);
}
