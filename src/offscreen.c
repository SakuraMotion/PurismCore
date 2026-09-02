/*
 * Purism Core: offscreen surface rendering
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "private.h"
#include "array.h"
#include "blendshape.h"
#include "debug.h"
#include "interpolate.h"
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
  psm__f32             *opa_src = ms->offscreen_key_src.opacity;
  if (!opa_src)
    return;

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
      psm__i32  ki = kp ? *kp : -1;
      /* ki < 0 means "no keyforms"; any negative (not just -1) must be
       * skipped, else idx goes out of bounds (matches the color loop and
       * the psm__verify_offscreen_window load check). */
      if (ki >= 0) {
        for (psm__i32 j = 0; j < nc; j++) {
          psm__i32 idx = b->keyform_idx[j] + ki;
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
  psm__i32 *col_off = ms->offscreen_key_src.key_mul_color_off;
  psm__f32 *mr = ms->keyform_mul_color_src.r;
  psm__f32 *mg = ms->keyform_mul_color_src.g;
  psm__f32 *mb = ms->keyform_mul_color_src.b;
  psm__f32 *sr = ms->keyform_scr_color_src.r;
  psm__f32 *sg = ms->keyform_scr_color_src.g;
  psm__f32 *sb = ms->keyform_scr_color_src.b;

  if (!col_off || !mr || !mg || !mb || !sr || !sg || !sb)
    return;

  off = 0;

  for (psm__i32 i = 0; i < count; i++) {
    struct psm__binding *b = surfaces[i].binding;
    if (!b || !b->idx_dirty) {
      if (b)
        off += b->max_blend;
      continue;
    }

    psm__i32 *kp = surfaces[i].keyform_idx;
    psm__i32  ki = kp ? *kp : -1;
    psm__i32  nc = b->blend_count;

    if (ki >= 0 && nc > 0) {
      psm__i32 cb = col_off[ki];
      if (cb < 0)   /* offscreen keyform has no color override */
        goto skip_color;
      for (psm__i32 j = 0; j < nc; j++) {
        psm__i32 idx = b->keyform_idx[j] + cb;
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

/*
 * Fused per-offscreen dirty stage (replaces the former batch
 * interp_offscreens + blend_offscreens + the offscreen multiplication
 * inside psm__apply_part_opacity).
 *
 * An offscreen surface is recomputed only when:
 *   - its key data moved (binding idx/weight dirty), or
 *   - a blend shape targeting it is dirty, or
 *   - its enable state flipped.
 *
 * The owner-part multiplication (final opacity = key opacity *
 * owner part opacity) is applied here on the freshly re-interpolated
 * base. The offscreen enable mirrors its owner part's enable, so a
 * part-chain flip (the only thing that can change the part opacity,
 * since input opacities are static) shows up as an enable flip.
 */
PSM__DEF void
psm__process_offscreens(struct psm__model *m)
{
  if (m->source->header->version < csmMocVersion_53)
    return;

  psm__i32 count = m->offscreens.count;
  if (count <= 0)
    return;

  struct psm__sections *ms = m->source->sections;
  psm__i32 *owner_idx = ms->offscreen_src.owner_idx;
  struct psm__offscreen *surfaces = m->offscreens.surfaces;
  bool *en = m->offscreens.enable;
  psm__u8 *last_en = m->offscreen_last_enable;
  psm__u8 *bsd = m->offscreen_blend_dirty;
  psm__f32 *opa = m->offscreens.opacity;
  psm__f32 *part_opa = m->parts.opacity;

  for (psm__i32 i = 0; i < count; i++) {
    if (!en[i]) {
      last_en[i] = 0;
      continue;
    }

    struct psm__binding *b = surfaces[i].binding;
    psm__i32 self_dirty = b ? (b->idx_dirty || b->weight_dirty) : 1;
    psm__i32 en_flip = (en[i] != (last_en[i] != 0));

    if (!self_dirty && !bsd[i] && !en_flip) {
      last_en[i] = 1;
      continue;
    }

    psm__interp_offscreen_one(m, i);
    psm__blend_offscreen_one(m, i);

    psm__i32 owner = owner_idx ? owner_idx[i] : -1;
    if (owner >= 0 && owner < m->parts.count)
      opa[i] *= part_opa[owner];

    last_en[i] = 1;
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
  return m->source->sections->offscreen_src.blend_mode;
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
  return m->source->sections->offscreen_src.owner_idx;
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
  return m->source->sections->offscreen_src.mask_len;
}

PSMDEF const int **
csmGetOffscreenMasks(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->offscreen_src.drawable_mask_runtime;
}

PSMDEF const csmFlags *
csmGetOffscreenConstantFlags(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmFlags *)m->source->sections->offscreen_src.drawable_flag;
}
#endif /* PSM_COMPAT_VERSION >= 0x06000000L */
