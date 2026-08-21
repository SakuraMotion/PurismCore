/*
 * Warp and rotation deformer transform tests.
 *
 * Sets up minimal model state on the stack to exercise
 * psm__warp_transform and psm__rotation_transform directly.
 */

static void
make_warp_model(struct psm__model *m,
    struct psm__deformer_node *dn, struct psm__warp *wc,
    psm__f32 **pos_arr, psm__f32 *pos,
    psm__i32 row, psm__i32 col, int quad)
{
  memset(m, 0, sizeof(*m));
  memset(dn, 0, sizeof(*dn));
  memset(wc, 0, sizeof(*wc));

  dn->local_idx = 0;
  wc->row = row;
  wc->col = col;
  wc->quad_transform = quad;

  pos_arr[0] = pos;
  m->deformers.nodes = dn;
  m->deformers.warps.items = wc;
  m->deformers.warps.pos = pos_arr;
}

/*
 * 2x2 grid (3x3 control points):
 *
 *   (0,2) (1,2) (2,2)
 *   (0,1) (1,1) (2,1)
 *   (0,0) (1,0) (2,0)
 */
TEST(warp_interior)
{
  psm__f32 grid[] = {
    0,0,  1,0,  2,0,
    0,1,  1,1,  2,1,
    0,2,  1,2,  2,2,
  };
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__warp wc;
  psm__f32 *pos_arr[1];
  make_warp_model(&m, &dn, &wc, pos_arr,
      grid, 2, 2, 0);

  psm__f32 inp[2], out[2];

  inp[0] = 0.5f; inp[1] = 0.5f;
  psm__warp_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 1.0f, 0.01f);
  CHECK_FLOAT(out[1], 1.0f, 0.01f);

  inp[0] = 0.0f; inp[1] = 0.0f;
  psm__warp_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 0.0f, 0.01f);
  CHECK_FLOAT(out[1], 0.0f, 0.01f);

  inp[0] = 1.0f; inp[1] = 1.0f;
  psm__warp_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 2.0f, 0.01f);
  CHECK_FLOAT(out[1], 2.0f, 0.01f);

  inp[0] = 0.25f; inp[1] = 0.25f;
  psm__warp_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 0.5f, 0.01f);
  CHECK_FLOAT(out[1], 0.5f, 0.01f);
}

TEST(warp_quad_mode)
{
  psm__f32 grid[] = {
    0,0,  1,0,  2,0,
    0,1,  1,1,  2,1,
    0,2,  1,2,  2,2,
  };
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__warp wc;
  psm__f32 *pos_arr[1];
  make_warp_model(&m, &dn, &wc, pos_arr,
      grid, 2, 2, 1);

  psm__f32 inp[] = {0.5f, 0.5f};
  psm__f32 out[2];
  psm__warp_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 1.0f, 0.01f);
  CHECK_FLOAT(out[1], 1.0f, 0.01f);
}

TEST(warp_scaled_grid)
{
  psm__f32 grid[] = {
    -100,-100,   0,-100,  100,-100,
    -100,   0,   0,   0,  100,   0,
    -100, 100,   0, 100,  100, 100,
  };
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__warp wc;
  psm__f32 *pos_arr[1];
  make_warp_model(&m, &dn, &wc, pos_arr,
      grid, 2, 2, 0);

  psm__f32 inp[2], out[2];

  inp[0] = 0.5f; inp[1] = 0.5f;
  psm__warp_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 0.0f, 0.1f);
  CHECK_FLOAT(out[1], 0.0f, 0.1f);

  inp[0] = 0.0f; inp[1] = 0.0f;
  psm__warp_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], -100.0f, 0.1f);
  CHECK_FLOAT(out[1], -100.0f, 0.1f);
}

TEST(warp_extrapolation)
{
  psm__f32 grid[] = {
    0,0,  1,0,  2,0,
    0,1,  1,1,  2,1,
    0,2,  1,2,  2,2,
  };
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__warp wc;
  psm__f32 *pos_arr[1];
  make_warp_model(&m, &dn, &wc, pos_arr,
      grid, 2, 2, 0);

  psm__f32 inp[] = {-0.5f, 0.5f};
  psm__f32 out[2];
  psm__warp_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], -1.0f, 0.1f);
  CHECK_FLOAT(out[1], 1.0f, 0.1f);
}

TEST(warp_multiple_points)
{
  psm__f32 grid[] = {
    0,0,  1,0,  2,0,
    0,1,  1,1,  2,1,
    0,2,  1,2,  2,2,
  };
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__warp wc;
  psm__f32 *pos_arr[1];
  make_warp_model(&m, &dn, &wc, pos_arr,
      grid, 2, 2, 0);

  psm__f32 inp[] = {0.0f, 0.0f, 1.0f, 1.0f};
  psm__f32 out[4] = {0};
  psm__warp_transform(&m, 0, inp, out, 2);
  CHECK_FLOAT(out[0], 0.0f, 0.01f);
  CHECK_FLOAT(out[1], 0.0f, 0.01f);
  CHECK_FLOAT(out[2], 2.0f, 0.01f);
  CHECK_FLOAT(out[3], 2.0f, 0.01f);
}

static void
make_rot_model(struct psm__model *m,
    struct psm__deformer_node *dn,
    struct psm__rotation *rc,
    psm__f32 *angle, psm__f32 *scale,
    psm__f32 *ox, psm__f32 *oy,
    psm__i32 *rfx, psm__i32 *rfy)
{
  memset(m, 0, sizeof(*m));
  memset(dn, 0, sizeof(*dn));
  memset(rc, 0, sizeof(*rc));

  dn->local_idx = 0;
  dn->type = PSM__DEFORMER_TYPE_ROTATION;
  rc->base_angle = 0.0f;

  m->deformers.nodes = dn;
  m->deformers.rotations.items = rc;
  m->deformers.rotations.angle = angle;
  m->deformers.rotations.scale = scale;
  m->deformers.rotations.origin_x = ox;
  m->deformers.rotations.origin_y = oy;
  m->deformers.rotations.reflect_x = rfx;
  m->deformers.rotations.reflect_y = rfy;
}

TEST(rotation_identity)
{
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__rotation rc;
  psm__f32 a = 0, s = 1, ox = 0, oy = 0;
  psm__i32 rfx = 0, rfy = 0;
  make_rot_model(&m, &dn, &rc,
      &a, &s, &ox, &oy, &rfx, &rfy);

  psm__f32 inp[] = {1.0f, 0.0f}, out[2];
  psm__rotation_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 1.0f, 0.001f);
  CHECK_FLOAT(out[1], 0.0f, 0.001f);
}

TEST(rotation_90_degrees)
{
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__rotation rc;
  psm__f32 a = 90, s = 1, ox = 0, oy = 0;
  psm__i32 rfx = 0, rfy = 0;
  make_rot_model(&m, &dn, &rc,
      &a, &s, &ox, &oy, &rfx, &rfy);

  psm__f32 inp[] = {1.0f, 0.0f}, out[2];
  psm__rotation_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 0.0f, 0.01f);
  CHECK_FLOAT(out[1], 1.0f, 0.01f);
}

TEST(rotation_with_origin)
{
  /* origin is translation, not center of rotation:
   * result = M * input + origin */
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__rotation rc;
  psm__f32 a = 180, s = 1, ox = 5, oy = 5;
  psm__i32 rfx = 0, rfy = 0;
  make_rot_model(&m, &dn, &rc,
      &a, &s, &ox, &oy, &rfx, &rfy);

  /* M(180) = (-1, 0; 0, -1) */
  /* result = (-1,0;0,-1) * (1,0) + (5,5) = (4, 5) */
  psm__f32 inp[] = {1.0f, 0.0f}, out[2];
  psm__rotation_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 4.0f, 0.01f);
  CHECK_FLOAT(out[1], 5.0f, 0.01f);
}

TEST(rotation_with_scale)
{
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__rotation rc;
  psm__f32 a = 0, s = 2, ox = 0, oy = 0;
  psm__i32 rfx = 0, rfy = 0;
  make_rot_model(&m, &dn, &rc,
      &a, &s, &ox, &oy, &rfx, &rfy);

  psm__f32 inp[] = {3.0f, 4.0f}, out[2];
  psm__rotation_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], 6.0f, 0.01f);
  CHECK_FLOAT(out[1], 8.0f, 0.01f);
}

TEST(rotation_reflect_x)
{
  struct psm__model m;
  struct psm__deformer_node dn;
  struct psm__rotation rc;
  psm__f32 a = 0, s = 1, ox = 0, oy = 0;
  psm__i32 rfx = 1, rfy = 0;
  make_rot_model(&m, &dn, &rc,
      &a, &s, &ox, &oy, &rfx, &rfy);

  psm__f32 inp[] = {3.0f, 4.0f}, out[2];
  psm__rotation_transform(&m, 0, inp, out, 1);
  CHECK_FLOAT(out[0], -3.0f, 0.01f);
  CHECK_FLOAT(out[1], 4.0f, 0.01f);
}
