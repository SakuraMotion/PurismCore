/*
 * Purism Core: internal definitions and macros
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__PRIVATE_H
#define PSM__PRIVATE_H

/* Internal code always needs the full v6 blend type set. */
#ifndef PSM__BLENDTYPE_V6
#define PSM__BLENDTYPE_V6
#endif

#include "../include/PurismCore.h"
#include <stddef.h>

#if defined(__cplusplus)
  /* C++ has bool natively */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# include <stdbool.h>
#elif defined(_MSC_VER) && _MSC_VER >= 1800
# include <stdbool.h>
#else
  typedef unsigned char bool;
# define true  1
# define false 0
#endif

#ifndef PSM__DEF
#  if defined(PURISM_CORE_STATIC)
#    define PSM__DEF static
#  elif defined(__GNUC__) || defined(__clang__)
#    define PSM__DEF __attribute__((visibility("hidden")))
#  elif defined(_MSC_VER)
#    define PSM__DEF
#  else
#    define PSM__DEF
#  endif
#endif

enum {
  PSM__FLAG_IS_VISIBLE           = 0x01,
  PSM__FLAG_VISIBILITY_CHANGED   = 0x02,
  PSM__FLAG_OPACITY_CHANGED      = 0x04,
  PSM__FLAG_DRAW_ORDER_CHANGED   = 0x08,
  PSM__FLAG_RENDER_ORDER_CHANGED = 0x10,
  PSM__FLAG_VERTEX_CHANGED       = 0x20,
  PSM__FLAG_BLEND_COLOR_CHANGED  = 0x40,
  PSM__FLAG_ALL_CHANGED          = 0x7E,
  PSM__FLAG_ALL                  = 0x7F,
};

enum {
  PSM__CANVAS_FLAG_Y_REVERSED = 0x01,
};

#define PSM__VERFMT "%d.%d.%d"
#define PSM__VERARG(x) ((x) >> 24), (((x) >> 16) & 0xFF), ((x) & 0xFFFF)

static inline
psm__i32 psm__clamp_i32(psm__i32 v, psm__i32 lo, psm__i32 hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

/*
 * Pointer-to-float size ratio. Position keydata stores pointers
 * into the keyform position pool, but the arena is sized in
 * floats. On 64-bit platforms this is 2 (one pointer = two floats).
 */
#define PSM__PTR_FLOAT_RATIO (sizeof(void *) / sizeof(psm__f32))
#define PSM__MAX_AXES 20

static inline psm__u32 psm__align_to_16(psm__u32 n) { return (n + 15) & ~15u; }

#define psm__nop_predicate(...)

#endif /* PSM__PRIVATE_H */
