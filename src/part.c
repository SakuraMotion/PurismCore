/*
 * Purism Core: part hierarchy and opacity
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "private.h"
#include "array.h"
#include "debug.h"
#include "gather.h"
#include "moc3.h"
#include "model.h"
#include "part.h"

PSM__DEF void
psm__enable_parts(struct psm__model *m)
{
  psm__i32 count = m->parts.count;
  if (count <= 0)
    return;

  struct psm__part *items = m->parts.items;
  bool *enable = m->parts.enable;

  for (psm__i32 i = 0; i < count; i++) {
    struct psm__part *part = &items[i];
    bool en = part->local_enable;
    psm__i32 parent_part_idx = part->parent_part_idx;

    if (en && parent_part_idx != -1)
      en = enable[parent_part_idx];
#ifndef PSM_FAST_AND_DANGEROUS
    if (en && !part->binding)
      en = 0;
#endif
    if (en)
      en = !part->binding->out_of_range;

    enable[i] = en;
  }
}

PSM__DEF void
psm__gather_parts(struct psm__model *m)
{
  psm__i32 count = m->parts.count;
  if (count <= 0)
    return;

  struct psm__part *items = m->parts.items;
  if (!items)
    return;
  struct psm__sections *ms = m->source->sections;
  psm__i32 *keyform_off = ms->part_src.keyform_off;
  psm__f32 *draw_order_src = ms->part_key_src.draw_order;

  if (!keyform_off || !draw_order_src)
    return;

  struct psm__binding *bindings[count];
  for (psm__i32 i = 0; i < count; i++)
    bindings[i] = items[i].binding;

  struct psm__gather_channel ch[] = {
    { draw_order_src, m->parts.keydata.draw_order },
  };
  psm__gather_scalars(count, bindings, keyform_off,
      ms->count_info->part_kf, &m->parts.keydata.interp, ch, 1);
}

PSM__DEF void
psm__apply_part_opacity(struct psm__model *m)
{
  psm__i32 count = m->parts.count;
  if (count <= 0)
    return;

  struct psm__part *items = m->parts.items;
  psm__i32 *offscreen_indices = m->parts.offscreen_src_idx;
  bool *enable = m->parts.enable;
  psm__f32 *input_opacity = m->parts.input_opacity,
                *part_opa = m->parts.opacity;

  for (psm__i32 i = 0; i < count; i++) {
    if (!enable[i])
      continue;

    psm__f32 opacity = input_opacity[i];
    part_opa[i] = opacity;

    psm__i32 parent_index = items[i].parent_part_idx;
    if (parent_index != -1 && offscreen_indices[parent_index] == -1) {
      opacity *= part_opa[parent_index];
      part_opa[i] = opacity;
    }

    if (offscreen_indices[i] != -1) {
      psm__i32 offscreen_idx = offscreen_indices[i];
      m->offscreens.opacity[offscreen_idx] *= opacity;
    }
  }
}

PSMDEF int
csmGetPartCount(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->parts.count;
}

PSMDEF const char **
csmGetPartIds(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->part_src.id_runtime;
}

PSMDEF float *
csmGetPartOpacities(csmModel *model)
{
  struct psm__model *m = (struct psm__model *)model;
  return m->parts.input_opacity;
}

PSMDEF const int *
csmGetPartParentPartIndices(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->part_src.parent_part_idx;
}

#if PSM_COMPAT_VERSION >= 0x06000000L
PSMDEF const int *
csmGetPartOffscreenIndices(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->parts.offscreen_src_idx;
}
#endif
