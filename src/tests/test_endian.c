/*
 * Purism Core: big-endian round-trip differential test
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <stdint.h>

#define PURISM_CORE_STATIC
#include "../private.h"
#include "../error.h"
#include "../debug.h"
#include "../arena.h"
#include "../array.h"
#include "../math2.h"
#include "../moc3.h"
#include "../model.h"
#include "../gather.h"
#include "../interpolate.h"
#include "../artmesh.h"
#include "../blendshape.h"
#include "../deformer.h"
#include "../glue.h"
#include "../offscreen.h"
#include "../param.h"
#include "../part.h"
#include "../render.h"
#include "../update.h"

#include "../core.c"
#include "../debug.c"
#include "../arena.c"
#include "../math2.c"
#include "../verify.c"
#include "../moc3.c"
#include "../model.c"
#include "../update.c"
#include "../param.c"
#include "../part.c"
#include "../deformer.c"
#include "../artmesh.c"
#include "../glue.c"
#include "../offscreen.c"
#include "../blendshape.c"
#include "../interpolate.c"
#include "../render.c"

#include "../samples/common.h"

static int g_pass, g_fail, g_skip;

static void
ind_swap32(void *p, size_t n)
{
  uint8_t *b = (uint8_t *)p;
  for (size_t i = 0; i < n; i++, b += 4) {
    uint8_t t;
    t = b[0]; b[0] = b[3]; b[3] = t;
    t = b[1]; b[1] = b[2]; b[2] = t;
  }
}

static void
ind_swap16(void *p, size_t n)
{
  uint8_t *b = (uint8_t *)p;
  for (size_t i = 0; i < n; i++, b += 2) {
    uint8_t t = b[0]; b[0] = b[1]; b[1] = t;
  }
}

static void
ind_swap_field(size_t width, const void *fieldptr, size_t count,
    const uint8_t *mapbase, uint8_t *synthbase, size_t size)
{
  if (count == 0 || fieldptr == NULL)
    return;
  if (width != 4 && width != 2)
    return;
  const uint8_t *p = (const uint8_t *)fieldptr;
  if (p < mapbase || p >= mapbase + size)
    return;                              /* runtime/arena field, not in file */
  size_t off = (size_t)(p - mapbase);
  if (off + count * width > size)
    return;
  if (width == 4)
    ind_swap32(synthbase + off, count);
  else
    ind_swap16(synthbase + off, count);
}

static void *
build_be(const uint8_t *raw, size_t n)
{
  /* throwaway LE parse to locate every in-file field */
  void *mapbuf = psm_aligned_alloc(csmAlignofMoc, n);
  if (!mapbuf)
    return NULL;
  memcpy(mapbuf, raw, n);
  csmMoc *moc = csmReviveMocInPlace(mapbuf, (unsigned int)n);
  if (!moc) {
    psm_aligned_free(mapbuf);
    return NULL;
  }
  struct psm__moc3_data *data = psm__moc_to_data(moc);
  struct psm__sections *ms = data->sections;
  struct psm__count_info *cnt = ms->count_info;
  psm__u8 ver = data->header->version;

  void *synthv = psm_aligned_alloc(csmAlignofMoc, n);
  if (!synthv) {
    psm_aligned_free(mapbuf);
    return NULL;
  }
  memcpy(synthv, raw, n);
  uint8_t *synth = (uint8_t *)synthv;
  const uint8_t *mapb = (const uint8_t *)mapbuf;

  /* offset table: fixed-size region right after the 64-byte header */
  size_t sec_count = (ver >= csmMocVersion_53) ? 480 : 160;
  if (64 + sec_count * 4 <= n)
    ind_swap32(synth + 64, sec_count);

  /* count_info: 32 ints (<v5.0) or 64 ints (>=v5.0) -- independent sizing */
  {
    size_t ci_off = (size_t)((const uint8_t *)cnt - mapb);
    size_t ci_ints = (ver >= csmMocVersion_50) ? 64 : 32;
    if (ci_off + ci_ints * 4 <= n)
      ind_swap32(synth + ci_off, ci_ints);
  }

  /* canvas_info: 5 leading f32 fields (the trailing u8 flag stays put) */
  {
    size_t cv_off = (size_t)((const uint8_t *)ms->canvas_info - mapb);
    if (cv_off + 5 * 4 <= n)
      ind_swap32(synth + cv_off, 5);
  }

  /* every dynamic section array, swapped iff its element is 2 or 4 bytes */
#define SW_S(T, M, C)  /* count_info / canvas_info handled above */
#define SW_D(T, M, C) \
    ind_swap_field(sizeof(T), (const void *)ms->M, (size_t)cnt->C, mapb, synth, n);
  PSM__SECTIONS_V30(SW_S, SW_D)
  if (ver >= csmMocVersion_33) { PSM__SECTIONS_V33(SW_S, SW_D) }
  if (ver >= csmMocVersion_42) { PSM__SECTIONS_V42(SW_S, SW_D) }
  if (ver >= csmMocVersion_50) { PSM__SECTIONS_V50(SW_S, SW_D) }
  if (ver >= csmMocVersion_53) { PSM__SECTIONS_V53(SW_S, SW_D) }
#undef SW_S
#undef SW_D

  /* mark the file big-endian */
  synth[offsetof(struct psm__moc3_header, endian_flag)] = 1;

  psm_aligned_free(mapbuf);
  return synth;
}

struct buf {
  uint8_t *data;
  size_t len, cap;
};

static void
buf_put(struct buf *b, const void *p, size_t n)
{
  if (b->len + n > b->cap) {
    size_t nc = b->cap ? b->cap * 2 : 4096;
    while (nc < b->len + n)
      nc *= 2;
    b->data = (uint8_t *)realloc(b->data, nc);
    b->cap = nc;
  }
  memcpy(b->data + b->len, p, n);
  b->len += n;
}

/* identical parameter sweep applied to both models before snapshotting */
static void
sweep(csmModel *m)
{
  int pc = csmGetParameterCount(m);
  float *v = csmGetParameterValues(m);
  const float *mn = csmGetParameterMinimumValues(m);
  const float *mx = csmGetParameterMaximumValues(m);
  for (int pass = 0; pass < 3; pass++) {
    for (int i = 0; i < pc; i++) {
      unsigned k = (unsigned)i * 2654435761u + (unsigned)pass * 40503u;
      float t = (float)(k % 1000) / 999.0f;
      v[i] = mn[i] + t * (mx[i] - mn[i]);
    }
    csmResetDrawableDynamicFlags(m);
    csmUpdateModel(m);
  }
}

static void
snapshot(csmModel *m, struct buf *b)
{
  int pc = csmGetParameterCount(m);
  buf_put(b, csmGetParameterValues(m), (size_t)pc * sizeof(float));

  int part_n = csmGetPartCount(m);
  buf_put(b, csmGetPartOpacities(m), (size_t)part_n * sizeof(float));

  int dc = csmGetDrawableCount(m);
  buf_put(b, csmGetDrawableOpacities(m), (size_t)dc * sizeof(float));
  buf_put(b, csmGetDrawableDrawOrders(m), (size_t)dc * sizeof(int));
#if PSM_COMPAT_VERSION >= 0x06000000L
  buf_put(b, csmGetRenderOrders(m), (size_t)dc * sizeof(int));
#else
  buf_put(b, csmGetDrawableRenderOrders(m), (size_t)dc * sizeof(int));
#endif
  buf_put(b, csmGetDrawableMultiplyColors(m), (size_t)dc * 4 * sizeof(float));
  buf_put(b, csmGetDrawableScreenColors(m), (size_t)dc * 4 * sizeof(float));

  const int *vc = csmGetDrawableVertexCounts(m);
  const csmVector2 **pos = csmGetDrawableVertexPositions(m);
  for (int i = 0; i < dc; i++)
    if (pos[i] && vc[i] > 0)
      buf_put(b, pos[i], (size_t)vc[i] * sizeof(csmVector2));

  float ppu, ow, oh, cw, ch;
  csmVector2 origin, sizepix;
  csmReadCanvasInfo(m, &sizepix, &origin, &ppu);
  (void)ow; (void)oh; (void)cw; (void)ch;
  buf_put(b, &sizepix, sizeof(sizepix));
  buf_put(b, &origin, sizeof(origin));
  buf_put(b, &ppu, sizeof(ppu));
}

static int
load_and_snapshot(void *mocbuf, size_t n, struct buf *out)
{
  csmMoc *moc = csmReviveMocInPlace(mocbuf, (unsigned int)n);
  if (!moc) {
    psm_aligned_free(mocbuf);
    return -1;
  }
  unsigned int msz = csmGetSizeofModel(moc);
  if (msz == 0 || msz > 256u * 1024 * 1024) {
    psm_aligned_free(mocbuf);
    return -1;
  }
  void *modelbuf = psm_aligned_alloc(csmAlignofModel, msz);
  if (!modelbuf) {
    psm_aligned_free(mocbuf);
    return -1;
  }
  csmModel *model = csmInitializeModelInPlace(moc, modelbuf, msz);
  if (!model) {
    psm_aligned_free(modelbuf);
    psm_aligned_free(mocbuf);
    return -1;
  }
  csmResetDrawableDynamicFlags(model);
  csmUpdateModel(model);
  sweep(model);
  snapshot(model, out);
  psm_aligned_free(modelbuf);
  psm_aligned_free(mocbuf);
  return 0;
}

/* returns 1 pass, 0 fail, -1 skip (not a loadable LE model) */
static int
run_one(const char *path, const char **why)
{
  size_t n = 0;
  void *raw = psm_read_file(path, &n, csmAlignofMoc, 0);
  if (!raw) { *why = "read failed"; return -1; }

  /* little-endian reference */
  void *refbuf = psm_aligned_alloc(csmAlignofMoc, n);
  memcpy(refbuf, raw, n);
  struct buf snap_le = {0};
  if (load_and_snapshot(refbuf, n, &snap_le) != 0) {
    free(snap_le.data);
    psm_aligned_free(raw);
    *why = "LE load failed";
    return -1;
  }

  /* independently synthesized big-endian copy */
  void *be = build_be((const uint8_t *)raw, n);
  psm_aligned_free(raw);
  if (!be) { free(snap_le.data); *why = "BE synth failed"; return -1; }

  /* consistency check must accept the valid BE file (double-swaps internally) */
  void *becopy = psm_aligned_alloc(csmAlignofMoc, n);
  memcpy(becopy, be, n);
  int consistent = csmHasMocConsistency(becopy, (unsigned int)n);
  psm_aligned_free(becopy);
  if (!consistent) {
    free(snap_le.data); psm_aligned_free(be);
    *why = "csmHasMocConsistency rejected BE file";
    return 0;
  }

  struct buf snap_be = {0};
  if (load_and_snapshot(be, n, &snap_be) != 0) {
    free(snap_le.data); free(snap_be.data);
    *why = "BE load failed";
    return 0;
  }

  int ok = (snap_le.len == snap_be.len) &&
           memcmp(snap_le.data, snap_be.data, snap_le.len) == 0;
  if (!ok)
    *why = (snap_le.len != snap_be.len) ? "snapshot length mismatch"
                                        : "output bytes differ LE vs BE";
  free(snap_le.data);
  free(snap_be.data);
  return ok ? 1 : 0;
}

int
main(int argc, char **argv)
{
  csmSetLogFunction(NULL);
  fprintf(stderr, "big-endian round-trip differential test:\n");

  for (int i = 1; i < argc; i++) {
    const char *why = "";
    int r = run_one(argv[i], &why);
    if (r == 1) {
      g_pass++;
    } else if (r == 0) {
      g_fail++;
      fprintf(stderr, "  FAIL %s: %s\n", argv[i], why);
    } else {
      g_skip++;
    }
  }

  fprintf(stderr, "\n%d passed, %d failed, %d skipped\n",
      g_pass, g_fail, g_skip);
  return g_fail ? 1 : 0;
}
