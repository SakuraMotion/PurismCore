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
PSM__DEF void psm__interp_glues(struct psm__model *model);

/* Per-object base-state recomputation (dirty update pipeline) */
PSM__DEF void psm__interp_warp_one(struct psm__model *model, psm__i32 si);
PSM__DEF void psm__interp_rotation_one(struct psm__model *model, psm__i32 si);
PSM__DEF void psm__interp_art_mesh_one(struct psm__model *model, psm__i32 i);
PSM__DEF void psm__interp_offscreen_one(struct psm__model *model, psm__i32 i);

#endif /* PSM__INTERPOLATE_H */
