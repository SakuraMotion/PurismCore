/*
 * Purism Core: art mesh processing
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "private.h"
#include "array.h"
#include "artmesh.h"
#include "debug.h"
#include "gather.h"
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
    bool en = am->local_enable;
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
  psm__i32 max_keyforms = ms->count_info->art_mesh_keyforms;
  struct psm__art_mesh_keydata *kd = &m->art_meshes.keydata;

  struct psm__binding *bindings[count];
  for (psm__i32 i = 0; i < count; i++)
    bindings[i] = meshes[i].binding;

  struct psm__gather_channel ch[] = {
    { ms->art_mesh_key_src.opacity,    kd->opacity },
    { ms->art_mesh_key_src.draw_order, kd->draw_order },
  };
  psm__gather_scalars(count, bindings, kb, max_keyforms, &kd->interp, ch, 2);

  psm__gather_positions(count, bindings, kb, max_keyforms,
      ms->key_pos_src.xy, ms->art_mesh_key_src.key_pos_off,
      ms->count_info->keyform_pos, kd->pos);

  if (m->source->header->version < csmMocVersion_42)
    return;

  psm__i32 *kcb = ms->art_mesh_src.key_color_off;
  if (!kcb || !ms->keyform_mul_color_src.r || !ms->keyform_scr_color_src.r)
    return;

  psm__gather_colors(count, bindings, kcb,
      ms->count_info->keyform_mul_colors,
      &ms->keyform_mul_color_src, &ms->keyform_scr_color_src,
      &kd->mul_color, &kd->scr_color);
}

PSM__DEF void
psm__apply_parts_to_meshes(struct psm__model *m)
{
  psm__i32 count = m->art_meshes.count;
  if (count <= 0)
    return;

  struct psm__art_mesh *meshes = m->art_meshes.meshes;
  bool *en = m->art_meshes.enable;
  psm__f32 *part_opa = m->parts.opacity;
  psm__i32 *part_off = m->parts.offscreen_src_idx;
  psm__f32 *am_opa = m->art_meshes.opacity;

  for (psm__i32 i = 0; i < count; i++) {
    if (!en[i])
      continue;
    psm__i32 pp = meshes[i].parent_part_idx;
    if (pp != -1 && part_off[pp] == -1)
      am_opa[i] *= part_opa[pp];
  }

  psm__u8 version = m->source->header->version;
  if (version < csmMocVersion_42)
    return;

  psm__f32 *d_mul = m->deformers.mul_color;
  psm__f32 *d_scr = m->deformers.scr_color;
  psm__f32 *am_mul = m->art_meshes.mul_color;
  psm__f32 *am_scr = m->art_meshes.scr_color;

  for (psm__i32 i = 0; i < count; i++) {
    psm__i32 ci = i * 4;
    if (!en[i] || am_opa[i] == 0.0f)
      continue;
    psm__i32 pd = meshes[i].parent_deformer_idx;
    if (pd == -1)
      continue;

    psm__f32 *pm = &d_mul[pd * 4];
    psm__f32 *ps = &d_scr[pd * 4];

    psm__f32 r = am_mul[ci + 0] * pm[0];
    psm__f32 g = am_mul[ci + 1] * pm[1];
    psm__f32 b = am_mul[ci + 2] * pm[2];

    am_mul[ci + 0] = psm__clamp_f32_01(r);
    am_mul[ci + 1] = psm__clamp_f32_01(g);
    am_mul[ci + 2] = psm__clamp_f32_01(b);
    am_mul[ci + 3] = 1.0f;

    r = fmaf(-am_scr[ci + 0], ps[0], am_scr[ci + 0] + ps[0]);
    g = fmaf(-am_scr[ci + 1], ps[1], am_scr[ci + 1] + ps[1]);
    b = fmaf(-am_scr[ci + 2], ps[2], am_scr[ci + 2] + ps[2]);

    am_scr[ci + 0] = psm__clamp_f32_01(r);
    am_scr[ci + 1] = psm__clamp_f32_01(g);
    am_scr[ci + 2] = psm__clamp_f32_01(b);
    am_scr[ci + 3] = 1.0f;
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
  struct psm__sections *ms = m->source->sections;
  return ms->art_mesh_src.id_runtime;
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
  struct psm__sections *ms = m->source->sections;
  return ms->art_mesh_src.texture_no;
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
  struct psm__sections *ms = m->source->sections;
  return ms->art_mesh_src.mask_len;
}

PSMDEF const int **
csmGetDrawableMasks(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  struct psm__sections *ms = m->source->sections;
  return ms->art_mesh_src.drawable_mask_runtime;
}

PSMDEF const int *
csmGetDrawableVertexCounts(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  struct psm__sections *ms = m->source->sections;
  return ms->art_mesh_src.vertex_count;
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
  struct psm__sections *ms = m->source->sections;
  return (const csmVector2 **)ms->art_mesh_src.uv_runtime;
}

PSMDEF const int *
csmGetDrawableIndexCounts(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  struct psm__sections *ms = m->source->sections;
  return ms->art_mesh_src.idx_len;
}

PSMDEF const unsigned short **
csmGetDrawableIndices(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  struct psm__sections *ms = m->source->sections;
  return (const unsigned short **)ms->art_mesh_src.pos_idx_runtime;
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
  struct psm__sections *ms = m->source->sections;
  return ms->art_mesh_src.parent_part_idx;
}
