/*
 * Purism Core: parameter declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__PARAM_H
#define PSM__PARAM_H

#include "private.h"
#include "model.h"

/* returns true if any non-repeat parameter was outside [min,max] (clamped) */
PSM__DEF bool psm__resolve_params(struct psm__params *);
PSM__DEF void psm__resolve_key_tables(struct psm__model *);
PSM__DEF void psm__resolve_blend_key_tables(struct psm__model *);
PSM__DEF void psm__resolve_bindings(struct psm__model *);
PSM__DEF void psm__resolve_blend_bindings(struct psm__model *);

#endif /* PSM__PARAM_H */
