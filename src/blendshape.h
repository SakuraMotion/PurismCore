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
PSM__DEF void psm__blend_glues(struct psm__model *model);

/* Per-object blend contributions (dirty update pipeline): re-add the
 * blend-shape terms of one object on top of its re-interpolated base. */
PSM__DEF void psm__blend_warp_one(struct psm__model *model, psm__i32 si);
PSM__DEF void psm__blend_rotation_one(struct psm__model *model, psm__i32 si);
PSM__DEF void psm__blend_art_mesh_one(struct psm__model *model, psm__i32 i);
PSM__DEF void psm__blend_offscreen_one(struct psm__model *model, psm__i32 i);

/* Per-frame dirty scan over one blend-shape table:
 * dirty[t] = a blend shape targeting object t is dirty this frame. */
PSM__DEF void psm__blend_targets_dirty(const struct psm__blend_shapes *bs,
    psm__i32 count, psm__u8 *dirty);

#endif /* PSM__BLENDSHAPE_H */
