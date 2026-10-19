/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The device image's factory content (fork): app/presets.c built without FM1_OPEN_WEB keeps one DEFAULT
 * project that must equal what a freshly started device exports, byte for byte apart from its name, while
 * the web build keeps all 16 demos. presets.c is #included a second time without the web define, with its
 * two public functions renamed. Exits non-zero on failure. */
#include "internal.h"
#undef FM1_OPEN_WEB
#define groove_factory_name device_factory_name
#define groove_factory_build device_factory_build
#define GROOVE_FACTORY_PROJECTS 1
#include "../../app/presets.c"
#undef groove_factory_name
#undef groove_factory_build
#undef GROOVE_FACTORY_PROJECTS
/* The device image's fm_factory.c: no V15 bank, INIT VOICE for unsaved library banks. */
#define groove_fm_factory_raw device_fm_factory_raw
#define groove_fm_factory device_fm_factory
#include "../../app/fm_factory.c"
#undef groove_fm_factory_raw
#undef groove_fm_factory
#include <stdio.h>
#include <stdlib.h>

void sim_boot(unsigned clear);
static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main(void)
{
    /* web build: all 16 demos */
    CHECK(groove_factory_name(15) && !groove_factory_name(16));
    /* device image: one DEFAULT */
    CHECK(!strcmp(device_factory_name(0), "DEFAULT") && !device_factory_name(1));
    struct groove_project *p = calloc(1, sizeof *p), *boot = calloc(1, sizeof *boot);
    CHECK(device_factory_build(1, p) < 0);
    CHECK(device_factory_build(0, p) == 0 && groove_project_valid(p));
    unsigned playing = 0;
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        CHECK(!p->preset[t].modified);
        for (unsigned i = 0; i < GROOVE_STEPS; ++i) playing |= p->step[t][i].velocity | p->step[t][i].drums;
    }
    CHECK(!playing);
    sim_boot(3);
    CHECK(groove_project_export((uint8_t *)boot, sizeof *boot) == 0);
    memcpy(boot->name, p->name, sizeof p->name);
    const uint8_t *a = (const uint8_t *)p, *b = (const uint8_t *)boot;
    for (unsigned i = 0, shown = 0; i < sizeof *p && shown < 8; ++i)
        if (a[i] != b[i]) { printf("byte %u: DEFAULT %u, start-up %u\n", i, a[i], b[i]); ++shown; }
    CHECK(!memcmp(p, boot, sizeof *p));
    free(p); free(boot);
    /* unsaved library banks on the device play INIT VOICE: one full-level sine carrier, no detune */
    uint8_t v[156];
    device_fm_factory(v, 77);
    CHECK(!memcmp(v + 145, "INIT VOICE", 10) && v[155] == 63 && v[134] == 0 && v[5 * 21 + 16] == 99 && v[16] == 0);
    for (unsigned op = 0; op < 6; ++op) CHECK(v[op * 21 + 18] == 1 && v[op * 21 + 20] == 7);
    CHECK(device_fm_factory_raw(0) == device_fm_factory_raw(127));
    /* the web build keeps the V15 bank: program 0 is PIANO 1 */
    groove_fm_factory(v, 0);
    CHECK(!memcmp(v + 145, "PIANO 1", 7));
    if (failures) { printf("default_test: %d failure(s)\n", failures); return 1; }
    printf("default_test: DEFAULT equals the start-up project; INIT VOICE for unsaved banks; the web build keeps 16 demos and the V15 bank\n");
    return 0;
}
