/*
 * Purism Core: unit test driver
 *
 * Single translation unit build that includes all library sources (for access
 * to static functions) and all test_*.c files.
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Include all library sources as a single translation unit */
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
#include "unit.h"

/* Test suites */
#include "test_arena.c"
#include "test_keysearch.c"
#include "test_interp.c"
#include "test_math.c"
#include "test_transform.c"
#include "test_misc.c"
#include "test_refdata.c"

int
main(void)
{
  csmSetLogFunction(NULL);
  fprintf(stderr, "unit tests:\n");

  SUITE("arena");
  RUN(arena_basic);
  RUN(arena_overflow);
  RUN(arena_dry_run);
  RUN(arena_alignment);
  RUN(arena_zero);
  RUN(arena_safe_mul);

  SUITE("key search");
  RUN(key_search_empty);
  RUN(key_search_single);
  RUN(key_search_two_keys);
  RUN(key_search_three_keys);
  RUN(key_search_snap);
  RUN(key_search_exact_boundary);
  RUN(key_search_many_keys);

  SUITE("interpolation");
  RUN(interp_f32_basic);
  RUN(interp_f32_strided);
  RUN(interp_i32_basic);
  RUN(interp_f32_enable);
  RUN(interp_multi_object);
  RUN(interp_single_keyform);
  RUN(interp_null_safety);

  SUITE("math");
  RUN(clamp_i32);
  RUN(clamp_idx);
  RUN(safe_order_level);
  RUN(vec2_ops);
  RUN(vec2_lerp);
  RUN(vec2_bilinear);
  RUN(align_to_16);

  SUITE("transform");
  RUN(warp_interior);
  RUN(warp_quad_mode);
  RUN(warp_scaled_grid);
  RUN(warp_extrapolation);
  RUN(warp_multiple_points);
  RUN(rotation_identity);
  RUN(rotation_90_degrees);
  RUN(rotation_with_origin);
  RUN(rotation_with_scale);
  RUN(rotation_reflect_x);

  SUITE("misc");
  RUN(blend_mode_remap);
  RUN(moc3_version);
  RUN(moc3_bad_magic);
  RUN(moc3_too_small);
  RUN(glob_exact);
  RUN(glob_star);
  RUN(glob_question);
  RUN(glob_case_insensitive);
  RUN(version_api);

  SUITE("reference data (extracted from real models)");
#ifdef REF0_WARP_ROW
  RUN(REF0_warp);
#endif
#ifdef REF1_WARP_ROW
  RUN(REF1_warp);
#endif
#ifdef REF2_WARP_ROW
  RUN(REF2_warp);
#endif
#ifdef REF3_WARP_ROW
  RUN(REF3_warp);
#endif
#ifdef REF4_WARP_ROW
  RUN(REF4_warp);
#endif
#ifdef REF0_ROT_BASE
  RUN(REF0_rot);
#endif
#ifdef REF1_ROT_BASE
  RUN(REF1_rot);
#endif
#ifdef REF2_ROT_BASE
  RUN(REF2_rot);
#endif
#ifdef REF3_ROT_BASE
  RUN(REF3_rot);
#endif
#ifdef REF4_ROT_BASE
  RUN(REF4_rot);
#endif
#ifdef REF0_DRAW_VC
  RUN(REF0_draw);
#endif
#ifdef REF1_DRAW_VC
  RUN(REF1_draw);
#endif
#ifdef REF2_DRAW_VC
  RUN(REF2_draw);
#endif
#ifdef REF3_DRAW_VC
  RUN(REF3_draw);
#endif
#ifdef REF4_DRAW_VC
  RUN(REF4_draw);
#endif

  fprintf(stderr, "\n%d passed, %d failed\n",
      g_pass, g_fail);

  return g_fail ? 1 : 0;
}
