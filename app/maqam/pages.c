/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The scale pages: project scale, per-track override, key layouts and the alt table editor. All state is
   in the maqam_* API and the engine tables; this file keeps only the page cursor. screen.c draws the view. */
#include "maqam.h"
#include "scales.h"
#include <string.h>

enum { KEY_BASE = 53, PC_KEYS = 27 };
enum { SUB_SCALE, SUB_TRACK, SUB_CHORD };
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
static const char layout_name[2][6] = {"SNAP", "CHROM"};            /* by MAQAM_* value */

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
    unsigned pc = key_pc(key_down), mel = maqam_melodic(t);
    sub = maqam_mod((int)sub + enc[1], MAQAM_SUBPAGES);
    if (sub == SUB_SCALE) {                                /* a track with its own scale edits only that track */
        if (d[0] || d[1] || d[2]) {
            unsigned s, r;
            track_base(t, &s, &r);
            s = step_scale(s, d[1], d[2]);
            r = maqam_mod((int)r + d[0], 24);
            if (mel && maqam_overridden(t)) maqam_set_override(t, s, r);
            else maqam_set_project(s, r);
        }
        if (d[3]) maqam_set_intonation(d[3] > 0);
        return;
    }
    if (sub == SUB_CHORD) {                                /* MODE TYPE INV VOICE of the selected track */
        for (unsigned i = 0; i < 4; ++i)
            if (d[i] && mel) maqam_chord_set(t, i, maqam_chord_get(t, i) + (d[i] > 0 ? 1U : -1U));   /* out of range: no change */
        return;
    }
    if (pc < 12) sel = pc;                                 /* a key played on the TRACK page is the alt key */
    if (d[1]) {                                            /* layouts are numbered in display order */
        int l = clampi(maqam_layout_raw(t) + d[1], 0, 1);
        if (mel && maqam_overridden(t)) maqam_set_layout(t, l);
        else maqam_set_default_layout((unsigned)l);
    }
    if (!mel) return;
    if (d[0] > 0 && !maqam_overridden(t)) {
        unsigned s, r;
        track_base(t, &s, &r);
        maqam_set_override(t, s, r);
    } else if (d[0] < 0) maqam_clear_override(t);
    sel = maqam_mod((int)sel + d[2], 12);
    if (d[3]) {
        struct groove_tuning g;
        maqam_engine_get(t, &g);
        maqam_set_alt(t, sel, clampi(g.alt[sel] + d[3], -127, 127));
    }
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
        if (s == MAQAM_CUSTOM) cat(out, n, "custom");
        else {
            cat(out, n, tonic);
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
    static const char title[MAQAM_SUBPAGES][6] = {"SCALE", "TRACK", "CHORD"};
    static const char label[4][6] = {"MODE", "TYPE", "INV", "VOICE"};
    static const char value[][5] = {"TRI", "7TH", "SUS2", "SUS4", "5TH", "5+8", "DRN", "DRN5", "ROOT", "1ST", "2ND", "3RD", "AUTO",
                                    "CLOS", "OPEN", "BASS", "OFF", "ON"};   /* TYPE, INV, VOICE, MODE */
    struct groove_tuning g;
    memset(v, 0, sizeof *v);
    memset(&g, 0, sizeof g);
    v->title = title[sub];
    v->index = (uint8_t)(sub + 1);
    v->tuned = (uint8_t)maqam_melodic(t);
    if (v->tuned) maqam_engine_get(t, &g);
    unsigned s = maqam_project_scale(), r = maqam_project_root();
    if (v->tuned) { s = maqam_track_scale(t); r = maqam_track_root(t); }
    unsigned custom = s == MAQAM_CUSTOM, set = custom ? 0 : SCALES[s].set;
    v->custom = (uint8_t)custom;
    maqam_degree_name(s, r, r, v->rname);                 /* a CUSTOM scale gives the fixed name */
    subtitle(v->sub, sizeof v->sub, s, v->rname, maqam_last_descending(t));
    memcpy(v->status, "TRACK ", 6);
    maqam_num(v->status + 6, (int)t + 1, 0);
    cat(v->status, sizeof v->status, !v->tuned ? "  DRUMS - NOT TUNED"
        : custom ? "  CUSTOM TABLE"
        : maqam_overridden(t) ? "  OWN SCALE" : "  FOLLOWS PROJECT");
    if (!custom && set != SET_WEST) {                     /* a hint: it never moves the root */
        memcpy(v->note, "home: ", 6);
        cat(v->note, sizeof v->note, maqam_slot_name(SCALES[s].home));
    }
    memcpy(v->cents, sub == SUB_TRACK ? g.alt : g.main, 12);
    strip(s, r, &g, v, !sub && !custom);
    if (no_space) put(v->warn, sizeof v->warn, "SAVE FAILED: REDUCE SAMPLE BANK");

    if (sub == SUB_CHORD) {                               /* the last chord, spelled, and its name */
        static const uint8_t base[4] = {16, 0, 8, 13};
        uint8_t n[8];
        unsigned c = maqam_chord_last(t, n);
        for (unsigned i = 0; i < 4; ++i) {
            v->knob[i][0] = label[i];
            v->knob[i][1] = value[base[i] + maqam_chord_get(t, i)];
        }
        v->sub[0] = 0;
        for (unsigned i = 0; i < c; ++i) {
            char nm[4];
            maqam_degree_name(s, r, (unsigned)(100 * n[i] + g.main[n[i] % 12U] + 25) / 50U, nm);
            if (i) cat(v->sub, sizeof v->sub, " ");
            cat(v->sub, sizeof v->sub, nm);
        }
        v->status[0] = 0;
        if (c) maqam_chord_name(t, 0, n, c, v->status);
    } else if (!sub) {                                    /* SCALE: ROOT SET SCALE TUNING */
        const char *const k[3][2] = {{"ROOT", custom ? "--" : v->rname}, {"SET", custom ? "--" : SET_NAMES[set]},
                                     {"SCALE", custom ? "CUSTOM" : SCALES[s].name}};
        memcpy(v->knob, k, sizeof k);
        v->knob[3][0] = "TUNING";
        v->knob[3][1] = maqam_trad() ? "TRAD" : "EQUAL";
    } else {                                              /* TRACK: SCOPE KEYS ALT KEY ALT */
        v->sel = (uint16_t)(1U << sel);
        maqam_num(v->atext, g.alt[sel], 1);
        v->knob[0][0] = "SCOPE"; v->knob[0][1] = maqam_overridden(t) ? "OWN" : "FOLLOW";
        v->knob[1][0] = "KEYS"; v->knob[1][1] = layout_name[maqam_layout_raw(t)];
        v->knob[2][0] = "ALT KEY"; v->knob[2][1] = maqam_slot_name(2 * sel);
        v->knob[3][0] = "ALT"; v->knob[3][1] = v->atext;
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
