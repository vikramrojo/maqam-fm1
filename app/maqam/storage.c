/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Per-slot maqam settings on V14: the project record fills its sector, so the 8 settings bytes live in
   one fork-owned sector below the patch library (GROOVE_MAQAM_OFFSET). Each entry carries the key of
   the project it was saved with (the V14 tuning key), so a slot replaced elsewhere loads defaults.
   A write stages the whole sector in the shared staging sector with its own magic and a commit word,
   then copies it into place: power loss leaves the old sector or completes the new one on restart. */
#include "maqam.h"
#include <stddef.h>
#include <string.h>

enum { MAGIC = 0x31534d46U /* FMS1 */, STAGE_MAGIC = 0x31514d46U /* FMQ1 */, VERSION = 1,
       STAGE = GROOVE_PROJECT_SLOTS * FM1_FLASH_SECTOR_BYTES };
struct entry { uint32_t key; uint8_t t[MAQAM_TRAILER_BYTES]; uint32_t crc; };
struct sector { uint32_t magic, version; struct entry e[GROOVE_PROJECT_SLOTS]; };
struct stage { uint32_t magic, crc, commit; struct sector body; };
_Static_assert(sizeof(struct stage) <= FM1_FLASH_SECTOR_BYTES, "maqam settings fit one sector");

MAQAM_SMALL static uint32_t entry_crc(const struct entry *e)
{ return ~groove_crc32(~0U, e, offsetof(struct entry, crc)); }
MAQAM_SMALL static uint32_t body_crc(const struct sector *s) { return ~groove_crc32(~0U, s, sizeof *s); }
/* An old sample bank stored before this layout may reach into the sector. */
MAQAM_SMALL static int covered(void)
{
    uint32_t h[3];
    if (fm1_flash_size() < GROOVE_FLASH_BYTES) return 1;
    int r = fm1_flash_read(GROOVE_SAMPLE_OFFSET, h, sizeof h);
    return r ? r : h[0] == 0x53314d46U && h[2] > GROOVE_MAQAM_OFFSET - GROOVE_SAMPLE_OFFSET;
}
MAQAM_SMALL static int place(const struct sector *s)
{
    struct sector check;
    int r = fm1_flash_erase(GROOVE_MAQAM_OFFSET);
    if (!r) r = fm1_flash_write(GROOVE_MAQAM_OFFSET, s, sizeof *s);
    if (!r) r = fm1_flash_read(GROOVE_MAQAM_OFFSET, &check, sizeof check);
    if (!r && memcmp(&check, s, sizeof check)) r = FM1_FLASH_IO;
    return r;
}
MAQAM_SMALL int maqam_storage_recover(void)
{
    struct stage st;
    int r = fm1_flash_read(STAGE, &st, sizeof st);
    if (r || st.magic != STAGE_MAGIC) return r;
    if (!st.commit && st.crc == body_crc(&st.body)) {
        struct sector now;
        r = fm1_flash_read(GROOVE_MAQAM_OFFSET, &now, sizeof now);
        if (!r && memcmp(&now, &st.body, sizeof now)) r = place(&st.body);
    }
    return r ? r : fm1_flash_erase(STAGE);   /* an uncommitted stage is dropped: the old sector stands */
}
MAQAM_SMALL int maqam_storage_load(unsigned slot, uint32_t key, uint8_t t[MAQAM_TRAILER_BYTES])
{
    struct sector s;
    if (slot >= GROOVE_PROJECT_SLOTS || covered() || fm1_flash_read(GROOVE_MAQAM_OFFSET, &s, sizeof s) ||
        s.magic != MAGIC || s.version != VERSION)
        return 0;
    const struct entry *e = &s.e[slot];
    if (e->key != key || e->crc != entry_crc(e)) return 0;
    memcpy(t, e->t, MAQAM_TRAILER_BYTES);
    return 1;
}
MAQAM_SMALL int maqam_storage_save(unsigned slot, uint32_t key, const uint8_t t[MAQAM_TRAILER_BYTES])
{
    static struct stage st;
    if (slot >= GROOVE_PROJECT_SLOTS) return GROOVE_PROJECT_INVALID;
    int r = covered();
    if (r) return r < 0 ? r : maqam_trailer_default(t) ? 0 : GROOVE_PROJECT_TUNING_SPACE;
    if ((r = groove_storage_prepare())) return r;
    if ((r = fm1_flash_read(GROOVE_MAQAM_OFFSET, &st.body, sizeof st.body))) return r;
    if (st.body.magic != MAGIC || st.body.version != VERSION) {
        memset(&st.body, 0xff, sizeof st.body);
        st.body.magic = MAGIC; st.body.version = VERSION;
    }
    struct entry *e = &st.body.e[slot], next = {key, {0}, 0};
    memcpy(next.t, t, sizeof next.t);
    next.crc = entry_crc(&next);
    if (!memcmp(e, &next, sizeof next)) return 0;
    *e = next;
    st.magic = STAGE_MAGIC; st.crc = body_crc(&st.body); st.commit = ~0U;
    uint32_t commit = 0;
    r = fm1_flash_erase(STAGE);
    if (!r) r = fm1_flash_write(STAGE, &st, sizeof st);
    if (!r) r = fm1_flash_write(STAGE + offsetof(struct stage, commit), &commit, sizeof commit);
    return r ? r : maqam_storage_recover();
}
