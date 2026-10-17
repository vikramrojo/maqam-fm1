/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* 24-tone note names (maqam-dev D11): key*100 + effective table offset, rounded to the nearest quarter
   tone. Derived on the fly, never stored. A zero offset gives exactly the stock 12-tone name. */
#include "maqam.h"
#include <string.h>

/* The screen font is uppercase-only, so half-flat is not written as a 'd': MAQAM_HF (0x1a) is a lowercase d
   glyph that only names use (a real 'd' would be drawn as D, which reads as a separate note). */
#define HF MAQAM_HF
static const char *const slot_names[24] = {"C", "C+", "C#", "D" HF, "D", "D+", "D#", "E" HF, "E", "F" HF, "F", "F+",
                                           "F#", "G" HF, "G", "G+", "G#", "A" HF, "A", "A+", "A#", "B" HF, "B", "C" HF};
MAQAM_SMALL const char *maqam_slot_name(unsigned slot) { return slot_names[slot % 24U]; }

MAQAM_SMALL static int fdiv(int a, int b) { int q = a / b; return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q; }

/* The signed octave number, as the stock names write it (note 60 = C4, note 0 = C-1). */
MAQAM_SMALL static char *put_int(char *p, int v)
{
    char d[12];
    unsigned n = 0, u = v < 0 ? (unsigned)-v : (unsigned)v;
    do d[n++] = (char)('0' + u % 10U); while (u /= 10U);
    if (v < 0) *p++ = '-';
    while (n) *p++ = d[--n];
    return p;
}
MAQAM_SMALL void maqam_name24(unsigned note, int cents, char out[12])
{
    /* Quarter tones above C-1; the sum is rounded (ties up) to the nearest 50 c. */
    int c = (int)note * 100 + cents, q = fdiv(c + 25, 50);
    int slot = q - 24 * fdiv(q, 24), octave = fdiv(q, 24) - 1;
    if (slot == 23) ++octave;                            /* C half-flat belongs to the C above it */
    char *p = out;
    for (const char *s = slot_names[slot]; *s; ++s) *p++ = *s;
    p = put_int(p, octave);
    *p = 0;
}
MAQAM_SMALL void maqam_note_name(unsigned track, unsigned note, unsigned alt, char out[12])
{
    int cents = 0;
    if (maqam_melodic(track)) {
        struct groove_tuning g;
        maqam_engine_get(track, &g);
        cents = alt ? g.alt[note % 12U] : g.main[note % 12U];
    } else alt = 0;
    maqam_name24(note, cents, out);
    if (alt) strcat(out, ".");                           /* the alt mark */
}
