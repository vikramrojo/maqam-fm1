/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Project scale, per-track override and intonation. Tables live in the engine (V13 saves them); this
   file keeps only the labels and the 8-byte trailer. */
#include "maqam.h"
#include <string.h>

static struct {
    uint8_t scale, root, trad, layout, over;
    uint8_t tscale[GROOVE_TRACKS], troot[GROOVE_TRACKS];   /* a track's own scale, valid if its over bit is set */
} st = {0, 0, 1, 0, 0, {0}, {0}};

void maqam_init(void) { memset(&st, 0, sizeof st); st.trad = 1; maqam_keys_reset(); maqam_page_save_result(0); }
unsigned maqam_default_layout(void) { return st.layout; }
int maqam_set_default_layout(unsigned l)
{
    if (l > MAQAM_WHITE) return -1;
    if (st.layout != l) { st.layout = (uint8_t)l; maqam_engine_touch(); }
    return 0;
}
unsigned maqam_project_scale(void) { return st.scale; }
unsigned maqam_project_root(void) { return st.root; }
unsigned maqam_trad(void) { return st.trad; }
unsigned maqam_melodic(unsigned t)
{
    if (t >= GROOVE_TRACKS || t == 7) return 0;
    unsigned s = groove_source_type(t);
    return s != GROOVE_808 && s != GROOVE_SAMPLE;
}
unsigned maqam_overridden(unsigned t) { return t < GROOVE_TRACKS && (st.over >> t & 1U); }
unsigned maqam_track_scale(unsigned t) { return maqam_overridden(t) ? st.tscale[t] : st.scale; }
unsigned maqam_track_root(unsigned t)
{
    if (!maqam_overridden(t)) return st.root;
    return st.tscale[t] == MAQAM_CUSTOM ? 0 : st.troot[t];
}

/* Push scale/root to a track. reset: replace the alt table with the defaults; else keep it. */
static int apply(unsigned t, unsigned scale, unsigned root, unsigned reset)
{
    struct groove_tuning g;
    maqam_engine_get(t, &g);
    int r = maqam_main(scale, root, st.trad, g.main);
    if (r) return -1;                       /* nothing pushed */
    if (reset) maqam_alt_default(scale, root, g.alt);
    return maqam_engine_set(t, &g);
}
/* Retune a track to (scale, root); alt edits survive unless the scale or root actually changes. */
static int retune(unsigned t, unsigned scale, unsigned root)
{
    unsigned same = maqam_track_scale(t) == scale && maqam_track_root(t) == root;
    return apply(t, scale, root, !same);
}
static int ok_pick(unsigned scale, unsigned root) { return scale < maqam_scale_count() && root < 24; }

int maqam_set_project(unsigned scale, unsigned root)
{
    if (!ok_pick(scale, root)) return -1;
    int r = 0;
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t)
        if (!maqam_overridden(t) && maqam_melodic(t) && retune(t, scale, root)) r = -1;
    st.scale = (uint8_t)scale;
    st.root = (uint8_t)root;
    return r;
}
int maqam_set_intonation(unsigned trad)
{
    int r = 0;
    st.trad = trad ? 1 : 0;
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        unsigned s = maqam_track_scale(t);
        if (maqam_melodic(t) && s != MAQAM_CUSTOM && apply(t, s, maqam_track_root(t), 0)) r = -1;
    }
    return r;
}
int maqam_set_override(unsigned t, unsigned scale, unsigned root)
{
    if (!maqam_melodic(t) || !ok_pick(scale, root)) return -1;
    int r = retune(t, scale, root);
    if (r) return r;
    st.over |= (uint8_t)(1U << t);
    st.tscale[t] = (uint8_t)scale;
    st.troot[t] = (uint8_t)root;
    return 0;
}
int maqam_clear_override(unsigned t)
{
    if (!maqam_overridden(t)) return 0;
    int r = retune(t, st.scale, st.root);
    if (!r) st.over &= (uint8_t)~(1U << t);
    return r;
}
int maqam_set_alt(unsigned t, unsigned key, int cents)
{
    struct groove_tuning g;
    if (!maqam_melodic(t) || key >= GROOVE_TUNING_KEYS || cents < -127 || cents > 127) return -1;
    maqam_engine_get(t, &g);
    g.alt[key] = (int8_t)cents;
    return maqam_engine_set(t, &g);
}

/* ---- persistence ---- */
static uint8_t check(const uint8_t *t)
{
    uint8_t x = 0;
    for (unsigned i = 0; i < MAQAM_TRAILER_BYTES - 1; ++i) x ^= t[i];
    return (uint8_t)~x;
}
void maqam_project_save(uint8_t t[MAQAM_TRAILER_BYTES])
{
    t[0] = MAQAM_TRAILER_TAG;
    t[1] = st.scale;
    t[2] = st.root;
    t[3] = (uint8_t)(st.trad | st.layout << 1);
    t[4] = st.over;
    t[5] = t[6] = 0;
    t[7] = check(t);
}
static int trailer_ok(const uint8_t *t)
{
    return t && t[0] == MAQAM_TRAILER_TAG && t[7] == check(t) && t[1] < maqam_scale_count() && t[2] < 24 &&
           !(t[3] & ~7U) && (t[3] >> 1 & 3U) < 3 && !t[5] && !t[6];
}
/* The engine tables decide: a track whose main table is not the project scale's gets its own label by
   matching the library (first hit in index order), else CUSTOM, and its tables are left untouched. */
void maqam_project_loaded(const uint8_t *t)
{
    struct groove_tuning g[GROOVE_TRACKS];
    int8_t want[GROOVE_TUNING_KEYS];
    unsigned over = 0;
    maqam_init();
    if (trailer_ok(t)) {
        st.scale = t[1]; st.root = t[2]; st.trad = t[3] & 1U; st.layout = t[3] >> 1 & 3U;
        over = t[4];
    }
    if (maqam_engine_capture(g)) return;
    int fits = !maqam_main(st.scale, st.root, st.trad, want);
    for (unsigned i = 0; i < GROOVE_TRACKS; ++i) {
        if (!maqam_melodic(i)) continue;
        if (!(over >> i & 1U) && fits && !memcmp(g[i].main, want, sizeof want)) continue;
        unsigned s, r;
        st.over |= (uint8_t)(1U << i);
        if (maqam_match(g[i].main, st.trad, &s, &r)) { st.tscale[i] = MAQAM_CUSTOM; continue; }
        st.tscale[i] = (uint8_t)s;
        st.troot[i] = (uint8_t)r;
    }
}
void maqam_refresh(void)
{
    uint8_t t[MAQAM_TRAILER_BYTES];
    int8_t m[GROOVE_TUNING_KEYS];
    struct groove_tuning g[GROOVE_TRACKS];
    __typeof__(st) old = st;
    maqam_project_save(t);
    maqam_project_loaded(t);
    if (maqam_engine_capture(g)) return;
    for (unsigned i = 0; i < GROOVE_TRACKS; ++i)   /* keep a label that still describes the table */
        if ((old.over >> i & 1U) && (st.over >> i & 1U) && old.tscale[i] != MAQAM_CUSTOM &&
            !maqam_main(old.tscale[i], old.troot[i], st.trad, m) && !memcmp(m, g[i].main, sizeof m)) {
            st.tscale[i] = old.tscale[i];
            st.troot[i] = old.troot[i];
        }
}
