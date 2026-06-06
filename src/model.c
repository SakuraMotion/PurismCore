/*
 * Purism Core: model initialization and accessors
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <string.h>
#include "private.h"
#include "array.h"
#include "debug.h"
#include "error.h"
#include "arena.h"
#include "moc3.h"
#include "model.h"
#include "update.h"

/*
 * Remap v6 extended blend modes to the closest v5 equivalents.
 */
static inline psm__i32
psm__remap_blend_mode(psm__i32 mode)
{
#if PSM_COMPAT_VERSION >= 0x06000000L
  return mode;
#else
  switch (mode) {
  case csmColorBlendType_Normal:
    return csmColorBlendType_Normal;
  /* We may not have these enums defined if built as a single-file bundle. */
  case 1: /* csmColorBlendType_AddCompatible */
  case 3: /* csmColorBlendType_Add */
  case 4: /* csmColorBlendType_AddGlow */
  case 9: /* csmColorBlendType_Lighten */
  case 10: /* csmColorBlendType_Screen */
  case 11: /* csmColorBlendType_ColorDodge */
    return csmColorBlendType_AddCompatible;
  case 2: /* csmColorBlendType_MultiplyCompatible */
  case 6: /* csmColorBlendType_Multiply */
  case 5: /* csmColorBlendType_Darken */
  case 7: /* csmColorBlendType_ColorBurn */
  case 8: /* csmColorBlendType_LinearBurn */
    return csmColorBlendType_MultiplyCompatible;
  default:
    return csmColorBlendType_Normal;
  }
#endif
}

/*
 * Safe lookup of param_count via binding index;
 * returns 0 for out-of-bounds.
 */
static inline psm__i32
psm__safe_param_count(const struct psm__sections *src,
    const struct psm__count_info *cnt, const psm__i32 *idx_arr, psm__i32 i)
{
  psm__i32 bi = idx_arr[i];
  if (!psm__valid_idx(bi, cnt->bindings))
    return 0;
  return psm__clamp_i32(src->binding_src.axis_idx_count[bi],
      0, PSM__MAX_AXES);
}

static struct psm__model *
psm__alloc_model(struct psm__arena *arena, psm__u8 ver,
    const struct psm__sections *src, struct psm__count_info *cnt)
{

  /*
   * Field allocation macros - allocate and assign only
   * in real mode.  In dry-run mode, psm__arena_alloc
   * returns NULL so assignment is skipped.
   */
#define psm__alloc_field(field, T, n) { \
    void *_p = psm__arena_alloc(arena, \
        psm__arena_safe_mul(arena, sizeof(T), (n))); \
    if (_p) (field) = (T *)_p; \
  }
#define psm__alloc_field_size(field, T, size) { \
    void *_p = psm__arena_alloc(arena, (size)); \
    if (_p) (field) = (T *)_p; \
  }

#define MAX_COMB(pc) ((psm__u32)1 << (pc))

  psm__u32 part_tmp_total = 0;
  if (cnt->parts > 0 && src->part_src.binding_idx) {
    for (psm__i32 i = 0; i < cnt->parts; i++) {
      psm__i32 pc = psm__safe_param_count(src, cnt,
          src->part_src.binding_idx, i);
      part_tmp_total += MAX_COMB(pc);
    }
  }

  psm__u32 warp_pos_total = 0, warp_tmp_total = 0;
  if (cnt->warps > 0 && src->warp_src.binding_idx &&
      src->warp_src.vertex_count) {
    for (psm__i32 i = 0; i < cnt->warps; i++) {
      psm__i32 vc = src->warp_src.vertex_count[i];
      if (vc > 0)
        warp_pos_total += psm__align_to_16(2 * sizeof(psm__f32) * vc);
      psm__i32 pc = psm__safe_param_count(src, cnt,
          src->warp_src.binding_idx, i);
      warp_tmp_total += MAX_COMB(pc);
    }
  }

  psm__u32 rot_tmp_total = 0;
  if (cnt->rotations > 0 && src->rotation_src.binding_idx) {
    for (psm__i32 i = 0; i < cnt->rotations; i++) {
      psm__i32 pc = psm__safe_param_count(src, cnt,
          src->rotation_src.binding_idx, i);
      rot_tmp_total += MAX_COMB(pc);
    }
  }

  psm__u32 am_pos_total = 0, am_tmp_total = 0;
  if (cnt->art_meshes > 0 && src->art_mesh_src.binding_idx &&
      src->art_mesh_src.vertex_count) {
    for (psm__i32 i = 0; i < cnt->art_meshes; i++) {
      psm__i32 vc = src->art_mesh_src.vertex_count[i];
      if (vc > 0)
        am_pos_total += psm__align_to_16(2 * sizeof(psm__f32) * vc);
      psm__i32 pc = psm__safe_param_count(src, cnt,
          src->art_mesh_src.binding_idx, i);
      am_tmp_total += MAX_COMB(pc);
    }
  }

  psm__u32 kb_ptr_total = 0, kb_idx_total = 0;
  if (cnt->bindings > 0 && src->binding_src.axis_idx_count) {
    for (psm__i32 i = 0; i < cnt->bindings; i++) {
      psm__i32 pc = psm__clamp_i32(src->binding_src.axis_idx_count[i],
          0, PSM__MAX_AXES);
      kb_ptr_total += pc;
      kb_idx_total += MAX_COMB(pc);
    }
  }

  psm__u32 glue_tmp_total = 0;
  if (cnt->glues > 0 && src->glue_src.binding_idx) {
    for (psm__i32 i = 0; i < cnt->glues; i++) {
      psm__i32 pc = psm__safe_param_count(src, cnt,
          src->glue_src.binding_idx, i);
      glue_tmp_total += MAX_COMB(pc);
    }
  }

  psm__i32 do_max_count = 0, do_max_level = 0;
  if (cnt->draw_groups > 0 && src->draw_group_src.obj_count &&
      src->draw_group_src.max_order && src->draw_group_src.min_order) {
    for (psm__i32 i = 0; i < cnt->draw_groups; i++) {
      psm__i32 c = src->draw_group_src.obj_count[i];
      psm__i32 mx = src->draw_group_src.max_order[i];
      psm__i32 mn = src->draw_group_src.min_order[i];
      psm__i32 lvl = psm__safe_order_level(mx, mn);
      if (c > 0 && c > do_max_count)
        do_max_count = c;
      if (lvl > 0 && lvl > do_max_level)
        do_max_level = lvl;
    }
  }

  psm__u32 bs_constr_ptrs_total = 0;
  if (ver >= csmMocVersion_42 &&
      src->blend_binding_src.bs_constraint_idx_count) {
    for (psm__i32 i = 0; i < cnt->blend_bindings; i++) {
      bs_constr_ptrs_total += src->blend_binding_src.bs_constraint_idx_count[i];
    }
  }

  psm__u32 os_tmp_total = 0;
  if (ver >= csmMocVersion_53 && src->offscreen_src.owner_idx &&
      src->part_src.binding_idx) {
    for (psm__i32 i = 0; i < cnt->offscreens; i++) {
      psm__i32 oi = src->offscreen_src.owner_idx[i];
      if (!psm__valid_idx(oi, cnt->parts)) continue;
      psm__i32 pc = psm__safe_param_count(src, cnt,
          src->part_src.binding_idx, oi);
      os_tmp_total += MAX_COMB(pc);
    }
  }

  /* Model struct - must be allocated first */
  struct psm__model *m = PSM__ARENA_NEW(arena, struct psm__model, 1);

  /* Parts */
  psm__alloc_field(m->parts.items, struct psm__part, cnt->parts);
  psm__alloc_field(m->parts.opacity, psm__f32, cnt->parts);
  psm__alloc_field(m->parts.draw_order, psm__i32, cnt->parts);
  psm__alloc_field(m->parts.input_opacity, psm__f32, cnt->parts);
  psm__alloc_field(m->parts.enable, bool, cnt->parts);
  if (ver < csmMocVersion_53) {
    psm__alloc_field(m->parts.offscreen_src_idx, psm__i32, cnt->parts);
  }
  struct psm__part_keydata *pk = &m->parts.keydata;
  psm__alloc_field(pk->interp.max_blend, psm__i32, cnt->parts);
  psm__alloc_field(pk->interp.tmp, psm__f32, part_tmp_total);
  psm__alloc_field(pk->interp.blend_count, psm__i32, cnt->parts);
  psm__alloc_field(pk->interp.weights, psm__f32, part_tmp_total);
  psm__alloc_field(pk->draw_order, psm__f32, part_tmp_total);

  /* Deformers (generic) */
  psm__alloc_field(m->deformers.nodes,
      struct psm__deformer_node, cnt->deformers);
  psm__alloc_field(m->deformers.enable, bool, cnt->deformers);
  psm__alloc_field(m->deformers.opacity, psm__f32, cnt->deformers);
  psm__alloc_field(m->deformers.scale, psm__f32, cnt->deformers);
  psm__alloc_field(m->deformers.mul_color, psm__f32, 4 * cnt->deformers);
  psm__alloc_field(m->deformers.scr_color, psm__f32, 4 * cnt->deformers);

  /* Warp deformers */
  struct psm__warps *w = &m->deformers.warps;
  struct psm__warp_keydata *wk = &w->keydata;
  psm__alloc_field(w->items, struct psm__warp, cnt->warps);
  psm__alloc_field(w->enable, bool, cnt->warps);
  psm__alloc_field(w->opacity, psm__f32, cnt->warps);
  psm__alloc_field(w->pos, psm__f32 *, cnt->warps);
  psm__f32 *warp_pos_data = PSM__ARENA_NEW_SIZE(
      arena, psm__f32, warp_pos_total);
  psm__alloc_field(w->mul_color, psm__f32, 4 * cnt->warps);
  psm__alloc_field(w->scr_color, psm__f32, 4 * cnt->warps);
  psm__alloc_field(wk->interp.max_blend, psm__i32, cnt->warps);
  psm__alloc_field(wk->interp.tmp, psm__f32, warp_tmp_total);
  psm__alloc_field(wk->interp.blend_count, psm__i32, cnt->warps);
  psm__alloc_field(wk->interp.weights, psm__f32, warp_tmp_total);
  psm__alloc_field(wk->opacity, psm__f32, warp_tmp_total);
  psm__alloc_field(wk->pos, psm__f32 *, PSM__PTR_FLOAT_RATIO * warp_tmp_total);
  psm__alloc_field(wk->mul_color.r, psm__f32, warp_tmp_total);
  psm__alloc_field(wk->mul_color.g, psm__f32, warp_tmp_total);
  psm__alloc_field(wk->mul_color.b, psm__f32, warp_tmp_total);
  psm__alloc_field(wk->scr_color.r, psm__f32, warp_tmp_total);
  psm__alloc_field(wk->scr_color.g, psm__f32, warp_tmp_total);
  psm__alloc_field(wk->scr_color.b, psm__f32, warp_tmp_total);

  /* Rotation deformers */
  struct psm__rotations *r = &m->deformers.rotations;
  struct psm__rotation_keydata *rk = &r->keydata;
  psm__alloc_field(r->items, struct psm__rotation, cnt->rotations);
  psm__alloc_field(r->enable, bool, cnt->rotations);
  psm__alloc_field(r->opacity, psm__f32, cnt->rotations);
  psm__alloc_field(r->scale, psm__f32, cnt->rotations);
  psm__alloc_field(r->origin_x, psm__f32, cnt->rotations);
  psm__alloc_field(r->origin_y, psm__f32, cnt->rotations);
  psm__alloc_field(r->angle, psm__f32, cnt->rotations);
  psm__alloc_field(r->reflect_x, psm__i32, cnt->rotations);
  psm__alloc_field(r->reflect_y, psm__i32, cnt->rotations);
  psm__alloc_field(r->mul_color, psm__f32, 4 * cnt->rotations);
  psm__alloc_field(r->scr_color, psm__f32, 4 * cnt->rotations);
  psm__alloc_field(rk->interp.max_blend, psm__i32, cnt->rotations);
  psm__alloc_field(rk->interp.tmp, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->interp.blend_count, psm__i32, cnt->rotations);
  psm__alloc_field(rk->interp.weights, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->opacity, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->angle, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->origin_x, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->origin_y, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->scale, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->mul_color.r, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->mul_color.g, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->mul_color.b, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->scr_color.r, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->scr_color.g, psm__f32, rot_tmp_total);
  psm__alloc_field(rk->scr_color.b, psm__f32, rot_tmp_total);

  /* Art meshes */
  struct psm__art_meshes *am = &m->art_meshes;
  struct psm__art_mesh_keydata *ak = &am->keydata;
  psm__alloc_field(am->meshes, struct psm__art_mesh, cnt->art_meshes);
  psm__alloc_field(am->enable, bool, cnt->art_meshes);
  psm__alloc_field(am->const_flags, psm__u8, cnt->art_meshes);
  psm__alloc_field(am->change_flags, psm__u8, cnt->art_meshes);
  psm__alloc_field(am->blend_mode, psm__i32, cnt->art_meshes);
  psm__alloc_field(am->draw_order, psm__i32, cnt->art_meshes);
  psm__alloc_field(am->pos, psm__f32 *, cnt->art_meshes);
  psm__f32 *am_pos_data = PSM__ARENA_NEW_SIZE(arena, psm__f32, am_pos_total);
  psm__alloc_field(am->opacity, psm__f32, cnt->art_meshes);
  psm__alloc_field(am->mul_color, psm__f32, 4 * cnt->art_meshes);
  psm__alloc_field(am->scr_color, psm__f32, 4 * cnt->art_meshes);
  psm__alloc_field(am->last_render_order, psm__i32, cnt->art_meshes);
  psm__alloc_field(am->last_draw_order, psm__i32, cnt->art_meshes);
  psm__alloc_field(am->last_opacity, psm__f32, cnt->art_meshes);
  psm__alloc_field(am->last_mul_color, psm__f32, 4 * cnt->art_meshes);
  psm__alloc_field(am->last_scr_color, psm__f32, 4 * cnt->art_meshes);
  psm__alloc_field(ak->interp.max_blend, psm__i32, cnt->art_meshes);
  psm__alloc_field(ak->interp.tmp, psm__f32, am_tmp_total);
  psm__alloc_field(ak->interp.blend_count, psm__i32, cnt->art_meshes);
  psm__alloc_field(ak->interp.weights, psm__f32, am_tmp_total);
  psm__alloc_field(ak->opacity, psm__f32, am_tmp_total);
  psm__alloc_field(ak->draw_order, psm__f32, am_tmp_total);
  psm__alloc_field(ak->pos, psm__f32 *, PSM__PTR_FLOAT_RATIO * am_tmp_total);
  psm__alloc_field(ak->mul_color.r, psm__f32, am_tmp_total);
  psm__alloc_field(ak->mul_color.g, psm__f32, am_tmp_total);
  psm__alloc_field(ak->mul_color.b, psm__f32, am_tmp_total);
  psm__alloc_field(ak->scr_color.r, psm__f32, am_tmp_total);
  psm__alloc_field(ak->scr_color.g, psm__f32, am_tmp_total);
  psm__alloc_field(ak->scr_color.b, psm__f32, am_tmp_total);

  /* Parameters */
  psm__alloc_field(m->params.items, struct psm__param, cnt->parameters);
  if (ver < csmMocVersion_42) {
    psm__alloc_field(m->params.type, psm__i32, cnt->parameters);
  }
  psm__alloc_field(m->params.input_value, psm__f32, cnt->parameters);

  /* Parameter bindings */
  psm__alloc_field(m->axes.items, struct psm__axis,
      cnt->axes);

  /* Keyform bindings */
  psm__alloc_field(m->bindings.items, struct psm__binding, cnt->bindings);
  struct psm__axis **kb_ptrs = PSM__ARENA_NEW(
      arena, struct psm__axis *, kb_ptr_total);
  psm__i32 *kb_indices = PSM__ARENA_NEW(arena, psm__i32, kb_idx_total);
  psm__f32 *kb_weights = PSM__ARENA_NEW(arena, psm__f32, kb_idx_total);

  /* BlendShape (v4.2+) */
  struct psm__blend_constraint **bs_constr_ptrs = NULL;
  if (ver >= csmMocVersion_42) {
    psm__alloc_field(m->blend_constraints.items,
        struct psm__blend_constraint, cnt->bs_constraints);
    psm__alloc_field(m->blend_axes.items, struct psm__blend_axis,
        cnt->blend_axes);
    psm__alloc_field(m->blend_bindings.items, struct psm__blend_binding,
        cnt->blend_bindings);
    bs_constr_ptrs = PSM__ARENA_NEW(arena, struct psm__blend_constraint *,
        bs_constr_ptrs_total);
    psm__alloc_field(m->bs_warps.items,
        struct psm__blend_shape, cnt->bs_warps);
    psm__alloc_field(m->bs_art_meshes.items,
        struct psm__blend_shape, cnt->bs_art_meshes);

    if (ver >= csmMocVersion_50) {
      psm__alloc_field(m->bs_parts.items,
          struct psm__blend_shape, cnt->bs_parts);
      psm__alloc_field(m->bs_rotations.items,
          struct psm__blend_shape, cnt->bs_rotations);
      psm__alloc_field(m->bs_glues.items,
          struct psm__blend_shape, cnt->bs_glues);
    }

    if (ver >= csmMocVersion_53) {
      psm__alloc_field(m->bs_offscreens.items,
          struct psm__blend_shape, cnt->bs_offscreens);
    }
  }

  /* Draw order groups */
  struct psm__draw_item *do_items = NULL;
  if (cnt->draw_groups > 0 && src->draw_group_src.obj_count &&
      src->draw_group_src.max_order && src->draw_group_src.min_order) {
    psm__alloc_field(m->draw_groups.groups, struct psm__draw_group,
        cnt->draw_groups);
    do_items = PSM__ARENA_NEW(arena, struct psm__draw_item, cnt->draw_items);
    psm__alloc_field(m->draw_groups.sort.first, psm__i32, do_max_level);
    psm__alloc_field(m->draw_groups.sort.next, psm__i32, do_max_count);
    psm__alloc_field(m->draw_groups.sort.last, psm__i32, do_max_level);
  }

  /* Glues */
  struct psm__glue_keydata *gk = &m->glues.keydata;
  psm__alloc_field(m->glues.items, struct psm__glue, cnt->glues);
  psm__alloc_field(gk->interp.max_blend, psm__i32, cnt->glues);
  psm__alloc_field(gk->interp.blend_count, psm__i32, cnt->glues);
  psm__alloc_field(gk->interp.tmp, psm__f32, glue_tmp_total);
  psm__alloc_field(gk->interp.weights, psm__f32, glue_tmp_total);
  psm__alloc_field(gk->intensity, psm__f32, glue_tmp_total);
  psm__alloc_field(m->glues.intensity, psm__f32, glue_tmp_total);

  /* Offscreen rendering (v5.3+) */
  if (ver >= csmMocVersion_53) {
    struct psm__offscreens *os = &m->offscreens;
    struct psm__offscreen_keydata *ok = &os->keydata;
    psm__alloc_field(os->surfaces, struct psm__offscreen, cnt->offscreens);
    psm__alloc_field(os->opacity, psm__f32, cnt->offscreens);
    psm__alloc_field(os->enable, bool, cnt->offscreens);
    psm__alloc_field(os->mul_color, psm__f32, 4 * cnt->offscreens);
    psm__alloc_field(os->scr_color, psm__f32, 4 * cnt->offscreens);
    psm__alloc_field(ok->interp.max_blend, psm__i32, cnt->offscreens);
    psm__alloc_field(ok->interp.tmp, psm__f32, os_tmp_total);
    psm__alloc_field(ok->interp.blend_count, psm__i32, cnt->offscreens);
    psm__alloc_field(ok->interp.weights, psm__f32, os_tmp_total);
    psm__alloc_field(ok->opacity, psm__f32, os_tmp_total);
    psm__alloc_field(ok->mul_color.r, psm__f32, os_tmp_total);
    psm__alloc_field(ok->mul_color.g, psm__f32, os_tmp_total);
    psm__alloc_field(ok->mul_color.b, psm__f32, os_tmp_total);
    psm__alloc_field(ok->scr_color.r, psm__f32, os_tmp_total);
    psm__alloc_field(ok->scr_color.g, psm__f32, os_tmp_total);
    psm__alloc_field(ok->scr_color.b, psm__f32, os_tmp_total);
  }

  /* Parameter extensions */
  if (ver < csmMocVersion_42 || !src->param_ext_src.key_runtime) {
    psm__alloc_field(m->param_ext.keys, psm__f32 *, cnt->parameters);
    psm__alloc_field(m->param_ext.key_counts, psm__i32, cnt->parameters);
  }

  /* Render orders */
  psm__i32 ro_count = cnt->art_meshes +
      (ver >= csmMocVersion_53 ? cnt->offscreens : 0);
  psm__alloc_field(m->render_order, psm__i32, ro_count);

#undef psm__alloc_field
#undef psm__alloc_field_size

  /* In dry-run mode, return NULL */
  if (arena->base == NULL)
    return NULL;

  m->parts.count = cnt->parts;
  m->parts.keydata.interp.object_count = cnt->parts;
  if (ver >= csmMocVersion_53)
    m->parts.offscreen_src_idx = src->part_src.offscreen_idx;

  m->deformers.count = cnt->deformers;
  w->count = cnt->warps;
  wk->interp.object_count = cnt->warps;
  r->count = cnt->rotations;
  rk->interp.object_count = cnt->rotations;

  am->count = cnt->art_meshes;
  ak->interp.object_count = cnt->art_meshes;

  m->params.count = cnt->parameters;
  if (ver >= csmMocVersion_42)
    m->params.type = src->param_src.type;

  m->axes.count = cnt->axes;
  m->bindings.count = cnt->bindings;

  m->draw_groups.count = cnt->draw_groups;
  m->glues.count = cnt->glues;
  gk->interp.object_count = cnt->glues;

  if (ver >= csmMocVersion_42) {
    m->blend_constraints.count = cnt->bs_constraints;
    if (src->blend_axis_src.keys_count &&
        src->blend_axis_src.keys_begin) {
      m->blend_axes.count = cnt->blend_axes;
    }
    if (src->blend_binding_src.axis_idx &&
        src->blend_binding_src.key_bs_begin) {
      m->blend_bindings.count = cnt->blend_bindings;
    }
    m->bs_warps.count = cnt->bs_warps;
    m->bs_art_meshes.count = cnt->bs_art_meshes;

    if (ver >= csmMocVersion_50) {
      m->bs_parts.count = cnt->bs_parts;
      m->bs_rotations.count = cnt->bs_rotations;
      m->bs_glues.count = cnt->bs_glues;
    }

    if (ver >= csmMocVersion_53 && src->bs_offscreen_src.target_idx &&
        src->bs_offscreen_src.bs_binding_count &&
        src->bs_offscreen_src.bs_binding_begin) {
      m->bs_offscreens.count = cnt->bs_offscreens;
    }
  }

  if (ver >= csmMocVersion_53) {
    m->offscreens.count = cnt->offscreens;
    m->offscreens.keydata.interp.object_count = cnt->offscreens;
  }

  if (ver >= csmMocVersion_42 && src->param_ext_src.key_runtime) {
    m->param_ext.keys = (psm__f32 **)src->param_ext_src.key_runtime;
    m->param_ext.key_counts = src->param_ext_src.keys_count;
  }

  /* Set up warp position pointers */
  {
    psm__f32 *pos = warp_pos_data;
    for (psm__i32 i = 0; i < cnt->warps; i++) {
      w->pos[i] = pos;
      psm__i32 vc = src->warp_src.vertex_count[i];
      pos = (psm__f32 *)((psm__u8 *)pos +
          psm__align_to_16(2 * sizeof(psm__f32) * vc));
    }
  }

  /* Set up art mesh position pointers */
  {
    psm__f32 *pos = am_pos_data;
    for (psm__i32 i = 0; i < cnt->art_meshes; i++) {
      am->pos[i] = pos;
      psm__i32 vc = src->art_mesh_src.vertex_count[i];
      pos = (psm__f32 *)((psm__u8 *)pos +
          psm__align_to_16(2 * sizeof(psm__f32) * vc));
    }
  }

  /* Set up keyform binding internal pointers */
  if (m && kb_ptrs && m->bindings.items) {
    struct psm__axis **bp = kb_ptrs;
    struct psm__axis **bp_end = kb_ptrs + kb_ptr_total;
    psm__i32 *ki = kb_indices;
    psm__f32 *kwt = kb_weights;

    for (psm__i32 i = 0; i < cnt->bindings; i++) {
      struct psm__binding *kc = &m->bindings.items[i];
      psm__i32 bc = src->binding_src.axis_idx_count[i];

      bc = psm__clamp_i32(bc, 0, PSM__MAX_AXES);
      psm__u32 mc = 1 << bc;

      /* Ensure bp doesn't exceed allocated buffer */
      psm__i32 remaining = (bp < bp_end) ? (psm__i32)(bp_end - bp) : 0;
      bc = psm__clamp_i32(bc, 0, remaining);

      kc->axes = bp;
      kc->keyform_idx = ki;
      kc->weights = kwt;
      kc->axis_count = bc;
      kc->max_blend = mc;

      bp += bc;
      ki += mc;
      kwt += mc;
    }
  }

  /* Set up draw order group items */
  if (cnt->draw_groups > 0 && do_items && src->draw_group_src.obj_count) {
    struct psm__draw_item *ip = do_items;
    for (psm__i32 i = 0; i < cnt->draw_groups; i++) {
      struct psm__draw_group *grp = &m->draw_groups.groups[i];
      psm__i32 count = src->draw_group_src.obj_count[i];
      if (!psm__valid_range(ip - do_items, count, cnt->draw_items)) {
        grp->items = NULL;
        grp->count = 0;
        continue;
      }
      grp->items = ip;
      grp->count = count;
      if (count > 0) ip += count;
    }
  }

  /* Set up blend shape constraint pointers */
  if (ver >= csmMocVersion_42 && bs_constr_ptrs &&
      src->blend_binding_src.bs_constraint_idx_count) {
    struct psm__blend_constraint **ptr = bs_constr_ptrs;
    for (psm__i32 i = 0; i < cnt->blend_bindings; i++) {
      struct psm__blend_binding *bb = &m->blend_bindings.items[i];
      psm__i32 cc = src->blend_binding_src.bs_constraint_idx_count[i];
      bb->constraints = ptr;
      bb->constraint_count = cc;
      ptr += cc;
    }
  }

  return m;
}


static void
psm__init_mul_color(psm__f32 *buf, psm__i32 count)
{
  for (psm__i32 i = 0; i < count; i++) {
    buf[i * 4 + 0] = 1.0f;
    buf[i * 4 + 1] = 1.0f;
    buf[i * 4 + 2] = 1.0f;
    buf[i * 4 + 3] = 1.0f;
  }
}

static void
psm__init_scr_color(psm__f32 *buf, psm__i32 count)
{
  for (psm__i32 i = 0; i < count; i++)
    buf[i * 4 + 3] = 1.0f;
}

static int
psm__init_model_data(struct psm__model *m, const struct psm__moc3_data *moc)
{
  psm__u8 ver = moc->header->version;
  struct psm__sections *ms = moc->sections;
  struct psm__count_info *cnt = ms->count_info;

  m->source = moc;
  m->y_reversed =
      (ms->canvas_info->flag & PSM__CANVAS_FLAG_Y_REVERSED) != 0;
  m->force_update = 1;

  /* Parameter bindings */
  if (cnt->axes > 0 && m->axes.items &&
      ms->axis_src.keys_count && ms->axis_src.keys_begin &&
      ms->keys_src.key) {
    for (psm__i32 i = 0; i < cnt->axes; i++) {
      struct psm__axis *c = &m->axes.items[i];
      psm__i32 key_cnt = ms->axis_src.keys_count[i];
      psm__i32 keyform_offset = ms->axis_src.keys_begin[i];
      c->key_count = key_cnt;
      c->out_of_range = 1;
      if (psm__valid_range(keyform_offset, key_cnt, cnt->keys))
        c->keys = &ms->keys_src.key[keyform_offset];
      else
        c->keys = NULL;
    }
  }

  /* Keyform bindings */
  if (cnt->bindings > 0 && m->bindings.items) {
    for (psm__i32 i = 0; i < cnt->bindings; i++) {
      struct psm__binding *kc = &m->bindings.items[i];
      psm__i32 bc = ms->binding_src.axis_idx_count[i];
      psm__i32 pb = ms->binding_src.axis_idx_begin[i];

      PSM__FAIL(!psm__valid_range(pb, bc,
              cnt->axis_indices), PSM__ERR_FILE_CORRUPT,
          "binding[%d] OOB: pb=%d bc=%d max=%d", i, pb, bc, cnt->axis_indices);

      kc->idx_dirty = 1;
      kc->weight_dirty = 1;
      kc->out_of_range = 1;

      if (!kc->axes)
        continue;

      for (psm__i32 j = 0; j < bc; j++) {
        psm__i32 idx = ms->axis_idx_src.index[pb + j];
        PSM__FAIL(!psm__valid_idx(idx, cnt->axes),
            PSM__ERR_FILE_CORRUPT, "binding[%d] ptr[%d] OOB: idx=%d max=%d",
            i, j, idx, cnt->axes);
        kc->axes[j] = &m->axes.items[idx];
      }
    }
  }

  /* Parts */
  if (cnt->parts > 0 && m->parts.items && m->bindings.items &&
      m->parts.keydata.interp.max_blend) {
    psm__i32 tmp_len = 0;
    for (psm__i32 i = 0; i < cnt->parts; i++) {
      struct psm__part *part = &m->parts.items[i];
      psm__i32 bi = ms->part_src.binding_idx[i];
      if (!psm__valid_idx(bi, cnt->bindings))
        continue;
      struct psm__binding *binding = &m->bindings.items[bi];

      part->binding = binding;
      part->parent_part_idx = ms->part_src.parent_part_idx[i];
      part->local_enable = ms->part_src.enable[i];

      m->parts.input_opacity[i] = ms->part_src.visible[i] ? 1.0f : 0.0f;

      if (ver < csmMocVersion_53)
        m->parts.offscreen_src_idx[i] = -1;

      psm__i32 mc = binding->max_blend;
      psm__i32 kb = ms->part_src.keyform_offset[i];
      if (!psm__valid_range(kb, mc, cnt->part_kf)) {
        part->binding = NULL;
        continue;
      }
      m->parts.keydata.interp.max_blend[i] = mc;
      tmp_len += mc;
    }
    m->parts.keydata.interp.tmp_len = tmp_len;
  }

  /* Parameters */
  if (cnt->parameters > 0 && m->params.items && m->params.input_value &&
      ms->param_src.minimum_value && ms->param_src.maximum_value &&
      ms->param_src.default_value && ms->param_src.repeat &&
      ms->param_src.decimal_places && ms->param_src.axis_begin &&
      ms->param_src.axis_count) {
    for (psm__i32 i = 0; i < cnt->parameters; i++) {
      struct psm__param *param = &m->params.items[i];

      if (ver >= csmMocVersion_42 && ms->param_src.type) {
        param->type = ms->param_src.type[i];
      } else {
        param->type = csmParameterType_Normal;
        if (ver < csmMocVersion_42)
          m->params.type[i] = csmParameterType_Normal;
      }

      param->range[0] = ms->param_src.minimum_value[i];
      param->range[1] = ms->param_src.maximum_value[i];
      param->range_length = param->range[1] - param->range[0];
      param->repeat = ms->param_src.repeat[i];

      psm__i32 dp = ms->param_src.decimal_places[i];
      param->snap_eps = powf(0.1f, (psm__f32)dp);
      param->interp_eps = param->snap_eps * 1.5f;

      psm__i32 axis_begin = ms->param_src.axis_begin[i];
      psm__i32 axis_count = ms->param_src.axis_count[i];

      {
        int rc = psm__valid_opt_range(axis_begin, axis_count, cnt->axes);
        if (rc < 0)
          PSM__LOGF("param[%d]: binding range "
              "[%d, %d) OOB (max %d)", i, axis_begin, axis_begin + axis_count,
              cnt->axes);
        if (rc == 1) {
          param->axes = &m->axes.items[axis_begin];
          param->axis_count = axis_count;
        } else {
          param->axes = NULL;
          param->axis_count = 0;
        }
      }

      param->value = ms->param_src.default_value[i];
      m->params.input_value[i] = param->value;
      param->dirty = 1;

      param->blend_axis_count = 0;
      param->blend_axes = NULL;
    }
  }

  /* Deformers */
  if (cnt->deformers > 0 && m->deformers.nodes && m->bindings.items) {
    for (psm__i32 i = 0; i < cnt->deformers; i++) {
      struct psm__deformer_node *node = &m->deformers.nodes[i];
      psm__i32 bi = ms->deformer_src.binding_idx[i];
      if (!psm__valid_idx(bi, cnt->bindings))
        continue;

      node->binding = &m->bindings.items[bi];
      node->parent_part_idx = ms->deformer_src.parent_part_idx[i];
      node->parent_deformer_idx = ms->deformer_src.parent_deformer_idx[i];
      node->type = ms->deformer_src.type[i];
      node->local_idx = ms->deformer_src.local_idx[i];
      node->local_enable = ms->deformer_src.enable[i];
    }
  }

  /* Warp deformers */
  {
    struct psm__warp_src *ws = &ms->warp_src;
    struct psm__interp *interp = &m->deformers.warps.keydata.interp;
    if (cnt->warps > 0 && m->deformers.warps.items &&
        m->bindings.items && interp->max_blend) {
      psm__i32 tmp_len = 0;
      for (psm__i32 i = 0; i < cnt->warps; i++) {
        struct psm__warp *warp = &m->deformers.warps.items[i];
        psm__i32 bi = ws->binding_idx[i];
        if (!psm__valid_idx(bi, cnt->bindings))
          continue;
        struct psm__binding *binding = &m->bindings.items[bi];

        warp->binding = binding;
        warp->row = ws->row[i];
        warp->column = ws->column[i];
        warp->vertex_count = ws->vertex_count[i];
        if (ver >= csmMocVersion_33 && ws->quad_transform)
          warp->quad_transform = ws->quad_transform[i];

        psm__i32 mc = binding->max_blend;
        psm__i32 kb = ws->keyform_offset[i];
        if (!psm__valid_range(kb, mc, cnt->warp_kf)) {
          warp->binding = NULL;
          continue;
        }
        interp->max_blend[i] = mc;
        tmp_len += mc;
      }
      interp->tmp_len = tmp_len;
    }
  }

  /* Rotation deformers */
  {
    struct psm__rotation_src *rs = &ms->rotation_src;
    struct psm__interp *interp = &m->deformers.rotations.keydata.interp;
    if (cnt->rotations > 0 && m->deformers.rotations.items &&
        m->bindings.items && interp->max_blend) {
      psm__i32 tmp_len = 0;
      for (psm__i32 i = 0; i < cnt->rotations; i++) {
        struct psm__rotation *rot = &m->deformers.rotations.items[i];
        psm__i32 bi = rs->binding_idx[i];
        if (!psm__valid_idx(bi, cnt->bindings))
          continue;
        struct psm__binding *binding = &m->bindings.items[bi];

        rot->binding = binding;
        rot->base_angle = rs->base_angle[i];

        psm__i32 mc = binding->max_blend;
        psm__i32 kb = rs->keyform_offset[i];
        if (!psm__valid_range(kb, mc, cnt->rotation_kf)) {
          rot->binding = NULL;
          continue;
        }
        interp->max_blend[i] = mc;
        tmp_len += mc;
      }
      interp->tmp_len = tmp_len;
    }
  }

  /* Art meshes */
  if (cnt->art_meshes > 0 && m->art_meshes.meshes && m->bindings.items &&
      m->art_meshes.keydata.interp.max_blend) {
    psm__i32 tmp_len = 0;
    for (psm__i32 i = 0; i < cnt->art_meshes; i++) {
      struct psm__art_mesh *mesh = &m->art_meshes.meshes[i];
      psm__i32 bi = ms->art_mesh_src.binding_idx[i];
      if (!psm__valid_idx(bi, cnt->bindings))
        continue;
      struct psm__binding *binding = &m->bindings.items[bi];

      mesh->binding = binding;
      mesh->parent_part_idx = ms->art_mesh_src.parent_part_idx[i];
      mesh->parent_deformer_idx = ms->art_mesh_src.parent_deformer_idx[i];
      mesh->vertex_count = ms->art_mesh_src.vertex_count[i];
      mesh->local_enable = ms->art_mesh_src.enable[i];
      m->art_meshes.const_flags[i] =
          ms->art_mesh_src.drawable_flag[i];

      if (ver < csmMocVersion_53) {
        psm__u8 flag = ms->art_mesh_src.drawable_flag[i];
        psm__i32 blend_mode = (flag & csmBlendMultiplicative)
                ? csmColorBlendType_MultiplyCompatible
                : csmColorBlendType_Normal;
        if (flag & csmBlendAdditive)
          blend_mode = csmColorBlendType_AddCompatible;
        m->art_meshes.blend_mode[i] = blend_mode;
      } else {
        psm__i32 blend_mode = psm__remap_blend_mode(
            ms->art_mesh_src.blend_mode[i]);
        m->art_meshes.blend_mode[i] = blend_mode;
#if PSM_COMPAT_VERSION < 0x06000000L
        /*
         * v5 callers read blend mode from constant flags
         * (csmGetDrawableBlendModes doesn't exist in v5).
         * Bake the remapped mode into const_flags.
         */
        psm__u8 *cf = &m->art_meshes.const_flags[i];
        *cf &= ~(csmBlendAdditive | csmBlendMultiplicative);
        if (blend_mode == csmColorBlendType_AddCompatible)
          *cf |= csmBlendAdditive;
        else if (blend_mode == csmColorBlendType_MultiplyCompatible)
          *cf |= csmBlendMultiplicative;
#endif
      }

      psm__i32 mc = binding->max_blend;
      psm__i32 kb = ms->art_mesh_src.keyform_offset[i];
      if (!psm__valid_range(kb, mc, cnt->art_mesh_kf)) {
        mesh->binding = NULL;
        continue;
      }
      m->art_meshes.keydata.interp.max_blend[i] = mc;
      tmp_len += mc;
    }
    m->art_meshes.keydata.interp.tmp_len = tmp_len;
  }

  /* Link keyform color data to MOC3 source (v4.2+). */
  if (ver >= csmMocVersion_42) {
    struct psm__warp_keydata *wk = &m->deformers.warps.keydata;
    struct psm__rotation_keydata *rk = &m->deformers.rotations.keydata;
    struct psm__art_mesh_keydata *ak = &m->art_meshes.keydata;
    {
      struct psm__key_color_src *mc = &ms->kf_mul_color_src;
      struct psm__key_color_src *sc = &ms->kf_scr_color_src;

      /* Warp deformer colors */
      if (mc->r && sc->r && wk->mul_color.r &&
          ms->warp_key_src.key_mul_color_offset &&
          ms->warp_key_src.key_scr_color_offset) {
        psm__i32 n = wk->interp.tmp_len;
        psm__i32 *mb = ms->warp_key_src.key_mul_color_offset;
        psm__i32 *sb = ms->warp_key_src.key_scr_color_offset;
        if (n > cnt->warp_kf)
          n = cnt->warp_kf;
        for (psm__i32 i = 0; i < n; i++) {
          psm__i32 mi = mb[i], si = sb[i];
          if (psm__valid_idx(mi, cnt->kf_mul_colors)) {
            wk->mul_color.r[i] = mc->r[mi];
            wk->mul_color.g[i] = mc->g[mi];
            wk->mul_color.b[i] = mc->b[mi];
          }
          if (psm__valid_idx(si, cnt->kf_scr_colors)) {
            wk->scr_color.r[i] = sc->r[si];
            wk->scr_color.g[i] = sc->g[si];
            wk->scr_color.b[i] = sc->b[si];
          }
        }
      }

      /* Rotation deformer colors */
      if (mc->r && sc->r && rk->mul_color.r &&
          ms->rotation_key_src.key_mul_color_offset &&
          ms->rotation_key_src.key_scr_color_offset) {
        psm__i32 n = rk->interp.tmp_len;
        psm__i32 *mb = ms->rotation_key_src.key_mul_color_offset;
        psm__i32 *sb = ms->rotation_key_src.key_scr_color_offset;
        if (n > cnt->rotation_kf)
          n = cnt->rotation_kf;
        for (psm__i32 i = 0; i < n; i++) {
          psm__i32 mi = mb[i], si = sb[i];
          if (psm__valid_idx(mi, cnt->kf_mul_colors)) {
            rk->mul_color.r[i] = mc->r[mi];
            rk->mul_color.g[i] = mc->g[mi];
            rk->mul_color.b[i] = mc->b[mi];
          }
          if (psm__valid_idx(si, cnt->kf_scr_colors)) {
            rk->scr_color.r[i] = sc->r[si];
            rk->scr_color.g[i] = sc->g[si];
            rk->scr_color.b[i] = sc->b[si];
          }
        }
      }

      /* Art mesh colors */
      if (mc->r && sc->r && ak->mul_color.r &&
          ms->art_mesh_key_src.key_mul_color_offset &&
          ms->art_mesh_key_src.key_scr_color_offset) {
        psm__i32 n = ak->interp.tmp_len;
        psm__i32 *mb = ms->art_mesh_key_src.key_mul_color_offset;
        psm__i32 *sb = ms->art_mesh_key_src.key_scr_color_offset;
        if (n > cnt->art_mesh_kf)
          n = cnt->art_mesh_kf;
        for (psm__i32 i = 0; i < n; i++) {
          psm__i32 mi = mb[i], si = sb[i];
          if (psm__valid_idx(mi, cnt->kf_mul_colors)) {
            ak->mul_color.r[i] = mc->r[mi];
            ak->mul_color.g[i] = mc->g[mi];
            ak->mul_color.b[i] = mc->b[mi];
          }
          if (psm__valid_idx(si, cnt->kf_scr_colors)) {
            ak->scr_color.r[i] = sc->r[si];
            ak->scr_color.g[i] = sc->g[si];
            ak->scr_color.b[i] = sc->b[si];
          }
        }
      }
    }
  }

  /* Draw order groups */
  {
    struct psm__draw_group_src *gs = &ms->draw_group_src;
    struct psm__draw_group_obj_src *os = &ms->draw_group_obj_src;
    if (cnt->draw_groups > 0 && m->draw_groups.groups &&
        gs->obj_total_count && gs->max_order && gs->min_order &&
        gs->obj_begin && os->type && os->index && os->self_group_idx) {
      for (psm__i32 i = 0; i < cnt->draw_groups; i++) {
        struct psm__draw_group *grp = &m->draw_groups.groups[i];
        psm__i32 max_order = gs->max_order[i];
        psm__i32 min_order = gs->min_order[i];
        psm__i32 begin_idx = gs->obj_begin[i];

        grp->total_count = gs->obj_total_count[i];
        grp->max_order = max_order;
        grp->min_order = min_order;
        grp->order_level = psm__safe_order_level(max_order, min_order);
        grp->cursor = 0;

        if (!psm__valid_range(begin_idx, grp->count, cnt->draw_items)) {
          PSM__LOGF("draw_order_group[%d]: "
              "range [%d, %d) OOB (max %d)", i, begin_idx,
              begin_idx + grp->count, cnt->draw_items);
          grp->count = 0;
        } else {
          for (psm__i32 j = 0; j < grp->count; j++) {
            struct psm__draw_item *item = &grp->items[j];
            item->object_type = os->type[begin_idx + j];
            item->object_idx = os->index[begin_idx + j];
            item->group_idx = os->self_group_idx[begin_idx + j];
            item->draw_order = 0;
          }
        }
      }
    }
  }

  /* Glues */
  {
    struct psm__glue_src *gls = &ms->glue_src;
    struct psm__glue_info_src *gis = &ms->glue_info_src;
    psm__i32 *max_combs = m->glues.keydata.interp.max_blend;
    if (cnt->glues > 0 && m->glues.items && m->bindings.items && max_combs &&
        gls->binding_idx && gls->art_mesh_index_a &&
        gls->art_mesh_index_b && gls->info_count && gls->info_begin &&
        gis->weight && gis->position_idx) {
      psm__i32 tmp_len = 0;
      for (psm__i32 i = 0; i < cnt->glues; i++) {
        struct psm__glue *glue = &m->glues.items[i];
        psm__i32 bi = gls->binding_idx[i];
        if (!psm__valid_idx(bi, cnt->bindings)) {
          glue->binding = NULL;
          continue;
        }
        struct psm__binding *binding = &m->bindings.items[bi];
        psm__i32 info_begin = gls->info_begin[i];

        glue->binding = binding;
        glue->mesh_idx0 = gls->art_mesh_index_a[i];
        glue->mesh_idx1 = gls->art_mesh_index_b[i];
        glue->glue_info_count = gls->info_count[i];

        if (!psm__valid_range(info_begin, glue->glue_info_count,
                cnt->glue_info)) {
          PSM__LOGF("glue[%d]: info range "
              "[%d, %d) OOB (max %d)", i, info_begin,
              info_begin + glue->glue_info_count, cnt->glue_info);
          glue->weights = NULL;
          glue->pos_idx = NULL;
          glue->glue_info_count = 0;
        } else {
          glue->weights = &gis->weight[info_begin];
          glue->pos_idx = &gis->position_idx[info_begin];
        }

        psm__i32 mc = binding->max_blend;
        psm__i32 kb = gls->keyform_offset[i];
        if (!psm__valid_range(kb, mc, cnt->glue_kf)) {
          glue->binding = NULL;
          continue;
        }
        max_combs[i] = mc;
        tmp_len += mc;
      }
      m->glues.keydata.interp.tmp_len = tmp_len;
    }
  }

  /* BlendShape (v4.2+) */
  if (ver >= csmMocVersion_42) {
    struct psm__blend_constraint_src *bscs = &ms->blend_constraint_src;
    struct psm__blend_constraint_val_src
        *bsvs = &ms->blend_constraint_val_src;
    if (bscs->parameter_idx && bscs->value_begin &&
        bscs->value_count && bsvs->key && bsvs->weight) {
      for (psm__i32 i = 0; i < cnt->bs_constraints; i++) {
        struct psm__blend_constraint *constr = &m->blend_constraints.items[i];
        psm__i32 pi = bscs->parameter_idx[i];
        psm__i32 vb = bscs->value_begin[i];
        psm__i32 vc = bscs->value_count[i];
        if (!psm__valid_idx(pi, cnt->parameters)) {
          constr->param = NULL;
          constr->keys = NULL;
          constr->weights = NULL;
          constr->count = 0;
          constr->weight = 1.0f;
          continue;
        }
        if (!psm__valid_range(vb, vc, cnt->bs_constraint_vals)) {
          constr->param = &m->params.items[pi];
          constr->keys = NULL;
          constr->weights = NULL;
          constr->count = 0;
          constr->weight = 1.0f;
          continue;
        }
        constr->param = &m->params.items[pi];
        constr->keys = &bsvs->key[vb];
        constr->weights = &bsvs->weight[vb];
        constr->count = vc;
        constr->weight = 1.0f;
      }
    }

    struct psm__blend_axis_src *ba_src = &ms->blend_axis_src;
    if (ba_src->keys_count && ba_src->keys_begin && ba_src->base_key_idx) {
      for (psm__i32 i = 0; i < cnt->blend_axes; i++) {
        struct psm__blend_axis *ba = &m->blend_axes.items[i];
        psm__i32 kc = ba_src->keys_count[i];
        psm__i32 kb = ba_src->keys_begin[i];
        if (psm__valid_range(kb, kc, cnt->keys)) {
          ba->key_count = kc;
          ba->keys = &ms->keys_src.key[kb];
        } else {
          PSM__LOGF("blend_pb[%d]: key range "
              "[%d, %d) OOB (max %d)", i, kb, kb + kc, cnt->keys);
          ba->keys = NULL;
          ba->key_count = 0;
        }
        ba->base_key_idx = ba_src->base_key_idx[i];
        ba->idx = 0;
        ba->weight = 0.0f;
        ba->idx_dirty = 1;
        ba->weight_dirty = 1;
      }
    }

    for (psm__i32 i = 0; i < cnt->parameters; i++) {
      struct psm__param *pc = &m->params.items[i];
      if (ms->param_src.blend_axis_count &&
          ms->param_src.blend_axis_begin &&
          m->blend_axes.items) {
        psm__i32 bs_cnt = ms->param_src.blend_axis_count[i];
        psm__i32 bs_begin = ms->param_src.blend_axis_begin[i];
        if (psm__valid_opt_range(bs_begin,
                bs_cnt, cnt->blend_axes) == 1) {
          pc->blend_axis_count = bs_cnt;
          pc->blend_axes = &m->blend_axes.items[bs_begin];
        }
      }
    }

    struct psm__blend_binding_src *bb_src = &ms->blend_binding_src;
    if (bb_src->axis_idx && bb_src->key_bs_begin) {
      for (psm__i32 i = 0; i < cnt->blend_bindings; i++) {
        struct psm__blend_binding *bb = &m->blend_bindings.items[i];
        psm__i32 bpi = bb_src->axis_idx[i];
        if (m->blend_axes.items && psm__valid_idx(bpi,
                cnt->blend_axes))
          bb->axis = &m->blend_axes.items[bpi];
        else
          bb->axis = NULL;
        bb->key_src_offset = bb_src->key_bs_begin[i];
        bb->blend_count = 0;
        bb->idx_dirty = 1;
        bb->weight_dirty = 1;
        bb->weight = 1.0f;

        psm__i32 cc = bb->constraint_count;
        if (cc > 0 && bb->constraints && bb_src->bs_constraint_idx_begin &&
            ms->blend_constraint_idx_src.constraint_idx) {
          psm__i32 cb = bb_src->bs_constraint_idx_begin[i];
          for (psm__i32 j = 0; j < cc; j++) {
            psm__i32 ci = ms->blend_constraint_idx_src.constraint_idx[cb + j];
            if (psm__valid_idx(ci, cnt->bs_constraints))
              bb->constraints[j] = &m->blend_constraints.items[ci];
            else
              bb->constraints[j] = NULL;
          }
        }
      }
    }

    if (ms->bs_warp_src.target_idx && ms->bs_warp_src.bs_binding_count &&
        ms->bs_warp_src.bs_binding_begin) {
      for (psm__i32 i = 0; i < cnt->bs_warps; i++) {
        struct psm__blend_shape *shape = &m->bs_warps.items[i];
        shape->target_idx = ms->bs_warp_src.target_idx[i];
        shape->axis_count = ms->bs_warp_src.bs_binding_count[i];
        psm__i32 bb = ms->bs_warp_src.bs_binding_begin[i];
        if (psm__valid_range(bb, shape->axis_count, cnt->blend_bindings))
          shape->bindings = &m->blend_bindings.items[bb];
        else
          shape->bindings = NULL;
      }
    }

    if (ms->bs_art_mesh_src.target_idx &&
        ms->bs_art_mesh_src.bs_binding_count &&
        ms->bs_art_mesh_src.bs_binding_begin) {
      for (psm__i32 i = 0; i < cnt->bs_art_meshes; i++) {
        struct psm__blend_shape *shape = &m->bs_art_meshes.items[i];
        shape->target_idx = ms->bs_art_mesh_src.target_idx[i];
        shape->axis_count = ms->bs_art_mesh_src.bs_binding_count[i];
        psm__i32 bb = ms->bs_art_mesh_src.bs_binding_begin[i];
        if (psm__valid_range(bb, shape->axis_count, cnt->blend_bindings))
          shape->bindings = &m->blend_bindings.items[bb];
        else
          shape->bindings = NULL;
      }
    }
  }

  /* BlendShape (v5.0+) */
  if (ver >= csmMocVersion_50) {
    if (ms->bs_part_src.target_idx && ms->bs_part_src.bs_binding_count &&
        ms->bs_part_src.bs_binding_begin) {
      for (psm__i32 i = 0; i < cnt->bs_parts; i++) {
        struct psm__blend_shape *shape = &m->bs_parts.items[i];
        shape->target_idx = ms->bs_part_src.target_idx[i];
        shape->axis_count = ms->bs_part_src.bs_binding_count[i];
        psm__i32 bb = ms->bs_part_src.bs_binding_begin[i];
        if (psm__valid_range(bb, shape->axis_count, cnt->blend_bindings))
          shape->bindings = &m->blend_bindings.items[bb];
        else
          shape->bindings = NULL;
      }
    }

    if (ms->bs_rotation_src.target_idx &&
        ms->bs_rotation_src.bs_binding_count &&
        ms->bs_rotation_src.bs_binding_begin) {
      for (psm__i32 i = 0; i < cnt->bs_rotations; i++) {
        struct psm__blend_shape *shape = &m->bs_rotations.items[i];
        shape->target_idx = ms->bs_rotation_src.target_idx[i];
        shape->axis_count = ms->bs_rotation_src.bs_binding_count[i];
        psm__i32 bb = ms->bs_rotation_src.bs_binding_begin[i];
        if (psm__valid_range(bb, shape->axis_count, cnt->blend_bindings))
          shape->bindings = &m->blend_bindings.items[bb];
        else
          shape->bindings = NULL;
      }
    }

    if (ms->bs_glue_src.target_idx && ms->bs_glue_src.bs_binding_count &&
        ms->bs_glue_src.bs_binding_begin) {
      for (psm__i32 i = 0; i < cnt->bs_glues; i++) {
        struct psm__blend_shape *shape = &m->bs_glues.items[i];
        shape->target_idx = ms->bs_glue_src.target_idx[i];
        shape->axis_count = ms->bs_glue_src.bs_binding_count[i];
        psm__i32 bb = ms->bs_glue_src.bs_binding_begin[i];
        if (psm__valid_range(bb, shape->axis_count, cnt->blend_bindings))
          shape->bindings = &m->blend_bindings.items[bb];
        else
          shape->bindings = NULL;
      }
    }
  }

  /* Offscreen rendering (v5.3+) */
  if (ver >= csmMocVersion_53) {
    if (cnt->offscreens > 0 && ms->offscreen_src.owner_idx) {
      psm__i32 tmp_len = 0;
      psm__i32 *os_key_idx = ms->part_key_src.key_idx;

      for (psm__i32 i = 0; i < cnt->offscreens; i++) {
        struct psm__offscreen *surf = &m->offscreens.surfaces[i];
        psm__i32 oi = ms->offscreen_src.owner_idx[i];

        if (!psm__valid_idx(oi, cnt->parts)) {
          surf->binding = NULL;
          surf->owner_enable = NULL;
          surf->keyform_idx = NULL;
          m->offscreens.keydata.interp.max_blend[i] = 0;
          continue;
        }

        /* Check source arrays */
        if (!ms->part_src.binding_idx || !ms->part_src.keyform_offset) {
          surf->binding = NULL;
          surf->owner_enable = NULL;
          surf->keyform_idx = NULL;
          m->offscreens.keydata.interp.max_blend[i] = 0;
          continue;
        }

        psm__i32 pbi = ms->part_src.binding_idx[oi];
        psm__i32 kbi = ms->part_src.keyform_offset[oi];

        if (psm__valid_opt_idx(pbi, cnt->bindings) == 1)
          surf->binding = &m->bindings.items[pbi];
        else
          surf->binding = NULL;

        if (m->parts.enable)
          surf->owner_enable = &m->parts.enable[oi];
        else
          surf->owner_enable = NULL;

        /* Set offscreen keyform index pointer */
        if (os_key_idx && kbi >= 0 && kbi < cnt->part_kf) {
          surf->keyform_idx = &os_key_idx[kbi];
        } else {
          surf->keyform_idx = NULL;
        }

        psm__i32 mc = surf->binding ? surf->binding->max_blend : 0;
        m->offscreens.keydata.interp.max_blend[i] = mc;
        tmp_len += mc;
      }
      m->offscreens.keydata.interp.tmp_len = tmp_len;

      for (psm__i32 i = 0; i < 4 * cnt->offscreens; i++) {
        m->offscreens.mul_color[i] = 1.0f;
        m->offscreens.scr_color[i] = 1.0f;
      }
    } else {
      m->offscreens.keydata.interp.tmp_len = 0;
    }

    if (ms->bs_offscreen_src.target_idx &&
        ms->bs_offscreen_src.bs_binding_count &&
        ms->bs_offscreen_src.bs_binding_begin) {
      for (psm__i32 i = 0; i < cnt->bs_offscreens; i++) {
        struct psm__blend_shape *shape = &m->bs_offscreens.items[i];
        shape->target_idx = ms->bs_offscreen_src.target_idx[i];
        shape->axis_count = ms->bs_offscreen_src.bs_binding_count[i];
        psm__i32 bb = ms->bs_offscreen_src.bs_binding_begin[i];
        if (psm__valid_range(bb, shape->axis_count, cnt->blend_bindings))
          shape->bindings = &m->blend_bindings.items[bb];
        else
          shape->bindings = NULL;
      }
    }
  }

  /* Parameter extensions */
  if (ver >= csmMocVersion_42 && ms->param_ext_src.key_runtime) {
    if (ms->keys_src.key && ms->param_ext_src.keys_begin) {
      for (psm__i32 i = 0; i < cnt->parameters; i++) {
        psm__i32 kb = ms->param_ext_src.keys_begin[i];
        if (kb < 0 || kb > cnt->keys) {
          PSM__LOGF("param_ext[%d]: keyform_offset %d OOB (max %d)",
              i, kb, cnt->keys);
          m->param_ext.keys[i] = NULL;
        } else {
          m->param_ext.keys[i] = kb < cnt->keys ? &ms->keys_src.key[kb] : NULL;
        }
      }
    }
  } else {
    for (psm__i32 i = 0; i < cnt->parameters; i++) {
      psm__i32 axis_begin = ms->param_src.axis_begin[i];
      psm__i32 axis_count = ms->param_src.axis_count[i];

      if (axis_begin < 0 || axis_count <= 0 || !ms->keys_src.key ||
          !ms->axis_src.keys_begin || !ms->axis_src.keys_count) {
        m->param_ext.keys[i] = NULL;
        m->param_ext.key_counts[i] = 0;
        continue;
      }
      if (!psm__valid_range(axis_begin, axis_count, cnt->axes)) {
        PSM__LOGF("param_ext fallback[%d]: axis_begin %d OOB (max %d)",
            i, axis_begin, cnt->axes);
        m->param_ext.keys[i] = NULL;
        m->param_ext.key_counts[i] = 0;
        continue;
      }
      psm__i32 best = axis_begin, best_kc = 0;
      for (psm__i32 j = 0; j < axis_count; j++) {
        psm__i32 idx = axis_begin + j;
        if (idx < cnt->axes && ms->axis_src.keys_count[idx]
                > best_kc) {
          best_kc = ms->axis_src.keys_count[idx];
          best = idx;
        }
      }
      psm__i32 fkb = ms->axis_src.keys_begin[best];
      if (!psm__valid_range(fkb, best_kc, cnt->keys)) {
        PSM__LOGF("param_ext fallback[%d]: key range [%d, %d) OOB (max %d)",
            i, fkb, fkb + best_kc, cnt->keys);
        m->param_ext.keys[i] = NULL;
        m->param_ext.key_counts[i] = 0;
        continue;
      }
      m->param_ext.keys[i] = &ms->keys_src.key[fkb];
      m->param_ext.key_counts[i] = best_kc;
    }
  }

  /* Render orders */
  for (psm__i32 i = 0; i < cnt->art_meshes; i++)
    m->render_order[i] = i;

  /* Color defaults: mul=(1,1,1,1), scr alpha=1 */
  psm__i32 dc = cnt->deformers;
  psm__i32 wc = cnt->warps;
  psm__i32 rc = cnt->rotations;
  psm__i32 ac = cnt->art_meshes;
  psm__i32 oc = cnt->offscreens;

  if (m->deformers.mul_color)
    psm__init_mul_color(m->deformers.mul_color, dc);
  if (m->deformers.scr_color)
    psm__init_scr_color(m->deformers.scr_color, dc);
  if (m->deformers.warps.mul_color)
    psm__init_mul_color(m->deformers.warps.mul_color, wc);
  if (m->deformers.warps.scr_color)
    psm__init_scr_color(m->deformers.warps.scr_color, wc);
  if (m->deformers.rotations.mul_color)
    psm__init_mul_color(m->deformers.rotations.mul_color, rc);
  if (m->deformers.rotations.scr_color)
    psm__init_scr_color(m->deformers.rotations.scr_color, rc);
  if (m->art_meshes.mul_color)
    psm__init_mul_color(m->art_meshes.mul_color, ac);
  if (m->art_meshes.scr_color)
    psm__init_scr_color(m->art_meshes.scr_color, ac);
  if (m->art_meshes.last_mul_color)
    psm__init_mul_color(m->art_meshes.last_mul_color, ac);
  if (m->art_meshes.last_scr_color)
    psm__init_scr_color(m->art_meshes.last_scr_color, ac);
  if (m->offscreens.mul_color)
    psm__init_mul_color(m->offscreens.mul_color, oc);
  if (m->offscreens.scr_color)
    psm__init_scr_color(m->offscreens.scr_color, oc);

  return PSM__OK;
}

static int
psm__model_size(psm_size *out, const struct psm__moc3_data *moc)
{
  struct psm__arena arena = PSM__ARENA_INIT(NULL, 0);
  psm__alloc_model(&arena, moc->header->version, moc->sections,
      moc->sections->count_info);
  if (!psm__arena_ok(&arena))
    return PSM__ERR_FILE_CORRUPT;
  *out = psm__arena_total(&arena);
  return PSM__OK;
}

static int
psm__init_model(struct psm__model **out,
    const struct psm__moc3_data *moc, void *p, psm_size n)
{
  /* Dry run to calculate required size */
  struct psm__arena arena = PSM__ARENA_INIT(NULL, 0);
  psm__alloc_model(&arena, moc->header->version, moc->sections,
      moc->sections->count_info);
  PSM__FAILM(!psm__arena_ok(&arena), PSM__ERR_FILE_CORRUPT,
      "allocation size overflow");
  psm__u32 required = psm__arena_total(&arena);

  PSM__FAIL(n < required, PSM__ERR_INVALID_DATA,
      "insufficient memory (%u < %u)", (unsigned)n, required);

  /* Zero the buffer, then allocate and assign pointers */
  memset(p, 0, n);
  arena = PSM__ARENA_INIT(p, n);
  struct psm__model *m = psm__alloc_model(&arena,
      moc->header->version, moc->sections, moc->sections->count_info);
  PSM__FAILM(!m || !psm__arena_ok(&arena), PSM__ERR_FILE_CORRUPT,
      "arena allocation failed");

  PSM__FAILM(psm__init_model_data(m, moc) != PSM__OK,
      PSM__ERR_FILE_CORRUPT, "model data init failed");
  psm__update_model(m);
  *out = m;
  return PSM__OK;
}


PSMDEF unsigned int
csmGetSizeofModel(const csmMoc *moc)
{
  psm_size size;
  PSM__FAILM(psm__model_size(&size, psm__moc_to_data(moc)) != PSM__OK,
      0, "could not get model size");
  return size;
}

PSMDEF csmModel *
csmInitializeModelInPlace(const csmMoc *moc, void *address, unsigned int size)
{
  struct psm__model *model;
  PSM__FAILM(psm__init_model(&model, psm__moc_to_data(moc),
      address, size) != PSM__OK, NULL, "could not init model");
  return (csmModel *)model;
}

PSMDEF void
csmReadCanvasInfo(const csmModel *model, csmVector2 *outSizeInPixels,
    csmVector2 *outOriginInPixels, float *outPixelsPerUnit)
{
  const struct psm__model *m = (const struct psm__model *)model;
  const struct psm__canvas_info *c = m->source->sections->canvas_info;
  outSizeInPixels->X = c->width;
  outSizeInPixels->Y = c->height;
  outOriginInPixels->X = c->origin_x;
  outOriginInPixels->Y = c->origin_y;
  *outPixelsPerUnit = c->pix_per_unit;
}

