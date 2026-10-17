/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Chord mode: chords stacked from the track's scale degrees, inversions, AUTO voice leading, voicing, and the
   ownership of the notes a held chord key sounds. Chooses note numbers only; the engine tunes them. */
#include "maqam.h"
#include "scales.h"
#include <string.h>

enum { OFFSETS = 4, END = 0xFF, NOTES = 8, KEYS = 27 };

static uint8_t set[GROOVE_TRACKS][MAQAM_CH_FIELDS];
static struct { uint8_t n, note[NOTES]; } last[GROOVE_TRACKS];
static struct { uint8_t track, n, p, note[NOTES]; } own[KEYS];   /* notes [0, p) are posted */

/* Steps through the form's degrees from the played one (0 = root). */
static const uint8_t SHAPE[MAQAM_CHORD_TYPES][OFFSETS] = {
    {0, 2, 4, END}, {0, 2, 4, 6}, {0, 1, 4, END}, {0, 3, 4, END}, {0, 4, END, END}, {0, 4, 7, END}, {0, 1, 2, 3}};
static const uint8_t LIMIT[MAQAM_CH_FIELDS] = {1, MAQAM_CHORD_TYPES - 1, MAQAM_INV_AUTO, MAQAM_VOICE_BASS};

MAQAM_SMALL void maqam_chords_reset(void)
{
    memset(set, 0, sizeof set);
    memset(last, 0, sizeof last);
    memset(own, 0, sizeof own);
}
MAQAM_SMALL unsigned maqam_chord_get(unsigned track, unsigned field)
{
    return track < GROOVE_TRACKS && field < MAQAM_CH_FIELDS ? set[track][field] : 0;
}
MAQAM_SMALL int maqam_chord_set(unsigned track, unsigned field, unsigned value)
{
    if (track >= GROOVE_TRACKS || field >= MAQAM_CH_FIELDS || value > LIMIT[field]) return -1;
    set[track][field] = (uint8_t)value;
    return 0;
}
MAQAM_SMALL void maqam_chord_commit(unsigned track, const uint8_t *notes, unsigned n)
{
    if (track >= GROOVE_TRACKS || n > NOTES) return;
    last[track].n = (uint8_t)n;
    memcpy(last[track].note, notes, n);
}
MAQAM_SMALL unsigned maqam_chord_last(unsigned track, uint8_t out[8])
{
    if (track >= GROOVE_TRACKS) return 0;
    memcpy(out, last[track].note, NOTES);
    return last[track].n;
}

/* Interval names (M6): the notes' tuned pitches in quarter tones above the lowest, against fixed shapes. */
static const char NAMES[][5] = {"5TH", "8VA", "5+8", "SUS2", "SUS4", "ADD9", "JINS", "SUB", "MIN", "NEU", "MAJ", "SUP", "CUST", "DOM"};
/* name << 29 | one bit per note above the lowest, at its quarter tones */
static const uint32_t FIXED[8] = {0x00004000, 0x00000400, 0x21000000, 0x41004000, 0x41000400, 0x60004010, 0x80004400, 0xB0004100};
MAQAM_SMALL void maqam_chord_name(unsigned track, unsigned alt, const uint8_t *notes, unsigned n, char out[8])
{
    struct groove_tuning g;
    int base = 0, hi = 0;
    unsigned id = 12, m = 0, all = 0;   /* m: the shape above the lowest note; all: the pitch classes mod an octave (24) */
    if (n < 2 || n > NOTES) { strcpy(out, "SINGLE"); return; }
    maqam_engine_get(track, &g);
    for (unsigned i = 0; i < n; ++i) {
        int p = notes[i] * 100 + (alt ? g.alt : g.main)[notes[i] % 12U];
        if (!i) base = p;
        hi = (p - base + 25) / 50;
        all |= 1U << (unsigned)hi % 24U;
        if (i) m |= 1U << (hi < 29 ? hi : 0);
    }
    for (unsigned i = 0; i < 8; ++i)
        if (!((FIXED[i] ^ m) << 3)) { id = FIXED[i] >> 29; goto done; }
    if (n == 4 && hi <= 10) { id = 6; goto done; }
    for (unsigned d = 0; d < 5; ++d) {    /* the fifth nearest 700 c first: E G C is C major, not E minor-plus */
        unsigned h = 14 + (d & 1 ? (d + 1) / 2 : -(d / 2));
        for (unsigned k = 0; k < 24; ++k) {   /* each pitch class as the root: third 5-9, fifth 12-16, seventh 19-22 (18 = none) */
            unsigned r = (all >> k | all << (24 - k)) & 0xFFFFFFU;
            for (unsigned t = 5; t < 10; ++t)
                for (unsigned v = 18; v < 23; ++v)
                    if (all >> k & 1U && r == (1U | 1U << t | 1U << h | (v > 18 ? 1U << v : 0))) {
                        char *q = out + 3;
                        memcpy(out, NAMES[t == 8 && v == 20 ? 13 : t + 2], 3);
                        if (h != 14) *q++ = h < 14 ? '-' : '+';
                        if (v > 18) *q++ = '7';
                        *q = 0;
                        return;
                    }
        }
    }
done:
    strcpy(out, NAMES[id]);
}
/* Replaces the stock name in out by the interval name of step `step`'s chord, on a melodic track. */
MAQAM_SMALL void maqam_step_chord_name(unsigned track, unsigned step, char out[8])
{
    struct groove_step w;
    struct groove_sequence_options o;
    uint8_t n[NOTES];
    if (!maqam_melodic(track)) return;
    groove_sequence_read(track, step, &w, 1);
    groove_sequence_options_copy(&o);
    const uint8_t *row = o.chord[((w.drums & 31U) - 1) & 15U];
    unsigned c = w.drums & 31U ? row[0] : 1;
    for (unsigned i = 0; i < c; ++i) n[i] = (uint8_t)(w.note + (i ? row[i] : 0));
    maqam_chord_name(track, w.drums >> 13 & 1U, n, c, out);
}

/* Trailer bytes [5..6]: chord-mode mask; type | inv << 3 | voice << 6 of the lowest chord-mode track. */
MAQAM_SMALL void maqam_chords_save(uint8_t t[2])
{
    t[0] = t[1] = 0;
    for (unsigned k = GROOVE_TRACKS; k--;)   /* ends on the lowest-numbered track in chord mode */
        if (set[k][0]) { t[0] |= (uint8_t)(1U << k); t[1] = (uint8_t)(set[k][1] | set[k][2] << 3 | set[k][3] << 6); }
}
MAQAM_SMALL int maqam_chords_load(const uint8_t t[2], unsigned apply)
{
    unsigned b = t[1];
    if ((b & 7U) > MAQAM_CHORD_JINS || (b >> 3 & 7U) > MAQAM_INV_AUTO || b >> 6 > MAQAM_VOICE_BASS) return -1;
    for (unsigned k = 0; k < GROOVE_TRACKS && apply; ++k) {
        set[k][0] = t[0] >> k & 1U;
        set[k][1] = b & 7U;
        set[k][2] = b >> 3 & 7U;
        set[k][3] = (uint8_t)(b >> 6);
    }
    return 0;
}

MAQAM_SMALL static void sort(int16_t *b, unsigned n)
{
    for (unsigned i = 1; i < n; ++i)
        for (unsigned j = i; j && b[j - 1] > b[j]; --j) { int16_t t = b[j]; b[j] = b[j - 1]; b[j - 1] = t; }
}
/* The chord stacked on the played note, ascending, in b (may be outside 0..127); the count, 0 if none. */
MAQAM_SMALL static unsigned stack(unsigned track, unsigned note, unsigned desc, int16_t b[NOTES])
{
    unsigned scale, root, deg = 24, len = 0, idx = 0, n = 0;
    const int8_t *key = maqam_keys_assignment(track, &scale, &root);
    uint8_t slot[24];
    if (!key) return 0;
    note = maqam_snap(key, note, desc);
    for (unsigned i = 0; i < 24; ++i)
        if (key[i] == (int)(note % 12U)) deg = i;
    if (deg == 24) return 0;
    uint32_t form = scale_up(&SCALES[scale]);
    if (!(form >> deg & 1U)) form = scale_down(&SCALES[scale]);   /* a descending-only degree: its form */
    for (unsigned i = 0; i < 24; ++i)
        if (form >> i & 1U) { if (i == deg) idx = len; slot[len++] = (uint8_t)i; }
    const uint8_t *shape = SHAPE[set[track][MAQAM_CH_TYPE]];
    for (; n < OFFSETS && shape[n] != END; ++n) {
        unsigned j = idx + shape[n], s = slot[j % len];
        /* the slot's pitch in quarter tones above the played note, then the note of its key nearest to it */
        int x = 2 * (int)note + (int)s + 24 * (int)(j / len) - (int)deg - 2 * key[s];
        b[n] = (int16_t)(key[s] + 12 * maqam_fdiv(x + 12, 24));
    }
    return n;
}
/* Inversion, voicing and octave shift of a stacked chord; sorted, 0..127 only, no duplicates, at most 8. */
MAQAM_SMALL static unsigned voice(const int16_t *base, unsigned n, unsigned inv, unsigned voicing, int shift, uint8_t *out)
{
    int16_t b[NOTES + 1];
    unsigned c = 0;
    if (inv > n - 1) inv = n - 1;
    for (unsigned i = 0; i < n; ++i) b[i] = (int16_t)(base[i] + (i < inv ? 12 : 0));
    sort(b, n);
    if (voicing == MAQAM_VOICE_OPEN && n >= 3) b[1] = (int16_t)(b[1] + 12);
    if (voicing == MAQAM_VOICE_BASS) {
        memmove(b + 1, b, n * sizeof b[0]);
        b[0] = (int16_t)(b[1] - 12);
        ++n;
    }
    for (unsigned i = 0; i < n; ++i) b[i] = (int16_t)(b[i] + shift);
    sort(b, n);
    for (unsigned i = 0; i < n && c < NOTES; ++i)
        if (b[i] >= 0 && b[i] <= 127 && (!c || out[c - 1] != b[i])) out[c++] = (uint8_t)b[i];
    return c;
}
MAQAM_SMALL unsigned maqam_chord_notes(unsigned track, unsigned note, unsigned desc, uint8_t out[8])
{
    int16_t b[NOTES];
    unsigned n = track < GROOVE_TRACKS ? stack(track, note, desc, b) : 0;
    if (!n) return 0;
    unsigned inv = set[track][MAQAM_CH_INV], v = set[track][MAQAM_CH_VOICE], p = last[track].n;
    if (inv != MAQAM_INV_AUTO || !p) return voice(b, n, inv == MAQAM_INV_AUTO ? 0 : inv, v, 0, out);
    /* AUTO: the inversion and octave nearest the last chord (sorted notes, shorter length), ties to the lower
       inversion, then shift 0 */
    static const int8_t SHIFT[3] = {0, -12, 12};
    unsigned best = ~0U, count = 0;
    for (unsigned i = 0; i < n; ++i)
        for (unsigned h = 0; h < 3; ++h) {
            uint8_t c[NOTES];
            unsigned m = voice(b, n, i, v, SHIFT[h], c), d = 0;
            if (!m) continue;
            for (unsigned k = 0; k < m && k < p; ++k) d += (unsigned)(c[k] > last[track].note[k] ? c[k] - last[track].note[k] : last[track].note[k] - c[k]);
            if (d < best) { best = d; count = m; memcpy(out, c, m); }
        }
    return count;
}

MAQAM_SMALL unsigned maqam_chord_holders(unsigned track, unsigned note, unsigned skip)
{
    unsigned c = 0;
    for (unsigned k = 0; k < KEYS; ++k)
        if (k != skip && own[k].track == track)
            for (unsigned i = 0; i < own[k].p; ++i) c += own[k].note[i] == note;
    return c;
}
MAQAM_SMALL unsigned maqam_chord_start(unsigned key, unsigned track, unsigned note, unsigned desc)
{
    if (key >= KEYS) return 0;
    own[key].n = own[key].p = 0;
    if (!maqam_chord_get(track, MAQAM_CH_MODE) || !maqam_melodic(track)) return 0;
    unsigned n = maqam_chord_notes(track, note, desc, own[key].note);
    if (!n) return 0;
    maqam_chord_commit(track, own[key].note, n);
    own[key].track = (uint8_t)track;
    return own[key].n = (uint8_t)n;
}
MAQAM_SMALL unsigned maqam_chord_owned(unsigned key) { return key < KEYS && own[key].n; }
MAQAM_SMALL int maqam_chord_play(unsigned key, int (*out)(unsigned, unsigned, unsigned))
{
    while (own[key].p < own[key].n) {
        unsigned note = own[key].note[own[key].p], t = own[key].track;
        if (maqam_chord_holders(t, note, key) && !out(t, note, 0)) return 0;
        if (!out(t, note, 1)) return 0;
        ++own[key].p;
    }
    return 1;
}
MAQAM_SMALL int maqam_chord_stop(unsigned key, int (*out)(unsigned, unsigned, unsigned))
{
    while (own[key].p) {
        unsigned note = own[key].note[own[key].p - 1], t = own[key].track;
        if (!maqam_chord_holders(t, note, key) && !out(t, note, 0)) return 0;
        --own[key].p;
    }
    own[key].n = 0;
    return 1;
}
MAQAM_SMALL void maqam_chord_sync(const uint8_t key_note[27])
{
    for (unsigned k = 0; k < KEYS; ++k)
        if (key_note[k] == 0xFF) own[k].n = own[k].p = 0;
}
