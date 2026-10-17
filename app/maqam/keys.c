/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Key layouts (CHROM/SNAP/WHITE), direction, the OCT alt gesture and double-tap octave, key lights.
   Chooses note numbers only; the engine tunes them from the track's tables. */
#include "maqam.h"
#include "scales.h"
#include <string.h>

enum { FOLLOW = 0xFF, RESET_MS = 1000, TAP_MS = 300, KEY_BASE = 53, C4 = 60 };

static uint8_t lay[GROOVE_TRACKS];
static struct { int16_t last; uint32_t ms; uint8_t have, desc; } dir[GROOVE_TRACKS];
static struct { uint8_t btn, valid; uint32_t ms; } tap;
static int alt_track = -1;
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
MAQAM_SMALL static unsigned white(unsigned scale, unsigned root, const int8_t *key, unsigned key_index, int octave,
                      unsigned desc)
{
    static const int8_t idx[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    int rel = (int)key_index - (C4 - KEY_BASE);         /* semitones above C4 at octave 0 */
    int pc = (int)maqam_mod(rel, 12);
    if (idx[pc] < 0) return 128;                        /* black key */
    int w = 7 * maqam_fdiv(rel, 12) + idx[pc];
    uint32_t up = scale_up(&SCALES[scale]), down = scale_down(&SCALES[scale]);
    uint32_t layout = scale_popcount(down) > scale_popcount(up) ? down : up, cur = desc ? down : up;
    int n = (int)scale_popcount(layout), deg = (int)maqam_mod(w, n), oct = maqam_fdiv(w, n), slot = 0;
    for (int s = 0, seen = 0; s < 24; ++s)
        if (layout >> s & 1U) { if (seen++ == deg) { slot = s; break; } }
    int a = slot;                                       /* absolute slot relative to this octave's root */
    if (!(cur >> slot & 1U))                            /* missing in this form: next one in the direction */
        for (int i = 1; i < 24; ++i) {
            int c = slot + (desc ? -i : i);
            if (cur >> maqam_mod(c, 24) & 1U) { a = c; break; }
        }
    int p = (int)root + 24 * oct + a;                   /* quarter tones above C4 */
    int lo = C4 + maqam_fdiv(p, 2), hi = lo + (int)maqam_mod(p, 2);
    int want = key[maqam_mod(a, 24)];
    int m = (hi % 12 == want) ? hi : lo;
    m += 12 * octave;
    return in_range(m) ? (unsigned)m : 128;
}

MAQAM_SMALL unsigned maqam_key_note(unsigned track, unsigned key, int octave, unsigned desc)
{
    int base = KEY_BASE + (int)key + octave * 12;
    base = base < 0 ? 0 : base > 127 ? 127 : base;
    unsigned scale, root, layout = maqam_layout(track);
    const int8_t *k = assignment(track, &scale, &root);
    if (!k || layout == MAQAM_CHROM) return groove_key_note(track, key, octave);
    if (layout == MAQAM_WHITE) return white(scale, root, k, key, octave, desc);
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

MAQAM_SMALL unsigned maqam_oct_buttons(unsigned track, unsigned active, unsigned held, unsigned pressed,
                           unsigned key_down, uint32_t now, int *octave)
{
    unsigned own = active && maqam_melodic(track);
    unsigned want = own && (held & 3U);
    if (alt_track >= 0 && (!want || alt_track != (int)track)) {
        maqam_engine_alt_hold((unsigned)alt_track, 0);
        alt_track = -1;
    }
    if (want && alt_track < 0) {
        maqam_engine_alt_hold(track, 1);
        alt_track = (int)track;
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
