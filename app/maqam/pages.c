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

MAQAM_SMALL void maqam_page_open(void) { sub = SUB_PROJECT; sel = 0; }
MAQAM_SMALL void maqam_page_save_result(int r)
{
    if (r == 0 || r == GROOVE_PROJECT_TUNING_SPACE) no_space = r != 0;
}

/* ---- small helpers ---- */
MAQAM_SMALL static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
MAQAM_SMALL static unsigned wrap24(int v) { return (unsigned)(((v % 24) + 24) % 24); }
MAQAM_SMALL static unsigned set_first(unsigned set)
{
    for (unsigned i = 0; i < NSCALES; ++i) if (SCALES[i].set == set) return i;
    return 0;
}
MAQAM_SMALL static unsigned set_len(unsigned set)
{
    unsigned n = 0;
    for (unsigned i = 0; i < NSCALES; ++i) n += SCALES[i].set == set;
    return n ? n : 1;
}
/* Move to another set (jumps to that set's first scale; clamped at the ends) and/or around the scales of the
   current set (wraps). */
MAQAM_SMALL static unsigned step_scale(unsigned s, int dset, int dscale)
{
    unsigned set = SCALES[s].set;
    if (dset) {
        unsigned next = (unsigned)clampi((int)set + dset, 0, NSETS - 1);
        if (next != set) { set = next; s = set_first(set); }
    }
    if (dscale) {
        int n = (int)set_len(set), at = (int)(s - set_first(set));
        s = set_first(set) + (unsigned)(((at + dscale % n) % n + n) % n);
    }
    return s;
}
MAQAM_SMALL static unsigned key_pc(uint32_t keys)
{
    for (unsigned k = 0; k < PC_KEYS; ++k)
        if (keys >> k & 1U) return (KEY_BASE + k) % 12U;
    return 12;
}
MAQAM_SMALL static int has_key(uint32_t keys) { return (keys & ((1U << PC_KEYS) - 1U)) != 0; }
/* The root a key press asks for: the key's own tone, a quarter tone lower or higher with OCT- / OCT+ held. */
MAQAM_SMALL static unsigned key_root(unsigned pc, unsigned oct)
{
    return wrap24((int)pc * 2 + ((oct & 2U) ? 1 : (oct & 1U) ? -1 : 0));
}
static const uint8_t layout_order[3] = {MAQAM_SNAP, MAQAM_WHITE, MAQAM_CHROM};   /* display order */
static const char *const layout_name[3] = {"SNAP", "CHROM", "WHITE"};            /* by MAQAM_* value */
MAQAM_SMALL static int layout_pos(unsigned l) { for (int i = 0; i < 3; ++i) if (layout_order[i] == l) return i; return 0; }

/* The track's scale and root for editing: its own, or the project's when its table is CUSTOM. */
MAQAM_SMALL static void track_base(unsigned t, unsigned *s, unsigned *r)
{
    *s = maqam_track_scale(t); *r = maqam_track_root(t);
    if (*s == MAQAM_CUSTOM) { *s = maqam_project_scale(); *r = maqam_project_root(); }
}

/* ---- control ---- */
MAQAM_SMALL void maqam_page_control(unsigned t, const int16_t enc[7], uint32_t key_down, unsigned oct)
{
    int d[4] = {enc[2], enc[3], enc[4], enc[5]};
    sub = (unsigned)clampi((int)sub + enc[1], 0, MAQAM_SUBPAGES - 1);
    unsigned pc = key_pc(key_down), pressed = has_key(key_down);
    if (sub == SUB_PROJECT) {
        unsigned s = maqam_project_scale(), r = maqam_project_root();
        if (d[0] || d[1] || d[2] || pressed) {
            r = pressed ? key_root(pc, oct) : wrap24((int)r + d[0]);
            s = step_scale(s, d[1], d[2]);
            maqam_set_project(s, r);
        }
        if (d[3]) maqam_set_intonation(d[3] > 0);
    } else if (sub == SUB_TRACK) {
        if (!maqam_melodic(t)) return;
        unsigned s, r;
        track_base(t, &s, &r);
        if (d[0] > 0 && !maqam_overridden(t)) maqam_set_override(t, s, r);
        else if (d[0] < 0) maqam_clear_override(t);
        if (d[1] || d[2] || d[3] || pressed) {
            r = pressed ? key_root(pc, oct) : wrap24((int)r + d[1]);
            maqam_set_override(t, step_scale(s, d[2], d[3]), r);
        }
    } else if (sub == SUB_KEYS) {
        if (d[0]) maqam_set_default_layout(layout_order[clampi(layout_pos(maqam_default_layout()) + d[0], 0, 2)]);
        if (d[1] && maqam_melodic(t)) {
            int at = maqam_layout_raw(t) < 0 ? 0 : 1 + layout_pos((unsigned)maqam_layout_raw(t));
            at = clampi(at + d[1], 0, 3);
            maqam_set_layout(t, at ? layout_order[at - 1] : -1);
        }
    } else {
        sel = pressed ? pc : (unsigned)((((int)sel + d[0]) % 12 + 12) % 12);
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

/* ---- view ---- */
MAQAM_SMALL unsigned maqam_cents_text(int c, char out[6])
{
    char d[12];
    unsigned n = 0, i = 0, u = c < 0 ? (unsigned)-c : (unsigned)c;
    if (!c) { out[0] = '0'; out[1] = 0; return 1; }
    out[n++] = c < 0 ? '-' : '+';
    do d[i++] = (char)('0' + u % 10U); while (u /= 10U);
    while (i) out[n++] = d[--i];
    out[n] = 0;
    return n;
}
MAQAM_SMALL void maqam_cents_pos(unsigned key, unsigned len, int *x, unsigned *y)
{
    *x = 6 + (int)key * 19 + (19 - 6 * (int)len) / 2;
    *y = 105U + 9U * (key & 1U);
}
MAQAM_SMALL static void put(char *dst, unsigned n, const char *s) { strncpy(dst, s, n - 1); dst[n - 1] = 0; }
MAQAM_SMALL static void cat(char *dst, unsigned n, const char *s)
{
    unsigned l = (unsigned)strlen(dst);
    if (l < n - 1) put(dst + l, n - l, s);
}
MAQAM_SMALL static void lower(char *s) { for (; *s; ++s) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32); }
MAQAM_SMALL static void num(char *dst, unsigned n, int v, int plus)
{
    char d[12], o[14];
    unsigned i = 0, u = v < 0 ? (unsigned)-v : (unsigned)v, k = 0;
    do d[i++] = (char)('0' + u % 10U); while (u /= 10U);
    if (v < 0) o[k++] = '-'; else if (plus) o[k++] = '+';
    while (i) o[k++] = d[--i];
    o[k] = 0;
    put(dst, n, o);
}
MAQAM_SMALL static void subtitle(char *out, unsigned n, unsigned s, unsigned r, unsigned desc)
{
    out[0] = 0;
    if (s == MAQAM_CUSTOM) { put(out, n, "custom"); }
    else {
        char w[12];
        maqam_degree_name(s, r, r, w);
        put(out, n, w);
        cat(out, n, " ");
        put(w, sizeof w, SET_NAMES[SCALES[s].set]); lower(w); cat(out, n, w);
        cat(out, n, " ");
        put(w, sizeof w, SCALES[s].name); lower(w); cat(out, n, w);
        if (scale_up(&SCALES[s]) != scale_down(&SCALES[s])) cat(out, n, desc ? "\x19" : "\x18");
    }
    cat(out, n, maqam_trad() ? " trad" : " equal");
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
MAQAM_SMALL static void knob(struct maqam_view *v, unsigned i, const char *label, const char *value)
{
    put(v->knob[i][0], sizeof v->knob[i][0], label);
    put(v->knob[i][1], sizeof v->knob[i][1], value);
}
MAQAM_SMALL void maqam_page_view(unsigned t, struct maqam_view *v)
{
    static const char *const title[MAQAM_SUBPAGES] = {"1/4 PROJECT", "2/4 TRACK", "3/4 KEYS", "4/4 ALT"};
    struct groove_tuning g;
    char tmp[16];
    memset(v, 0, sizeof *v);
    memset(&g, 0, sizeof g);
    v->sub_page = (uint8_t)sub;
    put(v->title, sizeof v->title, title[sub]);
    v->tuned = (uint8_t)maqam_melodic(t);
    if (v->tuned) maqam_engine_get(t, &g);
    unsigned ps = maqam_project_scale(), pr = maqam_project_root(), s = ps, r = pr;
    if (v->tuned && sub != SUB_PROJECT) { s = maqam_track_scale(t); r = maqam_track_root(t); }
    unsigned custom = s == MAQAM_CUSTOM, set = custom ? 0 : SCALES[s].set;
    v->custom = (uint8_t)custom;
    subtitle(v->sub, sizeof v->sub, s, r, maqam_last_descending(t));
    num(tmp, sizeof tmp, (int)t + 1, 0);
    put(v->status, sizeof v->status, "TRACK ");
    cat(v->status, sizeof v->status, tmp);
    cat(v->status, sizeof v->status, !v->tuned ? "  DRUMS - NOT TUNED"
        : maqam_track_scale(t) == MAQAM_CUSTOM ? "  CUSTOM TABLE"
        : maqam_overridden(t) ? "  OVERRIDE" : "  FOLLOWS PROJECT");
    if (!custom && set != SET_WEST) {                     /* a hint: it never moves the root */
        put(v->note, sizeof v->note, "home: ");
        cat(v->note, sizeof v->note, maqam_slot_name(SCALES[s].home));
    }
    memcpy(v->cents, g.main, 12);
    strip(s, r, &g, v, sub <= SUB_TRACK && !custom);
    if (no_space) put(v->warn, sizeof v->warn, "SAVE FAILED: REDUCE SAMPLE BANK");

    const char *scale_name = custom ? "CUSTOM" : SCALES[s].name, *set_name = custom ? "--" : SET_NAMES[set];
    if (sub == SUB_PROJECT) {
        maqam_degree_name(s, r, r, tmp);
        knob(v, 0, "ROOT", tmp);
        knob(v, 1, "SET", set_name);
        knob(v, 2, "SCALE", scale_name);
        knob(v, 3, "TUNING", maqam_trad() ? "TRAD" : "EQUAL");
        put(v->body[0], sizeof v->body[0], "KEY = ROOT   OCT-/OCT+ + KEY = 1/4");
        put(v->body[1], sizeof v->body[1], maqam_trad() ? "TRAD: EXACT CENTS PER SCALE" : "EQUAL: QUARTER-TONE GRID");
        put(v->body[2], sizeof v->body[2], "ROOT, SET AND SCALE ARE ALL PROJECT-WIDE");
    } else if (sub == SUB_TRACK) {
        knob(v, 0, "OVERRIDE", maqam_overridden(t) ? "ON" : "OFF");
        maqam_degree_name(s, r, r, tmp);
        knob(v, 1, "ROOT", custom ? "--" : tmp);
        knob(v, 2, "SET", set_name);
        knob(v, 3, "SCALE", scale_name);
        put(v->body[0], sizeof v->body[0], "KEY = TRACK ROOT   K1 LEFT = FOLLOW");
        put(v->body[1], sizeof v->body[1], custom ? "HAND-EDITED TABLE, KEPT AS IS" : "OTHER TRACKS KEEP THE PROJECT SCALE");
        put(v->body[2], sizeof v->body[2], "TUNING IS ALWAYS THE PROJECT'S");
    } else if (sub == SUB_KEYS) {
        int raw = maqam_layout_raw(t);
        knob(v, 0, "PROJECT", layout_name[maqam_default_layout()]);
        knob(v, 1, "TRACK", raw < 0 ? "FOLLOW" : layout_name[raw]);
        put(v->body[0], sizeof v->body[0], "SNAP: EVERY KEY, NEAREST DEGREE");
        put(v->body[1], sizeof v->body[1], "WHITE: WHITE KEYS WALK THE SCALE");
        put(v->body[2], sizeof v->body[2], "CHROM: PLAIN CHROMATIC KEYS");
        if (v->tuned) {
            put(tmp, sizeof tmp, layout_name[maqam_layout(t)]);
            put(v->note, sizeof v->note, "plays: ");
            cat(v->note, sizeof v->note, tmp);
            lower(v->note);
        }
    } else {
        char nm[12], line[36];
        int main_c = g.main[sel], alt_c = g.alt[sel];
        memcpy(v->cents, g.alt, 12);
        v->sel = (uint16_t)(1U << sel);
        v->big = 1;
        knob(v, 0, "KEY", maqam_slot_name(2 * sel));
        num(tmp, sizeof tmp, alt_c, 1);
        knob(v, 1, "ALT +/-1", tmp);
        knob(v, 2, "ALT +/-10", tmp);
        knob(v, 3, "RESET", "TURN");
        maqam_name_scale(s, r, 60 + sel, alt_c, nm);
        put(line, sizeof line, "ALT ");
        num(tmp, sizeof tmp, alt_c, 0);
        cat(line, sizeof line, tmp);
        cat(line, sizeof line, " ");
        cat(line, sizeof line, nm);
        put(v->body[0], sizeof v->body[0], line);
        maqam_name_scale(s, r, 60 + sel, main_c, nm);
        put(line, sizeof line, "MAIN ");
        num(tmp, sizeof tmp, main_c, 0);
        cat(line, sizeof line, tmp);
        cat(line, sizeof line, " ");
        cat(line, sizeof line, nm);
        put(v->body[1], sizeof v->body[1], line);
        put(v->body[2], sizeof v->body[2], v->tuned ? "PRESS A KEY OR K1.  ALT = OCT HELD" : "DRUM TRACKS ARE NOT TUNED");
    }
}
