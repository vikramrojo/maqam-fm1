/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The only file that calls groove_tuning_*. V13 does not mark the project changed on tuning-only edits. */
#include "maqam.h"

int maqam_engine_set(unsigned track, const struct groove_tuning *t)
{
    int r = groove_tuning_set(track, t);
    if (!r) groove_project_touch();
    return r;
}
void maqam_engine_get(unsigned track, struct groove_tuning *t) { groove_tuning_get(track, t); }
int maqam_engine_capture(struct groove_tuning t[GROOVE_TRACKS]) { return groove_tuning_capture(t); }
void maqam_engine_alt_hold(unsigned track, unsigned on) { groove_tuning_alt_hold(track, on); }
void maqam_engine_touch(void) { groove_project_touch(); }
