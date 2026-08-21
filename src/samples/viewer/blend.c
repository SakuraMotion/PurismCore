/*
 * Purism Core: sample model viewer blend setup
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "viewer.h"

static void ApplyBlendMode(int mode)
{
    if (mode == PSM_BLEND_ADDITIVE)
    {
        rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    }
    else if (mode == PSM_BLEND_MULTIPLICATIVE)
    {
        rlSetBlendFactorsSeparate(RL_DST_COLOR, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    }
    else
    { /* normal (premultiplied over) */
        rlSetBlendFactorsSeparate(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD,
                                  RL_FUNC_ADD);
    }
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
}

void ApplyBlend(csmFlags flags)
{
    ApplyBlendMode((flags & csmBlendAdditive)         ? PSM_BLEND_ADDITIVE
                   : (flags & csmBlendMultiplicative) ? PSM_BLEND_MULTIPLICATIVE
                                                      : PSM_BLEND_NORMAL);
}

#if PSM_COMPAT_VERSION >= 0x06000000L
static bool BlendColorIsSimple(int color)
{
    return color == 0 || color == 1 || color == 2 || color == 3 || color == 4 || color == 6;
}

bool BlendIsExotic(int extended)
{
    int color = extended & 0xFF;
    int alpha = (extended >> 8) & 0xFF;
    return !BlendColorIsSimple(color) || alpha >= 3;
}

bool BlendAlphaNeedsCoverage(int extended)
{
    int color = extended & 0xFF;
    int alpha = (extended >> 8) & 0xFF;
    if (alpha == 0) return false;
    return color == 0 || BlendIsExotic(extended);
}

void ApplyExtendedBlend(int extended)
{
    int color = extended & 0xFF;
    int alpha = (extended >> 8) & 0xFF;

    if (color == 1 || color == 3 || color == 4)
    { /* additive color */
        rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    }
    else if (color == 2 || color == 6)
    { /* multiplicative */
        rlSetBlendFactorsSeparate(RL_DST_COLOR, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    }
    else
    {                                          /* normal color */
        int sf = (alpha == 1)   ? RL_DST_ALPHA /* Atop */
                 : (alpha == 2) ? RL_ZERO      /* Out */
                                : RL_ONE;      /* Over / approx others */
        rlSetBlendFactorsSeparate(sf, RL_ONE_MINUS_SRC_ALPHA, sf, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    }
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
}
#endif
