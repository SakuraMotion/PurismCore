/*
 * Purism Core: part declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__PART_H
#define PSM__PART_H

#include "private.h"
#include "model.h"

PSM__DEF void psm__enable_parts(struct psm__model *model);
PSM__DEF void psm__gather_parts(struct psm__model *model);
PSM__DEF void psm__apply_part_opacity(struct psm__model *model);

#endif /* PSM__PART_H */
