/* Math utility tests */

TEST(clamp_i32)
{
  CHECK(psm__clamp_i32(5, 0, 10) == 5);
  CHECK(psm__clamp_i32(-1, 0, 10) == 0);
  CHECK(psm__clamp_i32(15, 0, 10) == 10);
  CHECK(psm__clamp_i32(0, 0, 0) == 0);
}

TEST(clamp_idx)
{
  CHECK(psm__clamp_idx(3, 10) == 3);
  CHECK(psm__clamp_idx(-1, 10) == 0);
  CHECK(psm__clamp_idx(15, 10) == 9);
  CHECK(psm__clamp_idx(0, 0) == 0);
}

TEST(safe_order_level)
{
  CHECK(psm__safe_order_level(10, 5) == 6);
  CHECK(psm__safe_order_level(5, 5) == 1);
  CHECK(psm__safe_order_level(3, 5) == 0);
  CHECK(psm__safe_order_level(0x7FFFFFFF, 0) == 0);
}

TEST(vec2_ops)
{
  struct psm__vec2 a = psm__v2(1.0f, 2.0f);
  struct psm__vec2 b = psm__v2(3.0f, 4.0f);

  struct psm__vec2 sum = psm__v2_add(a, b);
  CHECK_FLOAT(sum.x, 4.0f, 0.001f);
  CHECK_FLOAT(sum.y, 6.0f, 0.001f);

  struct psm__vec2 diff = psm__v2_sub(b, a);
  CHECK_FLOAT(diff.x, 2.0f, 0.001f);
  CHECK_FLOAT(diff.y, 2.0f, 0.001f);

  struct psm__vec2 scaled = psm__v2_scale(a, 3.0f);
  CHECK_FLOAT(scaled.x, 3.0f, 0.001f);
  CHECK_FLOAT(scaled.y, 6.0f, 0.001f);

  struct psm__vec2 neg = psm__v2_neg(a);
  CHECK_FLOAT(neg.x, -1.0f, 0.001f);
  CHECK_FLOAT(neg.y, -2.0f, 0.001f);
}

TEST(vec2_lerp)
{
  struct psm__vec2 a = psm__v2(0.0f, 0.0f);
  struct psm__vec2 b = psm__v2(10.0f, 20.0f);

  struct psm__vec2 mid = psm__v2_lerp(a, b, 0.5f);
  CHECK_FLOAT(mid.x, 5.0f, 0.001f);
  CHECK_FLOAT(mid.y, 10.0f, 0.001f);

  struct psm__vec2 start = psm__v2_lerp(a, b, 0.0f);
  CHECK_FLOAT(start.x, 0.0f, 0.001f);

  struct psm__vec2 end = psm__v2_lerp(a, b, 1.0f);
  CHECK_FLOAT(end.x, 10.0f, 0.001f);
}

TEST(vec2_bilinear)
{
  struct psm__vec2 p00 = psm__v2(0, 0);
  struct psm__vec2 p10 = psm__v2(1, 0);
  struct psm__vec2 p01 = psm__v2(0, 1);
  struct psm__vec2 p11 = psm__v2(1, 1);

  struct psm__vec2 center =
      psm__v2_bilinear(p00, p10, p01, p11, 0.5f, 0.5f);
  CHECK_FLOAT(center.x, 0.5f, 0.001f);
  CHECK_FLOAT(center.y, 0.5f, 0.001f);

  struct psm__vec2 corner =
      psm__v2_bilinear(p00, p10, p01, p11, 0.0f, 0.0f);
  CHECK_FLOAT(corner.x, 0.0f, 0.001f);
  CHECK_FLOAT(corner.y, 0.0f, 0.001f);
}

TEST(align_to_16)
{
  CHECK(psm__align_to_16(0) == 0);
  CHECK(psm__align_to_16(1) == 16);
  CHECK(psm__align_to_16(16) == 16);
  CHECK(psm__align_to_16(17) == 32);
  CHECK(psm__align_to_16(255) == 256);
}
