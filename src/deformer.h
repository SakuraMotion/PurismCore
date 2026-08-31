/*
 * Purism Core: deformer declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__DEFORMER_H
#define PSM__DEFORMER_H

#include "private.h"
#include "model.h"

#define PSM__DEFORMER_TYPE_WARP     0
#define PSM__DEFORMER_TYPE_ROTATION 1

PSM__DEF void psm__enable_deformers(struct psm__model *);
PSM__DEF void psm__gather_warps(struct psm__model *);
PSM__DEF void psm__gather_rotations(struct psm__model *);
PSM__DEF void psm__process_deformers(struct psm__model *);

/* Deformer field transforms (sample the deformer's own grid/rotation
 * state). Used by the fused deformer and art-mesh dirty stages. */
PSM__DEF void psm__warp_transform(struct psm__model *m, psm__i32 di,
    const psm__f32 *inputs, psm__f32 *outputs, psm__i32 count);
PSM__DEF void psm__rotation_transform(struct psm__model *m, psm__i32 di,
    const psm__f32 *inputs, psm__f32 *outputs, psm__i32 count);

#endif /* PSM__DEFORMER_H */
