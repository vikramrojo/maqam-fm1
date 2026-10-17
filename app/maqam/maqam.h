/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef MAQAM_H
#define MAQAM_H
#include <stdint.h>
#include "../instrument.h"

/* Scales are library indices (scales.h, frozen). Roots are quarter tones above C, 0..23. Control task only. */
enum { MAQAM_CUSTOM = 0xFF, MAQAM_TRAILER_BYTES = 8, MAQAM_TRAILER_TAG = 0xA1 };

/* Control-path code (every function in pages.c state.c keys.c names.c tuning.c engine.c; none runs in the audio
   task) is built for size: the firmware flags are fixed, so the attribute does it. clang: minsize implies optsize. */
#ifndef MAQAM_SMALL /* a build may override it (e.g. -DMAQAM_SMALL= to compare sizes) */
#define MAQAM_SMALL __attribute__((minsize))
#endif

/* The half-flat letter in 24-tone names: screen.c draws this code as a lowercase d (see names.c). */
#define MAQAM_HF "\x1a"
/* The flat letter: screen.c draws this code as a lowercase b (a real b would be drawn as B, a note). */
#define MAQAM_FLAT "\x1b"

/* tuning.c: pure maths. */
unsigned maqam_scale_count(void);
int maqam_fdiv(int a, int b);                     /* floor division */
unsigned maqam_mod(int a, int b);                 /* a - b * maqam_fdiv(a, b): 0..b-1 for b > 0 */
/* key[slot] = pitch class 0..11 carrying that grid slot, -1 for unused slots; 0 ok, <0 no assignment. */
int maqam_assign(unsigned scale, unsigned root, int8_t key[24]);
/* 0 ok, -1 bad argument or no assignment (main zeroed), -2 an entry left -127..127 (clamped). */
int maqam_main(unsigned scale, unsigned root, unsigned trad, int8_t main[GROOVE_TUNING_KEYS]);
void maqam_alt_default(unsigned scale, unsigned root, int8_t alt[GROOVE_TUNING_KEYS]);
/* First (scale, root) in index order whose main table equals `main`; 0 found, -1 none. */
int maqam_match(const int8_t main[GROOVE_TUNING_KEYS], unsigned trad, unsigned *scale, unsigned *root);

/* engine.c: the only callers of groove_tuning_*. set/apply mark the project changed. */
int maqam_engine_set(unsigned track, const struct groove_tuning *t);
void maqam_engine_get(unsigned track, struct groove_tuning *t);
int maqam_engine_capture(struct groove_tuning t[GROOVE_TRACKS]);
void maqam_engine_alt_hold(unsigned track, unsigned on);
void maqam_engine_touch(void); /* mark the project changed */

/* state.c: project scale, per-track override, intonation. Every change pushes tables to the engine.
   Setters return 0, or <0 for a bad argument / a table that does not fit (nothing pushed for that track). */
void maqam_init(void);
int maqam_set_project(unsigned scale, unsigned root);
int maqam_set_intonation(unsigned trad);
int maqam_set_override(unsigned track, unsigned scale, unsigned root);
int maqam_clear_override(unsigned track);
int maqam_set_alt(unsigned track, unsigned key, int cents); /* -127..127 */
unsigned maqam_project_scale(void);
unsigned maqam_project_root(void);
unsigned maqam_trad(void);
unsigned maqam_melodic(unsigned track);          /* tuned by maqam: not track 7, not 808/sampler */
unsigned maqam_overridden(unsigned track);
unsigned maqam_track_scale(unsigned track);      /* library index, or MAQAM_CUSTOM */
unsigned maqam_track_root(unsigned track);       /* 0..23 (0 for CUSTOM) */
/* Re-derive each track's label from the engine tables (after V13 undo or any outside table edit). */
void maqam_refresh(void);

/* Key layout, in the order the KEYS page cycles them. The project default is saved in the trailer (flags
   bits 1-2); a track may override it (RAM only). 0 SNAP is the default so a stock or absent trailer gives SNAP. */
enum { MAQAM_SNAP, MAQAM_WHITE, MAQAM_CHROM };
unsigned maqam_default_layout(void);
int maqam_set_default_layout(unsigned layout);   /* 0 ok, <0 bad argument */

/* keys.c: key note choice, alt gesture, key lights. Control task only. Notes are 0..127, or 128 = silent.
   Key 0 is MIDI 53 (F3) at octave 0; key 7 is C4. The engine tunes the note; only numbers are chosen here. */
void maqam_keys_reset(void);                      /* per-track layouts and directions to defaults */
unsigned maqam_layout(unsigned track);            /* effective layout (CHROM for CUSTOM or chromatic scales) */
int maqam_layout_raw(unsigned track);             /* the track's own layout, or -1 when it follows the project */
unsigned maqam_last_descending(unsigned track);   /* the last key of the track went down (within the 1 s window state) */
int maqam_set_layout(unsigned track, int layout); /* -1 follows the project default; 0 ok */
/* Pure: the note for `key` going up (descending=0) or down (1). */
unsigned maqam_key_note(unsigned track, unsigned key, int octave, unsigned descending);
/* A key goes down: updates the direction (lower than the previous key within 1 s = descending). */
unsigned maqam_key_press(unsigned track, unsigned key, int octave, uint32_t now);
/* Bit k set: key k carries a degree of either form. 0 for drum, CUSTOM and chromatic tracks. */
uint32_t maqam_key_lights(unsigned track);
/* OCT buttons. oct_held/oct_pressed: bit 0 OCT-, bit 1 OCT+. active: not a step page. key_down: a key went
   down this frame. Returns 1 if maqam owns the buttons (melodic track, active): OCT held turns alt hold on,
   a double tap within 300 ms with no key between changes *octave by one (-2..2). Always call: it releases
   alt hold when the gesture ends or the track changes. */
unsigned maqam_oct_buttons(unsigned track, unsigned active, unsigned oct_held, unsigned oct_pressed,
                           unsigned key_down, uint32_t now, int *octave);

/* Persistence (hooks called by app/project.c). Trailer: [0] tag, [1] scale, [2] root, [3] flags
   (bit 0 TRAD, bits 1-2 key layout), [4] override mask, [5..6] 0, [7] ~(xor of [0..6]). */
void maqam_project_save(uint8_t t[MAQAM_TRAILER_BYTES]);
/* Call after the engine tables are loaded. NULL or an invalid trailer: defaults, tables decide. */
void maqam_project_loaded(const uint8_t *t);

/* names.c: 24-tone names. maqam_name24 writes the name of `note` raised by `cents` (rounded to the nearest
   quarter tone) with its octave ("Ed4", "Cd5" for B4; the d is MAQAM_HF +50 c; 12-tone names exactly as the stock screen when
   cents is 0). maqam_note_name reads the track's main table, or its alt table when alt is set (alt names get
   a trailing '.', the alt mark); drum tracks give the stock name. out holds at least 12 bytes. */
/* The one number formatter: v as decimal text in out (12 bytes), '-' for a negative, '+' first when plus and
   v >= 0. Returns the length. The cents row under the key strip uses (c, c > 0). */
unsigned maqam_num(char out[12], int v, unsigned plus);
const char *maqam_slot_name(unsigned slot);       /* the 24 base names, slot 0 = C */
void maqam_name24(unsigned note, int cents, char out[12]);
void maqam_note_name(unsigned track, unsigned note, unsigned alt, char out[12]);
/* Scale-aware spelling (D8): `scale` on `root` (0..23); a CUSTOM or unspellable scale gives maqam_name24.
   maqam_name_scale: the name with octave. maqam_degree_name: pitch q (quarter tones above C, any) without
   octave, out holds 4 bytes; returns 1 if spelled, 0 if it is a fixed 24 name. */
void maqam_name_scale(unsigned scale, unsigned root, unsigned note, int cents, char out[12]);
unsigned maqam_degree_name(unsigned scale, unsigned root, unsigned q, char out[4]);

/* pages.c: the scale pages (control task only). The screen hooks in screen.c only open the page, pass the
   encoders/keys in and draw this view; every value is read back from the maqam_* state. */
enum { MAQAM_SUBPAGES = 4 };
struct maqam_view {
    const char *title;               /* the page word: PROJECT, TRACK, KEYS, ALT (static) */
    uint8_t index;                   /* its position 1..MAQAM_SUBPAGES (drawn n/4) */
#define MAQAM_SUB_BIG 19                /* longest subtitle drawn in the large font */
    char sub[32];                    /* "Ed ARAB Sikah trad" (arrow after the scale when its forms differ) */
    char status[28];                 /* "TRACK 3  FOLLOWS PROJECT" / OVERRIDE / CUSTOM TABLE / DRUMS - NOT TUNED */
    char note[16];                   /* "home: D" hint ("" for WEST scales); "plays: SNAP" on the KEYS page */
    char line[2][24];                /* ALT page only: "ALT -100 Eb4" (large), "MAIN -47 Ed4" */
    const char *knob[4][2];          /* label, value: static strings, or rname / atext below; NULL label = no knob */
    char rname[4], atext[12];        /* the tonic's name; the alt cents with a sign ("+0") */
    char warn[36];                   /* "REDUCE SAMPLE BANK" when the last save could not store tuning */
    int8_t cents[12];                /* the cents under each cell: main table, or alt on the ALT page */
    char lab[12][4];                 /* cell names: spelled where a key carries a degree, else the fixed name */
    uint16_t lit, root, sel;         /* cell masks: scale keys, root key, selected alt key */
    uint8_t ncell;                   /* >0: the degree row, ncell cells in pitch order (names carry ^ / v marks);
                                        0: the 12-key strip, cell i = pitch class i */
    uint8_t custom, tuned;
};
void maqam_page_next(void);                      /* SCL on the page: the next subpage, wrapping; the page is reopened on the last one */
/* The overview / step header tag ("Ed RAST"); 0 for drums and WEST chromatic, "CUST" for a custom table. 12 bytes. */
unsigned maqam_header_tag(unsigned track, char out[12]);
/* Encoders as the panel gives them (0 selects the track and is handled by the caller); key_down: panel keys
   that went down (ALT: the first selects that key; the keys play as on any page). */
void maqam_page_control(unsigned track, const int16_t enc[7], uint32_t key_down);
void maqam_page_view(unsigned track, struct maqam_view *v);
/* A project save finished with result r (0 or a GROOVE_PROJECT_* / flash code): remembers whether tuning could
   not be stored (GROOVE_PROJECT_TUNING_SPACE) so the scale page can say REDUCE SAMPLE BANK. Called by
   groove_project_save_slot, so every save path counts. Cleared by a good save and by maqam_init. */
void maqam_page_save_result(int r);
#endif
