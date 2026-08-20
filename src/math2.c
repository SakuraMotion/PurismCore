/*
 * Purism Core: math utilities
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include "private.h"
#include "math2.h"

PSM__DEF psm__f32
psm__signed_angle(const psm__f32 *v1, const psm__f32 *v2)
{
  psm__f32 angle1 = atan2f(v1[1], v1[0]);
  psm__f32 angle2 = atan2f(v2[1], v2[0]);
  return remainderf(angle1 - angle2, PSM__TWO_PI);
}
