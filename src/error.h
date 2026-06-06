/*
 * Purism Core: error handling
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__ERROR_H
#define PSM__ERROR_H

#include "private.h"

#define PSM__FAILM(cond, v, msg) \
  if (cond) { PSM__LOG(msg); return v; }
#define PSM__FAIL(cond, v, fmt, ...) \
  if (cond) { PSM__LOGF(fmt, __VA_ARGS__); return v; }

enum {
  PSM__OK,
  PSM__FAILED,
  PSM__ERR_PARAMETER_RANGE_ERROR,
  PSM__ERR_FILE_UNRECOGNIZED,
  PSM__ERR_FILE_CORRUPT,
  PSM__ERR_INVALID_DATA,
  PSM__ERR_INVALID_PARAMETER,
  PSM__ERR_MAX,
};

#endif /* PSM__ERROR_H */
