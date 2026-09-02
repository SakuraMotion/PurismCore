/*
 * Purism Core: keyform interpolation
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <string.h>
#include "private.h"
#include "array.h"
#include "debug.h"
#include "interpolate.h"
#include "math2.h"
#include "moc3.h"
#include "model.h"

static void
psm__interp_f32(struct psm__interp *interp, const psm__f32 *targets,
    psm__f32 *out, psm__i32 out_stride, const bool *enable)
{
  if (!interp || !targets || !out)
    return;

  psm__i32 obj_count = interp->object_count;
  if (obj_count <= 0)
    return;

  psm__i32 *max_comb = interp->max_blend;
  psm__i32 *comb = interp->blend_count;
  psm__f32 *wt = interp->weights;
  psm__f32 *tmp = interp->tmp;
  psm__i32  tmp_len = interp->tmp_len;

  if (!max_comb || !comb || !wt)
    return;

  /*
   * Pre-multiply targets by weights into scratch buffer.
   * Avoids redundant multiplies when the same weight layout
   * is reused across channels (e.g. R/G/B color interp).
   */
  if (tmp && tmp_len > 0) {
    for (psm__i32 i = 0; i < tmp_len; i++)
      tmp[i] = targets[i] * wt[i];
  }

  psm__i32 off = 0;
  for (psm__i32 i = 0; i < obj_count; i++) {
    psm__i32 mc = max_comb[i];
    if (enable == NULL || enable[i]) {
      psm__i32 n = comb[i];
      psm__f32 sum = 0.0f;
      if (tmp) {
        for (psm__i32 j = 0; j < n; j++)
          sum += tmp[off + j];
      } else {
        for (psm__i32 j = 0; j < n; j++)
          sum += targets[off + j] * wt[off + j];
      }
      out[i * out_stride] = sum;
    }
    off += mc;
  }
}

static void
psm__interp_i32(struct psm__interp *interp, const psm__f32 *targets,
    psm__i32 *out, const bool *enable)
{
  if (!interp || !targets || !out)
    return;

  psm__i32 obj_count = interp->object_count;
  if (obj_count <= 0)
    return;

  psm__i32 *max_comb = interp->max_blend;
  psm__i32 *comb = interp->blend_count;
  psm__f32 *wt = interp->weights;
  psm__f32 *tmp = interp->tmp;
  psm__i32  tmp_len = interp->tmp_len;

  if (!max_comb || !comb || !wt)
    return;

  if (tmp && tmp_len > 0) {
    for (psm__i32 i = 0; i < tmp_len; i++)
      tmp[i] = targets[i] * wt[i];
  }

  psm__i32 off = 0;
  for (psm__i32 i = 0; i < obj_count; i++) {
    psm__i32 mc = max_comb[i];
    if (enable == NULL || enable[i]) {
      psm__i32 n = comb[i];
      psm__f32 sum = 0.0f;
      if (tmp) {
        for (psm__i32 j = 0; j < n; j++)
          sum += tmp[off + j];
      } else {
        for (psm__i32 j = 0; j < n; j++)
          sum += targets[off + j] * wt[off + j];
      }
      out[i] = psm__f32_to_i32(sum + 0.001f);
    }
    off += mc;
  }
}

PSM__DEF void
psm__interp_parts(struct psm__model *m)
{
  struct psm__interp *ip = &m->parts.keydata.interp;
  psm__interp_i32(ip, m->parts.keydata.draw_order, m->parts.draw_order,
      m->parts.enable);
}

/* ------------------------------------------------------------------ */
/* Per-object interpolation kernels (dirty update).                    */
/*                                                                    */
/* The batch functions above process every enabled object every       */
/* frame. The per-object versions below recompute exactly one object  */
/* and are used by the fused per-stage dirty pipeline: an object that */
/* is "clean" (key data unchanged, no upstream change, no enable      */
/* flip) keeps its previous frame's final state and is skipped.       */
/* Re-interpolating a clean object is idempotent (same key table      */
/* index/weight => same base values), so it can always be used to     */
/* reset in-place accumulated state before re-applying transforms.    */
/* ------------------------------------------------------------------ */

static psm__i32
psm__interp_off(const struct psm__interp *ip, psm__i32 i)
{
  psm__i32 off = 0;
  const psm__i32 *mc = ip->max_blend;
  for (psm__i32 k = 0; k < i; k++)
    off += mc[k];
  return off;
}

static void
psm__interp_f32_obj(struct psm__interp *ip, psm__i32 i,
    const psm__f32 *targets, psm__f32 *out, psm__i32 out_stride)
{
  if (!ip || !targets || !out)
    return;

  psm__i32 *max_comb = ip->max_blend;
  psm__i32 *comb = ip->blend_count;
  psm__f32 *wt = ip->weights;

  if (!max_comb || !comb || !wt)
    return;

  psm__i32 mc = max_comb[i];
  psm__i32 n = psm__clamp_i32(comb[i], 0, mc);
  psm__f32 sum = 0.0f;
  psm__i32 off = psm__interp_off(ip, i);
  for (psm__i32 j = 0; j < n; j++)
    sum += targets[off + j] * wt[off + j];
  out[i * out_stride] = sum;
}

static void
psm__interp_i32_obj(struct psm__interp *ip, psm__i32 i,
    const psm__f32 *targets, psm__i32 *out)
{
  if (!ip || !targets || !out)
    return;

  psm__i32 *max_comb = ip->max_blend;
  psm__i32 *comb = ip->blend_count;
  psm__f32 *wt = ip->weights;

  if (!max_comb || !comb || !wt)
    return;

  psm__i32 mc = max_comb[i];
  psm__i32 n = psm__clamp_i32(comb[i], 0, mc);
  psm__f32 sum = 0.0f;
  psm__i32 off = psm__interp_off(ip, i);
  for (psm__i32 j = 0; j < n; j++)
    sum += targets[off + j] * wt[off + j];
  out[i] = psm__f32_to_i32(sum + 0.001f);
}

static void
psm__interp_f32_array_obj(struct psm__interp *ip, psm__i32 i,
    psm__f32 **targets, psm__f32 **out, psm__i32 total)
{
  if (!ip || !targets || !out || total <= 0)
    return;

  psm__i32 *max_comb = ip->max_blend;
  psm__i32 *comb = ip->blend_count;
  psm__f32 *wt = ip->weights;

  if (!max_comb || !comb || !wt)
    return;

  psm__f32 *dst = out[i];
  if (!dst)
    return;

  psm__i32 mc = max_comb[i];
  psm__i32 n = psm__clamp_i32(comb[i], 0, mc);

  memset(dst, 0, (size_t)total * sizeof(psm__f32));
  if (n <= 0)
    return;

  psm__i32 off = psm__interp_off(ip, i);
  for (psm__i32 j = 0; j < n; j++) {
    psm__f32 w = wt[off + j];
    psm__f32 *src = targets[off + j];
    if (src) {
      for (psm__i32 k = 0; k < total; k++)
        dst[k] += src[k] * w;
    }
  }
}

static void
psm__interp_colors_obj(struct psm__interp *ip, psm__i32 i,
    const struct psm__color3 *kd_mul, const struct psm__color3 *kd_scr,
    psm__f32 *mul_out, psm__f32 *scr_out)
{
  psm__interp_f32_obj(ip, i, kd_mul->r, mul_out + 0, 4);
  psm__interp_f32_obj(ip, i, kd_mul->g, mul_out + 1, 4);
  psm__interp_f32_obj(ip, i, kd_mul->b, mul_out + 2, 4);
  psm__interp_f32_obj(ip, i, kd_scr->r, scr_out + 0, 4);
  psm__interp_f32_obj(ip, i, kd_scr->g, scr_out + 1, 4);
  psm__interp_f32_obj(ip, i, kd_scr->b, scr_out + 2, 4);
}

/* Recompute the key-data base state of one warp deformer (local idx si):
 * opacity, grid positions, and (v4.2+) colors. */
PSM__DEF void
psm__interp_warp_one(struct psm__model *m, psm__i32 si)
{
  struct psm__warps    *w = &m->deformers.warps;
  struct psm__interp   *ip = &w->keydata.interp;
  struct psm__sections *ms = m->source->sections;

  psm__i32 *vc = ms->warp_src.vertex_count;

  psm__interp_f32_obj(ip, si, w->keydata.opacity, w->opacity, 1);

  if (vc && w->keydata.pos && w->pos)
    psm__interp_f32_array_obj(ip, si, w->keydata.pos, w->pos, vc[si] * 2);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__interp_colors_obj(ip, si, &w->keydata.mul_color,
      &w->keydata.scr_color, w->mul_color, w->scr_color);
}

/* Recompute the key-data base state of one rotation deformer (local si):
 * opacity, angle, origin, scale, and (v4.2+) colors. */
PSM__DEF void
psm__interp_rotation_one(struct psm__model *m, psm__i32 si)
{
  struct psm__rotations *r = &m->deformers.rotations;
  struct psm__interp    *ip = &r->keydata.interp;

  psm__interp_f32_obj(ip, si, r->keydata.opacity, r->opacity, 1);
  psm__interp_f32_obj(ip, si, r->keydata.angle, r->angle, 1);
  psm__interp_f32_obj(ip, si, r->keydata.origin_x, r->origin_x, 1);
  psm__interp_f32_obj(ip, si, r->keydata.origin_y, r->origin_y, 1);
  psm__interp_f32_obj(ip, si, r->keydata.scale, r->scale, 1);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__interp_colors_obj(ip, si, &r->keydata.mul_color,
      &r->keydata.scr_color, r->mul_color, r->scr_color);
}

/* Recompute the key-data base state of one art mesh (index i):
 * opacity, draw order, vertex positions, and (v4.2+) colors. */
PSM__DEF void
psm__interp_art_mesh_one(struct psm__model *m, psm__i32 i)
{
  struct psm__art_meshes *am = &m->art_meshes;
  struct psm__interp     *ip = &am->keydata.interp;
  struct psm__sections   *ms = m->source->sections;

  psm__i32 *vc = ms->art_mesh_src.vertex_count;

  psm__interp_f32_obj(ip, i, am->keydata.opacity, am->opacity, 1);
  psm__interp_i32_obj(ip, i, am->keydata.draw_order, am->draw_order);

  if (vc && am->keydata.pos && am->pos)
    psm__interp_f32_array_obj(ip, i, am->keydata.pos, am->pos, vc[i] * 2);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__interp_colors_obj(ip, i, &am->keydata.mul_color,
      &am->keydata.scr_color, am->mul_color, am->scr_color);
}

/* Recompute the key-data base state of one offscreen surface (index i):
 * opacity and (v4.2+) colors. v5.3+ only. */
PSM__DEF void
psm__interp_offscreen_one(struct psm__model *m, psm__i32 i)
{
  struct psm__offscreens *os = &m->offscreens;
  struct psm__interp *ip = &os->keydata.interp;

  if (m->source->header->version < csmMocVersion_53)
    return;

  psm__interp_f32_obj(ip, i, os->keydata.opacity, os->opacity, 1);

  psm__interp_colors_obj(ip, i, &os->keydata.mul_color,
      &os->keydata.scr_color, os->mul_color, os->scr_color);
}

PSM__DEF void
psm__interp_glues(struct psm__model *m)
{
  struct psm__glues *g = &m->glues;
  if (g->count <= 0 || !g->keydata.intensity || !g->intensity)
    return;
  psm__interp_f32(&g->keydata.interp, g->keydata.intensity, g->intensity, 1,
      NULL);
}
