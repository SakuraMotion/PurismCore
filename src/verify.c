/*
 * Purism Core: MOC3 load-time validation
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "private.h"
#include "array.h"
#include "debug.h"
#include "deformer.h"
#include "error.h"
#include "moc3.h"
#include "verify.h"

PSM__DEF int
psm__verify_count_info(psm__u8 ver, const struct psm__count_info *cnt)
{
  const psm__i32 *field = (const psm__i32 *)cnt;

  psm__i32 n = PSM__COUNT_INFO_INTS(ver);
  for (psm__i32 i = 0; i < n; i++)
    PSM__FAIL(field[i] < 0, PSM__ERR_FILE_CORRUPT,
        "count field[%d] = %d is negative", i, field[i]);

  PSM__FAILM(((psm__u32)cnt->warps + (psm__u32)cnt->rotations) !=
                 (psm__u32)cnt->deformers,
      PSM__ERR_FILE_CORRUPT, "deformer count mismatch");
  return PSM__OK;
}

/* Section bounds: 8-byte aligned, count sane,
 * [offset, offset+size) in [0,n). */
// clang-format off
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
// clang-format on

PSM__DEF int
psm__verify_sections(struct psm__sections *ms, psm__u8 *p,
    psm__u32 *offsets, psm_size n, psm_size off, psm__u8 ver, bool bounds)
{
  psm_size prev_end = off;
  psm_size ci_bytes = (psm_size)PSM__COUNT_INFO_INTS(ver) * sizeof(psm__i32);

  int i = 0;

  /*
   * count_info (section 0) is padded in-struct to 256 bytes but only ci_bytes
   * are on disk; use ci_bytes for the monotonic cursor. Dynamic sections that
   * are empty get a NULL pointer (the runtime relies on that).
   */
// clang-format off
#define psm__predicate_static(TYPE, MEMBER, COUNT) { \
    if (bounds) { \
      psm__bounds_check_static(TYPE, COUNT, offsets, i, n) \
      psm_size _ssz = (i == 0) ? ci_bytes : sizeof(TYPE) * (COUNT); \
      psm_size _static_end = offsets[i] + _ssz; \
      if (_static_end > prev_end) prev_end = _static_end; \
    } \
    ms->MEMBER = (TYPE *)(p + offsets[i]); \
    i++; \
  }
#define psm__predicate_dynamic(TYPE, MEMBER, COUNT_MEMBER) { \
    if (bounds) { \
      psm__bounds_check_dynamic(TYPE, COUNT_MEMBER, offsets, i, n, ms->count_info) \
      if (offsets[i] < prev_end) { \
        PSM__LOGF("section[%d] not monotonic: off=%u prev=%u sz=%u", \
            i, (unsigned)offsets[i], (unsigned)prev_end, (unsigned)_sz); \
        return PSM__ERR_FILE_CORRUPT; \
      } \
      prev_end = offsets[i] + _sz; \
      ms->MEMBER = _sz ? (TYPE *)(p + offsets[i]) : NULL; \
    } else { \
      ms->MEMBER = (TYPE *)(p + offsets[i]); \
    } \
    i++; \
  }
// clang-format on

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
  return PSM__OK;
}

static psm__u32
psm__binding_keyform_count(const struct psm__sections *src,
    const struct psm__count_info *cnt, psm__i32 bi)
{
  const struct psm__binding_src *bs = &src->binding_src;

  if (bi < 0 || bi >= cnt->bindings || !bs->key_table_idx_off ||
      !bs->key_table_idx_len || !src->key_table_idx_src.idx ||
      !src->key_table_src.keys_len)
    return 1;

  psm__i32 off = bs->key_table_idx_off[bi];
  psm__i32 len = bs->key_table_idx_len[bi];
  if (len <= 0 || !psm__valid_range(off, len, cnt->key_table_idx))
    return 1;

  psm__u32 prod = 1;
  for (psm__i32 k = 0; k < len; k++) {
    psm__i32 kt = src->key_table_idx_src.idx[off + k];
    if (!psm__valid_idx(kt, cnt->key_tables))
      continue;
    psm__i32 kc = src->key_table_src.keys_len[kt];
    if (kc <= 1)
      continue;  /* 0/1-key tables do not extend the keyform grid */
    if (prod > 0x7FFFFFFFu / (psm__u32)kc)
      return 0x7FFFFFFF;  /* saturate: exceeds any valid key_len */
    prod *= (psm__u32)kc;
  }
  return prod;
}

static int
psm__verify_bs_windows(const struct psm__sections *src,
    const struct psm__blend_src *bs, psm__i32 shape_count,
    psm__i32        target_keyforms,
    const psm__i32 *key_pos_off, const psm__i32 *vertex_count,
    psm__i32 target_count, psm__i32 max_pos,
    const psm__i32 *key_mul_off, psm__i32 max_mul_colors,
    const psm__i32 *key_scr_off, psm__i32 max_scr_colors)
{
  const struct psm__count_info *cnt = src->count_info;

  const psm__i32 *kt_idx = src->blend_binding_src.key_table_idx;
  const psm__i32 *ks_off = src->blend_binding_src.key_bs_off;
  const psm__i32 *kc_len = src->blend_key_table_src.keys_len;

  if (!bs->target_idx || !bs->bs_binding_off || !bs->bs_binding_len ||
      !kt_idx || !ks_off || !kc_len)
    return PSM__OK;

  for (psm__i32 i = 0; i < shape_count; i++) {
    psm__i32 bo = bs->bs_binding_off[i];
    psm__i32 bn = bs->bs_binding_len[i];
    if (bn <= 0 || !psm__valid_range(bo, bn, cnt->blend_bindings))
      continue;

    /* pos is gated on the target existing and having vertices */
    psm__i32 vc = 0;
    bool     do_pos = false;
    if (key_pos_off && vertex_count) {
      psm__i32 ti = bs->target_idx[i];
      if (psm__valid_idx(ti, target_count) && vertex_count[ti] > 0) {
        vc = vertex_count[ti];
        do_pos = true;
      }
    }

    for (psm__i32 j = 0; j < bn; j++) {
      psm__i32 bb = bo + j;
      psm__i32 kti = kt_idx[bb];
      if (!psm__valid_idx(kti, cnt->blend_key_tables))
        continue;
      psm__i32 kc = kc_len[kti];
      if (kc < 1) kc = 1;   /* a 0/1-key binding still reads keyform index 0 */
      psm__i32 so = ks_off[bb];

      /* F4: the whole keyform window must fit */
      PSM__FAIL(!psm__valid_range(so, kc, target_keyforms),
          PSM__ERR_FILE_CORRUPT,
          "bs binding[%d] keyform window off=%d kc=%d > max=%d",
          bb, so, kc, target_keyforms);

      if (!do_pos && !key_mul_off && !key_scr_off)
        continue;   /* keyform-only target */

      for (psm__i32 k = 0; k < kc; k++) {
        psm__i32 ki = so + k;
        if (!psm__valid_idx(ki, target_keyforms))
          continue;
        if (do_pos) {
          psm__i32 po = key_pos_off[ki];
          PSM__FAIL(po < 0 || (psm__u32)po + 2u * (psm__u32)vc >
                                  (psm__u32)max_pos,
              PSM__ERR_FILE_CORRUPT,
              "bs pos window ki=%d po=%d vc=%d max=%d", ki, po, vc, max_pos);
        }
        if (key_mul_off) {
          psm__i32 ci = key_mul_off[ki];
          PSM__FAIL(ci >= max_mul_colors, PSM__ERR_FILE_CORRUPT,
              "bs mul color window ki=%d ci=%d max=%d", ki, ci, max_mul_colors);
        }
        if (key_scr_off) {
          psm__i32 ci = key_scr_off[ki];
          PSM__FAIL(ci >= max_scr_colors, PSM__ERR_FILE_CORRUPT,
              "bs scr color window ki=%d ci=%d max=%d", ki, ci, max_scr_colors);
        }
      }
    }
  }
  return PSM__OK;
}

static int
psm__verify_offscreen_window(const struct psm__sections *src,
    psm__i32 offscreen_count, psm__i32 offscreen_keyforms)
{
  const struct psm__count_info *cnt = src->count_info;

  const psm__i32 *owner = src->offscreen_src.owner_idx;
  const psm__i32 *binding_idx = src->part_src.binding_idx;
  const psm__i32 *keyform_off = src->part_src.keyform_off;
  const psm__i32 *key_idx = src->part_key_src.key_idx;
  const psm__i32 *mul_off = src->offscreen_key_src.key_mul_color_off;

  if (!owner || !binding_idx || !keyform_off || !key_idx)
    return PSM__OK;

  for (psm__i32 i = 0; i < offscreen_count; i++) {
    psm__i32 oi = owner[i];
    if (!psm__valid_idx(oi, cnt->parts))
      continue;
    psm__i32 kbi = keyform_off[oi];
    if (!psm__valid_idx(kbi, cnt->part_keyforms))
      continue;
    psm__i32 ki = key_idx[kbi];
    if (ki < 0)
      continue;  /* no offscreen keyforms for this surface */

    psm__i32 prod = (psm__i32)psm__binding_keyform_count(src, cnt,
        binding_idx[oi]);

    PSM__FAIL(!psm__valid_range(ki, prod, offscreen_keyforms),
        PSM__ERR_FILE_CORRUPT,
        "offscreen[%d] keyform window ki=%d prod=%d > max=%d",
        i, ki, prod, offscreen_keyforms);

    if (mul_off) {
      psm__i32 cb = mul_off[ki];
      /*
       * gather_offscreens indexes BOTH the mul and scr color pools with the
       * mul offset (it never reads key_scr_color_off), so cb+prod must fit
       * in both pools. cb < 0 means "no color" and is skipped at runtime.
       */
      PSM__FAIL(cb >= 0 &&
                    (!psm__valid_range(cb, prod, cnt->keyform_mul_colors) ||
                        !psm__valid_range(cb, prod, cnt->keyform_scr_colors)),
          PSM__ERR_FILE_CORRUPT,
          "offscreen[%d] color window cb=%d prod=%d > max(%d,%d)",
          i, cb, prod, cnt->keyform_mul_colors, cnt->keyform_scr_colors);
    }
  }
  return PSM__OK;
}

PSM__DEF int
psm__verify_idx(psm__u8 ver, const struct psm__sections *src)
{
  const struct psm__count_info *cnt = src->count_info;

// clang-format off
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
   * Validate the keyform grid covers the full combo span. The combo
   * builder reaches keyform index product(key_counts)-1, so key_len must be
   * at least that product. With the per-object keyform_off+key_len<=*_keyforms
   * range check this bounds every reachable keyform index inside the object's
   * declared keyforms.
   */
#define psm__check_key_combo(obj_src, keyform_total, obj_len) \
  for (psm__i32 _i = 0; _i < (obj_len); _i++) { \
    psm__i32 _bi = (obj_src).binding_idx[_i]; \
    if (_bi < 0 || _bi >= cnt->bindings) continue; \
    psm__u32 _prod = psm__binding_keyform_count(src, cnt, _bi); \
    psm__i32 _kl = (obj_src).key_len[_i]; \
    PSM__FAIL(_kl < 0 || _prod > (psm__u32)_kl, \
        PSM__ERR_FILE_CORRUPT, \
        "%s[%d] combo span %u > key_len %d", \
        #obj_src, _i, _prod, _kl); \
    psm__i32 _mc = 1 << psm__clamp_i32( \
        src->binding_src.key_table_idx_len[_bi], 0, PSM__MAX_KEY_TABLES); \
    PSM__FAIL(!psm__valid_range((obj_src).keyform_off[_i], _mc, (keyform_total)), \
        PSM__ERR_FILE_CORRUPT, \
        "%s[%d] keyform_off=%d + max_blend=%d > total %d", #obj_src, _i, \
        (obj_src).keyform_off[_i], _mc, (keyform_total)); \
  }
// clang-format on

  /* Part sources */
  for (psm__i32 i = 0; i < cnt->parts; i++) {
    psm__model_check_index(src->part_src.binding_idx, i, cnt->bindings);
    psm__model_check_range(src->part_src.keyform_off,
        src->part_src.key_len, i, cnt->part_keyforms);
    psm__model_check_index_or_neg1(
        src->part_src.parent_part_idx, i, cnt->parts);
  }
  psm__check_key_combo(src->part_src, cnt->part_keyforms, cnt->parts);

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
    psm__i32 col = src->warp_src.col[i];
    psm__i32 vc = src->warp_src.vertex_count[i];
    PSM__FAIL(row <= 0 || col <= 0, PSM__ERR_FILE_CORRUPT,
        "warp[%d] grid row=%d col=%d", i, row, col);
    /* row/col are only bounded > 0, so compute in unsigned (well-defined
     * wraparound); row+1 in int overflows when row == INT_MAX. */
    psm__u32 expect = ((psm__u32)row + 1u) * ((psm__u32)col + 1u);
    PSM__FAIL((psm__u32)vc != expect, PSM__ERR_FILE_CORRUPT,
        "warp[%d] vert_count=%d expected=%u", i, vc, (unsigned)expect);
  }

  psm__check_key_combo(src->warp_src, cnt->warp_keyforms, cnt->warps);

  /* Warp deformer keyform positions */
  for (psm__i32 i = 0; i < cnt->warps; i++) {
    psm__i32 off = src->warp_src.keyform_off[i];
    psm__i32 count = src->warp_src.key_len[i];
    psm__i32 vc = src->warp_src.vertex_count[i];
    for (psm__i32 j = 0; j < count; j++) {
      psm__i32 po = src->warp_key_src.key_pos_off[off + j];
      PSM__FAIL(po < 0 || (psm__u32)po + 2u * (psm__u32)vc >
                              (psm__u32)cnt->keyform_pos,
          PSM__ERR_FILE_CORRUPT,
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

  psm__check_key_combo(src->rotation_src, cnt->rotation_keyforms,
      cnt->rotations);

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
                          2u * (psm__u32)src->art_mesh_src.vertex_count[i] >
                      (psm__u32)cnt->uvs,
        PSM__ERR_FILE_CORRUPT,
        "art_mesh[%d]: UV [%d, +%d*2) oob (max %d)",
        i, src->art_mesh_src.uv_off[i],
        src->art_mesh_src.vertex_count[i], cnt->uvs);
    psm__model_check_range(src->art_mesh_src.idx_off,
        src->art_mesh_src.idx_len, i, cnt->idx);
    psm__model_check_range(src->art_mesh_src.mask_off,
        src->art_mesh_src.mask_len, i, cnt->masks);
  }

  psm__check_key_combo(src->art_mesh_src, cnt->art_mesh_keyforms,
      cnt->art_meshes);

  /*
   * Art mesh keyform position indices. Each keyform stores vc vertices
   * (2 floats each), so the readable span is [po, po + 2*vc); validate
   * the full span, not just the start index.
   */
  for (psm__i32 i = 0; i < cnt->art_meshes; i++) {
    psm__i32 off = src->art_mesh_src.keyform_off[i];
    psm__i32 count = src->art_mesh_src.key_len[i];
    psm__i32 vc = src->art_mesh_src.vertex_count[i];
    for (psm__i32 j = 0; j < count; j++) {
      psm__i32 po = src->art_mesh_key_src.key_pos_off[off + j];
      PSM__FAIL(po < 0 || (psm__u32)po + 2u * (psm__u32)vc >
                              (psm__u32)cnt->keyform_pos,
          PSM__ERR_FILE_CORRUPT,
          "art_mesh[%d] kf[%d] pos_off=%d vc=%d max=%d",
          i, j, po, vc, cnt->keyform_pos);
    }
  }

  /*
   * Triangle index VALUES. Each art mesh's index slice
   * [idx_off, idx_off+idx_len) contains
   * holds vertex indices into that mesh's own vertex array, so every index
   * must be < vertex_count. The library never dereferences these, but callers
   * receive them directly from csmGetDrawableIndices, so a malformed index
   * could cause an OOB read in a renderer.
   *
   * We reject at load time.
   */
  if (src->idx_src.idx) {
    for (psm__i32 i = 0; i < cnt->art_meshes; i++) {
      psm__i32 off = src->art_mesh_src.idx_off[i];
      psm__i32 len = src->art_mesh_src.idx_len[i];
      psm__i32 vc = src->art_mesh_src.vertex_count[i];
      for (psm__i32 j = 0; j < len; j++) {
        psm__i32 vi = (psm__i32)src->idx_src.idx[off + j];
        PSM__FAIL(vi >= vc, PSM__ERR_FILE_CORRUPT,
            "art_mesh[%d] index[%d]=%d >= vertex_count %d", i, j, vi, vc);
      }
    }
  }

  /* Parameter sources */
  for (psm__i32 i = 0; i < cnt->parameters; i++) {
    psm__model_check_range(src->param_src.key_table_off,
        src->param_src.key_table_len, i, cnt->key_tables);
  }

  /* Keyform binding sources */
  for (psm__i32 i = 0; i < cnt->bindings; i++) {
    psm__model_check_range(src->binding_src.key_table_idx_off,
        src->binding_src.key_table_idx_len, i, cnt->key_table_idx);
    psm__i32 pc = src->binding_src.key_table_idx_len[i];
    PSM__FAIL(pc < 0 || pc > PSM__MAX_KEY_TABLES, PSM__ERR_FILE_CORRUPT,
        "binding[%d] param_count=%d oob", i, pc);
  }

  /* Parameter binding index sources */
  for (psm__i32 i = 0; i < cnt->key_table_idx; i++) {
    psm__model_check_index(src->key_table_idx_src.idx, i, cnt->key_tables);
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

  psm__check_key_combo(src->glue_src, cnt->glue_keyforms, cnt->glues);

  /* Glue sources */
  for (psm__i32 i = 0; i < cnt->glues; i++) {
    psm__model_check_index(src->glue_src.binding_idx, i, cnt->bindings);
    psm__model_check_range(src->glue_src.keyform_off,
        src->glue_src.key_len, i, cnt->glue_keyforms);
    psm__model_check_index(src->glue_src.art_mesh_idx_a, i, cnt->art_meshes);
    psm__model_check_index(src->glue_src.art_mesh_idx_b, i, cnt->art_meshes);
    psm__model_check_range(src->glue_src.info_off,
        src->glue_src.info_len, i, cnt->glue_info);
    /* F5: glue info is consumed in (mesh0, mesh1) pairs -> must be even. */
    PSM__FAIL((src->glue_src.info_len[i] & 1) != 0, PSM__ERR_FILE_CORRUPT,
        "glue[%d]: odd info_len %d", i, src->glue_src.info_len[i]);
  }

  /* Glue position indices */
  if (src->glue_src.info_off && src->glue_src.info_len &&
      src->glue_src.art_mesh_idx_a && src->glue_src.art_mesh_idx_b &&
      src->glue_info_src.pos_idx && src->art_mesh_src.vertex_count) {
    for (psm__i32 i = 0; i < cnt->glues; i++) {
      psm__i32 m0 = src->glue_src.art_mesh_idx_a[i];
      psm__i32 m1 = src->glue_src.art_mesh_idx_b[i];
      if (m0 < 0 || m0 >= cnt->art_meshes || m1 < 0 || m1 >= cnt->art_meshes)
        continue;
      psm__i32 vc0 = src->art_mesh_src.vertex_count[m0];
      psm__i32 vc1 = src->art_mesh_src.vertex_count[m1];
      psm__i32 ib = src->glue_src.info_off[i];
      psm__i32 ic = src->glue_src.info_len[i];
      if (ib < 0 || ic <= 0 || ib + ic > cnt->glue_info)
        continue;
      for (psm__i32 j = 0; j < ic; j += 2) {
        psm__u16 p0 = src->glue_info_src.pos_idx[ib + j];
        PSM__FAIL(p0 >= (psm__u16)vc0, PSM__ERR_FILE_CORRUPT,
            "glue[%d] pos_idx[%d]=%u OOB (vc=%d)", i, j, p0, vc0);
        if (j + 1 < ic) {
          psm__u16 p1 = src->glue_info_src.pos_idx[ib + j + 1];
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
  if (src->draw_group_obj_src.type && src->draw_group_obj_src.idx) {
    for (psm__i32 i = 0; i < cnt->draw_items; i++) {
      psm__model_check_index_or_neg1(src->draw_group_obj_src.self_group_idx,
          i, cnt->draw_groups);
      psm__i32 t = src->draw_group_obj_src.type[i];
      psm__i32 oi = src->draw_group_obj_src.idx[i];
      PSM__FAIL(t != 0 && t != 1, PSM__ERR_FILE_CORRUPT,
          "draw_item[%d]: bad type %d", i, t);
      psm__i32 max = t ? cnt->parts : cnt->art_meshes;
      PSM__FAIL(oi < 0 || oi >= max, PSM__ERR_FILE_CORRUPT,
          "draw_item[%d]: index %d OOB (type=%d max=%d)", i, oi, t, max);
      /*
       * only part items (type 1) recurse into a child group via
       * self_group_idx, and at runtime that index must be valid. -1 is
       * allowed for art-mesh items (never used) but not for parts.
       */
      PSM__FAIL(t == 1 && src->draw_group_obj_src.self_group_idx[i] < 0,
          PSM__ERR_FILE_CORRUPT,
          "draw_item[%d]: part item has no group", i);
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
    psm__model_check_index(src->blend_binding_src.key_table_idx,
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

  /* Blend shape keyform-window bounds (v4.2 targets) */
// clang-format off
#define PSM__VERIFY(call) \
    { if ((call) != PSM__OK) return PSM__ERR_FILE_CORRUPT; }
// clang-format on

  PSM__VERIFY(psm__verify_bs_windows(src, &src->bs_warp_src, cnt->bs_warps,
      cnt->warp_keyforms,
      src->warp_key_src.key_pos_off, src->warp_src.vertex_count,
      cnt->warps, cnt->keyform_pos,
      src->warp_key_src.key_mul_color_off, cnt->keyform_mul_colors,
      src->warp_key_src.key_scr_color_off, cnt->keyform_scr_colors));

  PSM__VERIFY(psm__verify_bs_windows(src, &src->bs_art_mesh_src,
      cnt->bs_art_meshes, cnt->art_mesh_keyforms,
      src->art_mesh_key_src.key_pos_off, src->art_mesh_src.vertex_count,
      cnt->art_meshes, cnt->keyform_pos,
      src->art_mesh_key_src.key_mul_color_off, cnt->keyform_mul_colors,
      src->art_mesh_key_src.key_scr_color_off, cnt->keyform_scr_colors));

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

  /* Blend shape keyform-window bounds (v5.0 targets) */
  PSM__VERIFY(psm__verify_bs_windows(src, &src->bs_part_src, cnt->bs_parts,
      cnt->part_keyforms, NULL, NULL, 0, 0, NULL, 0, NULL, 0));
  PSM__VERIFY(psm__verify_bs_windows(src, &src->bs_glue_src, cnt->bs_glues,
      cnt->glue_keyforms, NULL, NULL, 0, 0, NULL, 0, NULL, 0));

  PSM__VERIFY(psm__verify_bs_windows(src, &src->bs_rotation_src,
      cnt->bs_rotations, cnt->rotation_keyforms,
      NULL, NULL, 0, 0,
      src->rotation_key_src.key_mul_color_off, cnt->keyform_mul_colors,
      src->rotation_key_src.key_scr_color_off, cnt->keyform_scr_colors));

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

  /* Blend shape keyform-window bounds (v5.3 targets) */
  PSM__VERIFY(psm__verify_bs_windows(src, &src->bs_offscreen_src,
      cnt->bs_offscreens, cnt->offscreen_keyforms,
      NULL, NULL, 0, 0,
      src->offscreen_key_src.key_mul_color_off, cnt->keyform_mul_colors,
      src->offscreen_key_src.key_scr_color_off, cnt->keyform_scr_colors));

  PSM__VERIFY(psm__verify_offscreen_window(src, cnt->offscreens,
      cnt->offscreen_keyforms));

#undef PSM__VERIFY

done_ver:
#undef psm__model_check_index
#undef psm__model_check_index_or_neg1
#undef psm__model_check_range

  return PSM__OK;
}
