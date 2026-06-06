/*
 * Purism Core: blend shape declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__BLENDSHAPE_H
#define PSM__BLENDSHAPE_H

#include "private.h"
#include "model.h"

PSM__DEF void psm__blend_parts(struct psm__model *model);
PSM__DEF void psm__blend_warps(struct psm__model *model);
PSM__DEF void psm__blend_rotations(struct psm__model *model);
PSM__DEF void psm__blend_art_meshes(struct psm__model *model);
PSM__DEF void psm__blend_glues(struct psm__model *model);
PSM__DEF void psm__blend_offscreens(struct psm__model *model);

#endif /* PSM__BLENDSHAPE_H */
