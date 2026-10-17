/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Edge-case screenshots of the scale pages (SCL), rendered by the real app/screen.c into
 * build/host/shots/ as PNGs. Not a test (no _test suffix, so build.sh skips it). Run from the repository root. */
#define main pages_test_main
#include "pages_test.c"
#undef main
#include <sys/stat.h>

/* pick the first root (0..23) of `scale` whose spelling falls back (want_spelled = 0) or works (1) */
static unsigned root_where(unsigned scale, unsigned want_spelled)
{
    for (unsigned r = 0; r < 24; ++r) {
        char out[4];
        if (maqam_degree_name(scale, r, r, out) == want_spelled) return r;
    }
    return 0;
}
static void project(const char *code, unsigned root, unsigned trad)
{
    sim_boot(1);
    maqam_init();
    ms(50);
    maqam_set_intonation(trad);
    maqam_set_project(find(code), root);
    open_scale();
    ms(400);
}
static void snap(const char *name)
{
    char path[64];
    snprintf(path, sizeof path, "shots/%s", name);
    shot(path);
    printf("build/host/shots/%s.png\n", name);
}

int main(void)
{
    mkdir("build/host/shots", 0755);
    /* degree rows on the PROJECT page */
    project("RAST", 0, 1);   snap("01_rast_c_trad_up_down");
    project("RAST", 0, 0);   snap("02_rast_c_equal");
    project("HIJAZ", 4, 1);  snap("03_hijaz_d_flats_sharps");
    project("SABA", 4, 1);   snap("04_saba_d_g_flat");
    project("SIKAH", 7, 1);  snap("05_sikah_on_e_half_flat");
    project("SUZNK", 0, 1);  snap("06_suznak_9_degrees");
    project("PEN", 0, 1);    snap("07_pentatonic_fixed_names");
    project("CHR", 0, 1);    snap("08_chromatic_12_cells");
    project("MAJ", 20, 1);   snap("09_major_on_bb_key");
    {   /* a seven-note scale at a root that would need double accidentals: fixed names */
        unsigned s = find("BAYAT"), r = root_where(s, 0);
        printf("BAYAT falls back at root %u\n", r);
        sim_boot(1); maqam_init(); ms(50); maqam_set_project(s, r); open_scale(); ms(400);
        snap("10_bayati_fallback_root");
    }
    project("BAYAT", 9, 1);  snap("11_bayati_quarter_root_e_plus");
    project("P.HUS", 18, 0); snap("12_psych_huseyni_on_a");
    project("J.DRB", 0, 0);  snap("13_jins_darbari");
    project("T.RST", 14, 1); snap("14_turkish_rast_on_g");

    /* TRACK page: an override, and a CUSTOM table */
    project("RAST", 0, 1);
    enc(1, 1); ms(100);                              /* ALGO: TRACK page */
    maqam_set_override(0, find("BAYAT"), 4); ms(300);
    snap("15_track_override_bayati_d");
    {
        struct groove_tuning t;
        groove_tuning_get(0, &t);
        int8_t *main_table = (int8_t *)&t;          /* `main` is the first member; the macro above renames it */
        main_table[1] = 17; main_table[2] = -3;
        groove_tuning_set(0, &t);
        maqam_refresh(); ms(300);
        snap("16_track_custom_table");
    }

    /* KEYS page */
    project("BAYAT", 4, 1);
    enc(1, 2); ms(200);
    snap("17_keys_layouts");

    /* ALT page: extremes on adjacent keys and an out-of-scale flat */
    project("BAYAT", 4, 1);
    enc(1, 3); ms(100);
    maqam_set_alt(0, 4, -100);                       /* E key: Eb */
    maqam_set_alt(0, 5, 127);
    maqam_set_alt(0, 6, -127);
    maqam_set_alt(0, 10, 50);                        /* A# key (Bb): B half-flat */
    ms(300);
    snap("18_alt_bayati_extremes");

    /* the sample-bank warning on the scale page */
    sim_boot(1);
    maqam_init();
    {
        unsigned bytes;
        uint8_t *b = make_bank(2028, &bytes);
        memcpy(sim_flash() + GROOVE_SAMPLE_OFFSET, b, bytes);
        free(b);
        groove_samples_init();
        for (unsigned i = 0; i < 4096 && groove_samples_status() == 1; ++i) groove_samples_poll();
    }
    ms(50);
    maqam_set_project(find("RAST"), 0);
    open_scale(); ms(300);
    btn(SAVE); btn(SAVE); ms(300);                   /* refused: non-zero tuning + over-size bank */
    btn(HOME); open_scale(); ms(400);
    snap("19_reduce_sample_bank");
    /* the scale tag in the header of the overview page */
    sim_boot(1);
    maqam_init();
    ms(50);
    maqam_set_project(find("RAST"), 7);
    ms(400);
    snap("20_overview_tag_rast");
    /* the stock DX7 scaling screens, the frame the scale pages follow (HOME+SCL, then SCL) */
    sim_boot(1); maqam_init(); ms(50);
    hold_home_scl(); ms(400);
    snap("21_dx7_scaling_stock");
    btn(SCL); ms(400);
    snap("22_dx7_curves_stock");
    return failures != 0;
}
