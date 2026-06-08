/*
 * Purism Core: offscreen surface rendering
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "private.h"
#include "array.h"
#include "debug.h"
#include "moc3.h"
#include "model.h"
#include "offscreen.h"

PSM__DEF void
psm__enable_offscreens(struct psm__model *m)
{
  if (m->source->header->version < csmMocVersion_53)
    return;

  psm__i32 count = m->offscreens.count;
  if (count <= 0)
    return;

  struct psm__offscreen *surfaces = m->offscreens.surfaces;
  bool *enable = m->offscreens.enable;

  for (psm__i32 i = 0; i < count; i++) {
    bool *oe = surfaces[i].owner_enable;
    enable[i] = oe ? *oe : 0;
  }
}

PSM__DEF void
psm__gather_offscreens(struct psm__model *m)
{
  if (m->source->header->version < csmMocVersion_53)
    return;

  psm__i32 count = m->offscreens.count;
  if (count <= 0)
    return;

  struct psm__offscreen *surfaces = m->offscreens.surfaces;
  if (!surfaces)
    return;

  struct psm__sections *ms = m->source->sections;
  psm__f32 *opa_src = ms->offscreen_key_src.opacity;
  if (!opa_src)
    return;

  psm__i32 max_kf = ms->count_info->offscreen_kf;
  struct psm__offscreen_keydata *kd = &m->offscreens.keydata;
  psm__i32 off = 0;

  /* Opacity and weights */
  for (psm__i32 i = 0; i < count; i++) {
    struct psm__binding *b = surfaces[i].binding;
    if (!b)
      continue;
    if (!b->idx_dirty && !b->weight_dirty) {
      off += b->max_blend;
      continue;
    }

    psm__i32 nc = b->blend_count;
    kd->interp.blend_count[i] = nc;

    if (b->idx_dirty && nc > 0) {
      psm__i32 *kp = surfaces[i].keyform_idx;
      psm__i32 ki = kp ? *kp : -1;
      if (ki != -1) {
        for (psm__i32 j = 0; j < nc; j++) {
          psm__i32 idx = b->keyform_idx[j] + ki;
          if ((psm__u32)idx >= (psm__u32)max_kf)
            continue;
          kd->opacity[off + j] = opa_src[idx];
        }
      }
    }

    if (b->weight_dirty && nc > 0) {
      memcpy(&kd->interp.weights[off], b->weights, nc * sizeof(psm__f32));
    }

    off += b->max_blend;
  }

  /* Colors */
  psm__i32 *col_begin = ms->offscreen_key_src.key_mul_color_off;
  psm__f32 *mr = ms->kf_mul_color_src.r;
  psm__f32 *mg = ms->kf_mul_color_src.g;
  psm__f32 *mb = ms->kf_mul_color_src.b;
  psm__f32 *sr = ms->kf_scr_color_src.r;
  psm__f32 *sg = ms->kf_scr_color_src.g;
  psm__f32 *sb = ms->kf_scr_color_src.b;

  if (!col_begin || !mr || !mg || !mb || !sr || !sg || !sb)
    return;

  psm__i32 max_kf_colors = ms->count_info->kf_mul_colors;
  off = 0;

  for (psm__i32 i = 0; i < count; i++) {
    struct psm__binding *b = surfaces[i].binding;
    if (!b || !b->idx_dirty) {
      if (b)
        off += b->max_blend;
      continue;
    }

    psm__i32 *kp = surfaces[i].keyform_idx;
    psm__i32 ki = kp ? *kp : -1;
    psm__i32 nc = b->blend_count;

    if (ki >= 0 && nc > 0) {
      if ((psm__u32)ki >= (psm__u32)max_kf)
        goto skip_color;
      psm__i32 cb = col_begin[ki];
      for (psm__i32 j = 0; j < nc; j++) {
        psm__i32 idx = b->keyform_idx[j] + cb;
        if ((psm__u32)idx >= (psm__u32)max_kf_colors)
          continue;
        kd->mul_color.r[off + j] = mr[idx];
        kd->mul_color.g[off + j] = mg[idx];
        kd->mul_color.b[off + j] = mb[idx];
        kd->scr_color.r[off + j] = sr[idx];
        kd->scr_color.g[off + j] = sg[idx];
        kd->scr_color.b[off + j] = sb[idx];
      }
    }
skip_color:
    off += b->max_blend;
  }
}

#if PSM_COMPAT_VERSION >= 0x06000000L
PSMDEF int
csmGetOffscreenCount(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->offscreens.count;
}

PSMDEF const int *
csmGetOffscreenBlendModes(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  const struct psm__sections *ms = m->source->sections;
  return ms->offscreen_src.blend_mode;
}

PSMDEF const float *
csmGetOffscreenOpacities(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->offscreens.opacity;
}

PSMDEF const int *
csmGetOffscreenOwnerIndices(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  const struct psm__sections *ms = m->source->sections;
  return ms->offscreen_src.owner_idx;
}

PSMDEF const csmVector4 *
csmGetOffscreenMultiplyColors(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmVector4 *)m->offscreens.mul_color;
}

PSMDEF const csmVector4 *
csmGetOffscreenScreenColors(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmVector4 *)m->offscreens.scr_color;
}

PSMDEF const int *
csmGetOffscreenMaskCounts(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  const struct psm__sections *ms = m->source->sections;
  return ms->offscreen_src.mask_len;
}

PSMDEF const int **
csmGetOffscreenMasks(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  const struct psm__sections *ms = m->source->sections;
  return ms->offscreen_src.drawable_mask_runtime;
}

PSMDEF const csmFlags *
csmGetOffscreenConstantFlags(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  const struct psm__sections *ms = m->source->sections;
  return (const csmFlags *)ms->offscreen_src.drawable_flag;
}
#endif /* PSM_COMPAT_VERSION >= 0x06000000L */
