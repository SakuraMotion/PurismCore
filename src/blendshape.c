/*
 * Purism Core: blend shape blending
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include "private.h"
#include "array.h"
#include "blendshape.h"
#include "debug.h"
#include "math2.h"
#include "moc3.h"
#include "model.h"

static inline psm__f32
psm__blend_shape_interp_f32(const struct psm__blend_binding *binding,
    const psm__f32 *kf_src)
{
  psm__i32 blend_count = binding->blend_count;
  psm__i32 begin = binding->key_src_offset;
  psm__f32 value;

  switch (blend_count) {
  case 0:
    return 0.0f;
  case 1: {
    psm__i32 idx0 = binding->keyform_idx[0] + begin;
    value = kf_src[idx0] * binding->weights[0];
    break;
  }
  case 2: {
    psm__i32 idx0 = binding->keyform_idx[0] + begin;
    psm__i32 idx1 = binding->keyform_idx[1] + begin;
    value = kf_src[idx0] * binding->weights[0]
          + kf_src[idx1] * binding->weights[1];
    break;
  }
  default:
    PSM__LOGF("invalid blend count %d", blend_count);
    return 0.0f;
  }

  return binding->weight * value;
}

static void
blend_scalar_f32(psm__i32 count, const struct psm__blend_shape *shapes,
    psm__f32 *values, const psm__f32 *kf_src, psm__f32 lo, psm__f32 hi)
{
  for (psm__i32 i = 0; i < count; i++) {
    psm__i32 ti = shapes[i].target_idx;
    psm__i32 bc = shapes[i].binding_count;
    psm__f32 value = values[ti];

    struct psm__blend_binding *binds = shapes[i].bindings;
    if (bc > 0 && binds) {
      for (psm__i32 j = 0; j < bc; j++)
        value += psm__blend_shape_interp_f32(&binds[j], kf_src);
    }

    values[ti] = psm__clamp_f32(value, lo, hi);
  }
}

static void
blend_scalar_i32(psm__i32 count, const struct psm__blend_shape *shapes,
    psm__i32 *values, const psm__f32 *kf_src)
{
  for (psm__i32 i = 0; i < count; i++) {
    psm__i32 ti = shapes[i].target_idx;
    psm__i32 bc = shapes[i].binding_count;
    psm__f32 value = (psm__f32)values[ti];

    struct psm__blend_binding *binds = shapes[i].bindings;
    if (bc > 0 && binds) {
      for (psm__i32 j = 0; j < bc; j++)
        value += psm__blend_shape_interp_f32(&binds[j], kf_src);
    }

    psm__f32 rounded = value + 0.001f;
    rounded = psm__clamp_f32(rounded, 0.0f, 1000.0f);
    values[ti] = (psm__i32)rounded;
  }
}

static void
psm__blend_positions(const struct psm__model *m, psm__i32 count,
    const struct psm__blend_shape *shapes, const psm__i32 *kf_pos_begin,
    psm__f32 **out_positions, const psm__i32 *vertex_counts)
{
  if (count <= 0)
    return;
  if (!shapes || !kf_pos_begin || !out_positions || !vertex_counts)
    return;

  struct psm__sections *ms = m->source->sections;
  psm__f32 *pos_xy = ms->key_pos_src.xy;
  if (!pos_xy)
    return;

  for (psm__i32 i = 0; i < count; i++) {
    psm__i32 ti = shapes[i].target_idx;
    psm__i32 bc = shapes[i].binding_count;
    if (bc <= 0)
      continue;

    psm__i32 vc = vertex_counts[ti];
    if (vc <= 0)
      continue;

    psm__i32 pc = vc * 2;
    psm__f32 *out = out_positions[ti];
    struct psm__blend_binding *binds = shapes[i].bindings;
    if (!out || !binds)
      continue;

    for (psm__i32 j = 0; j < bc; j++) {
      psm__i32 blend_count = binds[j].blend_count;
      if (blend_count == 0)
        continue;

      psm__i32 begin = binds[j].key_src_offset;
      psm__f32 cw = binds[j].weight;

      switch (blend_count) {
      case 1: {
        psm__i32 ki = binds[j].keyform_idx[0] + begin;
        psm__i32 pb = kf_pos_begin[ki];
        psm__f32 *p0 = &pos_xy[pb];
        psm__f32 w0 = binds[j].weights[0];
        for (psm__i32 k = 0; k < pc; k++)
          out[k] += p0[k] * w0 * cw;
        break;
      }
      case 2: {
        psm__i32 ki0 = binds[j].keyform_idx[0] + begin;
        psm__i32 ki1 = binds[j].keyform_idx[1] + begin;
        psm__i32 pb0 = kf_pos_begin[ki0];
        psm__i32 pb1 = kf_pos_begin[ki1];
        psm__f32 *p0 = &pos_xy[pb0];
        psm__f32 *p1 = &pos_xy[pb1];
        psm__f32 w0 = binds[j].weights[0];
        psm__f32 w1 = binds[j].weights[1];
        for (psm__i32 k = 0; k < pc; k++)
          out[k] += (w0 * p0[k] + p1[k] * w1) * cw;
        break;
      }
      default:
        PSM__LOGF("invalid blend count %d", blend_count);
        break;
      }
    }
  }
}

static void
psm__blend_colors(psm__i32 count, const struct psm__blend_shape *shapes,
    const psm__i32 *kf_color_begin,
    const psm__f32 *src_r, const psm__f32 *src_g, const psm__f32 *src_b,
    psm__f32 *out)
{
  if (count <= 0)
    return;
  if (!shapes || !kf_color_begin || !src_r || !src_g || !src_b || !out)
    return;

  for (psm__i32 i = 0; i < count; i++) {
    psm__i32 ti = shapes[i].target_idx;
    psm__i32 bc = shapes[i].binding_count;
    psm__i32 ob = ti * 4;

    struct psm__blend_binding *binds = shapes[i].bindings;
    if (bc > 0 && binds) {
      for (psm__i32 j = 0; j < bc; j++) {
        psm__i32 blend_count = binds[j].blend_count;
        if (blend_count == 0)
          continue;

        psm__i32 begin = binds[j].key_src_offset;
        psm__f32 cw = binds[j].weight;
        psm__f32 r, g, b;

        switch (blend_count) {
        case 1: {
          psm__i32 ki = binds[j].keyform_idx[0] + begin;
          psm__i32 ci = kf_color_begin[ki];
          psm__f32 w0 = binds[j].weights[0];
          r = src_r[ci] * w0;
          g = src_g[ci] * w0;
          b = src_b[ci] * w0;
          break;
        }
        case 2: {
          psm__i32 ki0 = binds[j].keyform_idx[0] + begin;
          psm__i32 ki1 = binds[j].keyform_idx[1] + begin;
          psm__i32 ci0 = kf_color_begin[ki0];
          psm__i32 ci1 = kf_color_begin[ki1];
          psm__f32 w0 = binds[j].weights[0];
          psm__f32 w1 = binds[j].weights[1];
          r = w0 * src_r[ci0] + src_r[ci1] * w1;
          g = w0 * src_g[ci0] + src_g[ci1] * w1;
          b = w0 * src_b[ci0] + src_b[ci1] * w1;
          break;
        }
        default:
          PSM__LOGF("invalid blend count %d", blend_count);
          continue;
        }

        out[ob + 0] += r * cw;
        out[ob + 1] += g * cw;
        out[ob + 2] += b * cw;
      }
    }

    out[ob + 0] = psm__clamp_f32_01(out[ob + 0]);
    out[ob + 1] = psm__clamp_f32_01(out[ob + 1]);
    out[ob + 2] = psm__clamp_f32_01(out[ob + 2]);
  }
}


PSM__DEF void
psm__blend_parts(struct psm__model *m)
{
  if (m->source->header->version < csmMocVersion_50)
    return;

  psm__i32 count = m->bs_parts.count;
  if (count <= 0)
    return;

  struct psm__blend_shape *shapes = m->bs_parts.items;
  if (!shapes)
    return;

  struct psm__sections *ms = m->source->sections;
  psm__i32 *calc_do = m->parts.draw_order;
  psm__f32 *do_src = ms->part_key_src.draw_order;

  if (!calc_do || !do_src)
    return;

  blend_scalar_i32(count, shapes, calc_do, do_src);
}

PSM__DEF void
psm__blend_warps(struct psm__model *m)
{
  if (m->source->header->version < csmMocVersion_42)
    return;

  struct psm__sections *ms = m->source->sections;
  psm__i32 count = m->bs_warps.count;
  struct psm__blend_shape *shapes = m->bs_warps.items;

  if (count <= 0 || !shapes)
    return;

  psm__blend_positions(m, count, shapes, ms->warp_key_src.key_pos_off,
      m->deformers.warps.pos, ms->warp_src.vertex_count);

  if (m->source->header->version < csmMocVersion_50)
    return;

  psm__f32 *op_src = ms->warp_key_src.opacity;
  psm__f32 *calc_op = m->deformers.warps.opacity;

  if (!op_src || !calc_op)
    return;

  blend_scalar_f32(count, shapes, calc_op, op_src, 0.0f, 1.0f);

  psm__blend_colors(count, shapes, ms->warp_key_src.key_mul_color_off,
      ms->kf_mul_color_src.r, ms->kf_mul_color_src.g,
      ms->kf_mul_color_src.b, m->deformers.warps.mul_color);

  psm__blend_colors(count, shapes, ms->warp_key_src.key_scr_color_off,
      ms->kf_scr_color_src.r, ms->kf_scr_color_src.g,
      ms->kf_scr_color_src.b, m->deformers.warps.scr_color);
}

PSM__DEF void
psm__blend_rotations(struct psm__model *m)
{
  if (m->source->header->version < csmMocVersion_50)
    return;

  struct psm__sections *ms = m->source->sections;
  psm__i32 count = m->bs_rotations.count;
  struct psm__blend_shape *shapes = m->bs_rotations.items;

  if (count <= 0 || !shapes)
    return;

  psm__f32 *ox_src = ms->rotation_key_src.origin_x;
  psm__f32 *calc_ox = m->deformers.rotations.origin_x;
  if (ox_src && calc_ox)
    blend_scalar_f32(count, shapes, calc_ox, ox_src, -INFINITY, INFINITY);

  psm__f32 *oy_src = ms->rotation_key_src.origin_y;
  psm__f32 *calc_oy = m->deformers.rotations.origin_y;
  if (oy_src && calc_oy)
    blend_scalar_f32(count, shapes, calc_oy, oy_src, -INFINITY, INFINITY);

  psm__f32 *op_src = ms->rotation_key_src.opacity;
  psm__f32 *calc_op = m->deformers.rotations.opacity;
  if (op_src && calc_op)
    blend_scalar_f32(count, shapes, calc_op, op_src, 0.0f, 1.0f);

  psm__blend_colors(count, shapes, ms->rotation_key_src.key_mul_color_off,
      ms->kf_mul_color_src.r, ms->kf_mul_color_src.g,
      ms->kf_mul_color_src.b, m->deformers.rotations.mul_color);

  psm__blend_colors(count, shapes, ms->rotation_key_src.key_scr_color_off,
      ms->kf_scr_color_src.r, ms->kf_scr_color_src.g,
      ms->kf_scr_color_src.b, m->deformers.rotations.scr_color);

  psm__f32 *ang_src = ms->rotation_key_src.angle;
  psm__f32 *calc_ang = m->deformers.rotations.angle;
  if (ang_src && calc_ang)
    blend_scalar_f32(count, shapes, calc_ang, ang_src, -3600.0f, 3600.0f);

  psm__f32 *sc_src = ms->rotation_key_src.scale;
  psm__f32 *calc_sc = m->deformers.rotations.scale;
  if (sc_src && calc_sc)
    blend_scalar_f32(count, shapes, calc_sc, sc_src, 0.0001f, 100.0f);
}

PSM__DEF void
psm__blend_art_meshes(struct psm__model *m)
{
  if (m->source->header->version < csmMocVersion_42)
    return;

  struct psm__sections *ms = m->source->sections;
  psm__i32 count = m->bs_art_meshes.count;
  struct psm__blend_shape *shapes = m->bs_art_meshes.items;
  if (count <= 0 || !shapes)
    return;

  psm__blend_positions(m, count, shapes, ms->art_mesh_key_src.key_pos_off,
      m->art_meshes.pos, ms->art_mesh_src.vertex_count);

  if (m->source->header->version < csmMocVersion_50)
    return;

  psm__f32 *do_src = ms->art_mesh_key_src.draw_order;
  psm__i32 *calc_do = m->art_meshes.draw_order;
  if (do_src && calc_do)
    blend_scalar_i32(count, shapes, calc_do, do_src);

  psm__f32 *op_src = ms->art_mesh_key_src.opacity;
  psm__f32 *calc_op = m->art_meshes.opacity;
  if (op_src && calc_op)
    blend_scalar_f32(count, shapes, calc_op, op_src, 0.0f, 1.0f);

  if (ms->art_mesh_key_src.key_mul_color_off &&
      ms->kf_mul_color_src.r && ms->kf_mul_color_src.g &&
      ms->kf_mul_color_src.b && m->art_meshes.mul_color) {
    psm__blend_colors(count, shapes, ms->art_mesh_key_src.key_mul_color_off,
        ms->kf_mul_color_src.r, ms->kf_mul_color_src.g,
        ms->kf_mul_color_src.b, m->art_meshes.mul_color);
  }

  if (ms->art_mesh_key_src.key_scr_color_off &&
      ms->kf_scr_color_src.r && ms->kf_scr_color_src.g &&
      ms->kf_scr_color_src.b && m->art_meshes.scr_color) {
    psm__blend_colors(count, shapes, ms->art_mesh_key_src.key_scr_color_off,
        ms->kf_scr_color_src.r, ms->kf_scr_color_src.g,
        ms->kf_scr_color_src.b, m->art_meshes.scr_color);
  }
}

PSM__DEF void
psm__blend_glues(struct psm__model *m)
{
  if (m->source->header->version < csmMocVersion_50)
    return;

  psm__i32 count = m->bs_glues.count;
  if (count <= 0)
    return;

  struct psm__blend_shape *shapes = m->bs_glues.items;
  if (!shapes)
    return;

  struct psm__sections *ms = m->source->sections;
  psm__f32 *calc_int = m->glues.intensity;
  psm__f32 *int_src = ms->glue_key_src.intensity;

  if (!calc_int || !int_src)
    return;

  blend_scalar_f32(count, shapes, calc_int, int_src, 0.0f, 1.0f);
}

PSM__DEF void
psm__blend_offscreens(struct psm__model *m)
{
  if (m->source->header->version < csmMocVersion_53)
    return;

  struct psm__sections *ms = m->source->sections;
  psm__i32 count = m->bs_offscreens.count;
  struct psm__blend_shape *shapes = m->bs_offscreens.items;

  if (count <= 0 || !shapes)
    return;

  psm__f32 *op_src = ms->offscreen_key_src.opacity;
  psm__f32 *calc_op = m->offscreens.opacity;
  if (op_src && calc_op)
    blend_scalar_f32(count, shapes, calc_op, op_src, 0.0f, 1.0f);

  psm__blend_colors(count, shapes, ms->offscreen_key_src.key_mul_color_off,
      ms->kf_mul_color_src.r, ms->kf_mul_color_src.g,
      ms->kf_mul_color_src.b, m->offscreens.mul_color);

  psm__blend_colors(count, shapes, ms->offscreen_key_src.key_scr_color_off,
      ms->kf_scr_color_src.r, ms->kf_scr_color_src.g,
      ms->kf_scr_color_src.b, m->offscreens.scr_color);
}
