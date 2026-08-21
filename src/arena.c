/*
 * Purism Core: arena allocator
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "arena.h"

#ifdef PSM_DEBUG_MALLOC
#  include <stdlib.h>

static void   **g_dbg_ptrs;
static psm_size g_dbg_count, g_dbg_cap;

static void *
psm__dbg_alloc(psm__u32 n)
{
  void *p = calloc(1, n ? n : 1);
  if (g_dbg_count == g_dbg_cap) {
    psm_size nc = g_dbg_cap ? g_dbg_cap * 2 : 256;
    void   **np = realloc(g_dbg_ptrs, nc * sizeof(void *));
    if (!np)
      return p;
    g_dbg_ptrs = np;
    g_dbg_cap = nc;
  }
  g_dbg_ptrs[g_dbg_count++] = p;
  return p;
}

void
psm__dbg_free_all(void)
{
  for (psm_size i = 0; i < g_dbg_count; i++)
    free(g_dbg_ptrs[i]);
  g_dbg_count = 0;
}
#endif /* PSM_DEBUG_MALLOC */

PSM__DEF void *
psm__arena_alloc(struct psm__arena *a, psm__u32 n)
{
  psm__u32 off = (a->off + 15) & ~15u;
  if (off < a->off) {
    a->overflow = true;
    return NULL;
  }
  psm__u32 end = off + n;
  if (end < off) {
    a->overflow = true;
    return NULL;
  }
  if (a->base == NULL) {
    a->off = end;
    return NULL;
  }
#ifdef PSM_DEBUG_MALLOC
  /* When fuzzing with ASAN, we use real malloc() in order to better catch OOB
     memory access */
  a->off = end;
  return psm__dbg_alloc(n);
#else
  if (end > a->cap) {
    a->overflow = true;
    return NULL;
  }
  void *p = a->base + off;
  a->off = end;
  return p;
#endif
}

PSM__DEF psm__u32
psm__arena_total(const struct psm__arena *a)
{
  return (a->off + 15) & ~15u;
}

PSM__DEF bool
psm__arena_ok(const struct psm__arena *a)
{
  return !a->overflow;
}
