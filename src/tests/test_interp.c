/* Interpolation tests */

TEST(interp_f32_basic)
{
  psm__i32 max_blend[] = {2};
  psm__i32 blend_count[] = {2};
  psm__f32 weights[] = {0.7f, 0.3f};
  psm__f32 targets[] = {10.0f, 20.0f};
  psm__f32 tmp[2];
  psm__f32 out[1] = {0.0f};

  struct psm__interp ip = {0};
  ip.object_count = 1;
  ip.max_blend = max_blend;
  ip.blend_count = blend_count;
  ip.weights = weights;
  ip.tmp = tmp;
  ip.tmp_len = 2;

  psm__interp_f32(&ip, targets, out, 1, NULL);
  CHECK_FLOAT(out[0], 13.0f, 0.001f);
}

TEST(interp_f32_strided)
{
  psm__i32 max_blend[] = {2};
  psm__i32 blend_count[] = {2};
  psm__f32 weights[] = {0.5f, 0.5f};
  psm__f32 targets[] = {1.0f, 3.0f};
  psm__f32 tmp[2];
  psm__f32 out[4] = {0};

  struct psm__interp ip = {0};
  ip.object_count = 1;
  ip.max_blend = max_blend;
  ip.blend_count = blend_count;
  ip.weights = weights;
  ip.tmp = tmp;
  ip.tmp_len = 2;

  /* stride 4: writes to out[0] only */
  psm__interp_f32(&ip, targets, out, 4, NULL);
  CHECK_FLOAT(out[0], 2.0f, 0.001f);
  CHECK_FLOAT(out[1], 0.0f, 0.001f);
}

TEST(interp_i32_basic)
{
  psm__i32 max_blend[] = {2};
  psm__i32 blend_count[] = {2};
  psm__f32 weights[] = {0.6f, 0.4f};
  psm__f32 targets[] = {100.0f, 200.0f};
  psm__f32 tmp[2];
  psm__i32 out[1] = {0};

  struct psm__interp ip = {0};
  ip.object_count = 1;
  ip.max_blend = max_blend;
  ip.blend_count = blend_count;
  ip.weights = weights;
  ip.tmp = tmp;
  ip.tmp_len = 2;

  psm__interp_i32(&ip, targets, out, NULL);
  CHECK(out[0] == 140);
}

TEST(interp_f32_enable)
{
  psm__i32 max_blend[] = {1, 1};
  psm__i32 blend_count[] = {1, 1};
  psm__f32 weights[] = {1.0f, 1.0f};
  psm__f32 targets[] = {5.0f, 10.0f};
  psm__f32 tmp[2];
  psm__f32 out[2] = {0.0f, 0.0f};
  bool enable[] = {true, false};

  struct psm__interp ip = {0};
  ip.object_count = 2;
  ip.max_blend = max_blend;
  ip.blend_count = blend_count;
  ip.weights = weights;
  ip.tmp = tmp;
  ip.tmp_len = 2;

  psm__interp_f32(&ip, targets, out, 1, enable);
  CHECK_FLOAT(out[0], 5.0f, 0.001f);
  CHECK_FLOAT(out[1], 0.0f, 0.001f);
}

TEST(interp_multi_object)
{
  psm__i32 max_blend[] = {2, 3};
  psm__i32 blend_count[] = {2, 2};
  psm__f32 weights[] = {
    0.5f, 0.5f,
    0.25f, 0.75f, 0.0f
  };
  psm__f32 targets[] = {
    10.0f, 20.0f,
    100.0f, 200.0f, 0.0f
  };
  psm__f32 tmp[5];
  psm__f32 out[2] = {0};

  struct psm__interp ip = {0};
  ip.object_count = 2;
  ip.max_blend = max_blend;
  ip.blend_count = blend_count;
  ip.weights = weights;
  ip.tmp = tmp;
  ip.tmp_len = 5;

  psm__interp_f32(&ip, targets, out, 1, NULL);
  CHECK_FLOAT(out[0], 15.0f, 0.001f);
  CHECK_FLOAT(out[1], 175.0f, 0.001f);
}

TEST(interp_single_keyform)
{
  psm__i32 max_blend[] = {1};
  psm__i32 blend_count[] = {1};
  psm__f32 weights[] = {1.0f};
  psm__f32 targets[] = {42.0f};
  psm__f32 tmp[1];
  psm__f32 out[1] = {0};

  struct psm__interp ip = {0};
  ip.object_count = 1;
  ip.max_blend = max_blend;
  ip.blend_count = blend_count;
  ip.weights = weights;
  ip.tmp = tmp;
  ip.tmp_len = 1;

  psm__interp_f32(&ip, targets, out, 1, NULL);
  CHECK_FLOAT(out[0], 42.0f, 0.001f);
}

TEST(interp_null_safety)
{
  /* Should not crash with NULL inputs */
  psm__interp_f32(NULL, NULL, NULL, 1, NULL);
  psm__interp_i32(NULL, NULL, NULL, NULL);

  struct psm__interp ip = {0};
  psm__interp_f32(&ip, NULL, NULL, 1, NULL);
  CHECK(1); /* survived */
}
