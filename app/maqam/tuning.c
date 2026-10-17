/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Pure maths: key assignment (tools/gen_scales.py KEY ASSIGNMENT, ported exactly), tables, matching. */
#include "maqam.h"
#include "scales.h"
#include <string.h>

unsigned maqam_scale_count(void) { return NSCALES; }
const char *maqam_scale_code(unsigned scale) { return scale < NSCALES ? SCALES[scale].code : "CUSTOM"; }

/* Depth-first, slots ascending, odd absolute slots try the upper key first. */
static int dfs(const uint8_t *slot, unsigned n, unsigned i, unsigned root, unsigned used, int8_t *key)
{
    if (i == n) return 1;
    unsigned a = (root + slot[i]) % 24U, c[2], nc = 1;
    if (a % 2U) { c[0] = ((a + 1U) / 2U) % 12U; c[1] = (a - 1U) / 2U; nc = 2; }
    else c[0] = a / 2U;
    for (unsigned j = 0; j < nc; ++j)
        if (!(used & (1U << c[j]))) {
            key[slot[i]] = (int8_t)c[j];
            if (dfs(slot, n, i + 1, root, used | 1U << c[j], key)) return 1;
        }
    key[slot[i]] = -1;
    return 0;
}
int maqam_assign(unsigned scale, unsigned root, int8_t key[24])
{
    uint8_t slot[24];
    unsigned n = 0;
    memset(key, -1, 24);
    if (scale >= NSCALES || root >= 24) return -1;
    uint32_t mask = SCALES[scale].up | SCALES[scale].down;
    for (unsigned s = 0; s < 24; ++s)
        if (mask >> s & 1U) slot[n++] = (uint8_t)s;
    return dfs(slot, n, 0, root, 0, key) ? 0 : -1;
}

/* Signed remainder in [-m/2, m/2). */
static int wrap(int v, int m)
{
    v %= m;
    if (v < 0) v += m;
    return v >= m / 2 ? v - m : v;
}
static int div10(int t) { return t >= 0 ? (t + 5) / 10 : -((-t + 5) / 10); } /* half away from zero */

int maqam_main(unsigned scale, unsigned root, unsigned trad, int8_t main[GROOVE_TUNING_KEYS])
{
    int8_t key[24];
    int r = 0;
    memset(main, 0, GROOVE_TUNING_KEYS);
    if (maqam_assign(scale, root, key)) return -1;
    for (unsigned s = 0; s < 24; ++s) {
        if (key[s] < 0) continue;
        int d = trad ? div10(wrap((int)root * 500 + SCALES[scale].cents_x10[s] - key[s] * 1000, 12000))
                     : wrap(((int)root + (int)s) * 50 - key[s] * 100, 1200);
        if (d < -127 || d > 127) { d = d < 0 ? -127 : 127; r = -2; }
        main[key[s]] = (int8_t)d;
    }
    return r;
}
void maqam_alt_default(unsigned scale, unsigned root, int8_t alt[GROOVE_TUNING_KEYS])
{
    int8_t key[24];
    memset(alt, 0, GROOVE_TUNING_KEYS);
    if (scale >= NSCALES || SCALES[scale].set == SET_WEST) return; /* WEST: all zero, quarter tones are one edit away */
    if (maqam_assign(scale, root, key)) return;
    for (unsigned s = 0; s < 24; ++s)
        if (key[s] >= 0 && !((root + s) % 2U)) alt[key[s]] = -50; /* semitone degree: its half-flat */
}
int maqam_match(const int8_t main[GROOVE_TUNING_KEYS], unsigned trad, unsigned *scale, unsigned *root)
{
    int8_t m[GROOVE_TUNING_KEYS];
    for (unsigned s = 0; s < NSCALES; ++s)
        for (unsigned r = 0; r < 24; ++r)
            if (maqam_main(s, r, trad, m) == 0 && !memcmp(m, main, sizeof m)) {
                *scale = s; *root = r;
                return 0;
            }
    return -1;
}
