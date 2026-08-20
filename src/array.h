/*
 * Purism Core: bounds-checking array macros
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__ARRAY_H
#define PSM__ARRAY_H

#include "private.h"
#include "error.h"

static inline int
psm__array_check_index(psm_size i, psm_size n)
{
#ifdef PSM_FAST_AND_DANGEROUS
  (void)i;
  (void)n;
  return PSM__OK;
#else
  return (i < n) ? PSM__OK : PSM__ERR_PARAMETER_RANGE_ERROR;
#endif
}

/*
 * psm__check_idx returns 1 if idx is in [0, max), 0 otherwise.
 * When PSM_FAST_AND_DANGEROUS is defined, always returns 1.
 */
#define psm__check_idx(idx, max) \
  (psm__array_check_index((psm_size)(idx), (max)) == PSM__OK)

/*
 * psm__check_offset_range checks if [offset, offset+count) is within [0, max).
 * Returns 1 if valid, 0 otherwise. Uses unsigned arithmetic to detect overflow.
 * When PSM_FAST_AND_DANGEROUS is defined, always returns 1.
 */
#ifdef PSM_FAST_AND_DANGEROUS
#  define psm__check_offset_range(offset, count, max) (1)
#else
#  define psm__check_offset_range(offset, count, max) \
    ((offset) >= 0 && (count) >= 0 && \
        (psm__u32)(max) - (psm__u32)(offset) >= (psm__u32)(count))
#endif

/*
 * Index relationship validators.
 *
 * psm__valid_idx: required index, must be in [0, max).
 * psm__valid_opt_idx: optional index, -1 means "none",
 *   otherwise [0, max). Returns: 1=valid, 0=none, -1=bad.
 * psm__valid_range: required range [begin, begin+count)
 *   must fit in [0, max).
 * psm__valid_opt_range: optional range, begin<0 with
 *   count==0 means "none". Returns: 1=valid, 0=none, -1=bad.
 */
#ifdef PSM_FAST_AND_DANGEROUS

#  define psm__valid_idx(idx, max)            1
#  define psm__valid_opt_idx(idx, max)        ((idx) >= 0 ? 1 : 0)
#  define psm__valid_range(begin, count, max) 1
#  define psm__valid_opt_range(begin, count, max) \
    ((begin) >= 0 ? 1 : 0)

#else

static inline bool
psm__valid_idx(psm__i32 idx, psm__i32 max)
{
  return (psm__u32)idx < (psm__u32)max;
}

static inline int
psm__valid_opt_idx(psm__i32 idx, psm__i32 max)
{
  if (idx < 0) return 0;
  return (psm__u32)idx < (psm__u32)max ? 1 : -1;
}

static inline bool
psm__valid_range(psm__i32 begin, psm__i32 count, psm__i32 max)
{
  /*
   * begin <= max is required: without it, (u32)max - (u32)begin underflows
   * to a huge value when begin > max and the range check wrongly passes.
   */
  return begin >= 0 && count >= 0 && begin <= max &&
         (psm__u32)max - (psm__u32)begin >= (psm__u32)count;
}

static inline int
psm__valid_opt_range(psm__i32 begin, psm__i32 count, psm__i32 max)
{
  if (begin < 0) return count == 0 ? 0 : -1;
  return psm__valid_range(begin, count, max) ? 1 : -1;
}

#endif

/*
 * psm__clamp_idx clamps idx to [0, max-1] for defensive access.
 * Returns 0 if max <= 0.
 */
static inline psm__i32
psm__clamp_idx(psm__i32 idx, psm__i32 max)
{
  if (max <= 0) return 0;
  return psm__clamp_i32(idx, 0, max - 1);
}

/*
 * psm__safe_order_level computes max_do - min_do + 1 safely, checking for:
 * - max_do < min_do (invalid range)
 * - integer overflow in the subtraction
 * Returns 0 for invalid input, otherwise the positive order level.
 */
static inline psm__i32
psm__safe_order_level(psm__i32 max_do, psm__i32 min_do)
{
  if (max_do < min_do)
    return 0;
  /*
   * Use unsigned arithmetic to safely compute range and detect overflow.
   * This handles all cases including max_do=INT32_MAX with min_do>=0.
   */
  psm__u32 range = (psm__u32)max_do - (psm__u32)min_do;
  if (range > 0x7FFFFFFE)  /* range + 1 would exceed INT32_MAX */
    return 0;
  return (psm__i32)(range + 1);
}

#endif /* PSM__ARRAY_H */
