/*
 * Extract reference data from MOC3 files for unit tests.
 * Outputs C arrays with per-file prefixes (REF0, REF1, ...).
 *
 * Usage: ./extract <file1.moc3> [file2.moc3 ...]
 * Output goes to stdout.
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

static void
print_float(float v)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%.8g", (double)v);
  if (!strchr(buf, '.') && !strchr(buf, 'e')) {
    size_t len = strlen(buf);
    buf[len] = '.';
    buf[len + 1] = '0';
    buf[len + 2] = '\0';
  }
  printf("%sf", buf);
}

static void
print_arr(const char *pfx, const char *name,
    const float *a, int n)
{
  printf("static const float %s_%s[] = {\n ",
      pfx, name);
  for (int i = 0; i < n; i++) {
    printf(" ");
    print_float(a[i]);
    printf(",");
    if ((i + 1) % 6 == 0 && i + 1 < n)
      printf("\n ");
  }
  printf("\n};\n");
}

static void
print_def_i(const char *pfx, const char *name, int v)
{
  printf("#define %s_%s %d\n", pfx, name, v);
}

static void
print_def_f(const char *pfx, const char *name, float v)
{
  printf("#define %s_%s ", pfx, name);
  print_float(v);
  printf("\n");
}

static void
extract_one(const char *path, const char *pfx)
{
  size_t moc_size;
  void *moc_data = psm_read_file(path,
      &moc_size, csmAlignofMoc, 0);
  if (!moc_data)
    return;

  csmMoc *moc = csmReviveMocInPlace(
      moc_data, (unsigned)moc_size);
  if (!moc) {
    psm_aligned_free(moc_data);
    return;
  }

  unsigned msz = csmGetSizeofModel(moc);
  void *mem = psm_aligned_alloc(csmAlignofModel, msz);
  csmModel *model = csmInitializeModelInPlace(
      moc, mem, msz);
  if (!model) {
    psm_aligned_free(mem);
    psm_aligned_free(moc_data);
    return;
  }

  struct psm__model *m = (struct psm__model *)model;

  /* Warp: find first with vc <= 36 */
  struct psm__warps *w = &m->deformers.warps;
  int warp_found = 0;
  for (int i = 0; i < w->count && !warp_found; i++) {
    int row = w->items[i].row;
    int col = w->items[i].col;
    int vc = (row + 1) * (col + 1);
    if (vc > 36 || !w->pos[i])
      continue;

    int di = -1;
    for (int d = 0; d < m->deformers.count; d++) {
      if (m->deformers.nodes[d].type == 0 &&
          m->deformers.nodes[d].local_idx == i) {
        di = d;
        break;
      }
    }
    if (di < 0)
      continue;

    print_def_i(pfx, "WARP_ROW", row);
    print_def_i(pfx, "WARP_COL", col);
    print_def_i(pfx, "WARP_VC", vc);
    print_def_i(pfx, "WARP_QUAD",
        w->items[i].quad_transform);
    print_arr(pfx, "warp_grid", w->pos[i], vc * 2);

    float ti[] = {0, 0, 0.5f, 0.5f, 1, 1, 0.25f, 0.75f};
    float to[8];
    psm__warp_transform(m, di, ti, to, 4);
    print_arr(pfx, "warp_in", ti, 8);
    print_arr(pfx, "warp_out", to, 8);
    warp_found = 1;
  }

  /* Rotation: first deformer */
  struct psm__rotations *r = &m->deformers.rotations;
  if (r->count > 0) {
    int ri = 0;
    int di = -1;
    for (int d = 0; d < m->deformers.count; d++) {
      if (m->deformers.nodes[d].type == 1 &&
          m->deformers.nodes[d].local_idx == ri) {
        di = d;
        break;
      }
    }
    if (di >= 0) {
      print_def_f(pfx, "ROT_BASE",
          r->items[ri].base_angle);
      print_def_f(pfx, "ROT_ANGLE", r->angle[ri]);
      print_def_f(pfx, "ROT_SCALE", r->scale[ri]);
      print_def_f(pfx, "ROT_OX", r->origin_x[ri]);
      print_def_f(pfx, "ROT_OY", r->origin_y[ri]);
      print_def_i(pfx, "ROT_RFX", r->reflect_x[ri]);
      print_def_i(pfx, "ROT_RFY", r->reflect_y[ri]);

      float ti[] = {0, 0, 1, 0, 0, 1, -0.5f, 0.5f};
      float to[8];
      psm__rot_transform(m, di, ti, to, 4);
      print_arr(pfx, "rot_in", ti, 8);
      print_arr(pfx, "rot_out", to, 8);
    }
  }

  /* Drawable: first visible with 4-20 vertices */
  int dc = csmGetDrawableCount(model);
  const int *dvc = csmGetDrawableVertexCounts(model);
  const csmVector2 **dpos =
      csmGetDrawableVertexPositions(model);
  const float *dopa = csmGetDrawableOpacities(model);

  for (int i = 0; i < dc; i++) {
    if (dvc[i] < 4 || dvc[i] > 20 || !dpos[i])
      continue;
    if (dopa[i] <= 0.0f)
      continue;

    print_def_i(pfx, "DRAW_VC", dvc[i]);
    print_def_f(pfx, "DRAW_OPA", dopa[i]);
    print_arr(pfx, "draw_pos",
        (const float *)dpos[i], dvc[i] * 2);
    break;
  }

  printf("\n");
  psm_aligned_free(mem);
  psm_aligned_free(moc_data);
}

int
main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr,
        "usage: %s <file1.moc3> [file2.moc3 ...]\n"
        "Output goes to stdout.\n",
        argv[0]);
    return 1;
  }

  csmSetLogFunction(NULL);

  printf("/*\n"
      " * Reference data extracted from MOC3 files.\n"
      " * Generated by extract.c — do not edit.\n"
      " */\n\n");

  for (int i = 1; i < argc; i++) {
    char pfx[16];
    snprintf(pfx, sizeof(pfx), "REF%d", i - 1);
    extract_one(argv[i], pfx);
  }

  printf("#define REF_COUNT %d\n", argc - 1);
  return 0;
}
