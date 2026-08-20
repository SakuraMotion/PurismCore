/*
 * Purism Core: logging
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM_NO_STDIO
#  include <stdarg.h>
#  include <stdio.h>
#endif

#include "private.h"
#include "debug.h"

static void psm__default_log(const char *);

static csmLogFunction psm__log_fn = psm__default_log;
static psm__log_level psm__my_log_level = PSM__LOG_VERBOSE;

static void
psm__default_log(const char *s)
{
#ifndef PSM_NO_STDIO
  fprintf(stderr, "%s", s);
#else
  (void)s;
#endif
}

PSM__DEF psm__log_level
psm__get_log_level(void)
{
  return psm__my_log_level;
}

PSM__DEF void
psm__set_log_level(psm__log_level level)
{
  psm__my_log_level = level;
}

PSM__DEF void
psm__debug_print(int level, const char *fmt, ...)
{
  if (level < psm__my_log_level || !psm__log_fn)
    return;

#ifndef PSM_NO_STDIO
  char    buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
#else
  const char *buf = fmt;
#endif

  psm__log_fn(buf);
}

PSMDEF csmLogFunction
csmGetLogFunction(void)
{
  return psm__log_fn;
}

PSMDEF void
csmSetLogFunction(csmLogFunction f)
{
  psm__log_fn = f;
}

PSMDEF int
csmGetLogLevel(void)
{
  return psm__my_log_level;
}

PSMDEF void
csmSetLogLevel(int level)
{
  psm__my_log_level = level;
}
