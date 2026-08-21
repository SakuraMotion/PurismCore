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
#include "verify.h"

#define PSM__MOC3_MAGIC      "MOC3"
#define PSM__MOC3_MAGIC_SIZE 4

static bool
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
  return ((x >> 24) & 0x000000FF) | ((x >> 8) & 0x0000FF00) |
         ((x << 8) & 0x00FF0000) | ((x << 24) & 0xFF000000);
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
    psm__u8 *p, psm_size n, psm_size off, bool needs_bswap)
{
  struct psm__sections *ms = moc3_data->sections;

  psm__u32 *offsets = moc3_data->offsets;
  psm__u8   ver = moc3_data->header->version;
  psm_size  sec_count;

  if (ver >= csmMocVersion_53)
    sec_count =
        sizeof(((struct psm__moc3_data_v53 *)0)->offsets) / sizeof(psm__u32);
  else
    sec_count =
        sizeof(((struct psm__moc3_data_v52 *)0)->offsets) / sizeof(psm__u32);

  if (needs_bswap)
    psm__bswap_many_32(offsets, sec_count);

  psm_size ci_ints = PSM__COUNT_INFO_INTS(ver);

  PSM__FAILM((offsets[0] & 3) != 0,
      PSM__ERR_FILE_CORRUPT, "count_info misaligned");
  PSM__FAILM(offsets[0] > n || n - offsets[0] < ci_ints * sizeof(psm__i32),
      PSM__ERR_FILE_CORRUPT, "count_info offset oob");
  PSM__FAILM(offsets[0] < off,
      PSM__ERR_FILE_CORRUPT, "count_info before header end");

  if (needs_bswap)
    psm__bswap_many_32(p + offsets[0], ci_ints);

#ifndef PSM_FAST_AND_DANGEROUS
  {
    int err = psm__verify_count_info(ver,
        (const struct psm__count_info *)(p + offsets[0]));
    if (err != PSM__OK)
      return err;
  }
  if (psm__verify_sections(ms, p, offsets, n, off, ver, true) != PSM__OK)
    return PSM__ERR_FILE_CORRUPT;
#else
  psm__verify_sections(ms, p, offsets, n, off, ver, false);
#endif

  if (needs_bswap)
    psm__bswap_model_data(ver, ms);

  return PSM__OK;
}

static int
psm__has_moc_consistency(const psm__u8 *p, psm_size n)
{
  psm__u8 ver, endian_flag;
  bool    needs_bswap;

  psm_size  header_size, sec_count, off;
  psm__u32 *offsets;

  struct psm__count_info *cnt = NULL;

  int r = PSM__OK;

  PSM__FAILM(!p || !n, PSM__ERR_INVALID_PARAMETER, "buffer or size is NULL");

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
    sec_count =
        sizeof(((struct psm__moc3_data_v53 *)0)->offsets) / sizeof(psm__u32);
    off = offsetof(struct psm__moc3_data_v53, sections);
  } else {
    header_size = sizeof(struct psm__moc3_data_v52);
    sec_count =
        sizeof(((struct psm__moc3_data_v52 *)0)->offsets) / sizeof(psm__u32);
    off = offsetof(struct psm__moc3_data_v52, sections);
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
      r = PSM__ERR_FILE_CORRUPT;
      goto restore;
    }
  }

  /* Validate count_info section first */
  if ((offsets[0] & 3) != 0) {
    PSM__LOG("count_info misaligned");
    r = PSM__ERR_FILE_CORRUPT;
    goto restore;
  }
  if (offsets[0] > n ||
      n - offsets[0] < PSM__COUNT_INFO_INTS(ver) * sizeof(psm__i32)) {
    PSM__LOG("count_info out of bounds");
    r = PSM__ERR_FILE_CORRUPT;
    goto restore;
  }
  if (offsets[0] < off) {
    PSM__LOG("count_info before header end");
    r = PSM__ERR_FILE_CORRUPT;
    goto restore;
  }

  cnt = (struct psm__count_info *)(p + offsets[0]);
  if (needs_bswap)
    psm__bswap_many_32(cnt, PSM__COUNT_INFO_INTS(ver));

  r = psm__verify_count_info(ver, cnt);
  if (r != PSM__OK)
    goto restore;

  {
    struct psm__sections tmp;
    memset(&tmp, 0, sizeof tmp);
    r =
        psm__verify_sections(&tmp, (psm__u8 *)p, offsets, n, off, ver, true);
    if (r != PSM__OK)
      goto restore;

    if (needs_bswap)
      psm__bswap_model_data(ver, &tmp);
    r = psm__verify_idx(ver, &tmp);
    if (needs_bswap)
      psm__bswap_model_data(ver, &tmp);
  }

restore:
  if (needs_bswap) {
    if (cnt)
      psm__bswap_many_32(cnt, PSM__COUNT_INFO_INTS(ver));
    psm__bswap_many_32(offsets, sec_count);
  }

  return r;
}

static int
psm__revive_moc_in_place(struct psm__moc3_data **moc, psm__u8 *p, psm_size n)
{
  struct psm__moc3_data *moc3_data;

  psm_size off = 0;

  PSM__FAILM(!p || !n, PSM__ERR_INVALID_PARAMETER, "buffer or size is NULL");

  PSM__FAILM(n < sizeof(struct psm__moc3_header),
      PSM__ERR_INVALID_DATA, "buffer too small");
  PSM__FAILM(memcmp(p, PSM__MOC3_MAGIC, PSM__MOC3_MAGIC_SIZE) != 0,
      PSM__ERR_FILE_UNRECOGNIZED, "unknown magic");

  psm__u8 ver = *(p + PSM__MOC3_MAGIC_SIZE);
  PSM__FAILM(ver == csmMocVersion_Unknown,
      PSM__ERR_FILE_CORRUPT, "invalid MOC3 version");
  PSM__FAILM(ver > csmMocVersion_53,
      PSM__ERR_FILE_CORRUPT, "unsupported MOC3 version");

  psm__u8 endian_flag = p[offsetof(struct psm__moc3_header, endian_flag)];
  PSM__FAILM(endian_flag != 0 && endian_flag != 1,
      PSM__ERR_FILE_CORRUPT, "invalid endian flag");

  if (ver >= csmMocVersion_53) {
    off = offsetof(struct psm__moc3_data_v53, sections);
    PSM__FAILM(n < off + sizeof(struct psm__moc3_data),
        PSM__ERR_FILE_CORRUPT, "buffer too small");

    struct psm__moc3_data_v53 *layout = (struct psm__moc3_data_v53 *)p;
    moc3_data = &layout->sections.source;

    moc3_data->header = &layout->header;
    moc3_data->offsets = layout->offsets;
    moc3_data->sections = &layout->sections;
    layout->header.data = moc3_data;
  } else {
    off = offsetof(struct psm__moc3_data_v52, sections);
    PSM__FAILM(n < off + sizeof(struct psm__moc3_data),
        PSM__ERR_FILE_CORRUPT, "buffer too small");

    struct psm__moc3_data_v52 *layout = (struct psm__moc3_data_v52 *)p;
    moc3_data = &layout->sections.source;

    moc3_data->header = &layout->header;
    moc3_data->offsets = layout->offsets;
    moc3_data->sections = &layout->sections;
    layout->header.data = moc3_data;
  }

  bool is_le = psm__is_le();
  bool needs_bswap = is_le != (moc3_data->header->endian_flag == 0);
  if (needs_bswap)
    moc3_data->header->endian_flag = !is_le;

  PSM__FAILM(psm__init_moc3_sections(moc3_data, p, n, off,
                 needs_bswap) != PSM__OK,
      PSM__ERR_FILE_CORRUPT, "model data init failed");

  struct psm__sections   *src = moc3_data->sections;
  struct psm__count_info *cnt = src->count_info;

#ifndef PSM_FAST_AND_DANGEROUS
  PSM__FAILM(psm__verify_idx(ver, src) != PSM__OK,
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
      psm__i32  valid = m_cnt;

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
      if (src->idx_src.idx && src->art_mesh_src.idx_off) {
        psm__i32 io = src->art_mesh_src.idx_off[i];
        if (psm__check_idx(io, cnt->idx))
          src->art_mesh_src.pos_idx_runtime[i] =
              &src->idx_src.idx[io];
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
    psm__u16 *pos_idx = src->idx_src.idx;
    psm__i32 *idx_off = src->art_mesh_src.idx_off;
    psm__i32 *idx_cnt = src->art_mesh_src.idx_len;

    if (!pos_idx || !idx_off || !idx_cnt)
      goto skip_y_reversal;

    count = cnt->art_meshes;
    for (psm__i32 i = 0; i < count; i++) {
      psm__i32 ic = idx_cnt[i];
      psm__i32 ib = idx_off[i];
      if (ic <= 0 || !psm__check_offset_range(ib, ic, cnt->idx))
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
        if (vc <= 0 || ub < 0 ||
            (psm__u32)ub + 2u * (psm__u32)vc > (psm__u32)cnt->uvs)
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
  static bool first_call = true;
  if (first_call) {
    psm__debug_print(PSM__LOG_OFF,
        "Sakura2D Purism Core version " PSM__VERFMT " (compat " PSM__VERFMT
        ")\n",
        PSM__VERARG(PSM_TRUE_VERSION), PSM__VERARG(PSM_COMPAT_VERSION));
    first_call = false;
  }

  struct psm__moc3_data *moc;

  int err = psm__revive_moc_in_place(&moc, (psm__u8 *)address, size);

  /*
   * Record the outcome in the header's scratch space so a caller can ask
   * csmGetMocError(address) why a load failed, even though we return NULL.
   * Only safe once the buffer is known to hold a full header.
   */
  if (size >= sizeof(struct psm__moc3_header))
    ((struct psm__moc3_header *)address)->last_error = err;

  PSM__FAILM(err != PSM__OK, NULL, "could not revive MOC3");
  return (csmMoc *)address;
}

/* Purism Core extension: see PurismCore.h. */
PSMDEF csmError
csmGetMocError(const csmMoc *moc)
{
  if (!moc)
    return csmError_NoError;
  return (csmError)((const struct psm__moc3_header *)moc)->last_error;
}
