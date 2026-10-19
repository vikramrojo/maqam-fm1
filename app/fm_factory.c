/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "fm_patch.h"
#include <string.h>

#if defined(FM1_OPEN_WEB) /* maqam: the V15 bank builds only for the web workbench (fork) */
#include "fm_factory.inc"

/* Expand once during startup, before the audio owners are running. MIDI
 * program changes then read SRAM without decompression or heap allocation. */
static uint8_t factory_raw[FM1_FM_PROGRAMS*128];
static unsigned factory_ready;
static FM1_DSP_CODE void factory_init(void)
{
    if (factory_ready) return;
    uint8_t *out=factory_raw;
    for (unsigned at=0;at<sizeof factory_packed;) {
        unsigned token=factory_packed[at++];
        if (token<128) *out++=(uint8_t)token;
        else {
            unsigned low=factory_packed[at++],distance=low&127U;
            if (low&128U) distance|=(unsigned)factory_packed[at++]<<7;
            for (unsigned n=token-128+3;n;--n) { *out=out[-(int)distance]; ++out; }
        }
    }
    factory_ready=1;
}
FM1_DSP_CODE const uint8_t *groove_fm_factory_raw(unsigned program)
{
    factory_init();
    return factory_raw+(program%FM1_FM_PROGRAMS)*128;
}
FM1_DSP_CODE void groove_fm_factory(uint8_t p[156], unsigned program)
{
    groove_fm_unpack(p,groove_fm_factory_raw(program));
}
#else
/* maqam: device image (fork). V14 plus the maqam code does not fit with the 10.5 KiB V15 bank, so an
 * unsaved library bank plays the DX7 INIT VOICE; tools/dx7_factory.py writes the same V15 bank as .syx
 * files to install through the workbench. */
static FM1_DSP_DATA uint8_t init_voice[128];   /* DX7 INIT VOICE in VMEM form, built on first use */
FM1_DSP_CODE const uint8_t *groove_fm_factory_raw(unsigned program)
{
    (void)program;
    uint8_t *v = init_voice;
    if (!v[0]) {
        for (unsigned op = 0; op < 6; ++op, v += 17) { memset(v, 99, 7); v[8] = 39; v[12] = 56; v[15] = 2; }
        init_voice[5 * 17 + 14] = 99;                                   /* operator 1 carrier at full level */
        memset(init_voice + 102, 99, 4); memset(init_voice + 106, 50, 4);
        init_voice[111] = 8; init_voice[112] = 35; init_voice[116] = 49; init_voice[117] = 24;
        memcpy(init_voice + 118, "INIT VOICE", 10);
    }
    return init_voice;
}
FM1_DSP_CODE void groove_fm_factory(uint8_t p[156], unsigned program) { groove_fm_unpack(p, groove_fm_factory_raw(program)); }
#endif
