/*
 * Purism Core: logging declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__DEBUG_H
#define PSM__DEBUG_H

#include "private.h"

typedef int psm__log_level;

enum {
  PSM__LOG_VERBOSE,
  PSM__LOG_DEBUG,
  PSM__LOG_INFO,
  PSM__LOG_WARN,
  PSM__LOG_ERR,
  PSM__LOG_OFF,
};

#define PSM__LOG_PREFIX "[PSM] "

#define PSM__LOG_PREFIX_DEBUG PSM__LOG_PREFIX "D: "
#define PSM__LOG_PREFIX_INFO  PSM__LOG_PREFIX "I: "
#define PSM__LOG_PREFIX_WARN  PSM__LOG_PREFIX "W: "
#define PSM__LOG_PREFIX_ERR   PSM__LOG_PREFIX "E: "

#define PSM__LOG(msg) \
  psm__debug_print(PSM__LOG_ERR, PSM__LOG_PREFIX_ERR msg "\n")
#define PSM__LOGF(fmt, ...) \
  psm__debug_print(PSM__LOG_ERR, PSM__LOG_PREFIX_ERR fmt "\n", __VA_ARGS__)
#define PSM__WARN(msg) \
  psm__debug_print(PSM__LOG_WARN, PSM__LOG_PREFIX_WARN msg "\n")
#define PSM__WARNF(fmt, ...) \
  psm__debug_print(PSM__LOG_WARN, PSM__LOG_PREFIX_WARN fmt "\n", __VA_ARGS__)
#define PSM__INFO(msg) \
  psm__debug_print(PSM__LOG_INFO, PSM__LOG_PREFIX_INFO msg "\n")
#define PSM__INFOF(fmt, ...) \
  psm__debug_print(PSM__LOG_INFO, PSM__LOG_PREFIX_INFO fmt "\n", __VA_ARGS__)
#define PSM__DBG(msg) \
  psm__debug_print(PSM__LOG_DEBUG, PSM__LOG_PREFIX_DEBUG msg "\n")
#define PSM__DBGF(fmt, ...) \
  psm__debug_print(PSM__LOG_DEBUG, PSM__LOG_PREFIX_DEBUG fmt "\n", __VA_ARGS__)

PSM__DEF psm__log_level psm__get_log_level(void);
PSM__DEF void           psm__set_log_level(psm__log_level level);
PSM__DEF void           psm__debug_print(int level, const char *format, ...);

#endif /* PSM__DEBUG_H */
