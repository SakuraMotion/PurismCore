/*
 * Negative control for the triangle-index-value check in verify_idx:
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

static int g_tested, g_rejected, g_no_indices;

static int
ends_moc3(const char *s)
{
  size_t l = strlen(s);
  return l > 5 && !strcmp(s+l-5, ".moc3");
}

static void
run_one(const char *path)
{
  size_t n = 0;
  void *raw = psm_read_file(path, &n, csmAlignofMoc, 0);
  if (!raw) return;
  csmMoc *moc = csmReviveMocInPlace(raw, (unsigned)n);
  if (!moc) { psm_aligned_free(raw); return; }
  struct psm__moc3_data *d = psm__moc_to_data(moc);
  struct psm__sections *ms = d->sections;
  struct psm__count_info *cnt = ms->count_info;
  psm__u8 ver = d->header->version;

  /* baseline: must validate clean */
  if (psm__verify_idx(ver, ms) != PSM__OK) { psm_aligned_free(raw); return; }

  if (!ms->idx_src.idx) { g_no_indices++; psm_aligned_free(raw); return; }

  /* find an art mesh with at least one index and corrupt its first index */
  for (psm__i32 i = 0; i < cnt->art_meshes; i++) {
    psm__i32 off = ms->art_mesh_src.idx_off[i];
    psm__i32 len = ms->art_mesh_src.idx_len[i];
    psm__i32 vc  = ms->art_mesh_src.vertex_count[i];
    if (len <= 0) continue;
    psm__u16 saved = ms->idx_src.idx[off];
    ms->idx_src.idx[off] = (psm__u16)(vc);          /* index == vc -> out of range */
    g_tested++;
    if (psm__verify_idx(ver, ms) != PSM__OK)
      g_rejected++;
    else
      fprintf(stderr, "  NOT REJECTED: %s art_mesh[%d] idx=vc=%d\n",
          path, i, vc);
    ms->idx_src.idx[off] = saved;
    break;
  }
  psm_aligned_free(raw);
}

int
main(int argc, char **argv)
{
  csmSetLogFunction(NULL);
  for (int a = 1; a < argc; a++) {
    run_one(argv[a]);
  }
  fprintf(stderr, "corrupted+tested: %d, rejected: %d, (no-index models: %d)\n",
      g_tested, g_rejected, g_no_indices);
  return (g_tested > 0 && g_rejected == g_tested) ? 0 : 1;
}
