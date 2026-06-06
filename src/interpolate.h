/*
 * Purism Core: interpolation declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__INTERPOLATE_H
#define PSM__INTERPOLATE_H

#include "private.h"
#include "model.h"

PSM__DEF void psm__interp_parts(struct psm__model *model);
PSM__DEF void psm__interp_warps(struct psm__model *model);
PSM__DEF void psm__interp_rotations(struct psm__model *model);
PSM__DEF void psm__interp_art_meshes(struct psm__model *model);
PSM__DEF void psm__interp_glues(struct psm__model *model);
PSM__DEF void psm__interp_offscreens(struct psm__model *model);

#endif /* PSM__INTERPOLATE_H */
