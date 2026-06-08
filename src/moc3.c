/*
 * Purism Core: MOC3 file format parsing
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <string.h>
#include "private.h"
#include "array.h"
#include "debug.h"
#include "deformer.h"
#include "error.h"
#include "arena.h"
#include "moc3.h"
#include "model.h"

#define PSM__MOC3_MAGIC "MOC3"
#define PSM__MOC3_MAGIC_SIZE 4

static int
psm__is_le(void)
{
  unsigned int x = 1;
  return *((unsigned char *)&x) == 1;
}

static inline psm__u16
psm__bswap_16(psm__u16 x)
{
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap16(x);
#elif defined(_MSC_VER)
  return _byteswap_ushort(x);
#else
  return (x >> 8) | (x << 8);
#endif
}

static inline psm__u32
psm__bswap_32(psm__u32 x)
{
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap32(x);
#elif defined(_MSC_VER)
  return _byteswap_ulong(x);
#else
  return ((x >> 24) & 0x000000FF) | ((x >>  8) & 0x0000FF00) |
         ((x <<  8) & 0x00FF0000) | ((x << 24) & 0xFF000000);
#endif
}

static void
psm__bswap_many_16(void *data, psm_size count)
{
  psm__u16 *p = (psm__u16 *)data;
  for (psm_size i = 0; i < count; i++)
    p[i] = psm__bswap_16(p[i]);
}

static void
psm__bswap_many_32(void *data, psm_size count)
{
  psm__u32 *p = (psm__u32 *)data;
  for (psm_size i = 0; i < count; i++)
    p[i] = psm__bswap_32(p[i]);
}

/* Check section bounds and alignment. All sections must be 8-byte aligned. */
#define psm__bounds_check_static(TYPE, COUNT, offsets, i, n) \
    if ((offsets[i] & 7) != 0) { \
      PSM__LOGF("section %d misaligned: offset=%u", i, (unsigned)offsets[i]); \
      return PSM__ERR_FILE_CORRUPT; \
    } \
    if ((COUNT) > (psm_size)-1 / sizeof(TYPE)) { \
      PSM__LOGF("section %d count overflow: count=%u", i, (unsigned)(COUNT)); \
      return PSM__ERR_FILE_CORRUPT; \
    } \
    psm_size _sz = sizeof(TYPE) * (COUNT); \
    if (offsets[i] > n || n - offsets[i] < _sz) { \
      PSM__LOGF("section %d out of bounds: offset=%u size=%u n=%u", i, (unsigned)offsets[i], (unsigned)_sz, (unsigned)n); \
      return PSM__ERR_FILE_CORRUPT; \
    }
#define psm__bounds_check_dynamic(TYPE, COUNT_MEMBER, offsets, i, n, cnt) \
    if ((offsets[i] & 7) != 0) { \
      PSM__LOGF("section %d misaligned: offset=%u", i, (unsigned)offsets[i]); \
      return PSM__ERR_FILE_CORRUPT; \
    } \
    if (cnt->COUNT_MEMBER < 0 || (psm_size)cnt->COUNT_MEMBER > (psm_size)-1 / sizeof(TYPE)) { \
      PSM__LOGF("section %d count invalid: count=%d", i, (int)cnt->COUNT_MEMBER); \
      return PSM__ERR_FILE_CORRUPT; \
    } \
    psm_size _sz = sizeof(TYPE) * (psm_size)cnt->COUNT_MEMBER; \
    if (offsets[i] > n || n - offsets[i] < _sz) { \
      PSM__LOGF("section %d out of bounds: offset=%u size=%u n=%u", i, (unsigned)offsets[i], (unsigned)_sz, (unsigned)n); \
      return PSM__ERR_FILE_CORRUPT; \
    }

static int
psm__get_moc_version(csmMocVersion *v, const psm__u8 *p, psm_size n)
{
  if (!p || !n)
    return PSM__ERR_INVALID_PARAMETER;
  if (n < (PSM__MOC3_MAGIC_SIZE + 1))
    return PSM__ERR_FILE_CORRUPT;
  if (memcmp(p, PSM__MOC3_MAGIC, PSM__MOC3_MAGIC_SIZE) != 0)
    return PSM__ERR_FILE_UNRECOGNIZED;
  *v = (csmMocVersion)(*(p + PSM__MOC3_MAGIC_SIZE));
  return PSM__OK;
}

static int
psm__verify_count_info(const struct psm__count_info *cnt)
{
  PSM__FAILM(cnt->parts < 0 || cnt->deformers < 0 ||
      cnt->warps < 0 || cnt->rotations < 0 ||
      cnt->art_meshes < 0 || cnt->parameters < 0 ||
      cnt->bindings < 0 || cnt->key_tables < 0 ||
      cnt->keys < 0 || cnt->uvs < 0 || cnt->indices < 0 || cnt->masks < 0 ||
      cnt->glues < 0 || cnt->keyform_pos < 0 || cnt->part_keyforms < 0 ||
      cnt->warp_keyforms < 0 || cnt->rotation_keyforms < 0 ||
      cnt->art_mesh_keyforms < 0 || cnt->glue_keyforms < 0,
      PSM__ERR_FILE_CORRUPT, "invalid count values");

  PSM__FAILM(((psm__u32)cnt->warps + (psm__u32)cnt->rotations) !=
             (psm__u32)cnt->deformers,
      PSM__ERR_FILE_CORRUPT, "deformer count mismatch");
  return PSM__OK;
}

static int
psm__verify_indices(psm__u8 ver, const struct psm__sections *src)
{
  const struct psm__count_info *cnt = src->count_info;

#define psm__check_nonnull(TYPE, MEMBER, COUNT_MEMBER) \
  if (cnt->COUNT_MEMBER > 0 && !src->MEMBER) { \
    PSM__LOGF("missing: %s (count=%d)", \
        #MEMBER, cnt->COUNT_MEMBER); \
    return PSM__ERR_FILE_CORRUPT; \
  }

  PSM__SECTIONS_V30(psm__nop_predicate, psm__check_nonnull)
  if (ver < csmMocVersion_33) goto done_nonnull;
  PSM__SECTIONS_V33(psm__nop_predicate, psm__check_nonnull)
  if (ver < csmMocVersion_42) goto done_nonnull;
  PSM__SECTIONS_V42(psm__nop_predicate, psm__check_nonnull)
  if (ver < csmMocVersion_50) goto done_nonnull;
  PSM__SECTIONS_V50(psm__nop_predicate, psm__check_nonnull)
  if (ver < csmMocVersion_53) goto done_nonnull;
  PSM__SECTIONS_V53(psm__nop_predicate, psm__check_nonnull)
done_nonnull:
#undef psm__check_nonnull

#define psm__model_check_index(arr, i, max) \
  PSM__FAIL((arr)[i] < 0 || (arr)[i] >= (max), \
      PSM__ERR_FILE_CORRUPT, \
      "invalid index: %s[%d]=%d (max=%d)", \
      #arr, i, (arr)[i], (max))

#define psm__model_check_index_or_neg1(arr, i, max) \
  PSM__FAIL((arr)[i] < -1 || (arr)[i] >= (max), \
      PSM__ERR_FILE_CORRUPT, \
      "invalid index: %s[%d]=%d (max=%d)", \
      #arr, i, (arr)[i], (max))

#define psm__model_check_range(begin_arr, count_arr, i, max) \
  PSM__FAIL((count_arr)[i] < 0 || \
      ((count_arr)[i] > 0 && ((begin_arr)[i] < 0 || \
          (psm__u32)(begin_arr)[i] + \
          (psm__u32)(count_arr)[i] > (psm__u32)(max))), \
      PSM__ERR_FILE_CORRUPT, \
      "invalid range: %s[%d] begin=%d count=%d (max=%d)", \
      #begin_arr, i, (begin_arr)[i], (count_arr)[i], (max))

  /*
   * Validate key_count matches max_blend from binding.
   * key_count is what the source declares; max_blend is
   * 2^(param_binding_count) which is the actual range
   * accessed at runtime via blend_count indexing.
   */
#define psm__check_key_combo(obj_src, keyform_total, obj_len) \
  for (psm__i32 _i = 0; _i < (obj_len); _i++) { \
    psm__i32 _bi = (obj_src).binding_idx[_i]; \
    if (_bi < 0 || _bi >= cnt->bindings) continue; \
    psm__i32 _pc = psm__clamp_i32(\
        src->binding_src.key_table_idx_len[_bi], 0, PSM__MAX_KEY_TABLES); \
    psm__i32 _mc = 1 << _pc; \
    psm__i32 _kb = (obj_src).keyform_off[_i]; \
    PSM__FAIL(!psm__valid_range(_kb, _mc, (keyform_total)), \
        PSM__ERR_FILE_CORRUPT, \
        "%s[%d] keyform_off=%d max_blend=%d total=%d", \
        #obj_src, _i, _kb, _mc, (keyform_total)); \
  }

  /* Part sources */
  for (psm__i32 i = 0; i < cnt->parts; i++) {
    psm__model_check_index(src->part_src.binding_idx, i, cnt->bindings);
    psm__model_check_range(src->part_src.keyform_off,
        src->part_src.key_len, i, cnt->part_keyforms);
    psm__model_check_index_or_neg1(
        src->part_src.parent_part_idx, i, cnt->parts);
  }
  psm__check_key_combo(src->part_src, cnt->part_keyforms, cnt->parts)

  /* Deformer sources */
  for (psm__i32 i = 0; i < cnt->deformers; i++) {
    psm__model_check_index(src->deformer_src.binding_idx, i, cnt->bindings);
    psm__model_check_index_or_neg1(
        src->deformer_src.parent_part_idx, i, cnt->parts);
    psm__model_check_index_or_neg1(src->deformer_src.parent_deformer_idx,
        i, cnt->deformers);

    psm__i32 dtype = src->deformer_src.type[i];
    psm__i32 sidx = src->deformer_src.local_idx[i];
    switch (dtype) {
    case PSM__DEFORMER_TYPE_WARP:
      if (sidx < 0 || sidx >= cnt->warps) {
        PSM__LOGF("deformer[%d].specific=%d (warp max=%d)",
            i, sidx, cnt->warps);
        return PSM__ERR_FILE_CORRUPT;
      }
      break;
    case PSM__DEFORMER_TYPE_ROTATION:
      if (sidx < 0 || sidx >= cnt->rotations) {
        PSM__LOGF("deformer[%d].specific=%d (rot max=%d)",
            i, sidx, cnt->rotations);
        return PSM__ERR_FILE_CORRUPT;
      }
      break;
    default:
      PSM__LOGF("deformer[%d].type=%d invalid", i, dtype);
      return PSM__ERR_FILE_CORRUPT;
    }
  }

  /* Warp deformer sources */
  for (psm__i32 i = 0; i < cnt->warps; i++) {
    psm__model_check_index(src->warp_src.binding_idx,
        i, cnt->bindings);
    psm__model_check_range(src->warp_src.keyform_off,
        src->warp_src.key_len, i, cnt->warp_keyforms);
    psm__i32 row = src->warp_src.row[i];
    psm__i32 col = src->warp_src.column[i];
    psm__i32 vc = src->warp_src.vertex_count[i];
    PSM__FAIL(row <= 0 || col <= 0, PSM__ERR_FILE_CORRUPT,
        "warp[%d] grid row=%d col=%d", i, row, col);
    psm__u32 expect = (psm__u32)(row + 1) * (psm__u32)(col + 1);
    PSM__FAIL((psm__u32)vc != expect, PSM__ERR_FILE_CORRUPT,
        "warp[%d] vert_count=%d expected=%u", i, vc, (unsigned)expect);
  }

  psm__check_key_combo(src->warp_src, cnt->warp_keyforms, cnt->warps)

  /* Warp deformer keyform positions */
  for (psm__i32 i = 0; i < cnt->warps; i++) {
    psm__i32 begin = src->warp_src.keyform_off[i];
    psm__i32 count = src->warp_src.key_len[i];
    psm__i32 vc = src->warp_src.vertex_count[i];
    for (psm__i32 j = 0; j < count; j++) {
      psm__i32 po = src->warp_key_src.key_pos_off[begin + j];
      PSM__FAIL(po < 0 || (psm__u32)po + (psm__u32)vc >
              (psm__u32)cnt->keyform_pos, PSM__ERR_FILE_CORRUPT,
          "warp[%d] kf[%d] pos_off=%d vc=%d max=%d",
          i, j, po, vc, cnt->keyform_pos);
    }
  }

  /* Rotation deformer sources */
  for (psm__i32 i = 0; i < cnt->rotations; i++) {
    psm__model_check_index(src->rotation_src.binding_idx, i, cnt->bindings);
    psm__model_check_range(src->rotation_src.keyform_off,
        src->rotation_src.key_len, i, cnt->rotation_keyforms);
  }

  psm__check_key_combo(src->rotation_src, cnt->rotation_keyforms, cnt->rotations)

  /* Art mesh sources */
  for (psm__i32 i = 0; i < cnt->art_meshes; i++) {
    psm__model_check_index(src->art_mesh_src.binding_idx, i, cnt->bindings);
    psm__model_check_range(src->art_mesh_src.keyform_off,
        src->art_mesh_src.key_len, i, cnt->art_mesh_keyforms);
    psm__model_check_index_or_neg1(
        src->art_mesh_src.parent_part_idx, i, cnt->parts);
    psm__model_check_index_or_neg1(src->art_mesh_src.parent_deformer_idx,
        i, cnt->deformers);
    PSM__FAIL(src->art_mesh_src.vertex_count[i] < 0 ||
        src->art_mesh_src.uv_off[i] < 0 ||
        (psm__u32)src->art_mesh_src.uv_off[i] +
            2u * (psm__u32)src->art_mesh_src.vertex_count[i]
            > (psm__u32)cnt->uvs, PSM__ERR_FILE_CORRUPT,
        "art_mesh[%d]: UV [%d, +%d*2) oob (max %d)",
        i, src->art_mesh_src.uv_off[i],
        src->art_mesh_src.vertex_count[i], cnt->uvs);
    psm__model_check_range(src->art_mesh_src.indices_off,
        src->art_mesh_src.indices_len, i, cnt->indices);
    psm__model_check_range(src->art_mesh_src.mask_off,
        src->art_mesh_src.mask_len, i, cnt->masks);
  }

  psm__check_key_combo(src->art_mesh_src, cnt->art_mesh_keyforms, cnt->art_meshes)

  /* Art mesh keyform position indices */
  for (psm__i32 i = 0; i < cnt->art_mesh_keyforms; i++) {
    psm__model_check_index(src->art_mesh_key_src.key_pos_off,
        i, cnt->keyform_pos);
  }

  /* Parameter sources */
  for (psm__i32 i = 0; i < cnt->parameters; i++) {
    psm__model_check_range(src->param_src.key_table_off, src->param_src.key_table_len,
        i, cnt->key_tables);
  }

  /* Keyform binding sources */
  for (psm__i32 i = 0; i < cnt->bindings; i++) {
    psm__model_check_range(src->binding_src.key_table_idx_off,
        src->binding_src.key_table_idx_len, i, cnt->key_table_indices);
    psm__i32 pc = src->binding_src.key_table_idx_len[i];
    PSM__FAIL(pc < 0 || pc > PSM__MAX_KEY_TABLES, PSM__ERR_FILE_CORRUPT,
        "binding[%d] param_count=%d oob", i, pc);
  }

  /* Parameter binding index sources */
  for (psm__i32 i = 0; i < cnt->key_table_indices; i++) {
    psm__model_check_index(src->key_table_idx_src.index,
        i, cnt->key_tables);
  }

  /* Parameter binding sources */
  for (psm__i32 i = 0; i < cnt->key_tables; i++) {
    psm__model_check_range(src->key_table_src.keys_off,
        src->key_table_src.keys_len, i, cnt->keys);
  }

  /* Drawable mask sources */
  for (psm__i32 i = 0; i < cnt->masks; i++) {
    psm__model_check_index_or_neg1(
        src->mask_src.art_mesh_idx, i, cnt->art_meshes);
  }

  psm__check_key_combo(src->glue_src, cnt->glue_keyforms, cnt->glues)

  /* Glue sources */
  for (psm__i32 i = 0; i < cnt->glues; i++) {
    psm__model_check_index(src->glue_src.binding_idx, i, cnt->bindings);
    psm__model_check_range(src->glue_src.keyform_off,
        src->glue_src.key_len, i, cnt->glue_keyforms);
    psm__model_check_index(src->glue_src.art_mesh_index_a, i, cnt->art_meshes);
    psm__model_check_index(src->glue_src.art_mesh_index_b, i, cnt->art_meshes);
    psm__model_check_range(src->glue_src.info_off,
        src->glue_src.info_len, i, cnt->glue_info);
  }

  /* Glue position indices */
  if (src->glue_src.info_off && src->glue_src.info_len &&
      src->glue_src.art_mesh_index_a && src->glue_src.art_mesh_index_b &&
      src->glue_info_src.position_idx && src->art_mesh_src.vertex_count) {
    for (psm__i32 i = 0; i < cnt->glues; i++) {
      psm__i32 m0 = src->glue_src.art_mesh_index_a[i];
      psm__i32 m1 = src->glue_src.art_mesh_index_b[i];
      if (m0 < 0 || m0 >= cnt->art_meshes || m1 < 0 || m1 >= cnt->art_meshes)
        continue;
      psm__i32 vc0 = src->art_mesh_src.vertex_count[m0];
      psm__i32 vc1 = src->art_mesh_src.vertex_count[m1];
      psm__i32 ib = src->glue_src.info_off[i];
      psm__i32 ic = src->glue_src.info_len[i];
      if (ib < 0 || ic <= 0 || ib + ic > cnt->glue_info)
        continue;
      for (psm__i32 j = 0; j < ic; j += 2) {
        psm__u16 p0 = src->glue_info_src.position_idx[ib + j];
        PSM__FAIL(p0 >= (psm__u16)vc0, PSM__ERR_FILE_CORRUPT,
            "glue[%d] pos_idx[%d]=%u OOB (vc=%d)", i, j, p0, vc0);
        if (j + 1 < ic) {
          psm__u16 p1 = src->glue_info_src.position_idx[ib + j + 1];
          PSM__FAIL(p1 >= (psm__u16)vc1, PSM__ERR_FILE_CORRUPT,
              "glue[%d] pos_idx[%d]=%u OOB (vc=%d)", i, j + 1, p1, vc1);
        }
      }
    }
  }

  /* Draw order group sources */
  for (psm__i32 i = 0; i < cnt->draw_groups; i++) {
    psm__model_check_range(src->draw_group_src.obj_off,
        src->draw_group_src.obj_len, i, cnt->draw_items);
  }

  /* Draw order group object sources */
  if (src->draw_group_obj_src.type && src->draw_group_obj_src.index) {
    for (psm__i32 i = 0; i < cnt->draw_items; i++) {
      psm__model_check_index_or_neg1(src->draw_group_obj_src.self_group_idx,
          i, cnt->draw_groups);
      psm__i32 t = src->draw_group_obj_src.type[i];
      psm__i32 oi = src->draw_group_obj_src.index[i];
      PSM__FAIL(t != 0 && t != 1, PSM__ERR_FILE_CORRUPT,
          "draw_item[%d]: bad type %d", i, t);
      psm__i32 max = t ? cnt->parts : cnt->art_meshes;
      PSM__FAIL(oi < 0 || oi >= max, PSM__ERR_FILE_CORRUPT,
          "draw_item[%d]: index %d OOB (type=%d max=%d)", i, oi, t, max);
    }
  }

  if (ver < csmMocVersion_42)
    goto done_ver;

  /* Warp deformer color indices */
  for (psm__i32 i = 0; i < cnt->warps; i++) {
    psm__model_check_range(src->warp_src.key_color_off,
        src->warp_src.key_len, i, cnt->keyform_mul_colors);
  }

  /* Rotation deformer color indices */
  for (psm__i32 i = 0; i < cnt->rotations; i++) {
    psm__model_check_range(src->rotation_src.key_color_off,
        src->rotation_src.key_len, i, cnt->keyform_mul_colors);
  }

  /* Art mesh color indices */
  for (psm__i32 i = 0; i < cnt->art_meshes; i++) {
    psm__model_check_range(src->art_mesh_src.key_color_off,
        src->art_mesh_src.key_len, i, cnt->keyform_mul_colors);
  }

  /* Parameter extension sources */
  for (psm__i32 i = 0; i < cnt->parameters; i++) {
    psm__model_check_range(src->param_keys_src.keys_off,
        src->param_keys_src.keys_len, i, cnt->keys);
  }

  /* Blend shape parameter binding sources */
  for (psm__i32 i = 0;
       i < cnt->blend_key_tables; i++) {
    psm__model_check_range(src->blend_key_table_src.keys_off,
        src->blend_key_table_src.keys_len, i, cnt->keys);
  }

  /* Parameter blend shape binding indices */
  for (psm__i32 i = 0; i < cnt->parameters; i++) {
    psm__model_check_range(src->param_src.blend_key_table_off,
        src->param_src.blend_key_table_len, i, cnt->blend_key_tables);
  }

  /* Blend shape keyform binding sources */
  for (psm__i32 i = 0; i < cnt->blend_bindings; i++) {
    psm__model_check_index(src->blend_binding_src.axis_idx,
        i, cnt->blend_key_tables);
    psm__model_check_range(src->blend_binding_src.bs_constraint_idx_off,
        src->blend_binding_src.bs_constraint_idx_len,
        i, cnt->bs_constraint_idx);
  }

  /* Blend shape warp deformer sources */
  for (psm__i32 i = 0;
       i < cnt->bs_warps; i++) {
    psm__model_check_index(src->bs_warp_src.target_idx, i, cnt->warps);
    psm__model_check_range(src->bs_warp_src.bs_binding_off,
        src->bs_warp_src.bs_binding_len, i, cnt->blend_bindings);
  }

  /* Blend shape art mesh sources */
  for (psm__i32 i = 0;
       i < cnt->bs_art_meshes; i++) {
    psm__model_check_index(src->bs_art_mesh_src.target_idx,
        i, cnt->art_meshes);
    psm__model_check_range(src->bs_art_mesh_src.bs_binding_off,
        src->bs_art_mesh_src.bs_binding_len, i, cnt->blend_bindings);
  }

  /* Blend shape constraint index sources */
  for (psm__i32 i = 0;
       i < cnt->bs_constraint_idx; i++) {
    psm__model_check_index(src->blend_constraint_idx_src.constraint_idx,
        i, cnt->bs_constraints);
  }

  /* Blend shape constraint sources */
  for (psm__i32 i = 0;
       i < cnt->bs_constraints; i++) {
    psm__model_check_index(src->blend_constraint_src.parameter_idx,
        i, cnt->parameters);
    psm__model_check_range(src->blend_constraint_src.value_off,
        src->blend_constraint_src.value_len, i, cnt->bs_constraint_vals);
  }

  if (ver < csmMocVersion_50)
    goto done_ver;

  /* Blend shape part sources */
  for (psm__i32 i = 0; i < cnt->bs_parts; i++) {
    psm__model_check_index(src->bs_part_src.target_idx, i, cnt->parts);
    psm__model_check_range(src->bs_part_src.bs_binding_off,
        src->bs_part_src.bs_binding_len, i, cnt->blend_bindings);
  }

  /* Blend shape rotation deformer sources */
  for (psm__i32 i = 0;
       i < cnt->bs_rotations; i++) {
    psm__model_check_index(src->bs_rotation_src.target_idx, i, cnt->rotations);
    psm__model_check_range(src->bs_rotation_src.bs_binding_off,
        src->bs_rotation_src.bs_binding_len, i, cnt->blend_bindings);
  }

  /* Blend shape glue sources */
  for (psm__i32 i = 0; i < cnt->bs_glues; i++) {
    psm__model_check_index(src->bs_glue_src.target_idx, i, cnt->glues);
    psm__model_check_range(src->bs_glue_src.bs_binding_off,
        src->bs_glue_src.bs_binding_len, i, cnt->blend_bindings);
  }

  if (ver < csmMocVersion_53)
    goto done_ver;

  /* Part offscreen rendering index */
  for (psm__i32 i = 0; i < cnt->parts; i++) {
    psm__model_check_index_or_neg1(src->part_src.offscreen_idx,
        i, cnt->offscreens);
  }

  /* Offscreen rendering sources */
  for (psm__i32 i = 0; i < cnt->offscreens; i++) {
    psm__model_check_index(src->offscreen_src.owner_idx, i, cnt->parts);
    psm__model_check_range(src->offscreen_src.mask_off,
        src->offscreen_src.mask_len, i, cnt->masks);
  }

  /* Blend shape offscreen rendering sources */
  for (psm__i32 i = 0;
       i < cnt->bs_offscreens; i++) {
    psm__model_check_index(src->bs_offscreen_src.target_idx,
        i, cnt->offscreens);
    psm__model_check_range(src->bs_offscreen_src.bs_binding_off,
        src->bs_offscreen_src.bs_binding_len, i, cnt->blend_bindings);
  }

done_ver:
#undef psm__model_check_index
#undef psm__model_check_index_or_neg1
#undef psm__model_check_range

  return PSM__OK;
}

static void
psm__bswap_model_data(psm__u8 ver, struct psm__sections *src)
{
  struct psm__count_info *cnt = src->count_info;

  /* count_info already swapped above */
  psm__bswap_many_32(&src->canvas_info->pix_per_unit, 1);
  psm__bswap_many_32(&src->canvas_info->origin_x, 1);
  psm__bswap_many_32(&src->canvas_info->origin_y, 1);
  psm__bswap_many_32(&src->canvas_info->width, 1);
  psm__bswap_many_32(&src->canvas_info->height, 1);

#define psm__bswap_predicate(TYPE, MEMBER, COUNT_MEMBER) \
    if (sizeof(TYPE) == 4) \
      psm__bswap_many_32(src->MEMBER, cnt->COUNT_MEMBER); \
    else if (sizeof(TYPE) == 2) \
      psm__bswap_many_16(src->MEMBER, cnt->COUNT_MEMBER);

  PSM__SECTIONS_V30(psm__nop_predicate, psm__bswap_predicate)
  if (ver < csmMocVersion_33) goto done;
  PSM__SECTIONS_V33(psm__nop_predicate, psm__bswap_predicate)
  if (ver < csmMocVersion_42) goto done;
  PSM__SECTIONS_V42(psm__nop_predicate, psm__bswap_predicate)
  if (ver < csmMocVersion_50) goto done;
  PSM__SECTIONS_V50(psm__nop_predicate, psm__bswap_predicate)
  if (ver < csmMocVersion_53) goto done;

  PSM__SECTIONS_V53(psm__nop_predicate, psm__bswap_predicate)

done:;
#undef psm__bswap_predicate
}

static int
psm__init_moc3_sections(struct psm__moc3_data *moc3_data,
    psm__u8 *p, psm_size n, psm_size off, psm__i32 needs_bswap)
{
  struct psm__sections *ms = moc3_data->sections;
  psm__u32 *offsets = moc3_data->offsets;
  psm__u8 ver = moc3_data->header->version;
  psm_size sec_count, prev_end = off;

  if (ver >= csmMocVersion_53)
    sec_count = sizeof(((struct psm__moc3_data_v53 *)0)->offsets) / sizeof(psm__u32);
  else
    sec_count = sizeof(((struct psm__moc3_data_v52 *)0)->offsets) / sizeof(psm__u32);

  if (needs_bswap)
    psm__bswap_many_32(offsets, sec_count);

  PSM__FAILM((offsets[0] & 3) != 0,
      PSM__ERR_FILE_CORRUPT, "count_info misaligned");
  PSM__FAILM(offsets[0] > n || n - offsets[0] < sizeof(struct psm__count_info),
      PSM__ERR_FILE_CORRUPT, "count_info offset oob");
  PSM__FAILM(offsets[0] < off,
      PSM__ERR_FILE_CORRUPT, "count_info before header end");

  if (needs_bswap)
    psm__bswap_many_32(p + offsets[0],
        sizeof(struct psm__count_info) / sizeof(psm__i32));

#ifndef PSM_FAST_AND_DANGEROUS
  {
    int err = psm__verify_count_info(
        (const struct psm__count_info *)(p + offsets[0]));
    if (err != PSM__OK)
      return err;
  }
#endif

  int i = 0;
#ifdef PSM_FAST_AND_DANGEROUS
  (void)n; (void)prev_end;
#define psm__predicate_static(TYPE, MEMBER, COUNT) \
    ms->MEMBER = (TYPE *)(p + offsets[i++]);
#define psm__predicate_dynamic(TYPE, MEMBER, COUNT_MEMBER) \
    ms->MEMBER = (TYPE *)(p + offsets[i++]);
#else
#define psm__predicate_static(TYPE, MEMBER, COUNT) { \
    psm__bounds_check_static(TYPE, COUNT, offsets, i, n) \
    psm_size _static_end = offsets[i] + sizeof(TYPE) * (COUNT); \
    if (_static_end > prev_end) prev_end = _static_end; \
    ms->MEMBER = (TYPE *)(p + offsets[i]); \
    i++; \
  }
#define psm__predicate_dynamic(TYPE, MEMBER, COUNT_MEMBER) { \
    psm__bounds_check_dynamic(TYPE, COUNT_MEMBER, offsets, i, n, ms->count_info) \
    if (offsets[i] < prev_end) { \
      PSM__LOGF("section[%d] not monotonic: off=%u prev=%u sz=%u", \
          i, (unsigned)offsets[i], (unsigned)prev_end, \
          (unsigned)_sz); \
      return PSM__ERR_FILE_CORRUPT; \
    } \
    prev_end = offsets[i] + _sz; \
    ms->MEMBER = _sz ? (TYPE *)(p + offsets[i]) : NULL; \
    i++; \
  }
#endif

  PSM__SECTIONS_V30(psm__predicate_static, psm__predicate_dynamic)
  if (ver < csmMocVersion_33) goto done;
  PSM__SECTIONS_V33(psm__predicate_static, psm__predicate_dynamic)
  if (ver < csmMocVersion_42) goto done;
  PSM__SECTIONS_V42(psm__predicate_static, psm__predicate_dynamic)
  if (ver < csmMocVersion_50) goto done;
  PSM__SECTIONS_V50(psm__predicate_static, psm__predicate_dynamic)
  if (ver < csmMocVersion_53) goto done;

  PSM__SECTIONS_V53(psm__predicate_static, psm__predicate_dynamic)

done:
#undef psm__predicate_static
#undef psm__predicate_dynamic

  if (needs_bswap)
    psm__bswap_model_data(ver, ms);

  return PSM__OK;
}

static int
psm__has_moc_consistency(const psm__u8 *p, psm_size n)
{
  psm__u8 ver, endian_flag;
  psm__i32 needs_bswap;
  psm_size header_size, sec_count;
  psm__u32 *offsets;
  struct psm__count_info *cnt = NULL;
  int result = PSM__OK;

  PSM__FAILM(n < sizeof(struct psm__moc3_header),
      PSM__ERR_INVALID_DATA, "buffer too small");
  PSM__FAILM(memcmp(p, PSM__MOC3_MAGIC, PSM__MOC3_MAGIC_SIZE) != 0,
      PSM__ERR_FILE_UNRECOGNIZED, "unknown magic");

  ver = *(p + PSM__MOC3_MAGIC_SIZE);
  PSM__FAILM(ver == csmMocVersion_Unknown,
      PSM__ERR_FILE_CORRUPT, "invalid MOC3 version");
  PSM__FAILM(ver > csmMocVersion_53,
      PSM__ERR_FILE_CORRUPT, "unsupported MOC3 version");

  endian_flag = p[offsetof(struct psm__moc3_header, endian_flag)];
  needs_bswap = psm__is_le() != (endian_flag == 0);

  if (ver >= csmMocVersion_53) {
    header_size = sizeof(struct psm__moc3_data_v53);
    sec_count = sizeof(((struct psm__moc3_data_v53 *)0)->offsets) / sizeof(psm__u32);
  } else {
    header_size = sizeof(struct psm__moc3_data_v52);
    sec_count = sizeof(((struct psm__moc3_data_v52 *)0)->offsets) / sizeof(psm__u32);
  }

  PSM__FAILM(n < header_size,
      PSM__ERR_INVALID_DATA, "buffer too small for header");

  offsets = (psm__u32 *)(p + sizeof(struct psm__moc3_header));

  if (needs_bswap)
    psm__bswap_many_32(offsets, sec_count);

  /* Validate section offsets */
  for (psm_size i = 0; i < sec_count; i++) {
    if ((psm__i32)offsets[i] < 0 || offsets[i] > n) {
      PSM__LOGF("section offset [%u] invalid", (unsigned)i);
      result = PSM__ERR_FILE_CORRUPT;
      goto restore;
    }
  }

  /* Validate count_info section first */
  if ((offsets[0] & 3) != 0) {
    PSM__LOG("count_info misaligned");
    result = PSM__ERR_FILE_CORRUPT;
    goto restore;
  }
  if (offsets[0] > n || n - offsets[0] < sizeof(struct psm__count_info)) {
    PSM__LOG("count_info out of bounds");
    result = PSM__ERR_FILE_CORRUPT;
    goto restore;
  }

  cnt = (struct psm__count_info *)(p + offsets[0]);
  if (needs_bswap)
    psm__bswap_many_32(cnt, sizeof(struct psm__count_info) / sizeof(psm__i32));

  result = psm__verify_count_info(cnt);
  if (result != PSM__OK)
    goto restore;

  /* Validate all sections using bounds-checking predicates */
  {
    int i = 0;

#define psm__predicate_static(TYPE, MEMBER, COUNT) { \
    psm__bounds_check_static(TYPE, COUNT, offsets, i, n) \
    i++; \
  }
#define psm__predicate_dynamic(TYPE, MEMBER, COUNT_MEMBER) { \
    psm__bounds_check_dynamic(TYPE, COUNT_MEMBER, offsets, i, n, cnt) \
    i++; \
  }

    PSM__SECTIONS_V30(psm__predicate_static, psm__predicate_dynamic)
    if (ver < csmMocVersion_33) goto restore;

    PSM__SECTIONS_V33(psm__predicate_static, psm__predicate_dynamic)
    if (ver < csmMocVersion_42) goto restore;

    PSM__SECTIONS_V42(psm__predicate_static, psm__predicate_dynamic)
    if (ver < csmMocVersion_50) goto restore;

    PSM__SECTIONS_V50(psm__predicate_static, psm__predicate_dynamic)
    if (ver < csmMocVersion_53) goto restore;

    PSM__SECTIONS_V53(psm__predicate_static, psm__predicate_dynamic)

#undef psm__predicate_static
#undef psm__predicate_dynamic
  }

restore:
  if (needs_bswap) {
    if (cnt)
      psm__bswap_many_32(cnt,
          sizeof(struct psm__count_info) / sizeof(psm__i32));
    psm__bswap_many_32(offsets, sec_count);
  }

  return result;
}

static int
psm__revive_moc_in_place(struct psm__moc3_data **moc, psm__u8 *p, psm_size n)
{
  struct psm__moc3_data *moc3_data;
  psm_size off = 0;

  PSM__FAILM(memcmp(p, PSM__MOC3_MAGIC, PSM__MOC3_MAGIC_SIZE) != 0,
      PSM__ERR_FILE_CORRUPT, "unknown magic");

  psm__u8 ver = *(p + PSM__MOC3_MAGIC_SIZE);
  PSM__FAILM(ver > csmMocVersion_53,
      PSM__ERR_FILE_CORRUPT, "unsupported MOC3 version");

  psm__u8 endian_flag = p[offsetof(struct psm__moc3_header, endian_flag)];
  PSM__FAILM(endian_flag != 0 && endian_flag != 1,
      PSM__ERR_FILE_CORRUPT, "invalid endian flag");

  if (ver >= csmMocVersion_53) {
    off = offsetof(struct psm__moc3_data_v53, sections);
    PSM__FAILM(n < off + sizeof(struct psm__moc3_data),
        PSM__ERR_FILE_CORRUPT, "buffer too small");

    struct psm__moc3_data_v53 *lay = (struct psm__moc3_data_v53 *)p;
    moc3_data = &lay->sections.source;

    moc3_data->header       = &lay->header;
    moc3_data->offsets      = lay->offsets;
    moc3_data->sections = &lay->sections;
    lay->header.data = moc3_data;
  } else
  {
    off = offsetof(struct psm__moc3_data_v52, sections);
    PSM__FAILM(n < off + sizeof(struct psm__moc3_data),
        PSM__ERR_FILE_CORRUPT, "buffer too small");

    struct psm__moc3_data_v52 *lay = (struct psm__moc3_data_v52 *)p;
    moc3_data = &lay->sections.source;

    moc3_data->header       = &lay->header;
    moc3_data->offsets      = lay->offsets;
    moc3_data->sections = &lay->sections;
    lay->header.data = moc3_data;
  }

  psm__i32 is_le = psm__is_le();
  psm__i32 needs_bswap = is_le != (moc3_data->header->endian_flag == 0);
  if (needs_bswap)
    moc3_data->header->endian_flag = (is_le == 0);

  PSM__FAILM(psm__init_moc3_sections(moc3_data, p, n, off,
          needs_bswap) != PSM__OK,
      PSM__ERR_FILE_CORRUPT, "model data init failed");

  struct psm__sections *src = moc3_data->sections;
  struct psm__count_info *cnt = src->count_info;

#ifndef PSM_FAST_AND_DANGEROUS
  PSM__FAILM(psm__verify_indices(ver, src) != PSM__OK,
      PSM__ERR_FILE_CORRUPT, "source index validation failed");
#endif

  psm__i32 count = cnt->art_meshes;
  if (count > 0) {
    psm__i32 *mb = src->art_mesh_src.mask_off;
    psm__i32 *mc = src->art_mesh_src.mask_len;
    psm__i32 *mi = src->mask_src.art_mesh_idx;

    if (!mb || !mc || !mi)
      goto skip_mask_processing;

    for (psm__i32 i = 0; i < count; i++) {
      psm__i32 m_cnt = mc[i];
      if (m_cnt <= 0 || !psm__check_offset_range(mb[i], m_cnt, cnt->masks))
        continue;
      psm__i32 *masks = &mi[mb[i]];
      psm__i32 valid = m_cnt;

      if (m_cnt > 1) {
        psm__i32 w = 0;
        for (psm__i32 j = 0; j < valid - 1; j++) {
          while (w < valid - 1 && masks[w] >= 0)
            w++;
          if (w < valid - 1) {
            memmove(&masks[w], &masks[w + 1],
                (valid - w - 1) * sizeof(psm__i32));
            valid--;
          }
        }
      }
      if (valid > 0 && masks[valid - 1] < 0)
        valid--;
      mc[i] = valid;
    }
  }
skip_mask_processing:

  count = cnt->parts;
  if (count > 0 && src->part_src.id && src->part_src.id_runtime) {
    for (psm__i32 i = 0; i < count; i++)
      src->part_src.id_runtime[i] =
          ((struct psm__id *)src->part_src.id)[i].data;
  }

  count = cnt->deformers;
  if (count > 0 && src->deformer_src.id && src->deformer_src.id_runtime) {
    for (psm__i32 i = 0; i < count; i++)
      src->deformer_src.id_runtime[i] =
          ((struct psm__id *)src->deformer_src.id)[i].data;
  }

  count = cnt->art_meshes;
  if (count > 0 && src->art_mesh_src.id && src->art_mesh_src.id_runtime) {
    for (psm__i32 i = 0; i < count; i++) {
      src->art_mesh_src.id_runtime[i] =
          ((struct psm__id *)src->art_mesh_src.id)[i].data;
      if (src->uv_src.xy && src->art_mesh_src.uv_off) {
        psm__i32 ub = src->art_mesh_src.uv_off[i];
        if (psm__check_idx(ub, cnt->uvs))
          src->art_mesh_src.uv_runtime[i] = &src->uv_src.xy[ub];
      }
      if (src->indices_src.index && src->art_mesh_src.indices_off) {
        psm__i32 io = src->art_mesh_src.indices_off[i];
        if (psm__check_idx(io, cnt->indices))
          src->art_mesh_src.position_idx_runtime[i] =
              &src->indices_src.index[io];
      }
      if (src->mask_src.art_mesh_idx && src->art_mesh_src.mask_off) {
        psm__i32 mb2 = src->art_mesh_src.mask_off[i];
        if (psm__check_idx(mb2, cnt->masks))
          src->art_mesh_src.drawable_mask_runtime[i] =
              &src->mask_src.art_mesh_idx[mb2];
      }
    }
  }

  count = cnt->parameters;
  if (count > 0 && src->param_src.id && src->param_src.id_runtime) {
    for (psm__i32 i = 0; i < count; i++)
      src->param_src.id_runtime[i] =
          ((struct psm__id *)src->param_src.id)[i].data;
  }

  count = cnt->glues;
  if (count > 0 && src->glue_src.id_runtime && src->glue_src.id) {
    for (psm__i32 i = 0; i < count; i++)
      src->glue_src.id_runtime[i] =
          ((struct psm__id *)src->glue_src.id)[i].data;
  }

  if (ver >= csmMocVersion_53) {
    count = cnt->offscreens;
    if (count > 0 && src->offscreen_src.drawable_mask_runtime &&
        src->mask_src.art_mesh_idx && src->offscreen_src.mask_off) {
      for (psm__i32 i = 0; i < count; i++) {
        psm__i32 ob = src->offscreen_src.mask_off[i];
        if (psm__check_idx(ob, cnt->masks))
          src->offscreen_src.drawable_mask_runtime[i] =
              &src->mask_src.art_mesh_idx[ob];
      }
    }
  }

  if (src->canvas_info && (src->canvas_info->flag &
          PSM__CANVAS_FLAG_Y_REVERSED) == 0) {
    psm__u16 *pos_idx = src->indices_src.index;
    psm__i32 *idx_begin = src->art_mesh_src.indices_off;
    psm__i32 *idx_cnt = src->art_mesh_src.indices_len;

    if (!pos_idx || !idx_begin || !idx_cnt)
      goto skip_y_reversal;

    count = cnt->art_meshes;
    for (psm__i32 i = 0; i < count; i++) {
      psm__i32 ic = idx_cnt[i];
      psm__i32 ib = idx_begin[i];
      if (ic <= 0 || !psm__check_offset_range(ib, ic, cnt->indices))
        continue;
      psm__u16 *idx = &pos_idx[ib];
      for (psm__i32 j = 0; j + 2 < ic; j += 3) {
        psm__u16 tmp = idx[j];
        idx[j] = idx[j + 2];
        idx[j + 2] = tmp;
      }
    }

    psm__f32 *uv_xy = src->uv_src.xy;
    psm__i32 *vert_cnt = src->art_mesh_src.vertex_count;
    psm__i32 *uv_off = src->art_mesh_src.uv_off;

    if (uv_xy && vert_cnt && uv_off) {
      for (psm__i32 i = 0; i < count; i++) {
        psm__i32 vc = vert_cnt[i];
        psm__i32 ub = uv_off[i];
        if (vc <= 0 || ub < 0 || (psm__u32)ub + 2u * (psm__u32)vc >
                (psm__u32)cnt->uvs)
          continue;
        psm__f32 *uv = &uv_xy[ub];
        for (psm__i32 j = 0; j < vc; j++)
          uv[j * 2 + 1] = 1.0f - uv[j * 2 + 1];
      }
    }
  }
skip_y_reversal:

  *moc = moc3_data;
  return PSM__OK;
}


PSMDEF csmMocVersion
csmGetMocVersion(const void *address, unsigned int size)
{
  csmMocVersion ver;
  if (psm__get_moc_version(&ver, (const psm__u8 *)address, size) != PSM__OK)
    return csmMocVersion_Unknown;
  return ver;
}

PSMDEF int
csmHasMocConsistency(void *address, unsigned int size)
{
  return psm__has_moc_consistency((const psm__u8 *)address, size) == PSM__OK;
}

PSMDEF csmMoc *
csmReviveMocInPlace(void *address, unsigned int size)
{
  static psm__i32 first_call = 1;
  if (first_call) {
    psm__debug_print(PSM__LOG_OFF, "Sakura2D Purism Core version " PSM__VERFMT
        " (compat " PSM__VERFMT ")\n", PSM__VERARG(PSM_TRUE_VERSION),
        PSM__VERARG(PSM_COMPAT_VERSION));
    first_call = 0;
  }

  struct psm__moc3_data *moc;
  int err = psm__revive_moc_in_place(&moc, (psm__u8 *)address, size);
  PSM__FAILM(err != PSM__OK, NULL, "could not revive MOC3");
  return (csmMoc *)address;
}
