/*
 * Purism Core: minimal unit test framework
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__UNIT_H
#define PSM__UNIT_H

#include <stdio.h>
#include <math.h>

static int g_pass, g_fail;

#define CHECK(cond) do { \
    if (cond) { g_pass++; } \
    else { \
      fprintf(stderr, "\n    FAIL %s:%d: %s", \
          __FILE__, __LINE__, #cond); \
      g_fail++; \
    } \
  } while (0)

#define CHECK_FLOAT(a, b, eps) \
    CHECK(fabsf((float)(a) - (float)(b)) < (eps))

#define CHECK_INT(a, b) \
    CHECK((a) == (b))

#define TEST(name) static void test_##name(void)

#define RUN(name) do { \
    int _before = g_fail; \
    fprintf(stderr, "  %-40s", #name); \
    test_##name(); \
    fprintf(stderr, "%s\n", \
        g_fail == _before ? "ok" : ""); \
  } while (0)

#define SUITE(name) \
    fprintf(stderr, "\n  -- %s --\n", name)

#endif /* PSM__UNIT_H */
