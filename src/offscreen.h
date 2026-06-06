/*
 * Purism Core: offscreen declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__OFFSCREEN_H
#define PSM__OFFSCREEN_H

#include "private.h"
#include "model.h"

PSM__DEF void psm__enable_offscreens(struct psm__model *);
PSM__DEF void psm__gather_offscreens(struct psm__model *);

#endif /* PSM__OFFSCREEN_H */
