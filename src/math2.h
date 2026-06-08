/*
 * Purism Core: math utilities
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__MATH2_H
#define PSM__MATH2_H

#include <math.h>
#include <stdint.h>
#include "private.h"

#define PSM__PI      3.14159265358979323846f
#define PSM__TWO_PI  6.28318530717958647692f

struct psm__vec2 {
  psm__f32 x, y;
};

static inline struct psm__vec2
psm__v2(psm__f32 x, psm__f32 y)
{
  return (struct psm__vec2){x, y};
}

static inline struct psm__vec2
psm__v2_load(const psm__f32 *arr, psm__i32 idx)
{
  return (struct psm__vec2){arr[idx * 2], arr[idx * 2 + 1]};
}

static inline void
psm__v2_store(psm__f32 *arr, psm__i32 idx, struct psm__vec2 v)
{
  arr[idx * 2] = v.x;
  arr[idx * 2 + 1] = v.y;
}

static inline struct psm__vec2
psm__v2_add(struct psm__vec2 a, struct psm__vec2 b)
{
  return (struct psm__vec2){a.x + b.x, a.y + b.y};
}

static inline struct psm__vec2
psm__v2_sub(struct psm__vec2 a, struct psm__vec2 b)
{
  return (struct psm__vec2){a.x - b.x, a.y - b.y};
}

static inline struct psm__vec2
psm__v2_scale(struct psm__vec2 v, psm__f32 s)
{
  return (struct psm__vec2){v.x * s, v.y * s};
}

static inline struct psm__vec2
psm__v2_neg(struct psm__vec2 v)
{
  return (struct psm__vec2){-v.x, -v.y};
}

static inline struct psm__vec2
psm__v2_lerp(struct psm__vec2 a, struct psm__vec2 b, psm__f32 t)
{
  return (struct psm__vec2){fmaf(t, b.x - a.x, a.x), fmaf(t, b.y - a.y, a.y)};
}

static inline struct psm__vec2
psm__v2_bary3(struct psm__vec2 a, struct psm__vec2 b, struct psm__vec2 c,
    psm__f32 wa, psm__f32 wb, psm__f32 wc)
{
  return (struct psm__vec2){fmaf(wc, c.x, fmaf(wb, b.x, wa * a.x)),
                      fmaf(wc, c.y, fmaf(wb, b.y, wa * a.y))};
}

static inline struct psm__vec2
psm__v2_bilinear(struct psm__vec2 p00, struct psm__vec2 p10,
    struct psm__vec2 p01, struct psm__vec2 p11, psm__f32 u, psm__f32 v)
{
  psm__f32 inv_u = 1.0f - u;
  psm__f32 x0 = fmaf(u, p10.x, inv_u * p00.x),
                y0 = fmaf(u, p10.y, inv_u * p00.y),
                x1 = fmaf(u, p11.x, inv_u * p01.x),
                y1 = fmaf(u, p11.y, inv_u * p01.y);
  psm__f32 inv_v = 1.0f - v;
  return (struct psm__vec2){fmaf(v, x1, inv_v * x0), fmaf(v, y1, inv_v * y0)};
}

static inline psm__f32
psm__clamp_f32(psm__f32 v, psm__f32 lo, psm__f32 hi)
{
  return fminf(fmaxf(v, lo), hi);
}

static inline psm__f32
psm__clamp_f32_01(psm__f32 v)
{
  return fminf(fmaxf(v, 0.0f), 1.0f);
}

/*
 * Convert float to int32 with clamping. NaN returns 0.
 * Note: (float)INT32_MAX rounds up to 2147483648.0f which overflows int32,
 * so use 2147483520.0f (largest float < 2^31).
 */
static inline psm__i32
psm__f32_to_i32(psm__f32 v)
{
  return (v == v)
      ? (psm__i32)psm__clamp_f32(v, -2147483648.0f, 2147483520.0f) : 0;
}

PSM__DEF psm__f32 psm__get_angle_not_abs(const psm__f32 *, const psm__f32 *);

#endif /* PSM__MATH2_H */
