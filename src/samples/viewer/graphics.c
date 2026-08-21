/*
 * Purism Core: sample model viewer graphics helpers
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "viewer.h"

#include "embed/draw.frag.h"
#include "embed/masked.frag.h"
#include "embed/maskwrite.frag.h"
#if PSM_COMPAT_VERSION >= 0x06000000L
#include "embed/blend.frag.h"
#include "embed/offscreen.frag.h"
#include "embed/offscreen_masked.frag.h"
#endif

#if defined(__EMSCRIPTEN__)
#define GLSL_VERSION_HEADER "#version 300 es\nprecision highp float;\nprecision highp int;\n"
#else
#define GLSL_VERSION_HEADER "#version 330\n"
#endif

static Shader LoadFs(const char *body)
{
    char *src = (char *)malloc(strlen(GLSL_VERSION_HEADER) + strlen(body) + 1);
    if (!src) return LoadShaderFromMemory(NULL, body); /* OOM: try unversioned */
    strcpy(src, GLSL_VERSION_HEADER);
    strcat(src, body);
    Shader s = LoadShaderFromMemory(NULL, src);
    free(src);
    return s;
}

void WorldToScreen(const View *v, float mx, float my, float *sx, float *sy)
{
    float wx = mx, wy = -my; /* Live2D y-up -> screen y-down */
    *sx = (wx - v->cx) * v->zoom + v->sw * 0.5f + v->panx;
    *sy = (wy - v->cy) * v->zoom + v->sh * 0.5f + v->pany;
}

/* Fit the camera to the model AABB at its current pose. */
void FitView(View *v, csmModel *model)
{
    int dc = csmGetDrawableCount(model);
    const int *vcount = csmGetDrawableVertexCounts(model);
    const csmVector2 **pos = csmGetDrawableVertexPositions(model);
    float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
    for (int d = 0; d < dc; d++)
    {
        for (int i = 0; i < vcount[d]; i++)
        {
            float x = pos[d][i].X, y = -pos[d][i].Y;
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }
    }
    if (maxx <= minx || maxy <= miny)
    { /* degenerate */
        v->cx = v->cy = 0.0f;
        v->zoom = 100.0f;
    }
    else
    {
        v->cx = (minx + maxx) * 0.5f;
        v->cy = (miny + maxy) * 0.5f;
        float zx = (v->sw * 0.8f) / (maxx - minx);
        float zy = (v->sh * 0.8f) / (maxy - miny);
        v->zoom = zx < zy ? zx : zy;
    }
    v->panx = v->pany = 0.0f;
}

void EmitDrawable(const View *v, int texId, const csmVector2 *pos, const csmVector2 *uv, const unsigned short *idx,
                  int idxCount)
{
    int i = 0;
    while (i < idxCount)
    {
        int chunk = idxCount - i;
        if (chunk > EMIT_CHUNK) chunk = EMIT_CHUNK; /* EMIT_CHUNK is a multiple of 3 (whole triangles) */
        rlCheckRenderBatchLimit(chunk);
        rlSetTexture((unsigned)texId);
        rlBegin(RL_TRIANGLES);
        rlColor4ub(255, 255, 255, 255);
        for (int k = 0; k < chunk; k++)
        {
            unsigned short j = idx[i + k];
            float sx, sy;
            WorldToScreen(v, pos[j].X, pos[j].Y, &sx, &sy);
            /* Cubism UVs are bottom-left origin; raylib textures are top-left. */
            rlTexCoord2f(uv[j].X, 1.0f - uv[j].Y);
            rlVertex2f(sx, sy);
        }
        rlEnd();
        i += chunk;
    }
}

/* A unit vector selecting RGBA channel c. */
void ChannelVec(int c, float v[4])
{
    v[0] = v[1] = v[2] = v[3] = 0.0f;
    v[c] = 1.0f;
}

/* Two mask lists describe the same clip if they are equal as sets. */
static bool SameMaskSet(const int *a, int na, const int *b, int nb)
{
    if (na != nb) return false;
    for (int i = 0; i < na; i++)
    {
        bool found = false;
        for (int j = 0; j < nb; j++)
            if (a[i] == b[j])
            {
                found = true;
                break;
            }
        if (!found) return false;
    }
    return true;
}

/* Group masked drawables by their mask-set. The grouping is static model
 * data (mask membership never changes), so it is built once at load. */
static bool BuildClipGroups(csmModel *model, ClipGroups *cg)
{
    int dc = csmGetDrawableCount(model);
    const int *mcount = csmGetDrawableMaskCounts(model);
    const int **masks = csmGetDrawableMasks(model);

    memset(cg, 0, sizeof(*cg));
    cg->of = (int *)malloc(sizeof(int) * (dc > 0 ? dc : 1));
    if (!cg->of) return false;

    for (int d = 0; d < dc; d++)
    {
        if (mcount[d] <= 0)
        {
            cg->of[d] = -1;
            continue;
        }
        int g = -1;
        for (int k = 0; k < cg->count; k++)
            if (SameMaskSet(masks[d], mcount[d], cg->masks[k], cg->maskCount[k]))
            {
                g = k;
                break;
            }
        if (g < 0)
        {
            if (cg->count >= MAX_CLIP_GROUPS)
            {
                fprintf(stderr, "warning: >%d clip groups; drawable %d unmasked\n", MAX_CLIP_GROUPS, d);
                cg->of[d] = -1;
                continue;
            }
            g = cg->count++;
            cg->masks[g] = masks[d];
            cg->maskCount[g] = mcount[d];
        }
        cg->of[d] = g;
    }
    cg->bufCount = (cg->count + GROUPS_PER_BUF - 1) / GROUPS_PER_BUF;
    return true;
}

static void ClipGroupsAllocBuffers(ClipGroups *cg, int w, int h)
{
    for (int b = 0; b < cg->bufCount; b++)
        cg->buf[b] = LoadRenderTexture(w, h);
}

static void ClipGroupsFreeBuffers(ClipGroups *cg)
{
    for (int b = 0; b < cg->bufCount; b++)
        UnloadRenderTexture(cg->buf[b]);
}

#if PSM_COMPAT_VERSION >= 0x06000000L
/* Walk a part ancestry chain: is `ancestor` part an ancestor of (or equal to)
 * `part`? -1 means "no part". */
static bool PartIsAncestor(const int *partParent, int ancestor, int part)
{
    if (ancestor < 0) return false;
    while (part >= 0)
    {
        if (part == ancestor) return true;
        part = partParent[part];
    }
    return false;
}

/* Does drawable d fall within offscreen o's scope? True iff o's owner part is
 * an ancestor (inclusive) of d's parent part. */
bool DrawableInOffscreen(const Offscreens *os, const int *partParent, const int *drawPart, int d, int o)
{
    if (o < 0) return true; /* the root screen contains everything */
    return PartIsAncestor(partParent, os->owner[o], drawPart[d]);
}

/* Precompute each offscreen's parent offscreen (mirrors SetupParentOffscreens):
 * walk up from the owner part's parent until a part that owns some offscreen. */
static void OffscreensLinkParents(Offscreens *os, const int *partParent)
{
    for (int o = 0; o < os->count; o++)
    {
        os->parent[o] = -1;
        int p = os->owner[o] >= 0 ? partParent[os->owner[o]] : -1;
        while (p >= 0)
        {
            int found = -1;
            for (int k = 0; k < os->count; k++)
                if (os->owner[k] == p)
                {
                    found = k;
                    break;
                }
            if (found >= 0)
            {
                os->parent[o] = found;
                break;
            }
            p = partParent[p];
        }
    }
}

/* Build the per-offscreen mask groups (channel-packed clip buffers), reusing
 * the same set-dedup the drawable masking uses. */
static bool OffscreensBuildMasks(Offscreens *os, csmModel *model)
{
    const int *mcount = csmGetOffscreenMaskCounts(model);
    const int **masks = csmGetOffscreenMasks(model);
    ClipGroups *cg = &os->mcg;

    memset(cg, 0, sizeof(*cg));
    cg->of = NULL; /* unused for offscreens; mgroup[] is the index */

    for (int o = 0; o < os->count; o++)
    {
        if (mcount[o] <= 0)
        {
            os->mgroup[o] = -1;
            continue;
        }
        int g = -1;
        for (int k = 0; k < cg->count; k++)
            if (SameMaskSet(masks[o], mcount[o], cg->masks[k], cg->maskCount[k]))
            {
                g = k;
                break;
            }
        if (g < 0)
        {
            if (cg->count >= MAX_CLIP_GROUPS)
            {
                os->mgroup[o] = -1;
                continue;
            }
            g = cg->count++;
            cg->masks[g] = masks[o];
            cg->maskCount[g] = mcount[o];
        }
        os->mgroup[o] = g;
    }
    cg->bufCount = (cg->count + GROUPS_PER_BUF - 1) / GROUPS_PER_BUF;
    return true;
}

static bool OffscreensInit(Offscreens *os, csmModel *model, int w, int h)
{
    memset(os, 0, sizeof(*os));
    os->count = csmGetOffscreenCount(model);
    if (os->count <= 0) return true; /* dormant: the flat path runs, byte-identical to before */
    if (os->count > MAX_OFFSCREENS)
    {
        fprintf(stderr, "warning: %d offscreens > %d; compositing disabled\n", os->count, MAX_OFFSCREENS);
        os->count = 0;
        return true;
    }
    os->owner = csmGetOffscreenOwnerIndices(model);
    OffscreensLinkParents(os, csmGetPartParentPartIndices(model));
    for (int o = 0; o < os->count; o++)
        os->rt[o] = LoadRenderTexture(w, h);
    OffscreensBuildMasks(os, model);
    ClipGroupsAllocBuffers(&os->mcg, (int)(w * DEFAULT_MASK_SCALE), (int)(h * DEFAULT_MASK_SCALE));
    return true;
}

static void OffscreensResize(Offscreens *os, int w, int h, float maskScale)
{
    for (int o = 0; o < os->count; o++)
    {
        UnloadRenderTexture(os->rt[o]);
        os->rt[o] = LoadRenderTexture(w, h);
    }
    ClipGroupsFreeBuffers(&os->mcg);
    ClipGroupsAllocBuffers(&os->mcg, (int)(w * maskScale), (int)(h * maskScale));
}

static void OffscreensFree(Offscreens *os)
{
    for (int o = 0; o < os->count; o++)
        UnloadRenderTexture(os->rt[o]);
    ClipGroupsFreeBuffers(&os->mcg);
}
#endif /* offscreens (v6) */

bool RendererInit(Renderer *r, const Model3 *m3, const char *dir, csmModel *model, int w, int h, float maskScale)
{
    memset(r, 0, sizeof(*r));
    r->maskScale = maskScale;
    r->texCount = m3->texCount;
    for (int i = 0; i < m3->texCount; i++)
    {
        char tp[1024];
        snprintf(tp, sizeof(tp), "%s%s", dir, m3->tex[i]);
        r->textures[i] = LoadTexture(tp);
        if (r->textures[i].id) SetTextureFilter(r->textures[i], TEXTURE_FILTER_BILINEAR);
    }

    r->sdraw = LoadFs(draw_frag);
    r->smask = LoadFs(masked_frag);
    r->smaskw = LoadFs(maskwrite_frag);
    r->locBase = GetShaderLocation(r->sdraw, "baseColor");
    r->locMul = GetShaderLocation(r->sdraw, "multiplyColor");
    r->locScr = GetShaderLocation(r->sdraw, "screenColor");
    r->mbase = GetShaderLocation(r->smask, "baseColor");
    r->mmul = GetShaderLocation(r->smask, "multiplyColor");
    r->mscr = GetShaderLocation(r->smask, "screenColor");
    r->mmask = GetShaderLocation(r->smask, "maskTexture");
    r->mres = GetShaderLocation(r->smask, "resolution");
    r->minv = GetShaderLocation(r->smask, "maskInvert");
    r->msel = GetShaderLocation(r->smask, "channelSelector");
    r->wmask = GetShaderLocation(r->smaskw, "channelMask");

#if PSM_COMPAT_VERSION >= 0x06000000L
    r->soff = LoadFs(offscreen_frag);
    r->soffm = LoadFs(offscreen_masked_frag);
    r->obBase = GetShaderLocation(r->soff, "baseColor");
    r->obMul = GetShaderLocation(r->soff, "multiplyColor");
    r->obScr = GetShaderLocation(r->soff, "screenColor");
    r->omBase = GetShaderLocation(r->soffm, "baseColor");
    r->omMul = GetShaderLocation(r->soffm, "multiplyColor");
    r->omScr = GetShaderLocation(r->soffm, "screenColor");
    r->omMask = GetShaderLocation(r->soffm, "maskTexture");
    r->omRes = GetShaderLocation(r->soffm, "resolution");
    r->omInv = GetShaderLocation(r->soffm, "maskInvert");
    r->omSel = GetShaderLocation(r->soffm, "channelSelector");

    r->sblend = LoadFs(blend_frag);
    r->sbBlend = GetShaderLocation(r->sblend, "blendTexture");
    r->sbMask = GetShaderLocation(r->sblend, "maskTexture");
    r->sbRes = GetShaderLocation(r->sblend, "resolution");
    r->sbCmode = GetShaderLocation(r->sblend, "colorMode");
    r->sbAmode = GetShaderLocation(r->sblend, "alphaMode");
    r->sbUsemask = GetShaderLocation(r->sblend, "useMask");
    r->sbInv = GetShaderLocation(r->sblend, "maskInvert");
    r->sbSel = GetShaderLocation(r->sblend, "channelSelector");
    r->sbBase = GetShaderLocation(r->sblend, "baseColor");
    r->sbMul = GetShaderLocation(r->sblend, "multiplyColor");
    r->sbScr = GetShaderLocation(r->sblend, "screenColor");
    r->backdrop = (Texture2D){ rlLoadTexture(NULL, w, h, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1), w, h, 1,
                               PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    r->scratch = LoadRenderTexture(w, h);
#endif

    if (!BuildClipGroups(model, &r->cg)) return false;
    ClipGroupsAllocBuffers(&r->cg, (int)(w * r->maskScale), (int)(h * r->maskScale));
#if PSM_COMPAT_VERSION >= 0x06000000L
    if (!OffscreensInit(&r->os, model, w, h)) return false;
#endif
    return true;
}

void RendererResize(Renderer *r, int w, int h)
{
    ClipGroupsFreeBuffers(&r->cg);
    ClipGroupsAllocBuffers(&r->cg, (int)(w * r->maskScale), (int)(h * r->maskScale));
#if PSM_COMPAT_VERSION >= 0x06000000L
    OffscreensResize(&r->os, w, h, r->maskScale);
    rlUnloadTexture(r->backdrop.id);
    r->backdrop = (Texture2D){ rlLoadTexture(NULL, w, h, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1), w, h, 1,
                               PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    UnloadRenderTexture(r->scratch);
    r->scratch = LoadRenderTexture(w, h);
#endif
}

void RendererFree(Renderer *r)
{
    for (int i = 0; i < r->texCount; i++)
        if (r->textures[i].id) UnloadTexture(r->textures[i]);
    UnloadShader(r->sdraw);
    UnloadShader(r->smask);
    UnloadShader(r->smaskw);
    ClipGroupsFreeBuffers(&r->cg);
    free(r->cg.of);
#if PSM_COMPAT_VERSION >= 0x06000000L
    UnloadShader(r->soff);
    UnloadShader(r->soffm);
    UnloadShader(r->sblend);
    rlUnloadTexture(r->backdrop.id);
    UnloadRenderTexture(r->scratch);
    OffscreensFree(&r->os);
#endif
}
