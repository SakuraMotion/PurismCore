/*
 * Purism Core: glue declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__GLUE_H
#define PSM__GLUE_H

#include "private.h"
#include "model.h"

PSM__DEF void psm__gather_glues(struct psm__model *model);
PSM__DEF void psm__apply_glues(struct psm__model *model);

#endif /* PSM__GLUE_H */
