/*
 * Purism Core: art mesh declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__ARTMESH_H
#define PSM__ARTMESH_H

#include "private.h"
#include "model.h"

PSM__DEF void psm__enable_art_meshes(struct psm__model *model);
PSM__DEF void psm__gather_art_meshes(struct psm__model *model);
PSM__DEF void psm__apply_parts_to_meshes(struct psm__model *model);

#endif /* PSM__ARTMESH_H */
