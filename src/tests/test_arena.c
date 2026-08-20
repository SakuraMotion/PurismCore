/* Arena allocator tests */

/* The arena assumes its backing memory is already suitably aligned (the
 * public API requires an aligned `address`); a bare stack array is only
 * 16-aligned by -O2 luck. Force it with a union carrying a max-align member. */
#define ARENA_BUF(name, n) union { psm__u8 b[n]; max_align_t _a; } name

TEST(arena_basic)
{
  ARENA_BUF(buf, 4096);
  struct psm__arena a = PSM__ARENA_INIT(buf.b, sizeof(buf.b));

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
  ARENA_BUF(buf, 64);
  struct psm__arena a = PSM__ARENA_INIT(buf.b, sizeof(buf.b));

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
  ARENA_BUF(buf, 4096);
  struct psm__arena a = PSM__ARENA_INIT(buf.b, sizeof(buf.b));

  psm__arena_alloc(&a, 1);
  void *p = psm__arena_alloc(&a, 4);
  CHECK(((size_t)p & 15) == 0);
}

TEST(arena_zero)
{
  ARENA_BUF(buf, 256);
  struct psm__arena a = PSM__ARENA_INIT(buf.b, sizeof(buf.b));

  /* Zero-size alloc: implementation may return non-NULL */
  psm__arena_alloc(&a, 0);
  CHECK(psm__arena_ok(&a));
}

TEST(arena_safe_mul)
{
  ARENA_BUF(buf, 64);
  struct psm__arena a = PSM__ARENA_INIT(buf.b, sizeof(buf.b));

  psm__u32 r = psm__arena_safe_mul(&a, 4, 8);
  CHECK(r == 32);

  /* overflow should flag the arena */
  psm__u32 big = psm__arena_safe_mul(&a, 0xFFFFFFFF, 2);
  (void)big;
  CHECK(!psm__arena_ok(&a));
}
