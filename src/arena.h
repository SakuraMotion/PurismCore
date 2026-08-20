/*
 * Purism Core: arena allocator
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__ARENA_H
#define PSM__ARENA_H

#include "private.h"

struct psm__arena {
  psm__u8 *base;
  psm__u32 off;
  psm__u32 cap;
  bool     overflow;
};

#define PSM__ARENA_INIT(buf, cap) \
  ((struct psm__arena){ (psm__u8 *)(buf), 0, (cap), 0 })

static inline psm__u32
psm__arena_safe_mul(struct psm__arena *a, psm__u32 x, psm__u32 y)
{
  psm__u32 r = x * y;
  if (x != 0 && r / x != y) {
    a->overflow = true;
    return 0;
  }
  return r;
}

#define PSM__ARENA_NEW(a, T, n) \
  ((T *)psm__arena_alloc((a), psm__arena_safe_mul((a), sizeof(T), (n))))
#define PSM__ARENA_NEW_SIZE(a, T, sz) \
  ((T *)psm__arena_alloc((a), (sz)))

PSM__DEF void    *psm__arena_alloc(struct psm__arena *, psm__u32);
PSM__DEF psm__u32 psm__arena_total(const struct psm__arena *);
PSM__DEF bool     psm__arena_ok(const struct psm__arena *);
#ifdef PSM_DEBUG_MALLOC
PSM__DEF void psm__dbg_free_all(void);
#endif

#endif /* PSM__ARENA_H */
