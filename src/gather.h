/*
 * Purism Core: common gather helpers for keyform data
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__GATHER_H
#define PSM__GATHER_H

#include <string.h>
#include "private.h"
#include "debug.h"
#include "moc3.h"
#include "model.h"

struct psm__gather_channel {
  const psm__f32 *src;
  psm__f32       *dst;
};

/*
 * Gather scalar keyform data into keydata workspace.
 * Common loop for all node types that use a flat keyform_offset array.
 *
 * bindings[i]       = pointer to binding for object i (NULL = skip)
 * keyform_offset[i] = per-object offset into keyform arrays
 */
static inline void
psm__gather_scalars(
    psm__i32                          count,
    struct psm__binding *const       *bindings,
    const psm__i32                   *keyform_offset,
    struct psm__interp               *interp,
    const struct psm__gather_channel *channels,
    psm__i32                          n_channels)
{
  psm__i32 offset = 0;
  for (psm__i32 i = 0; i < count; i++) {
    struct psm__binding *b = bindings[i];
    if (!b)
      continue;
    psm__i32 cc = b->blend_count;

    if (b->idx_dirty || b->weight_dirty)
      interp->blend_count[i] = cc;

    if (b->idx_dirty && cc > 0) {
      for (psm__i32 j = 0; j < cc; j++) {
        psm__i32 kfi = b->keyform_idx[j] + keyform_offset[i];
        for (psm__i32 c = 0; c < n_channels; c++)
          channels[c].dst[offset + j] = channels[c].src[kfi];
      }
    }

    if (b->weight_dirty && cc > 0)
      memcpy(&interp->weights[offset], b->weights, cc * sizeof(psm__f32));

    offset += b->max_blend;
  }
}

/*
 * Gather position pointer keyform data (warps, art meshes).
 *
 * We traverse keyform_idx -> keyform index -> pos_begin -> offset into pos_xy.
 * Each combo entry gets a pointer into the shared position pool.
 */
static inline void
psm__gather_positions(
    psm__i32                    count,
    struct psm__binding *const *bindings,
    const psm__i32             *keyform_offset,
    const psm__f32             *pos_xy,
    const psm__i32             *pos_begin,
    psm__f32                  **pos_dst)
{
  psm__i32 offset = 0;
  for (psm__i32 i = 0; i < count; i++) {
    struct psm__binding *b = bindings[i];
    if (!b)
      continue;
    if (b->idx_dirty && b->blend_count > 0) {
      for (psm__i32 j = 0; j < b->blend_count; j++) {
        psm__i32 kfi = b->keyform_idx[j] + keyform_offset[i];
        psm__i32 pi = pos_begin[kfi];
        pos_dst[offset + j] = (psm__f32 *)&pos_xy[pi];
      }
    }
    offset += b->max_blend;
  }
}

/*
 * Gather color keyform data from the global color pool.
 * Used by warps, rotations, and art meshes.
 */
static inline void
psm__gather_colors(
    psm__i32                         count,
    struct psm__binding *const      *bindings,
    const psm__i32                  *key_color_offset,
    const struct psm__key_color_src *mul_src,
    const struct psm__key_color_src *scr_src,
    struct psm__color3              *mul_dst,
    struct psm__color3              *scr_dst)
{
  psm__i32 offset = 0;
  for (psm__i32 i = 0; i < count; i++) {
    struct psm__binding *b = bindings[i];
    if (!b) continue;
    psm__i32 cc = b->blend_count;

    if (b->idx_dirty && cc > 0) {
      for (psm__i32 j = 0; j < cc; j++) {
        psm__i32 kfi = b->keyform_idx[j] + key_color_offset[i];
        psm__i32 oj = offset + j;
        mul_dst->r[oj] = mul_src->r[kfi];
        mul_dst->g[oj] = mul_src->g[kfi];
        mul_dst->b[oj] = mul_src->b[kfi];
        scr_dst->r[oj] = scr_src->r[kfi];
        scr_dst->g[oj] = scr_src->g[kfi];
        scr_dst->b[oj] = scr_src->b[kfi];
      }
    }

    offset += b->max_blend;
  }
}

/*
 * Gather rotation reflect flags (first keyform only).
 */
static inline void
psm__gather_reflect(
    psm__i32                    count,
    struct psm__binding *const *bindings,
    const psm__i32             *keyform_offset,
    const psm__i32             *rfx_src,
    const psm__i32             *rfy_src,
    psm__i32                   *rfx_dst,
    psm__i32                   *rfy_dst)
{
  for (psm__i32 i = 0; i < count; i++) {
    struct psm__binding *b = bindings[i];
    if (!b || !b->idx_dirty || b->blend_count <= 0)
      continue;
    psm__i32 kfi = b->keyform_idx[0] + keyform_offset[i];
    rfx_dst[i] = rfx_src[kfi];
    rfy_dst[i] = rfy_src[kfi];
  }
}

#endif /* PSM__GATHER_H */
