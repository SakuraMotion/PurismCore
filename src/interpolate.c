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

  psm__i32 obj_len = interp->object_count;
  if (obj_len <= 0)
    return;

  psm__i32 *max_comb = interp->max_blend;
  psm__i32 *comb = interp->blend_count;
  psm__f32 *wt = interp->weights;
  psm__f32 *tmp = interp->tmp;
  psm__i32 tmp_len = interp->tmp_len;

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
  for (psm__i32 i = 0; i < obj_len; i++) {
    psm__i32 mc = max_comb[i];
    if (enable == NULL || enable[i]) {
      psm__i32 n = psm__clamp_i32(comb[i], 0, mc);
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

  psm__i32 obj_len = interp->object_count;
  if (obj_len <= 0)
    return;

  psm__i32 *max_comb = interp->max_blend;
  psm__i32 *comb = interp->blend_count;
  psm__f32 *wt = interp->weights;
  psm__f32 *tmp = interp->tmp;
  psm__i32 tmp_len = interp->tmp_len;

  if (!max_comb || !comb || !wt)
    return;

  if (tmp && tmp_len > 0) {
    for (psm__i32 i = 0; i < tmp_len; i++)
      tmp[i] = targets[i] * wt[i];
  }

  psm__i32 off = 0;
  for (psm__i32 i = 0; i < obj_len; i++) {
    psm__i32 mc = max_comb[i];
    if (enable == NULL || enable[i]) {
      psm__i32 n = psm__clamp_i32(comb[i], 0, mc);
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

static void
psm__interp_f32_array(struct psm__interp *interp,
    psm__f32 **targets, psm__f32 **out,
    const psm__i32 *counts, psm__i32 elem_count, const bool *enable)
{
  if (!interp || !targets || !out || !counts)
    return;

  psm__i32 obj_len = interp->object_count;
  psm__i32 *max_comb = interp->max_blend;
  psm__i32 *comb = interp->blend_count;
  psm__f32 *wt = interp->weights;

  if (!max_comb || !comb || !wt || obj_len <= 0)
    return;

  psm__i32 off = 0;
  for (psm__i32 i = 0; i < obj_len; i++) {
    psm__i32 mc = max_comb[i];
    if (enable == NULL || enable[i]) {
      psm__i32 total = counts[i] * elem_count;
      if (total <= 0) {
        off += mc;
        continue;
      }
      psm__i32 n = psm__clamp_i32(comb[i], 0, mc);
      psm__f32 *dst = out[i];
      if (dst) {
        memset(dst, 0, total * sizeof(psm__f32));
        for (psm__i32 j = 0; j < n; j++) {
          psm__f32 w = wt[off + j];
          psm__f32 *src = targets[off + j];
          if (src) {
            for (psm__i32 k = 0; k < total; k++)
              dst[k] += src[k] * w;
          }
        }
      }
    }
    off += mc;
  }
}

static void
psm__interp_colors(
    struct psm__interp *interp,
    const struct psm__color3 *kd_mul,
    const struct psm__color3 *kd_scr,
    psm__f32 *mul_out,
    psm__f32 *scr_out,
    const bool *enable)
{
  psm__interp_f32(interp, kd_mul->r, mul_out + 0, 4, enable);
  psm__interp_f32(interp, kd_mul->g, mul_out + 1, 4, enable);
  psm__interp_f32(interp, kd_mul->b, mul_out + 2, 4, enable);
  psm__interp_f32(interp, kd_scr->r, scr_out + 0, 4, enable);
  psm__interp_f32(interp, kd_scr->g, scr_out + 1, 4, enable);
  psm__interp_f32(interp, kd_scr->b, scr_out + 2, 4, enable);
}

PSM__DEF void
psm__interp_parts(struct psm__model *m)
{
  struct psm__interp *ip = &m->parts.keydata.interp;
  psm__interp_i32(ip, m->parts.keydata.draw_order,
      m->parts.draw_order, m->parts.enable);
}

PSM__DEF void
psm__interp_warps(struct psm__model *m)
{
  struct psm__warps *w = &m->deformers.warps;
  struct psm__interp *ip = &w->keydata.interp;
  struct psm__sections *ms = m->source->sections;
  psm__i32 *vc = ms->warp_src.vertex_count;
  bool *en = w->enable;

  psm__interp_f32(ip, w->keydata.opacity, w->opacity, 1, en);

  if (vc && w->keydata.pos && w->pos)
    psm__interp_f32_array(ip, w->keydata.pos, w->pos, vc, 2, en);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__interp_colors(ip, &w->keydata.mul_color, &w->keydata.scr_color,
      w->mul_color, w->scr_color, en);
}

PSM__DEF void
psm__interp_rotations(struct psm__model *m)
{
  struct psm__rotations *r = &m->deformers.rotations;
  struct psm__interp *ip = &r->keydata.interp;
  bool *en = r->enable;

  psm__interp_f32(ip, r->keydata.opacity, r->opacity, 1, en);
  psm__interp_f32(ip, r->keydata.angle, r->angle, 1, en);
  psm__interp_f32(ip, r->keydata.origin_x, r->origin_x, 1, en);
  psm__interp_f32(ip, r->keydata.origin_y, r->origin_y, 1, en);
  psm__interp_f32(ip, r->keydata.scale, r->scale, 1, en);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__interp_colors(ip, &r->keydata.mul_color, &r->keydata.scr_color,
      r->mul_color, r->scr_color, en);
}

PSM__DEF void
psm__interp_art_meshes(struct psm__model *m)
{
  struct psm__art_meshes *am = &m->art_meshes;
  struct psm__interp *ip = &am->keydata.interp;
  struct psm__sections *ms = m->source->sections;
  psm__i32 *vc = ms->art_mesh_src.vertex_count;
  bool *en = am->enable;

  psm__interp_f32(ip, am->keydata.opacity, am->opacity, 1, en);
  psm__interp_i32(ip, am->keydata.draw_order, am->draw_order, en);

  if (vc && am->keydata.pos && am->pos)
    psm__interp_f32_array(ip, am->keydata.pos, am->pos, vc, 2, en);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__interp_colors(ip, &am->keydata.mul_color, &am->keydata.scr_color,
      am->mul_color, am->scr_color, en);
}

PSM__DEF void
psm__interp_glues(struct psm__model *m)
{
  struct psm__glues *g = &m->glues;
  if (g->count <= 0 || !g->keydata.intensity || !g->intensity)
    return;
  psm__interp_f32(&g->keydata.interp,
      g->keydata.intensity, g->intensity, 1, NULL);
}

PSM__DEF void
psm__interp_offscreens(struct psm__model *m)
{
  struct psm__offscreens *os = &m->offscreens;
  struct psm__interp *ip = &os->keydata.interp;
  bool *en = os->enable;

  if (m->source->header->version < csmMocVersion_53)
    return;

  psm__interp_f32(ip, os->keydata.opacity, os->opacity, 1, en);

  psm__interp_colors(ip, &os->keydata.mul_color, &os->keydata.scr_color,
      os->mul_color, os->scr_color, en);
}
