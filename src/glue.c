/*
 * Purism Core: glue processing
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "private.h"
#include "array.h"
#include "debug.h"
#include "gather.h"
#include "glue.h"
#include "moc3.h"
#include "model.h"
#include "math2.h"

PSM__DEF void
psm__gather_glues(struct psm__model *m)
{
  psm__i32 count = m->glues.count;
  if (count <= 0)
    return;

  struct psm__glue *items = m->glues.items;
  if (!items)
    return;
  struct psm__sections *ms = m->source->sections;

  psm__i32 *keyform_base_idx = ms->glue_src.keyform_off;
  psm__f32 *intensity_src = ms->glue_key_src.intensity;
  if (!keyform_base_idx || !intensity_src)
    return;

  struct psm__binding *const *bindings = m->glues.bindings;

  struct psm__gather_channel ch[] = {
    { intensity_src, m->glues.keydata.intensity },
  };
  psm__gather_scalars(count, bindings, keyform_base_idx,
      &m->glues.keydata.interp, ch, 1);
}

PSM__DEF void
psm__apply_glues(struct psm__model *m)
{
  psm__i32 count = m->glues.count;
  if (count <= 0)
    return;

  struct psm__glue *items = m->glues.items;

  psm__f32 **pos = m->art_meshes.pos;
  psm__f32  *calc_int = m->glues.intensity;

  if (!items || !pos || !calc_int)
    return;

  for (psm__i32 gi = 0; gi < count; gi++) {
    struct psm__glue *glue = &items[gi];

    psm__i32 ic = glue->glue_info_count;
    if (ic <= 0)
      continue;

    psm__i32 m0 = glue->mesh_idx0, m1 = glue->mesh_idx1;

    psm__f32  intensity = calc_int[gi];
    psm__f32 *p0 = pos[m0], *p1 = pos[m1];
    if (!p0 || !p1)
      continue;

    /* Dirty gate: the glue adds a stateful contribution on top of the
     * meshes' current positions. Re-apply it only when the glue itself
     * moved (intensity key data / blend shape) or one of its meshes was
     * recomputed this frame (fresh pre-glue positions). When all three
     * are clean the stored positions already include the previous
     * glue contribution, so skipping is exact. */
    struct psm__binding *b = glue->binding;
    psm__i32 self_dirty = b ? (b->idx_dirty || b->weight_dirty) : 1;
    if (!self_dirty && !m->glue_bs_dirty[gi] &&
        !m->mesh_changed[m0] && !m->mesh_changed[m1])
      continue;

    psm__f32 *wt = glue->weights;
    psm__u16 *pi = glue->pos_idx;
    if (!wt || !pi)
      continue;

    for (psm__i32 i = 0; i + 1 < ic; i += 2) {
      psm__i32 i0 = pi[i], i1 = pi[i + 1];
      psm__f32 w0 = wt[i], w1 = wt[i + 1];

      struct psm__vec2 a = psm__v2_load(p0, i0);
      struct psm__vec2 b = psm__v2_load(p1, i1);
      struct psm__vec2 d = psm__v2_sub(b, a);

      psm__v2_store(p0, i0, psm__v2_add(a, psm__v2_scale(d, intensity * w0)));
      psm__v2_store(p1, i1, psm__v2_sub(b, psm__v2_scale(d, intensity * w1)));
    }
  }
}
