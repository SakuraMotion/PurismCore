/*
 * Tests using reference data extracted from real MOC3 files.
 * refdata.h contains known-good transform inputs/outputs
 * from multiple models. No MOC3 files needed at runtime.
 */

#include "refdata.h"

/*
 * Macro to define a warp test for a given prefix.
 * Only expands if the prefix has WARP_ROW defined.
 */
#define WARP_TEST(P) \
TEST(P##_warp) \
{ \
  struct psm__model m; \
  struct psm__deformer_node dn; \
  struct psm__warp wc; \
  psm__f32 *pos_arr[1]; \
  psm__f32 grid[P##_WARP_VC * 2]; \
  memcpy(grid, P##_warp_grid, sizeof(grid)); \
  make_warp_model(&m, &dn, &wc, pos_arr, grid, \
      P##_WARP_ROW, P##_WARP_COL, P##_WARP_QUAD); \
  psm__f32 out[8]; \
  psm__warp_transform(&m, 0, P##_warp_in, out, 4); \
  for (int i = 0; i < 8; i++) \
    CHECK_FLOAT(out[i], P##_warp_out[i], 0.0001f); \
}

/*
 * Macro to define a rotation test for a given prefix.
 */
#define ROT_TEST(P) \
TEST(P##_rot) \
{ \
  struct psm__model m; \
  struct psm__deformer_node dn; \
  struct psm__rotation rc; \
  psm__f32 a = P##_ROT_ANGLE, s = P##_ROT_SCALE; \
  psm__f32 ox = P##_ROT_OX, oy = P##_ROT_OY; \
  psm__i32 rfx = P##_ROT_RFX, rfy = P##_ROT_RFY; \
  make_rot_model(&m, &dn, &rc, \
      &a, &s, &ox, &oy, &rfx, &rfy); \
  rc.base_angle = P##_ROT_BASE; \
  psm__f32 out[8]; \
  psm__rot_transform(&m, 0, P##_rot_in, out, 4); \
  for (int i = 0; i < 8; i++) \
    CHECK_FLOAT(out[i], P##_rot_out[i], 0.0001f); \
}

/*
 * Macro to define a drawable sanity test.
 */
#define DRAW_TEST(P) \
TEST(P##_draw) \
{ \
  CHECK(P##_DRAW_VC > 0); \
  CHECK(P##_DRAW_OPA >= 0.0f); \
  CHECK(P##_DRAW_OPA <= 1.0f); \
  for (int i = 0; i < P##_DRAW_VC * 2; i++) \
    CHECK(isfinite(P##_draw_pos[i])); \
}

/* Generate tests for each model that has warp data */
#ifdef REF0_WARP_ROW
WARP_TEST(REF0)
#endif
#ifdef REF1_WARP_ROW
WARP_TEST(REF1)
#endif
#ifdef REF2_WARP_ROW
WARP_TEST(REF2)
#endif
#ifdef REF3_WARP_ROW
WARP_TEST(REF3)
#endif
#ifdef REF4_WARP_ROW
WARP_TEST(REF4)
#endif

/* Generate rotation tests */
#ifdef REF0_ROT_BASE
ROT_TEST(REF0)
#endif
#ifdef REF1_ROT_BASE
ROT_TEST(REF1)
#endif
#ifdef REF2_ROT_BASE
ROT_TEST(REF2)
#endif
#ifdef REF3_ROT_BASE
ROT_TEST(REF3)
#endif
#ifdef REF4_ROT_BASE
ROT_TEST(REF4)
#endif

/* Generate drawable tests */
#ifdef REF0_DRAW_VC
DRAW_TEST(REF0)
#endif
#ifdef REF1_DRAW_VC
DRAW_TEST(REF1)
#endif
#ifdef REF2_DRAW_VC
DRAW_TEST(REF2)
#endif
#ifdef REF3_DRAW_VC
DRAW_TEST(REF3)
#endif
#ifdef REF4_DRAW_VC
DRAW_TEST(REF4)
#endif
