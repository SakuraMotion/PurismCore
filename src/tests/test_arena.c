/* Arena allocator tests */

TEST(arena_basic)
{
  psm__u8 buf[4096];
  struct psm__arena a = PSM__ARENA_INIT(buf, sizeof(buf));

  void *p1 = psm__arena_alloc(&a, 64);
  CHECK(p1 != NULL);
  CHECK(((size_t)p1 & 15) == 0);

  void *p2 = psm__arena_alloc(&a, 128);
  CHECK(p2 != NULL);
  CHECK(p2 != p1);
  CHECK(((size_t)p2 & 15) == 0);
  CHECK(psm__arena_ok(&a));
}

TEST(arena_overflow)
{
  psm__u8 buf[64];
  struct psm__arena a = PSM__ARENA_INIT(buf, sizeof(buf));

  psm__arena_alloc(&a, 1024);
  CHECK(!psm__arena_ok(&a));
}

TEST(arena_dry_run)
{
  struct psm__arena a = PSM__ARENA_INIT(NULL, 0);

  psm__arena_alloc(&a, 100);
  psm__arena_alloc(&a, 200);

  psm__u32 total = psm__arena_total(&a);
  CHECK(total >= 300);
}

TEST(arena_alignment)
{
  psm__u8 buf[4096];
  struct psm__arena a = PSM__ARENA_INIT(buf, sizeof(buf));

  psm__arena_alloc(&a, 1);
  void *p = psm__arena_alloc(&a, 4);
  CHECK(((size_t)p & 15) == 0);
}

TEST(arena_zero)
{
  psm__u8 buf[256];
  struct psm__arena a = PSM__ARENA_INIT(buf, sizeof(buf));

  /* Zero-size alloc: implementation may return non-NULL */
  psm__arena_alloc(&a, 0);
  CHECK(psm__arena_ok(&a));
}

TEST(arena_safe_mul)
{
  psm__u8 buf[64];
  struct psm__arena a = PSM__ARENA_INIT(buf, sizeof(buf));

  psm__u32 r = psm__arena_safe_mul(&a, 4, 8);
  CHECK(r == 32);

  /* overflow should flag the arena */
  psm__u32 big = psm__arena_safe_mul(&a, 0xFFFFFFFF, 2);
  (void)big;
  CHECK(!psm__arena_ok(&a));
}
