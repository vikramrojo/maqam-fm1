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
    if (failures) { printf("default_test: %d failure(s)\n", failures); return 1; }
    printf("default_test: DEFAULT equals the start-up project; the web build keeps 16 demos\n");
    return 0;
}
