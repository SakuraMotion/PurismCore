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
PSM__DEF void psm__apply_transforms(struct psm__model *);
PSM__DEF void psm__apply_transforms_to_meshes(struct psm__model *);

#endif /* PSM__DEFORMER_H */
