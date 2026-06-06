/*
 * Purism Core: warp and rotation deformer transforms
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <string.h>
#include "private.h"
#include "array.h"
#include "debug.h"
#include "deformer.h"
#include "gather.h"
#include "math2.h"
#include "moc3.h"
#include "model.h"

struct psm__warp_basis {
  struct psm__vec2 center;
  struct psm__vec2 dpdu;
  struct psm__vec2 dpdv;
};

struct psm__warp_cell {
  psm__f32 fu, fv;
  struct psm__vec2 p00, p10, p01, p11;
};

static inline struct psm__warp_basis
psm__warp_extrap_basis(const psm__f32 *pos,
    psm__i32 row, psm__i32 column, psm__i32 stride)
{
  struct psm__vec2 c00 = psm__v2(pos[0], pos[1]);
  struct psm__vec2 c10 = psm__v2_load(pos, column);
  struct psm__vec2 c01 = psm__v2_load(pos, row * stride);
  struct psm__vec2 c11 = psm__v2_load(pos, row * stride + column);

  struct psm__vec2 d11_00 = psm__v2_sub(c11, c00);
  struct psm__vec2 d10_01 = psm__v2_sub(c10, c01);

  struct psm__warp_basis b;
  b.dpdu = psm__v2_scale(psm__v2_sub(d11_00, d10_01), 0.5f);
  b.dpdv = psm__v2_scale(psm__v2_add(d10_01, d11_00), 0.5f);

  struct psm__vec2 sum = psm__v2_add(
      psm__v2_add(c00, c10), psm__v2_add(c01, c11));
  b.center = psm__v2_sub(
      psm__v2_scale(sum, 0.25f), psm__v2_scale(d11_00, 0.5f));

  return b;
}

static inline struct psm__warp_cell
psm__warp_get_extrapolation_cell(psm__f32 u, psm__f32 v,
    psm__f32 gu, psm__f32 gv, psm__i32 row, psm__i32 column, psm__i32 stride,
    const psm__f32 *pos, const struct psm__warp_basis *basis)
{
  psm__f32 fr = (psm__f32)row, fc = (psm__f32)column;
  struct psm__vec2 cen = basis->center;
  struct psm__vec2 du = basis->dpdu;
  struct psm__vec2 dv = basis->dpdv;

  struct psm__warp_cell cell;

  if (u <= 0.0f) {
    if (v <= 0.0f) {
      cell.fu = (u + 2.0f) * 0.5f;
      cell.fv = (v + 2.0f) * 0.5f;
      cell.p00 = psm__v2_sub(cen, psm__v2_add(
          psm__v2_scale(du, 2.0f), psm__v2_scale(dv, 2.0f)));
      cell.p10 = psm__v2_sub(cen, psm__v2_scale(du, 2.0f));
      cell.p01 = psm__v2_sub(cen, psm__v2_scale(dv, 2.0f));
      cell.p11 = psm__v2(pos[0], pos[1]);
    } else if (v < 1.0f) {
      psm__i32 cv = (psm__i32)gv;
      if (cv == row) cv = row - 1;
      cell.fv = gv - (psm__f32)cv;
      cell.fu = (u + 2.0f) * 0.5f;
      psm__f32 vc = (psm__f32)cv / fr;
      psm__f32 vn = (psm__f32)(cv + 1) / fr;
      cell.p00 = psm__v2_add(psm__v2_sub(cen, psm__v2_scale(dv, 2.0f)),
          psm__v2_scale(du, vc));
      cell.p10 = psm__v2_load(pos, cv * stride);
      cell.p01 = psm__v2_add(psm__v2_sub(cen, psm__v2_scale(dv, 2.0f)),
          psm__v2_scale(du, vn));
      cell.p11 = psm__v2_load(pos, (cv + 1) * stride);
    } else {
      cell.fu = (u + 2.0f) * 0.5f;
      cell.fv = (v - 1.0f) * 0.5f;
      cell.p00 = psm__v2_add(psm__v2_sub(cen, psm__v2_scale(dv, 2.0f)), du);
      cell.p10 = psm__v2_load(pos, row * stride);
      cell.p01 = psm__v2_add(psm__v2_sub(cen, psm__v2_scale(dv, 2.0f)),
          psm__v2_scale(du, 3.0f));
      cell.p11 = psm__v2_add(cen, psm__v2_scale(du, 3.0f));
    }
  } else if (u < 1.0f) {
    psm__i32 cu = (psm__i32)gu;
    if (cu == column) cu = column - 1;
    cell.fu = gu - (psm__f32)cu;
    psm__f32 uc = (psm__f32)cu / fc;
    psm__f32 un = (psm__f32)(cu + 1) / fc;
    if (v <= 0.0f) {
      cell.fv = (v + 2.0f) * 0.5f;
      cell.p00 = psm__v2_add(psm__v2_scale(dv, uc),
          psm__v2_sub(cen, psm__v2_scale(du, 2.0f)));
      cell.p10 = psm__v2_add(psm__v2_scale(dv, un),
          psm__v2_sub(cen, psm__v2_scale(du, 2.0f)));
      cell.p01 = psm__v2_load(pos, cu);
      cell.p11 = psm__v2_load(pos, cu + 1);
    } else {
      cell.fv = (v - 1.0f) * 0.5f;
      cell.p00 = psm__v2_load(pos, row * stride + cu);
      cell.p10 = psm__v2_load(pos, row * stride + cu + 1);
      cell.p01 = psm__v2_add(psm__v2_add(cen, psm__v2_scale(dv, uc)),
          psm__v2_scale(du, 3.0f));
      cell.p11 = psm__v2_add(psm__v2_add(cen, psm__v2_scale(dv, un)),
          psm__v2_scale(du, 3.0f));
    }
  } else {
    if (v <= 0.0f) {
      cell.fu = (u - 1.0f) * 0.5f;
      cell.fv = (v + 2.0f) * 0.5f;
      cell.p00 = psm__v2_add(psm__v2_sub(cen, psm__v2_scale(du, 2.0f)), dv);
      cell.p10 = psm__v2_add(psm__v2_sub(cen, psm__v2_scale(du, 2.0f)),
          psm__v2_scale(dv, 3.0f));
      cell.p01 = psm__v2_load(pos, column);
      cell.p11 = psm__v2_add(cen, psm__v2_scale(dv, 3.0f));
    } else if (v < 1.0f) {
      psm__i32 cv = (psm__i32)gv;
      if (cv == row) cv = row - 1;
      cell.fv = gv - (psm__f32)cv;
      cell.fu = (u - 1.0f) * 0.5f;
      psm__f32 vc = (psm__f32)cv / fr;
      psm__f32 vn = (psm__f32)(cv + 1) / fr;
      cell.p00 = psm__v2_load(pos, column + cv * stride);
      cell.p10 = psm__v2_add(psm__v2_add(cen, psm__v2_scale(dv, 3.0f)),
          psm__v2_scale(du, vc));
      cell.p01 = psm__v2_load(pos, column + (cv + 1) * stride);
      cell.p11 = psm__v2_add(psm__v2_add(cen, psm__v2_scale(dv, 3.0f)),
          psm__v2_scale(du, vn));
    } else {
      cell.fu = (u - 1.0f) * 0.5f;
      cell.fv = (v - 1.0f) * 0.5f;
      cell.p00 = psm__v2_load(pos, row * stride + column);
      cell.p10 = psm__v2_add(psm__v2_add(cen, psm__v2_scale(dv, 3.0f)), du);
      cell.p01 = psm__v2_add(psm__v2_add(cen, psm__v2_scale(du, 3.0f)), dv);
      cell.p11 = psm__v2_add(cen,
          psm__v2_add(psm__v2_scale(dv, 3.0f), psm__v2_scale(du, 3.0f)));
    }
  }

  return cell;
}

static inline struct psm__vec2
psm__triangle_interpolate(const struct psm__warp_cell *cell)
{
  psm__f32 fu = cell->fu, fv = cell->fv;
  if (fu + fv <= 1.0f) {
    psm__f32 w00 = 1.0f - fu - fv;
    return psm__v2_bary3(cell->p00, cell->p10, cell->p01, w00, fu, fv);
  } else {
    psm__f32 w10 = 1.0f - fv;
    psm__f32 w11 = fu + fv - 1.0f;
    psm__f32 w01 = 1.0f - fu;
    return psm__v2_bary3(cell->p10, cell->p11, cell->p01, w10, w11, w01);
  }
}

static void
psm__warp_transform(struct psm__model *m, psm__i32 di,
    const psm__f32 *inputs, psm__f32 *outputs, psm__i32 count)
{
  struct psm__deformer_node *dn = m->deformers.nodes;
  psm__i32 si = dn[di].local_idx;
  struct psm__warp *wc = &m->deformers.warps.items[si];
  psm__f32 *pos = m->deformers.warps.pos[si];

  psm__i32 row = wc->row, col = wc->column;
  psm__i32 is_quad = wc->quad_transform, stride = col + 1;
  psm__f32 fr = (psm__f32)row, fc = (psm__f32)col;

  psm__i32 extrap_setup = 0;
  struct psm__warp_basis basis;

  for (psm__i32 i = 0; i < count; i++) {
    struct psm__vec2 uv = psm__v2_load(inputs, i);
    psm__f32 gu = uv.x * fc, gv = uv.y * fr;

    if (uv.x >= 0.0f && uv.x < 1.0f && uv.y >= 0.0f && uv.y < 1.0f) {
      /* Interior: interpolate within grid cell */
      psm__i32 cu = (psm__i32)gu, cv = (psm__i32)gv;
      psm__f32 fu = gu - (psm__f32)cu;
      psm__f32 fv = gv - (psm__f32)cv;

      psm__i32 bi = cv * stride + cu;
      struct psm__vec2 p00 = psm__v2_load(pos, bi);
      struct psm__vec2 p10 = psm__v2_load(pos, bi + 1);
      struct psm__vec2 p01 = psm__v2_load(pos, bi + stride);
      struct psm__vec2 p11 = psm__v2_load(pos, bi + stride + 1);

      struct psm__vec2 result;
      if (is_quad) {
        result = psm__v2_bilinear(p00, p10, p01, p11, fu, fv);
      } else {
        struct psm__warp_cell cell = {fu, fv, p00, p10, p01, p11};
        result = psm__triangle_interpolate(&cell);
      }
      psm__v2_store(outputs, i, result);
    } else {
      /* Extrapolation: compute basis if needed */
      if (!extrap_setup) {
        basis = psm__warp_extrap_basis(pos, row, col, stride);
        extrap_setup = 1;
      }

      if (uv.x > -2.0f && uv.x < 3.0f && uv.y > -2.0f && uv.y < 3.0f) {
        /* Near-exterior: virtual cell + triangle */
        struct psm__warp_cell cell = psm__warp_get_extrapolation_cell(
                uv.x, uv.y, gu, gv, row, col, stride, pos, &basis);
        struct psm__vec2 r = psm__triangle_interpolate(&cell);
        psm__v2_store(outputs, i, r);
      } else {
        /* Far-exterior: simple affine */
        psm__f32 rx = basis.dpdv.x * uv.x
            + basis.center.x + basis.dpdu.x * uv.y;
        psm__f32 ry = basis.dpdv.y * uv.x
            + basis.center.y + basis.dpdu.y * uv.y;
        outputs[i * 2] = rx;
        outputs[i * 2 + 1] = ry;
      }
    }
  }
}

static void
psm__rot_transform(struct psm__model *m, psm__i32 di,
    const psm__f32 *inputs, psm__f32 *outputs, psm__i32 count)
{
  struct psm__deformer_node *dn = m->deformers.nodes;
  psm__i32 si = dn[di].local_idx;
  struct psm__rotation *rc = &m->deformers.rotations.items[si];

  psm__f32 base_angle = rc->base_angle;
  psm__f32 angle = m->deformers.rotations.angle[si];
  psm__f32 scale = m->deformers.rotations.scale[si];
  struct psm__vec2 origin = psm__v2(m->deformers.rotations.origin_x[si],
      m->deformers.rotations.origin_y[si]);
  psm__i32 rx = m->deformers.rotations.reflect_x[si];
  psm__i32 ry = m->deformers.rotations.reflect_y[si];

  psm__f32 angle_rad = (base_angle + angle) * PSM__PI / 180.0f;
  psm__f32 sin_a = sinf(angle_rad);
  psm__f32 cos_a = cosf(angle_rad);

  psm__f32 rxf = rx ? -1.0f : 1.0f;
  psm__f32 ryf = ry ? -1.0f : 1.0f;

  psm__f32 m00 = scale * cos_a * rxf;
  psm__f32 m01 = scale * (-sin_a) * ryf;
  psm__f32 m10 = scale * sin_a * rxf;
  psm__f32 m11 = scale * cos_a * ryf;

  for (psm__i32 i = 0; i < count; i++) {
    struct psm__vec2 p = psm__v2_load(inputs, i);
    struct psm__vec2 r = psm__v2(fmaf(m01, p.y, fmaf(m00, p.x, origin.x)),
        fmaf(m11, p.y, fmaf(m10, p.x, origin.y)));
    psm__v2_store(outputs, i, r);
  }
}

static inline struct psm__vec2
psm__deformer_transform_point(struct psm__model *m,
    psm__i32 deformer_idx, struct psm__vec2 p)
{
  psm__f32 in[2] = {p.x, p.y};
  psm__f32 out[2];
  psm__i32 pt = m->deformers.nodes[deformer_idx].type;
  if (pt == PSM__DEFORMER_TYPE_WARP)
    psm__warp_transform(m, deformer_idx, in, out, 1);
  else
    psm__rot_transform(m, deformer_idx, in, out, 1);
  return (struct psm__vec2){out[0], out[1]};
}

static void
psm__propagate_deformer_colors(psm__f32 *mo, psm__f32 *so,
    const psm__f32 *sm, const psm__f32 *ss,
    psm__i32 self_idx, psm__i32 spec_idx, psm__i32 parent_idx)
{
  psm__i32 s4 = self_idx * 4, p4 = spec_idx * 4;

  if (parent_idx == -1) {
    mo[s4 + 0] = sm[p4 + 0];
    mo[s4 + 1] = sm[p4 + 1];
    mo[s4 + 2] = sm[p4 + 2];
    mo[s4 + 3] = 1.0f;

    so[s4 + 0] = ss[p4 + 0];
    so[s4 + 1] = ss[p4 + 1];
    so[s4 + 2] = ss[p4 + 2];
    so[s4 + 3] = 1.0f;
  } else {
    psm__i32 pi4 = parent_idx * 4;
    const psm__f32 *pm = &mo[pi4];
    const psm__f32 *ps = &so[pi4];

    mo[s4 + 0] = sm[p4 + 0] * pm[0];
    mo[s4 + 1] = sm[p4 + 1] * pm[1];
    mo[s4 + 2] = sm[p4 + 2] * pm[2];
    mo[s4 + 3] = 1.0f;

    so[s4 + 0] = fmaf(-ss[p4 + 0], ps[0], ss[p4 + 0] + ps[0]);
    so[s4 + 1] = fmaf(-ss[p4 + 1], ps[1], ss[p4 + 1] + ps[1]);
    so[s4 + 2] = fmaf(-ss[p4 + 2], ps[2], ss[p4 + 2] + ps[2]);
    so[s4 + 3] = 1.0f;
  }
}

static void
psm__apply_warp(struct psm__model *m, psm__i32 di)
{
  struct psm__deformer_node *dn = m->deformers.nodes;
  struct psm__deformer_node *self = &dn[di];
  psm__f32 *d_opa = m->deformers.opacity;
  psm__f32 *d_scl = m->deformers.scale;
  psm__i32 pi = self->parent_deformer_idx;
  psm__i32 si = self->local_idx;

  if (pi == -1) {
    d_opa[di] = m->deformers.warps.opacity[si];
    d_scl[di] = 1.0f;
  } else {
    psm__f32 **pos = m->deformers.warps.pos;
    psm__i32 vc = m->deformers.warps.items[si].vertex_count;
    psm__i32 pt = dn[pi].type;

    switch (pt) {
    case PSM__DEFORMER_TYPE_WARP:
      psm__warp_transform(m, pi, pos[si], pos[si], vc);
      break;
    case PSM__DEFORMER_TYPE_ROTATION:
      psm__rot_transform(m, pi, pos[si], pos[si], vc);
      break;
    }

    d_opa[di] = m->deformers.warps.opacity[si] * d_opa[pi];
    d_scl[di] = d_scl[pi];
  }

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__propagate_deformer_colors(
      m->deformers.mul_color, m->deformers.scr_color,
      m->deformers.warps.mul_color, m->deformers.warps.scr_color,
      di, si, pi);
}

static void
psm__apply_rotation(struct psm__model *m, psm__i32 di)
{
  struct psm__deformer_node *dn = m->deformers.nodes;
  struct psm__deformer_node *self = &dn[di];
  psm__f32 *d_opa = m->deformers.opacity;
  psm__f32 *d_scl = m->deformers.scale;
  psm__i32 pi = self->parent_deformer_idx;
  psm__i32 si = self->local_idx;

  psm__f32 *r_opa = m->deformers.rotations.opacity;
  psm__f32 *r_scl = m->deformers.rotations.scale;
  psm__f32 *r_ox = m->deformers.rotations.origin_x;
  psm__f32 *r_oy = m->deformers.rotations.origin_y;
  psm__f32 *r_ang = m->deformers.rotations.angle;

  if (pi == -1) {
    d_opa[di] = r_opa[si];
    d_scl[di] = r_scl[si];
  } else {
    struct psm__vec2 origin = psm__v2(r_ox[si], r_oy[si]);
    struct psm__vec2 direction = {0.0f, 0.0f};
    psm__i32 pt = dn[pi].type;
    psm__f32 dir_delta = (pt == PSM__DEFORMER_TYPE_ROTATION) ? -10.0f : -0.1f;

    struct psm__vec2 t_origin = psm__deformer_transform_point(m, pi, origin);

    psm__f32 scale = 1.0f;
    psm__i32 iter;
    for (iter = 0; iter < 10; iter++) {
      struct psm__vec2 tp = psm__v2(origin.x, origin.y + scale * dir_delta);
      struct psm__vec2 tt = psm__deformer_transform_point(m, pi, tp);
      struct psm__vec2 d = psm__v2_sub(tt, t_origin);

      if (d.x != 0.0f || d.y != 0.0f) {
        direction = d;
        break;
      }

      tp = psm__v2(origin.x, origin.y - scale * dir_delta);
      tt = psm__deformer_transform_point(m, pi, tp);
      d = psm__v2_sub(tt, t_origin);

      if (d.x != 0.0f || d.y != 0.0f) {
        direction = psm__v2_neg(d);
        break;
      }

      scale *= 0.1f;
    }

    if (iter >= 16) {
      PSM__WARN("rotation direction did not converge");
    }

    psm__f32 base_dir[2] = {0.0f, dir_delta};
    psm__f32 dir_arr[2] = {direction.x, direction.y};
    psm__f32 angle_adj =
        (psm__get_angle_not_abs(base_dir, dir_arr) * -180.0f) / PSM__PI;

    origin = psm__deformer_transform_point(m, pi, origin);

    r_ox[si] = origin.x;
    r_oy[si] = origin.y;
    r_ang[si] += angle_adj;

    d_opa[di] = r_opa[si] * d_opa[pi];

    psm__f32 cs = r_scl[si] * d_scl[pi];
    d_scl[di] = cs;
    r_scl[si] = cs;
  }

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__propagate_deformer_colors(
      m->deformers.mul_color, m->deformers.scr_color,
      m->deformers.rotations.mul_color,
      m->deformers.rotations.scr_color, di, si, pi);
}


PSM__DEF void
psm__enable_deformers(struct psm__model *m)
{
  psm__i32 count = m->deformers.count;
  if (count <= 0)
    return;

  struct psm__deformer_node *nodes = m->deformers.nodes;
  bool *en = m->deformers.enable;
  bool *pen = m->parts.enable;
  bool *wen = m->deformers.warps.enable;
  bool *ren = m->deformers.rotations.enable;

  for (psm__i32 i = 0; i < count; i++) {
    struct psm__deformer_node *node = &nodes[i];
    bool e = node->local_enable;
    psm__i32 ppi = node->parent_part_idx;
    psm__i32 pdi = node->parent_deformer_idx;

    if (e && ppi != -1) e = pen[ppi];
    if (e && pdi != -1) e = en[pdi];
    if (e) e = !node->binding->out_of_range;

    en[i] = e;

    psm__i32 dt = node->type;
    psm__i32 si = node->local_idx;

    switch (dt) {
    case PSM__DEFORMER_TYPE_WARP:
      wen[si] = e;
      break;
    case PSM__DEFORMER_TYPE_ROTATION:
      ren[si] = e;
      break;
    default:
      PSM__LOG("unknown deformer type");
      break;
    }
  }
}

PSM__DEF void
psm__gather_warps(struct psm__model *m)
{
  psm__i32 count = m->deformers.warps.count;
  if (count <= 0)
    return;

  struct psm__sections *ms = m->source->sections;
  struct psm__warp *items = m->deformers.warps.items;
  if (!items)
    return;

  struct psm__warp_keydata *wk = &m->deformers.warps.keydata;
  psm__i32 *kb = ms->warp_src.keyform_offset;
  psm__i32 max_kf = ms->count_info->warp_kf;

  struct psm__binding *bindings[count];
  for (psm__i32 i = 0; i < count; i++)
    bindings[i] = items[i].binding;

  struct psm__gather_channel ch[] = {
    { ms->warp_key_src.opacity, wk->opacity },
  };
  psm__gather_scalars(count, bindings, kb, max_kf, &wk->interp, ch, 1);

  psm__gather_positions(count, bindings, kb, max_kf,
      ms->key_pos_src.xy, ms->warp_key_src.key_pos_offset,
      ms->count_info->kf_pos, wk->pos);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__i32 *ckb = ms->warp_src.key_color_offset;
  if (!ckb || !ms->kf_mul_color_src.r || !ms->kf_scr_color_src.r)
    return;

  psm__gather_colors(count, bindings, ckb,
      ms->count_info->kf_mul_colors,
      &ms->kf_mul_color_src, &ms->kf_scr_color_src,
      &wk->mul_color, &wk->scr_color);
}

PSM__DEF void
psm__gather_rotations(struct psm__model *m)
{
  psm__i32 count = m->deformers.rotations.count;
  if (count <= 0)
    return;

  struct psm__sections *ms = m->source->sections;
  struct psm__rotation *items = m->deformers.rotations.items;
  if (!items)
    return;

  struct psm__rotation_keydata *rk = &m->deformers.rotations.keydata;
  psm__i32 *kb = ms->rotation_src.keyform_offset;
  psm__i32 max_kf = ms->count_info->rotation_kf;

  struct psm__binding *bindings[count];
  for (psm__i32 i = 0; i < count; i++)
    bindings[i] = items[i].binding;

  struct psm__gather_channel ch[] = {
    { ms->rotation_key_src.opacity,  rk->opacity },
    { ms->rotation_key_src.angle,    rk->angle },
    { ms->rotation_key_src.origin_x, rk->origin_x },
    { ms->rotation_key_src.origin_y, rk->origin_y },
    { ms->rotation_key_src.scale,    rk->scale },
  };
  psm__gather_scalars(count, bindings, kb, max_kf, &rk->interp, ch, 5);

  psm__gather_reflect(count, bindings, kb, max_kf,
      ms->rotation_key_src.reflect_x, ms->rotation_key_src.reflect_y,
      m->deformers.rotations.reflect_x, m->deformers.rotations.reflect_y);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__i32 *ckb = ms->rotation_src.key_color_offset;
  if (!ckb || !ms->kf_mul_color_src.r || !ms->kf_scr_color_src.r)
    return;

  psm__gather_colors(count, bindings, ckb,
      ms->count_info->kf_mul_colors,
      &ms->kf_mul_color_src, &ms->kf_scr_color_src,
      &rk->mul_color, &rk->scr_color);
}

PSM__DEF void
psm__apply_transforms(struct psm__model *m)
{
  psm__i32 count = m->deformers.count;
  if (count <= 0)
    return;

  bool *en = m->deformers.enable;
  struct psm__deformer_node *nodes = m->deformers.nodes;

  /*
   * Iterate in array order. MOC3 stores deformers in
   * topological order (parents before children), so each
   * deformer's parent is already transformed.
   */
  for (psm__i32 i = 0; i < count; i++) {
    if (en[i]) {
      switch (nodes[i].type) {
      case PSM__DEFORMER_TYPE_WARP:
        psm__apply_warp(m, i);
        break;
      case PSM__DEFORMER_TYPE_ROTATION:
        psm__apply_rotation(m, i);
        break;
      }
    }
  }
}

PSM__DEF void
psm__apply_transforms_to_meshes(struct psm__model *m)
{
  psm__i32 count = m->art_meshes.count;
  if (count <= 0)
    return;

  struct psm__art_mesh *am = m->art_meshes.meshes;
  psm__f32 *d_opa = m->deformers.opacity;
  struct psm__deformer_node *dn = m->deformers.nodes;
  psm__f32 **cp = m->art_meshes.pos;
  psm__f32 *am_opa = m->art_meshes.opacity;
  bool *en = m->art_meshes.enable;

  for (psm__i32 i = 0; i < count; i++) {
    if (!en[i]) continue;

    psm__i32 pdi = am[i].parent_deformer_idx;
    if (pdi == -1) continue;

    psm__i32 vc = am[i].vertex_count;
    am_opa[i] *= d_opa[pdi];

    psm__i32 dt = dn[pdi].type;
    if (dt == PSM__DEFORMER_TYPE_WARP)
      psm__warp_transform(m, pdi, cp[i], cp[i], vc);
    else
      psm__rot_transform(m, pdi, cp[i], cp[i], vc);
  }
}
