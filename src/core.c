/*
 * Purism Core: version and misc functions
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "private.h"

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

PSMDEF csmMocVersion
csmGetLatestMocVersion(void)
{
  return csmMocVersion_53;
}
