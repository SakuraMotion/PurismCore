/*
 * Purism Core: update pipeline
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "private.h"
#include "error.h"
#include "debug.h"
#include "math2.h"
#include "moc3.h"
#include "model.h"
#include "update.h"
#include "param.h"
#include "part.h"
#include "interpolate.h"
#include "deformer.h"
#include "artmesh.h"
#include "glue.h"
#include "blendshape.h"
#include "offscreen.h"
#include "render.h"

static void
psm__reverse_y(struct psm__model *m)
{
  if (m->y_reversed)
    return;

  psm__i32 count = m->art_meshes.count;
  if (count <= 0)
    return;

  struct psm__art_mesh *meshes = m->art_meshes.meshes;

  bool      *en = m->art_meshes.enable;
  psm__f32 **pos = m->art_meshes.pos;

  for (psm__i32 i = 0; i < count; i++) {
    if (!en[i])
      continue;
    psm__i32 vc = meshes[i].vertex_count;
    if (vc <= 0)
      continue;
    psm__f32 *p = pos[i];
    for (psm__i32 j = 0; j < vc; j++)
      p[2 * j + 1] = -p[2 * j + 1];
  }
}

static void
psm__save_flags(struct psm__model *m)
{
  struct psm__art_meshes *am = &m->art_meshes;
  if (!am->state_changed)
    return;

  psm__u8  ver = m->source->header->version;
  psm__i32 n = am->count;

  memcpy(am->last_render_order, m->render_order, sizeof(psm__i32) * n);
  memcpy(am->last_draw_order, am->draw_order, sizeof(psm__i32) * n);
  memcpy(am->last_opacity, am->opacity, sizeof(psm__f32) * n);

  if (ver < csmMocVersion_42)
    return;

  memcpy(am->last_mul_color, am->mul_color, 4 * sizeof(psm__f32) * n);
  memcpy(am->last_scr_color, am->scr_color, 4 * sizeof(psm__f32) * n);
}

static void
psm__update_flags(struct psm__model *m)
{
  struct psm__art_meshes *am = &m->art_meshes;

  psm__u8  ver = m->source->header->version;
  psm__i32 n = am->count;

  /* Force update: mark everything dirty */
  if (m->force_update) {
    am->state_changed = 0;
    if (n <= 0)
      return;

    bool     *en = am->enable;
    psm__f32 *opacity = am->opacity;
    psm__u8  *df = am->change_flags;

    for (psm__i32 i = 0; i < n; i++) {
      if (!en[i] || opacity[i] == 0.0f)
        df[i] = PSM__FLAG_ALL_CHANGED;
      else
        df[i] = PSM__FLAG_ALL;
    }
    return;
  }

  /* Normal dirty update: compare previous vs current */
  if (am->state_changed) {
    am->state_changed = 0;
    if (n <= 0)
      return;

    bool     *en = am->enable;
    psm__f32 *opacity = am->opacity;
    psm__u8  *df = am->change_flags;
    psm__i32 *ro = m->render_order;
    psm__i32 *last_ro = am->last_render_order;
    psm__i32 *cdo = am->draw_order;
    psm__i32 *last_do = am->last_draw_order;
    psm__f32 *last_op = am->last_opacity;

    psm__f32 *mc = NULL, *lmc = NULL;
    psm__f32 *sc = NULL, *lsc = NULL;
    if (ver >= csmMocVersion_42) {
      mc = am->mul_color;
      lmc = am->last_mul_color;
      sc = am->scr_color;
      lsc = am->last_scr_color;
    }

    for (psm__i32 i = 0; i < n; i++) {
      psm__i32 visible = en[i] && opacity[i] != 0.0f;
      psm__i32 was = (df[i] & PSM__FLAG_IS_VISIBLE) != 0;
      psm__u8  flags = visible;

      if (visible != was)
        flags |= PSM__FLAG_VISIBILITY_CHANGED;
      if (opacity[i] != last_op[i])
        flags |= PSM__FLAG_OPACITY_CHANGED;
      if (cdo[i] != last_do[i])
        flags |= PSM__FLAG_DRAW_ORDER_CHANGED;
      if (ro[i] != last_ro[i])
        flags |= PSM__FLAG_RENDER_ORDER_CHANGED;
      if (en[i])
        flags |= PSM__FLAG_VERTEX_CHANGED;

      if (mc && (memcmp(&mc[i * 4], &lmc[i * 4], 16) != 0 ||
                    memcmp(&sc[i * 4], &lsc[i * 4], 16) != 0))
        flags |= PSM__FLAG_BLEND_COLOR_CHANGED;

      df[i] = flags;
    }
    return;
  }

  /* No dirty update: just refresh visibility bit */
  if (n <= 0)
    return;

  bool     *en = am->enable;
  psm__f32 *opacity = am->opacity;
  psm__u8  *df = am->change_flags;

  for (psm__i32 i = 0; i < n; i++) {
    if (!en[i] || opacity[i] == 0.0f)
      df[i] &= ~PSM__FLAG_IS_VISIBLE;
    else
      df[i] |= PSM__FLAG_IS_VISIBLE;
  }
}

PSM__DEF void
psm__update_model(struct psm__model *m)
{
  m->last_error = PSM__OK;
  psm__save_flags(m);

  if (psm__resolve_params(&m->params))
    m->last_error = PSM__ERR_PARAMETER_RANGE_ERROR;
  psm__resolve_key_tables(m);
  psm__resolve_blend_key_tables(m);
  psm__resolve_bindings(m);
  psm__resolve_blend_bindings(m);

  psm__i32 nparts = m->parts.count;
  if (nparts > 0) {
    psm__f32 *opa = m->parts.input_opacity;
    for (psm__i32 i = 0; i < nparts; i++)
      opa[i] = psm__clamp_f32_01(opa[i]);
  }

  psm__enable_parts(m);
  psm__gather_parts(m);
  psm__interp_parts(m);

  psm__enable_deformers(m);
  psm__gather_warps(m);
  psm__gather_rotations(m);
  psm__interp_warps(m);
  psm__interp_rotations(m);

  psm__enable_art_meshes(m);
  psm__gather_art_meshes(m);
  psm__interp_art_meshes(m);

  psm__gather_glues(m);
  psm__interp_glues(m);

  psm__enable_offscreens(m);
  psm__gather_offscreens(m);
  psm__interp_offscreens(m);

  psm__blend_parts(m);
  psm__blend_warps(m);
  psm__blend_rotations(m);
  psm__blend_art_meshes(m);
  psm__blend_glues(m);
  psm__blend_offscreens(m);

  psm__apply_transforms(m);
  psm__apply_transforms_to_meshes(m);
  psm__apply_part_opacity(m);
  psm__apply_parts_to_meshes(m);

  psm__apply_glues(m); /* must come before reverse_y! */

  psm__reverse_y(m);
  psm__sort_render_order(m);
  psm__update_flags(m);

  /* v6+: zero opacity for disabled offscreen surfaces */
  if (m->source->header->version >= csmMocVersion_53) {
    psm__i32  oc = m->offscreens.count;
    bool     *en = m->offscreens.enable;
    psm__f32 *opa = m->offscreens.opacity;
    if (oc > 0 && en && opa) {
      for (psm__i32 i = 0; i < oc; i++) {
        if (!en[i])
          opa[i] = 0.0f;
      }
    }
  }

  m->force_update = 0;
}

PSMDEF void
csmUpdateModel(csmModel *model)
{
  psm__update_model((struct psm__model *)model);
}

PSMDEF void
csmResetDrawableDynamicFlags(csmModel *model)
{
  struct psm__model *m = (struct psm__model *)model;
  psm__i32           count = m->art_meshes.count;
  psm__u8           *flags = m->art_meshes.change_flags;
  for (psm__i32 i = 0; i < count; i++)
    flags[i] &= PSM__FLAG_IS_VISIBLE;
  m->art_meshes.state_changed = 1;
}
