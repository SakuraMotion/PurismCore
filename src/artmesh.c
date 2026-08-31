/*
 * Purism Core: art mesh processing
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <string.h>
#include "private.h"
#include "array.h"
#include "artmesh.h"
#include "blendshape.h"
#include "debug.h"
#include "deformer.h"
#include "gather.h"
#include "interpolate.h"
#include "math2.h"
#include "moc3.h"
#include "model.h"

PSM__DEF void
psm__enable_art_meshes(struct psm__model *m)
{
  psm__i32 count = m->art_meshes.count;
  if (count <= 0)
    return;

  struct psm__art_mesh *meshes = m->art_meshes.meshes;

  bool *def_en = m->deformers.enable;
  bool *part_en = m->parts.enable;
  bool *enable = m->art_meshes.enable;

  for (psm__i32 i = 0; i < count; i++) {
    struct psm__art_mesh *am = &meshes[i];

    bool     en = am->local_enable;
    psm__i32 pp = am->parent_part_idx;
    psm__i32 pd = am->parent_deformer_idx;

    if (en && pp != -1) en = part_en[pp];
    if (en && pd != -1) en = def_en[pd];
    if (en) en = !am->binding->out_of_range;

    enable[i] = en;
  }
}

PSM__DEF void
psm__gather_art_meshes(struct psm__model *m)
{
  psm__i32 count = m->art_meshes.count;
  if (count <= 0)
    return;

  struct psm__sections *ms = m->source->sections;
  struct psm__art_mesh *meshes = m->art_meshes.meshes;
  if (!meshes)
    return;

  psm__i32 *kb = ms->art_mesh_src.keyform_off;

  struct psm__art_mesh_keydata *kd = &m->art_meshes.keydata;

  struct psm__binding *const *bindings = m->art_meshes.bindings;

  struct psm__gather_channel ch[] = {
    { ms->art_mesh_key_src.opacity, kd->opacity },
    { ms->art_mesh_key_src.draw_order, kd->draw_order },
  };
  psm__gather_scalars(count, bindings, kb, &kd->interp, ch, 2);

  psm__gather_positions(count, bindings, kb,
      ms->key_pos_src.xy, ms->art_mesh_key_src.key_pos_off, kd->pos);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__i32 *kcb = ms->art_mesh_src.key_color_off;
  if (!kcb || !ms->keyform_mul_color_src.r || !ms->keyform_scr_color_src.r)
    return;

  psm__gather_colors(count, bindings, kcb,
      &ms->keyform_mul_color_src, &ms->keyform_scr_color_src,
      &kd->mul_color, &kd->scr_color);
}

/*
 * Fused per-art-mesh dirty stage (replaces the former batch
 * interp_art_meshes + blend_art_meshes + apply_transforms_to_meshes +
 * apply_parts_to_meshes sequence).
 *
 * A mesh is recomputed only when:
 *   - its key data moved (binding idx/weight dirty), or
 *   - a blend shape targeting it is dirty, or
 *   - a dirty glue involves it (its final positions must be rebuilt
 *     before the glue is re-applied), or
 *   - its enable state flipped (stale state may linger), or
 *   - its parent deformer was recomputed this frame, or
 *   - its parent part's effective opacity changed (enable flip of the
 *     part chain; part input opacities are static per model).
 *
 * Clean meshes keep their previous frame's final state (transformed
 * positions, final opacity, propagated colors), so skipping is exact.
 * A mesh whose parent part/deformer is disabled is itself disabled
 * (enable propagation) and is skipped entirely: hidden drawables cost
 * nothing.
 */
PSM__DEF void
psm__process_art_meshes(struct psm__model *m)
{
  psm__i32 count = m->art_meshes.count;
  if (count <= 0)
    return;

  struct psm__art_mesh *meshes = m->art_meshes.meshes;

  bool *en = m->art_meshes.enable;
  psm__u8 *changed = m->mesh_changed;
  psm__u8 *last_en = m->mesh_last_enable;
  psm__u8 *mb = m->mesh_blend_dirty;
  psm__u8 *gd = m->glue_mesh_dirty;
  psm__u8 *dch = m->deformer_changed;
  struct psm__deformer_node *dn = m->deformers.nodes;
  psm__f32 *d_opa = m->deformers.opacity;
  psm__f32 **cp = m->art_meshes.pos;
  psm__f32 *am_opa = m->art_meshes.opacity;
  psm__f32 *part_opa = m->parts.opacity;
  psm__i32 *part_off = m->parts.offscreen_src_idx;
  psm__u8 version = m->source->header->version;

  for (psm__i32 i = 0; i < count; i++) {
    struct psm__art_mesh *mesh = &meshes[i];
    psm__i32 pdi = mesh->parent_deformer_idx;
    psm__i32 pp = mesh->parent_part_idx;
    struct psm__binding *b = mesh->binding;

    psm__i32 self_dirty = b ? (b->idx_dirty || b->weight_dirty) : 1;
    psm__i32 en_flip = (en[i] != (last_en[i] != 0));
    psm__i32 parent_changed = (pdi != -1) ? dch[pdi] : 0;

    /* The parent-part contribution of a mesh is its effective opacity,
     * which changes only when the part chain's enable state flips.
     * Such a flip also flips this mesh's own enable (part_en is part of
     * the mesh enable), which en_flip catches; when the mesh stays
     * enabled its parent part stayed enabled too, so the opacity is
     * unchanged and no separate part-change check is needed. */

    if (!en[i]) {
      changed[i] = 0;
      last_en[i] = 0;
      continue;
    }

    if (!self_dirty && !mb[i] && !gd[i] && !en_flip && !parent_changed) {
      changed[i] = 0;
      last_en[i] = 1;
      continue;
    }

    psm__i32 vc = mesh->vertex_count;

    /* Base reset (idempotent) + blend contributions. */
    psm__interp_art_mesh_one(m, i);
    psm__blend_art_mesh_one(m, i);

    /* Parent deformer transform + opacity. */
    if (pdi != -1) {
      am_opa[i] *= d_opa[pdi];
      psm__i32 dt = dn[pdi].type;
      if (dt == PSM__DEFORMER_TYPE_WARP)
        psm__warp_transform(m, pdi, cp[i], cp[i], vc);
      else
        psm__rotation_transform(m, pdi, cp[i], cp[i], vc);
    }

    /* Parent part opacity. */
    if (pp != -1 && part_off[pp] == -1)
      am_opa[i] *= part_opa[pp];

    /* Parent deformer colors (v4.2+). */
    if (version >= csmMocVersion_42 && am_opa[i] != 0.0f && pdi != -1) {
      psm__f32 *d_mul = m->deformers.mul_color;
      psm__f32 *d_scr = m->deformers.scr_color;
      psm__f32 *am_mul = m->art_meshes.mul_color;
      psm__f32 *am_scr = m->art_meshes.scr_color;
      psm__i32 ci = i * 4;
      psm__f32 *pm = &d_mul[pdi * 4];
      psm__f32 *ps = &d_scr[pdi * 4];

      psm__f32 r = am_mul[ci + 0] * pm[0];
      psm__f32 g = am_mul[ci + 1] * pm[1];
      psm__f32 bb = am_mul[ci + 2] * pm[2];

      am_mul[ci + 0] = psm__clamp_f32_01(r);
      am_mul[ci + 1] = psm__clamp_f32_01(g);
      am_mul[ci + 2] = psm__clamp_f32_01(bb);
      am_mul[ci + 3] = 1.0f;

      r = fmaf(-am_scr[ci + 0], ps[0], am_scr[ci + 0] + ps[0]);
      g = fmaf(-am_scr[ci + 1], ps[1], am_scr[ci + 1] + ps[1]);
      bb = fmaf(-am_scr[ci + 2], ps[2], am_scr[ci + 2] + ps[2]);

      am_scr[ci + 0] = psm__clamp_f32_01(r);
      am_scr[ci + 1] = psm__clamp_f32_01(g);
      am_scr[ci + 2] = psm__clamp_f32_01(bb);
      am_scr[ci + 3] = 1.0f;
    }

    changed[i] = 1;
    last_en[i] = 1;
  }
}

PSMDEF int
csmGetDrawableCount(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->art_meshes.count;
}

PSMDEF const char **
csmGetDrawableIds(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->art_mesh_src.id_runtime;
}

PSMDEF const csmFlags *
csmGetDrawableConstantFlags(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmFlags *)m->art_meshes.const_flags;
}

PSMDEF const csmFlags *
csmGetDrawableDynamicFlags(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmFlags *)m->art_meshes.change_flags;
}

#if PSM_COMPAT_VERSION >= 0x06000000L
PSMDEF const int *
csmGetDrawableBlendModes(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->art_meshes.blend_mode;
}
#endif

PSMDEF const int *
csmGetDrawableTextureIndices(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->art_mesh_src.texture_no;
}

PSMDEF const int *
csmGetDrawableDrawOrders(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->art_meshes.draw_order;
}

PSMDEF const float *
csmGetDrawableOpacities(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->art_meshes.opacity;
}

PSMDEF const int *
csmGetDrawableMaskCounts(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->art_mesh_src.mask_len;
}

PSMDEF const int **
csmGetDrawableMasks(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->art_mesh_src.drawable_mask_runtime;
}

PSMDEF const int *
csmGetDrawableVertexCounts(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->art_mesh_src.vertex_count;
}

PSMDEF const csmVector2 **
csmGetDrawableVertexPositions(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmVector2 **)m->art_meshes.pos;
}

PSMDEF const csmVector2 **
csmGetDrawableVertexUvs(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmVector2 **)m->source->sections->art_mesh_src.uv_runtime;
}

PSMDEF const int *
csmGetDrawableIndexCounts(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->art_mesh_src.idx_len;
}

PSMDEF const unsigned short **
csmGetDrawableIndices(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const unsigned short **)
      m->source->sections->art_mesh_src.pos_idx_runtime;
}

PSMDEF const csmVector4 *
csmGetDrawableMultiplyColors(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmVector4 *)m->art_meshes.mul_color;
}

PSMDEF const csmVector4 *
csmGetDrawableScreenColors(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmVector4 *)m->art_meshes.scr_color;
}

PSMDEF const int *
csmGetDrawableParentPartIndices(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->art_mesh_src.parent_part_idx;
}
