/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Key layouts (CHROM/SNAP), direction, the OCT alt gesture and double-tap octave, key lights.
   Chooses note numbers only; the engine tunes them from the track's tables. */
#include "maqam.h"
#include "scales.h"
#include <string.h>

enum { FOLLOW = 0xFF, RESET_MS = 1000, TAP_MS = 300, KEY_BASE = 53, C4 = 60 };

static uint8_t lay[GROOVE_TRACKS];
static struct { int16_t last; uint32_t ms; uint8_t have, desc; } dir[GROOVE_TRACKS];
static struct { uint8_t btn, valid; uint32_t ms; } tap;
static int alt_track = -1;                              /* the OCT gesture */
static struct { int scale, root, ok; int8_t key[24]; } cache = {-1, -1, 0, {0}};

MAQAM_SMALL void maqam_keys_reset(void)
{
    memset(lay, FOLLOW, sizeof lay);
    memset(dir, 0, sizeof dir);
    tap.valid = 0;
}


/* Assignment for the track, or NULL for drum, CUSTOM and chromatic tracks (they play every key). */
MAQAM_SMALL static const int8_t *assignment(unsigned track, unsigned *scale, unsigned *root)
{
    if (!maqam_melodic(track)) return 0;
    unsigned s = maqam_track_scale(track), r = maqam_track_root(track);
    if (s == MAQAM_CUSTOM || s >= maqam_scale_count()) return 0;
    if (cache.scale != (int)s || cache.root != (int)r) {
        cache.scale = (int)s; cache.root = (int)r;
        cache.ok = maqam_assign(s, r, cache.key) == 0;
    }
    if (!cache.ok) return 0;
    unsigned n = 0;
    for (unsigned i = 0; i < 24; ++i) n += cache.key[i] >= 0;
    if (n >= 12) return 0;                              /* chromatic */
    *scale = s; *root = r;
    return cache.key;
}
MAQAM_SMALL static unsigned pc_mask(const int8_t *key)
{
    unsigned m = 0;
    for (unsigned i = 0; i < 24; ++i)
        if (key[i] >= 0) m |= 1U << key[i];
    return m;
}

MAQAM_SMALL unsigned maqam_layout(unsigned track)
{
    unsigned s, r;
    if (track >= GROOVE_TRACKS || !assignment(track, &s, &r)) return MAQAM_CHROM;
    return (unsigned)maqam_layout_raw(track);
}
MAQAM_SMALL int maqam_set_layout(unsigned track, int layout)
{
    if (track >= GROOVE_TRACKS || layout > MAQAM_CHROM || layout < -1) return -1;
    lay[track] = layout < 0 ? FOLLOW : (uint8_t)layout;
    return 0;
}

MAQAM_SMALL int maqam_layout_raw(unsigned track)
{
    return track < GROOVE_TRACKS && maqam_overridden(track) && lay[track] != FOLLOW ? lay[track] : (int)maqam_default_layout();
}
MAQAM_SMALL unsigned maqam_last_descending(unsigned track) { return track < GROOVE_TRACKS && dir[track].have && dir[track].desc; }

MAQAM_SMALL static unsigned in_range(int m) { return m >= 0 && m <= 127; }

MAQAM_SMALL static unsigned snap(int base, unsigned pcs, unsigned desc)
{
    for (int d = 0; d < 12; ++d) {
        int m = base + (desc ? -d : d);
        if (pcs >> maqam_mod(m, 12) & 1U) return in_range(m) ? (unsigned)m : (unsigned)base;
    }
    return (unsigned)base;
}

MAQAM_SMALL unsigned maqam_key_note(unsigned track, unsigned key, int octave, unsigned desc)
{
    int base = KEY_BASE + (int)key + octave * 12;
    base = base < 0 ? 0 : base > 127 ? 127 : base;
    unsigned scale, root, layout = maqam_layout(track);
    const int8_t *k = assignment(track, &scale, &root);
    if (!k || layout == MAQAM_CHROM) return groove_key_note(track, key, octave);
    return snap(base, pc_mask(k), desc);
}
MAQAM_SMALL unsigned maqam_key_press(unsigned track, unsigned key, int octave, uint32_t now)
{
    if (track >= GROOVE_TRACKS) return groove_key_note(track, key, octave);
    unsigned d = dir[track].desc;
    if (!dir[track].have || now - dir[track].ms > RESET_MS) d = 0;
    else if ((int)key < dir[track].last) d = 1;
    else if ((int)key > dir[track].last) d = 0;
    dir[track].have = 1; dir[track].desc = (uint8_t)d;
    dir[track].last = (int16_t)key; dir[track].ms = now;
    return maqam_key_note(track, key, octave, d);
}

MAQAM_SMALL uint32_t maqam_key_lights(unsigned track)
{
    unsigned s, r;
    const int8_t *k = assignment(track, &s, &r);
    uint32_t bits = 0;
    if (!k) return 0;
    unsigned pcs = pc_mask(k);
    for (unsigned i = 0; i < 27; ++i)
        if (pcs >> ((KEY_BASE + i) % 12) & 1U) bits |= 1U << i;
    return bits;
}

MAQAM_SMALL static void alt_sync(unsigned track) { maqam_engine_alt_hold(track, alt_track == (int)track); }

MAQAM_SMALL unsigned maqam_oct_buttons(unsigned track, unsigned active, unsigned held, unsigned pressed,
                           unsigned key_down, uint32_t now, int *octave)
{
    unsigned own = active && maqam_melodic(track);
    unsigned want = own && (held & 3U);
    if (alt_track >= 0 && (!want || alt_track != (int)track)) {
        unsigned t = (unsigned)alt_track;
        alt_track = -1;
        alt_sync(t);
    }
    if (want && alt_track < 0) {
        alt_track = (int)track;
        alt_sync(track);
    }
    if (!own) { tap.valid = 0; return 0; }
    if (key_down) tap.valid = 0;
    for (unsigned b = 0; b < 2; ++b) {
        if (!(pressed >> b & 1U)) continue;
        if (tap.valid && tap.btn == b && now - tap.ms <= TAP_MS) {
            int o = *octave + (b ? 1 : -1);
            *octave = o < -2 ? -2 : o > 2 ? 2 : o;
            tap.valid = 0;
        } else {
            tap.valid = 1; tap.btn = (uint8_t)b; tap.ms = now;
        }
    }
    return 1;
}
