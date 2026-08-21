/*
 * Purism Core: version and misc functions
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "private.h"

#ifndef PSM_GIT_HASH
#  define PSM_GIT_HASH "unknown"
#endif

PSMDEF csmVersion
csmGetVersion(void)
{
  return PSM_COMPAT_VERSION;
}

PSMDEF csmVersion
csmGetTrueVersion(void)
{
  return PSM_TRUE_VERSION;
}

PSMDEF const char *
csmGetExtendedVersionString(void)
{
  static char buf[96];
  if (buf[0] == '\0')
    snprintf(buf, sizeof buf, PSM__VERFMT " (%s)",
        PSM__VERARG(PSM_TRUE_VERSION), PSM_GIT_HASH);
  return buf;
}

PSMDEF csmMocVersion
csmGetLatestMocVersion(void)
{
  return csmMocVersion_53;
}
