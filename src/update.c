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

/* ------------------------------------------------------------------ */
/* Lightweight per-stage profiler (PSM_PROFILE=1 to enable).           */
/* Every 300 updates appends one line to psm_profile.log showing the   */
/* average milliseconds per stage, to locate update-pipeline hotspots. */
/* ------------------------------------------------------------------ */
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <stdio.h>
#endif

enum {
  PSM__PROF_SAVE_FLAGS = 0,
  PSM__PROF_PARAMS,
  PSM__PROF_BLEND_MASKS,
  PSM__PROF_PARTS,
  PSM__PROF_DEFORMERS,
  PSM__PROF_ART_MESHES,
  PSM__PROF_GLUES,
  PSM__PROF_OFFSCREENS,
  PSM__PROF_GLUE_APPLY,
  PSM__PROF_REVERSE_Y,
  PSM__PROF_SORT_RENDER,
  PSM__PROF_UPDATE_FLAGS,
  PSM__PROF_N
};

struct psm__profiler {
  int checked;
  int enabled;
  long long freq;
  long long t0;
  long long acc[PSM__PROF_N];
  long long calls;
};

static struct psm__profiler s_prof;

/* Reset the interval clock at the start of an update so the gap between
 * updates (framework draw time etc.) is never attributed to a stage. */
static void
psm__prof_reset(struct psm__profiler *p)
{
  if (!p->checked) {
    p->checked = 1;
    p->enabled = 1;
#if defined(_WIN32)
    if (GetEnvironmentVariableA("PSM_PROFILE", NULL, 0) == 0)
      p->enabled = 0;
    if (p->enabled)
      QueryPerformanceFrequency((LARGE_INTEGER *)&p->freq);
#endif
  }
  if (!p->enabled)
    return;
#if defined(_WIN32)
  QueryPerformanceCounter((LARGE_INTEGER *)&p->t0);
#endif
}

static void
psm__prof_tick(struct psm__profiler *p, int stage)
{
  if (!p->checked) {
    p->checked = 1;
    p->enabled = 1; /* on by default in builds where timing is cheap */
#if defined(_WIN32)
    if (GetEnvironmentVariableA("PSM_PROFILE", NULL, 0) == 0)
      p->enabled = 0;
    if (p->enabled)
      QueryPerformanceFrequency((LARGE_INTEGER *)&p->freq);
#endif
    if (!p->enabled)
      return;
    p->t0 = 0;
  }
  if (!p->enabled)
    return;

#if defined(_WIN32)
  long long now;
  QueryPerformanceCounter((LARGE_INTEGER *)&now);
  if (p->t0)
    p->acc[stage] += now - p->t0;
  p->t0 = now;

  /* The UPDATE_FLAGS tick is the last stage of every update, so it is a
   * reliable once-per-update point for flushing the accumulated stats. */
  if (stage == PSM__PROF_UPDATE_FLAGS) {
    p->calls++;
    if (p->calls % 300 == 0) {
      FILE *f = fopen("psm_profile.log", "a");
      if (f) {
        double total = 0.0;
        fprintf(f, "updates=300");
        for (int i = 0; i < PSM__PROF_N; i++) {
          double ms = (double)p->acc[i] * 1000.0 /
              ((double)p->freq * 300.0);
          total += ms;
          fprintf(f, " s%d=%.2f", i, ms);
          p->acc[i] = 0;
        }
        fprintf(f, " total=%.2f\n", total);
        fclose(f);
      }
    }
  }
#endif
}

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
  psm__u8   *changed = m->mesh_changed;
  psm__f32 **pos = m->art_meshes.pos;

  /* Flip only meshes recomputed this frame; clean meshes already hold
   * flipped final positions from the previous frame. */
  for (psm__i32 i = 0; i < count; i++) {
    if (!en[i] || !changed[i])
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
      /* Only flag VERTEX_CHANGED for meshes actually recomputed this
       * frame (mesh_changed set in psm__process_art_meshes, which runs
       * before psm__update_flags). Clean meshes keep their previous
       * final positions, so their vertex buffers are already current and
       * need no re-upload / mask re-draw. */
      if (en[i] && m->mesh_changed[i])
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

/*
 * Per-frame dirty scan: glue_mesh_dirty[t] = 1 when a dirty glue
 * (intensity key data moved, or a blend shape targeting the glue is
 * dirty) involves art mesh t. Dirty meshes are forced to recompute so
 * their positions are rebuilt to the pre-glue state before the glue is
 * re-applied (the glue contribution is stateful / additive).
 */
static void
psm__glue_mesh_dirty(struct psm__model *m)
{
  psm__i32 nmesh = m->art_meshes.count;
  psm__u8 *gmd = m->glue_mesh_dirty;
  if (nmesh <= 0 || !gmd)
    return;
  memset(gmd, 0, (size_t)nmesh);

  psm__i32 nglue = m->glues.count;
  if (nglue <= 0 || !m->glues.items)
    return;

  struct psm__glue *items = m->glues.items;
  psm__u8 *gb = m->glue_bs_dirty;

  for (psm__i32 gi = 0; gi < nglue; gi++) {
    struct psm__glue *glue = &items[gi];
    struct psm__binding *b = glue->binding;
    psm__i32 self = b ? (b->idx_dirty || b->weight_dirty) : 1;
    if (!self && (!gb || !gb[gi]))
      continue;
    psm__i32 m0 = glue->mesh_idx0, m1 = glue->mesh_idx1;
    if (m0 >= 0 && m0 < nmesh)
      gmd[m0] = 1;
    if (m1 >= 0 && m1 < nmesh)
      gmd[m1] = 1;
  }
}

PSM__DEF void
psm__update_model(struct psm__model *m)
{
  int r = PSM__OK;

  m->last_error = r;
  psm__prof_reset(&s_prof);
  psm__save_flags(m);
  psm__prof_tick(&s_prof, PSM__PROF_SAVE_FLAGS);

  if ((r = psm__resolve_params(&m->params)) != PSM__OK)
    m->last_error = r;
  psm__resolve_key_tables(m);
  psm__resolve_blend_key_tables(m);
  psm__resolve_bindings(m);
  psm__resolve_blend_bindings(m);

  /* Dirty-update: per-target blend-shape dirty masks and glue->mesh
   * propagation. These feed the fused per-object stages below. */
  psm__blend_targets_dirty(&m->bs_warps, m->deformers.warps.count,
      m->warp_blend_dirty);
  psm__blend_targets_dirty(&m->bs_rotations, m->deformers.rotations.count,
      m->rot_blend_dirty);
  psm__blend_targets_dirty(&m->bs_art_meshes, m->art_meshes.count,
      m->mesh_blend_dirty);
  psm__blend_targets_dirty(&m->bs_glues, m->glues.count, m->glue_bs_dirty);
  psm__blend_targets_dirty(&m->bs_offscreens, m->offscreens.count,
      m->offscreen_blend_dirty);
  psm__glue_mesh_dirty(m);
  psm__prof_tick(&s_prof, PSM__PROF_BLEND_MASKS);

  /* Parts (eager: small scalar work only). Part input opacities are
   * static per model; the effective chain opacity is recomputed here
   * and feeds the fused mesh/offscreen stages. */
  psm__i32 nparts = m->parts.count;
  if (nparts > 0) {
    psm__f32 *opa = m->parts.input_opacity;
    for (psm__i32 i = 0; i < nparts; i++)
      opa[i] = psm__clamp_f32_01(opa[i]);
  }

  psm__enable_parts(m);
  psm__gather_parts(m);
  psm__interp_parts(m);
  psm__blend_parts(m);
  psm__apply_part_opacity(m);
  psm__prof_tick(&s_prof, PSM__PROF_PARTS);

  /* Deformers: fused interp + blend + apply in topological order.
   * Produces deformer_changed[] for the mesh stage. */
  psm__enable_deformers(m);
  psm__gather_warps(m);
  psm__gather_rotations(m);
  psm__process_deformers(m);
  psm__prof_tick(&s_prof, PSM__PROF_DEFORMERS);

  /* Art meshes: fused interp + blend + deformer + part stage.
   * Produces mesh_changed[] for the glue/reverse-y stages. */
  psm__enable_art_meshes(m);
  psm__gather_art_meshes(m);
  psm__process_art_meshes(m);
  psm__prof_tick(&s_prof, PSM__PROF_ART_MESHES);

  /* Glue intensities (eager: scalar work only). */
  psm__gather_glues(m);
  psm__interp_glues(m);
  psm__blend_glues(m);
  psm__prof_tick(&s_prof, PSM__PROF_GLUES);

  /* Offscreens: fused interp + blend + owner-part multiplication. */
  psm__enable_offscreens(m);
  psm__gather_offscreens(m);
  psm__process_offscreens(m);
  psm__prof_tick(&s_prof, PSM__PROF_OFFSCREENS);

  psm__apply_glues(m); /* must come before reverse_y! */
  psm__prof_tick(&s_prof, PSM__PROF_GLUE_APPLY);

  psm__reverse_y(m);
  psm__prof_tick(&s_prof, PSM__PROF_REVERSE_Y);

  psm__sort_render_order(m);
  psm__prof_tick(&s_prof, PSM__PROF_SORT_RENDER);

  psm__update_flags(m);
  psm__prof_tick(&s_prof, PSM__PROF_UPDATE_FLAGS);

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
  /* Align with official Cubism Core semantics: the reset only re-baselines
   * the internal diff state (state_changed), it must NOT clear the change
   * bits in the flag array in place. Host apps (e.g. the official Cubism
   * Framework) call csmUpdateModel() then csmResetDrawableDynamicFlags()
   * back to back, and only read the flags later while drawing. Clearing the
   * array here would erase the current frame's DidChange bits before the
   * host ever reads them (masks/clipping pipelines would see "no change"
   * forever). The array keeps showing the current frame's flags until the
   * next csmUpdateModel() overwrites it with the new diff. */
  m->art_meshes.state_changed = 1;
}
