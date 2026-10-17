/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* 24-tone note names (maqam-dev D11): key*100 + effective table offset, rounded to the nearest quarter
   tone. Derived on the fly, never stored. A zero offset gives exactly the stock 12-tone name.
   Scale-aware spelling (design D8): a scale whose two forms both have seven degrees is spelled one letter per
   degree (letter = tonic letter + degree number, one accidental b d - + #). All of it is computed from the
   scale masks and root on demand; there is no spelling data. The octave number is the spelled letter's own
   (Cd5 is a quarter tone under C5, B+4 would be the same pitch spelled from B4). */
#include "maqam.h"
#include "scales.h"
#include <string.h>

/* The screen font is uppercase-only, so half-flat is not written as a 'd': MAQAM_HF (0x1a) is a lowercase d
   glyph that only names use (a real 'd' would be drawn as D, which reads as a separate note). */
#define HF MAQAM_HF
static const char *const slot_names[24] = {"C", "C+", "C#", "D" HF, "D", "D+", "D#", "E" HF, "E", "F" HF, "F", "F+",
                                           "F#", "G" HF, "G", "G+", "G#", "A" HF, "A", "A+", "A#", "B" HF, "B", "C" HF};
MAQAM_SMALL unsigned maqam_num(char out[12], int v, unsigned plus)
{
    unsigned n = 0, u = v < 0 ? 0U - (unsigned)v : (unsigned)v;
    if (v < 0) out[n++] = '-'; else if (plus) out[n++] = '+';
    unsigned i = n;
    do out[n++] = (char)('0' + u % 10U); while (u /= 10U);
    out[n] = 0;
    for (unsigned j = n; i + 1 < j; ++i, --j) { char c = out[i]; out[i] = out[j - 1]; out[j - 1] = c; }
    return n;
}
MAQAM_SMALL const char *maqam_slot_name(unsigned slot) { return slot_names[slot % 24U]; }

MAQAM_SMALL void maqam_name24(unsigned note, int cents, char out[12])
{
    /* Quarter tones above C-1; the sum is rounded (ties up) to the nearest 50 c. */
    int c = (int)note * 100 + cents, q = maqam_fdiv(c + 25, 50);
    int slot = q - 24 * maqam_fdiv(q, 24), octave = maqam_fdiv(q, 24) - 1;
    if (slot == 23) ++octave;                            /* C half-flat belongs to the C above it */
    char *p = out;
    for (const char *s = slot_names[slot]; *s; ++s) *p++ = *s;
    maqam_num(p, octave, 0);                             /* the signed octave number, as the stock names write it */
}
MAQAM_SMALL void maqam_note_name(unsigned track, unsigned note, unsigned alt, char out[12])
{
    int cents = 0;
    unsigned scale = MAQAM_CUSTOM, root = 0;
    if (maqam_melodic(track)) {
        scale = maqam_track_scale(track);
        root = maqam_track_root(track);
        struct groove_tuning g;
        maqam_engine_get(track, &g);
        cents = alt ? g.alt[note % 12U] : g.main[note % 12U];
    } else alt = 0;
    maqam_name_scale(scale, root, note, cents, out);
    if (alt) strcat(out, ".");                           /* the alt mark */
}

/* ---- scale-aware spelling. Pitches below are quarter tones above C (0..23, mod 24). ---- */
static const uint8_t nat[7] = {0, 4, 8, 10, 14, 18, 22};   /* C D E F G A B */
/* Accidental (quarter tones: -2 b, -1 d, 0, 1 +, 2 #) that takes letter L to pitch q; |a| > 2 does not fit. */
MAQAM_SMALL static int accof(int q, int L)
{
    int a = (int)maqam_mod(q - nat[L], 24);
    return a > 12 ? a - 24 : a;
}
/* The letter of the tonic: of the root's spellings (natural, or the flat/sharp or half-flat/half-sharp pair)
   the one where every degree of both forms takes a single accidental with the fewest accidentals, flat on a
   tie. -1: not spellable (a form without seven degrees, or a double accidental). */
MAQAM_SMALL static int tonic_calc(unsigned s, unsigned r)
{
    uint32_t m[2] = {scale_up(&SCALES[s]), scale_down(&SCALES[s])};
    int best = -1, bc = 99, ba = 0, natural = 0;
    for (int L = 0; L < 7; ++L) natural |= accof((int)r, L) == 0;
    for (int L = 0; L < 7; ++L) {
        int a = accof((int)r, L), cost = 0;
        if (a < -2 || a > 2 || (natural && a)) continue;
        for (int f = 0; f < 2; ++f) {
            int n = 0;
            for (int sl = 0; sl < 24; ++sl)
                if (m[f] >> sl & 1U) {
                    int x = accof((int)r + sl, (L + n++) % 7);
                    cost += x < -2 || x > 2 ? 99 : x != 0;
                }
            if (n != 7 || !(m[f] & 1U)) return -1;
        }
        if (cost < bc || (cost == bc && a < ba)) { best = L; bc = cost; ba = a; }
    }
    return best;
}
/* The last (scale, root) asked: names are formatted for many notes in a row (the 16 step tiles, the degree
   row), all of the same scale and root, and the scale tables never change. */
MAQAM_SMALL static int tonic(unsigned s, unsigned r)
{
    static struct { uint8_t s, r; int8_t L; } last = {NSCALES, 0, -1};
    if (s >= NSCALES || r >= 24) return -1;
    if (s != last.s || r != last.r) { last.s = (uint8_t)s; last.r = (uint8_t)r; last.L = (int8_t)tonic_calc(s, r); }
    return last.L;
}
/* The letter of pitch q if it is a degree (the up form first, so a shared degree keeps its up spelling). */
MAQAM_SMALL static int letter_at(int L0, unsigned s, unsigned r, unsigned q)
{
    unsigned sl = (q + 24U - r) % 24U;
    for (unsigned f = 0; f < 2; ++f) {
        uint32_t m = f ? scale_down(&SCALES[s]) : scale_up(&SCALES[s]);
        if (m >> sl & 1U) {
            unsigned n = 0;
            for (unsigned i = 0; i < sl; ++i) n += m >> i & 1U;
            return (L0 + (int)n) % 7;
        }
    }
    return -1;
}
MAQAM_SMALL static char *put_name(char *p, int L, int a)
{
    *p++ = "CDEFGAB"[L];
    if (a < 0) *p++ = a == -2 ? MAQAM_FLAT[0] : HF[0];
    else if (a) *p++ = a == 2 ? '#' : '+';
    return p;
}
/* Name (no octave) of pitch q (quarter tones above C) in scale s on root r: out holds 3 bytes. Returns 1 if
   it is a spelled degree, 0 for the fixed 24 name. */
MAQAM_SMALL unsigned maqam_degree_name(unsigned s, unsigned r, unsigned q, char out[4])
{
    int L0 = tonic(s, r), L = L0 < 0 ? -1 : letter_at(L0, s, r, q % 24U);
    if (L < 0) { strcpy(out, slot_names[q % 24U]); return 0; }
    *put_name(out, L, accof((int)(q % 24U), L)) = 0;
    return 1;
}
MAQAM_SMALL void maqam_name_scale(unsigned s, unsigned r, unsigned note, int cents, char out[12])
{
    int L0 = tonic(s, r), L = -1, a = 0, q = maqam_fdiv((int)note * 100 + cents + 25, 50), qm = q - 24 * maqam_fdiv(q, 24);
    if (L0 >= 0) {
        int8_t key[24];
        L = letter_at(L0, s, r, (unsigned)qm);
        if (L < 0 && !maqam_assign(s, r, key))           /* not a degree: the letter of its key's degree */
            for (unsigned sl = 0; sl < 24; ++sl)
                if (key[sl] == (int)(note % 12U)) L = letter_at(L0, s, r, (r + sl) % 24U);
        a = L < 0 ? 0 : accof(qm, L);
        if (a < -2 || a > 2) L = -1;
    }
    if (L < 0) { maqam_name24(note, cents, out); return; }
    maqam_num(put_name(out, L, a), maqam_fdiv(q - a, 24) - 1, 0);
}
