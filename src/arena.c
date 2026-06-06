/*
 * Purism Core: arena allocator
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "arena.h"

PSM__DEF void *
psm__arena_alloc(struct psm__arena *a, psm__u32 n)
{
  psm__u32 off = (a->offset + 15) & ~15u;
  if (off < a->offset) {
    a->overflow = 1;
    return NULL;
  }
  psm__u32 end = off + n;
  if (end < off) {
    a->overflow = 1;
    return NULL;
  }
  if (a->base == NULL) {
    a->offset = end;
    return NULL;
  }
  if (end > a->capacity) {
    a->overflow = 1;
    return NULL;
  }
  void *p = a->base + off;
  a->offset = end;
  return p;
}

PSM__DEF psm__u32
psm__arena_total(const struct psm__arena *a)
{
  return (a->offset + 15) & ~15u;
}

PSM__DEF psm__i32
psm__arena_ok(const struct psm__arena *a)
{
  return !a->overflow;
}
