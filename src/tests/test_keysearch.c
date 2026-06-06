/* Key segment search tests */

TEST(key_search_empty)
{
  struct psm__key_search_result r =
      psm__find_key_segment(0.5f, NULL, 0,
          0.01f, 0.001f);
  CHECK(r.needs_check);
  CHECK(r.index == 0);
}

TEST(key_search_single)
{
  psm__f32 keys[] = {0.0f};
  struct psm__key_search_result r;

  r = psm__find_key_segment(0.0f, keys, 1,
      0.01f, 0.001f);
  CHECK(r.needs_check);
  CHECK(!r.is_outside);

  r = psm__find_key_segment(5.0f, keys, 1,
      0.01f, 0.001f);
  CHECK(r.is_outside);
}

TEST(key_search_two_keys)
{
  psm__f32 keys[] = {0.0f, 1.0f};
  struct psm__key_search_result r;

  r = psm__find_key_segment(-1.0f, keys, 2,
      0.01f, 0.001f);
  CHECK(r.is_outside);

  r = psm__find_key_segment(0.5f, keys, 2,
      0.01f, 0.001f);
  CHECK(!r.is_outside);
  CHECK(r.index == 0);
  CHECK_FLOAT(r.weight, 0.5f, 0.001f);

  r = psm__find_key_segment(2.0f, keys, 2,
      0.01f, 0.001f);
  CHECK(r.is_outside);
  CHECK(r.index == 1);
}

TEST(key_search_three_keys)
{
  psm__f32 keys[] = {-30.0f, 0.0f, 30.0f};
  struct psm__key_search_result r;

  r = psm__find_key_segment(15.0f, keys, 3,
      0.01f, 0.001f);
  CHECK(!r.is_outside);
  CHECK(r.index == 1);
  CHECK_FLOAT(r.weight, 0.5f, 0.001f);

  r = psm__find_key_segment(-15.0f, keys, 3,
      0.01f, 0.001f);
  CHECK(r.index == 0);
  CHECK_FLOAT(r.weight, 0.5f, 0.001f);
}

TEST(key_search_snap)
{
  psm__f32 keys[] = {0.0f, 1.0f};
  struct psm__key_search_result r;

  r = psm__find_key_segment(0.005f, keys, 2,
      0.01f, 0.001f);
  CHECK(r.needs_check);
  CHECK(r.index == 0);
}

TEST(key_search_exact_boundary)
{
  psm__f32 keys[] = {0.0f, 0.5f, 1.0f};
  struct psm__key_search_result r;

  /* Exactly at second key (within snap) */
  r = psm__find_key_segment(0.5f, keys, 3,
      0.01f, 0.001f);
  CHECK(r.needs_check);
  CHECK(r.index == 1);
}

TEST(key_search_many_keys)
{
  psm__f32 keys[] = {0, 10, 20, 30, 40, 50};
  struct psm__key_search_result r;

  r = psm__find_key_segment(35.0f, keys, 6,
      0.01f, 0.001f);
  CHECK(r.index == 3);
  CHECK_FLOAT(r.weight, 0.5f, 0.001f);

  r = psm__find_key_segment(5.0f, keys, 6,
      0.01f, 0.001f);
  CHECK(r.index == 0);
  CHECK_FLOAT(r.weight, 0.5f, 0.001f);
}
