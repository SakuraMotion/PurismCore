/*
 * Purism Core: sample model viewer rendering
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "viewer.h"

static void RenderMasks(csmModel *model, Renderer *r, const View *view)
{
    int dc = csmGetDrawableCount(model);
    const csmVector2 **pos = csmGetDrawableVertexPositions(model);
    const csmVector2 **uv = csmGetDrawableVertexUvs(model);
    const int *icount = csmGetDrawableIndexCounts(model);
    const unsigned short **idx = csmGetDrawableIndices(model);
    const int *texidx = csmGetDrawableTextureIndices(model);

    /* The mask buffers are MASK_SCALE of screen size. Render the masks with a
     * matchingly-scaled view so their content lands at the same normalized
     * screen position the masked drawables sample at (gl_FragCoord/resolution). */
    float s = r->maskScale;
    View mv = *view;
    mv.zoom *= s;
    mv.panx *= s;
    mv.pany *= s;
    mv.sw = (int)(view->sw * s);
    mv.sh = (int)(view->sh * s);

    BeginShaderMode(r->smaskw);
    for (int b = 0; b < r->cg.bufCount; b++)
    {
        BeginTextureMode(r->cg.buf[b]);
        ClearBackground(BLANK);
        /* Pure additive (src*1 + dst*1) so each group's coverage lands in its
         * own channel without disturbing the others. */
        rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ONE, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
        rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);

        int g0 = b * GROUPS_PER_BUF;
        int g1 = g0 + GROUPS_PER_BUF;
        if (g1 > r->cg.count) g1 = r->cg.count;
        for (int g = g0; g < g1; g++)
        {
            float cmask[4];
            ChannelVec(g % GROUPS_PER_BUF, cmask);
            SetShaderValue(r->smaskw, r->wmask, cmask, SHADER_UNIFORM_VEC4);
            for (int mi = 0; mi < r->cg.maskCount[g]; mi++)
            {
                int md = r->cg.masks[g][mi];
                if (md < 0 || md >= dc) continue;
                int t = texidx[md] >= 0 ? r->textures[texidx[md]].id : 0;
                EmitDrawable(&mv, t, pos[md], uv[md], idx[md], icount[md]);
            }
            rlDrawRenderBatchActive(); /* flush so this group draws with its channel */
        }
        EndTextureMode();
    }
    EndShaderMode();
    rlSetBlendMode(RL_BLEND_ALPHA); /* restore default for the main pass */
}

static void DrawOneDrawable(csmModel *model, Renderer *r, const View *view, bool maskingOn, int d, const float resv[2],
                            int blendOverride)
{
    const int *texidx = csmGetDrawableTextureIndices(model);
    const csmVector2 **pos = csmGetDrawableVertexPositions(model);
    const csmVector2 **uv = csmGetDrawableVertexUvs(model);
    const int *icount = csmGetDrawableIndexCounts(model);
    const unsigned short **idx = csmGetDrawableIndices(model);
    const float *opacity = csmGetDrawableOpacities(model);
    const csmVector4 *mul = csmGetDrawableMultiplyColors(model);
    const csmVector4 *scr = csmGetDrawableScreenColors(model);
    const csmFlags *cflags = csmGetDrawableConstantFlags(model);

    int g = maskingOn ? r->cg.of[d] : -1;
    csmVector4 mc = mul[d], sc = scr[d];
    float base[4] = { 1, 1, 1, opacity[d] };
    float mulv[4] = { mc.X, mc.Y, mc.Z, 1 };
    float scrv[4] = { sc.X, sc.Y, sc.Z, 0 };

    Shader sh = (g >= 0) ? r->smask : r->sdraw;
    BeginShaderMode(sh);
    /* Set the blend mode BEFORE binding the mask sampler: rlSetBlendMode
     * flushes the batch when the mode changes, and the flush clears the
     * extra-texture registration (activeTextureId). Registering the mask
     * first, then flushing, would unbind it and the mask sampler would read
     * a stale unit (~1 coverage = no clipping). */
#if PSM_COMPAT_VERSION >= 0x06000000L
    /* v6 drawables carry an extended color+alpha blend (e.g. the hologram is
     * HardLight+Atop): honor it so Atop/Out clip to the destination. */
    ApplyExtendedBlend(blendOverride >= 0 ? blendOverride : csmGetDrawableBlendModes(model)[d]);
#else
    ApplyBlend(cflags[d]);
#endif
    if (g >= 0)
    {
        SetShaderValue(sh, r->mbase, base, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->mmul, mulv, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->mscr, scrv, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->mres, resv, SHADER_UNIFORM_VEC2);
        float invf = (cflags[d] & csmIsInvertedMask) ? 1.0f : 0.0f;
        SetShaderValue(sh, r->minv, &invf, SHADER_UNIFORM_FLOAT);
        float csel[4];
        ChannelVec(g % GROUPS_PER_BUF, csel);
        SetShaderValue(sh, r->msel, csel, SHADER_UNIFORM_VEC4);
        SetShaderValueTexture(sh, r->mmask, r->cg.buf[g / GROUPS_PER_BUF].texture);
    }
    else
    {
        SetShaderValue(sh, r->locBase, base, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->locMul, mulv, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->locScr, scrv, SHADER_UNIFORM_VEC4);
    }

    int t = texidx[d] >= 0 ? r->textures[texidx[d]].id : 0;
    EmitDrawable(view, t, pos[d], uv[d], idx[d], icount[d]);
    EndShaderMode();
}

/* True if drawable d passes the debug filters (--only/--hide/--maxorder). */
static bool DrawablePassesFilters(int d, int ord, const Options *opt)
{
    if (ord > opt->maxOrder) return false;
    if (opt->onlyDrawable >= 0 && d != opt->onlyDrawable) return false;
    for (int h = 0; h < opt->hideCount; h++)
        if (opt->hideIdx[h] == d) return false;
    return true;
}

static void RenderModelFlat(csmModel *model, Renderer *r, const View *view, bool maskingOn, const Options *opt,
                            int *sorted)
{
    int dc = csmGetDrawableCount(model);
    const int *order = VIEWER_GET_RENDER_ORDERS(model);
    const float *opacity = csmGetDrawableOpacities(model);
    const csmFlags *dflags = csmGetDrawableDynamicFlags(model);

    /* sort drawable indices by render order (ascending = back to front).
     * Insertion sort: near-O(n) since order is stable frame to frame. */
    for (int i = 0; i < dc; i++)
        sorted[i] = i;
    for (int i = 1; i < dc; i++)
    {
        int key = sorted[i], j = i - 1;
        while (j >= 0 && order[sorted[j]] > order[key])
        {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    if (maskingOn) RenderMasks(model, r, view);

    float resv[2] = { (float)view->sw, (float)view->sh };
    rlDisableBackfaceCulling();

    for (int s = 0; s < dc; s++)
    {
        int d = sorted[s];
        if (!(dflags[d] & csmIsVisible) || opacity[d] <= 0.0f) continue;
        if (!DrawablePassesFilters(d, order[d], opt)) continue;
        DrawOneDrawable(model, r, view, maskingOn, d, resv, -1);
    }

    rlSetBlendMode(RL_BLEND_ALPHA); /* restore default for UI */
}

#if PSM_COMPAT_VERSION >= 0x06000000L
static void BindTarget(unsigned fbo, int w, int h)
{
    rlDrawRenderBatchActive(); /* flush whatever was queued for old target */
    rlEnableFramebuffer(fbo);
    rlViewport(0, 0, w, h);
    rlSetFramebufferWidth(w);
    rlSetFramebufferHeight(h);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, w, h, 0, -1.0, 1.0); /* y-down, raylib screen convention */
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

static void ScissorToOffscreen(const Offscreens *os, int o, int sh)
{
    if (o < 0)
    {
        rlDisableScissorTest();
        return;
    }
    const int *b = os->bbox[o];
    int x = b[0], y = b[1], w = b[2] - b[0], h = b[3] - b[1];
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    rlEnableScissorTest();
    rlScissor(x, sh - (y + h), w, h);
}

static void OffscreensComputeBboxes(Offscreens *os, csmModel *model, const View *view, bool maskingOn)
{
    int dc = csmGetDrawableCount(model);
    const csmVector2 **pos = csmGetDrawableVertexPositions(model);
    const int *vcount = csmGetDrawableVertexCounts(model);
    const csmFlags *dflags = csmGetDrawableDynamicFlags(model);
    const float *opacity = csmGetDrawableOpacities(model);
    const int *partParent = csmGetPartParentPartIndices(model);
    const int *drawPart = csmGetDrawableParentPartIndices(model);
    const int *bm = csmGetOffscreenBlendModes(model);
    const float *oop = csmGetOffscreenOpacities(model);
    const csmVector4 *mul = csmGetOffscreenMultiplyColors(model);
    const csmVector4 *scr = csmGetOffscreenScreenColors(model);

    for (int o = 0; o < os->count; o++)
    {
        os->bbox[o][0] = os->bbox[o][1] = 1 << 29; /* empty: x0,y0 = +inf */
        os->bbox[o][2] = os->bbox[o][3] = -(1 << 29);
        /* A group needs its own RT only if it changes pixels as a unit. */
        os->effectful[o] = bm[o] != 0 || oop[o] < 0.999f || (maskingOn && os->mgroup[o] >= 0) || mul[o].X != 1.0f ||
                           mul[o].Y != 1.0f || mul[o].Z != 1.0f || scr[o].X != 0.0f || scr[o].Y != 0.0f ||
                           scr[o].Z != 0.0f;
    }
    const int *dbm = csmGetDrawableBlendModes(model);
    for (int d = 0; d < dc; d++)
    {
        if (!(dflags[d] & csmIsVisible) || opacity[d] <= 0.0f) continue;
        float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
        for (int i = 0; i < vcount[d]; i++)
        {
            float sx, sy;
            WorldToScreen(view, pos[d][i].X, pos[d][i].Y, &sx, &sy);
            if (sx < minx) minx = sx;
            if (sx > maxx) maxx = sx;
            if (sy < miny) miny = sy;
            if (sy > maxy) maxy = sy;
        }
        /* clamp + pad by 1px (bilinear sampling) and to the screen */
        int x0 = (int)minx - 1, y0 = (int)miny - 1;
        int x1 = (int)maxx + 2, y1 = (int)maxy + 2;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > view->sw) x1 = view->sw;
        if (y1 > view->sh) y1 = view->sh;
        if (x1 <= x0 || y1 <= y0) continue;
        /* A drawable with an Atop/Out alpha op blends against its target's alpha,
         * so the group it draws into (its deepest owning group) must be a real RT,
         * not a pass-through to the opaque screen. Mark that group effectful. */
        bool needCov = BlendAlphaNeedsCoverage(dbm[d]);
        int deepest = -1;
        /* add to every offscreen whose owner part is an ancestor of this part */
        for (int p = drawPart[d]; p >= 0; p = partParent[p])
        {
            for (int o = 0; o < os->count; o++)
            {
                if (os->owner[o] != p) continue;
                if (deepest < 0) deepest = o; /* first match = deepest group */
                if (x0 < os->bbox[o][0]) os->bbox[o][0] = x0;
                if (y0 < os->bbox[o][1]) os->bbox[o][1] = y0;
                if (x1 > os->bbox[o][2]) os->bbox[o][2] = x1;
                if (y1 > os->bbox[o][3]) os->bbox[o][3] = y1;
            }
        }
        if (needCov && deepest >= 0) os->effectful[deepest] = true;
    }
}

/* Screen-space AABB (top-left px, clamped + 1px pad) of one drawable. */
static void DrawableScreenBbox(const View *view, csmModel *model, int d, int out[4])
{
    const csmVector2 **pos = csmGetDrawableVertexPositions(model);
    const int *vcount = csmGetDrawableVertexCounts(model);
    float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
    for (int i = 0; i < vcount[d]; i++)
    {
        float sx, sy;
        WorldToScreen(view, pos[d][i].X, pos[d][i].Y, &sx, &sy);
        if (sx < minx) minx = sx;
        if (sx > maxx) maxx = sx;
        if (sy < miny) miny = sy;
        if (sy > maxy) maxy = sy;
    }
    int x0 = (int)minx - 1, y0 = (int)miny - 1;
    int x1 = (int)maxx + 2, y1 = (int)maxy + 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > view->sw) x1 = view->sw;
    if (y1 > view->sh) y1 = view->sh;
    out[0] = x0;
    out[1] = y0;
    out[2] = x1;
    out[3] = y1;
}

#if defined(__EMSCRIPTEN__)
#include <GLES2/gl2.h>
#elif !defined(_WIN32)
#include <dlfcn.h>
#endif

typedef void (*PsmGlActiveTexture)(unsigned int);
typedef void (*PsmGlBindTexture)(unsigned int, unsigned int);
typedef void (*PsmGlCopyTexSubImage2D)(unsigned int, int, int, int, int, int, int, int);
static PsmGlActiveTexture psm_glActiveTexture;
static PsmGlBindTexture psm_glBindTexture;
static PsmGlCopyTexSubImage2D psm_glCopyTexSubImage2D;

#if !defined(__EMSCRIPTEN__) && defined(_WIN32)
extern void *__stdcall wglGetProcAddress(const char *name);
extern void *__stdcall GetModuleHandleA(const char *name);
extern void *__stdcall GetProcAddress(void *module, const char *name);
static void *GlProc(const char *name)
{
    void *p = wglGetProcAddress(name);                                  /* GL >1.1 */
    if (!p) p = GetProcAddress(GetModuleHandleA("opengl32.dll"), name); /* GL 1.1 */
    return p;
}
#endif

static void LoadGLProcs(void)
{
    if (psm_glActiveTexture) return;
#if defined(__EMSCRIPTEN__)
    psm_glActiveTexture = (PsmGlActiveTexture)glActiveTexture;
    psm_glBindTexture = (PsmGlBindTexture)glBindTexture;
    psm_glCopyTexSubImage2D = (PsmGlCopyTexSubImage2D)glCopyTexSubImage2D;
#elif defined(_WIN32)
    psm_glActiveTexture = (PsmGlActiveTexture)GlProc("glActiveTexture");
    psm_glBindTexture = (PsmGlBindTexture)GlProc("glBindTexture");
    psm_glCopyTexSubImage2D = (PsmGlCopyTexSubImage2D)GlProc("glCopyTexSubImage2D");
#else
    psm_glActiveTexture = (PsmGlActiveTexture)dlsym(RTLD_DEFAULT, "glActiveTexture");
    psm_glBindTexture = (PsmGlBindTexture)dlsym(RTLD_DEFAULT, "glBindTexture");
    psm_glCopyTexSubImage2D = (PsmGlCopyTexSubImage2D)dlsym(RTLD_DEFAULT, "glCopyTexSubImage2D");
#endif
}

static void CompositeBackdrop(Renderer *r, const View *view, unsigned destFbo, const int bbox[4], int colorMode,
                              int alphaMode, const float base[4], const float mulv[4], const float scrv[4], int mg,
                              float maskInvert, unsigned sourceTex)
{
    int x = bbox[0], y = bbox[1], w = bbox[2] - bbox[0], h = bbox[3] - bbox[1];
    if (w <= 0 || h <= 0) return;
    int gy = view->sh - (y + h); /* GL bottom-left origin */

    BindTarget(destFbo, view->sw, view->sh); /* flush + bind dest */
    rlDisableBackfaceCulling();
    rlEnableScissorTest();
    rlScissor(x, gy, w, h);

    /* Copy the dest's bbox into the backdrop texture (same pixel coords). Done on
     * texture unit 0 while no batch is pending; raylib re-binds its own textures
     * at draw time, so this transient binding does not disturb it. */
    LoadGLProcs();
    psm_glActiveTexture(PSM_GL_TEXTURE0);
    psm_glBindTexture(PSM_GL_TEXTURE_2D, r->backdrop.id);
    psm_glCopyTexSubImage2D(PSM_GL_TEXTURE_2D, 0, x, gy, x, gy, w, h);
    psm_glBindTexture(PSM_GL_TEXTURE_2D, 0);

    float resv[2] = { (float)view->sw, (float)view->sh };
    Shader sh = r->sblend;
    BeginShaderMode(sh);
    rlSetBlendFactorsSeparate(RL_ONE, RL_ZERO, RL_ONE, RL_ZERO, RL_FUNC_ADD,
                              RL_FUNC_ADD); /* REPLACE: shader is the result */
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
    SetShaderValue(sh, r->sbRes, resv, SHADER_UNIFORM_VEC2);
    SetShaderValue(sh, r->sbCmode, &colorMode, SHADER_UNIFORM_INT);
    SetShaderValue(sh, r->sbAmode, &alphaMode, SHADER_UNIFORM_INT);
    SetShaderValue(sh, r->sbBase, base, SHADER_UNIFORM_VEC4);
    SetShaderValue(sh, r->sbMul, mulv, SHADER_UNIFORM_VEC4);
    SetShaderValue(sh, r->sbScr, scrv, SHADER_UNIFORM_VEC4);
    float usemask = (mg >= 0) ? 1.0f : 0.0f;
    SetShaderValue(sh, r->sbUsemask, &usemask, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, r->sbInv, &maskInvert, SHADER_UNIFORM_FLOAT);
    if (mg >= 0)
    {
        float csel[4];
        ChannelVec(mg % GROUPS_PER_BUF, csel);
        SetShaderValue(sh, r->sbSel, csel, SHADER_UNIFORM_VEC4);
        SetShaderValueTexture(sh, r->sbMask, r->os.mcg.buf[mg / GROUPS_PER_BUF].texture);
    }
    SetShaderValueTexture(sh, r->sbBlend, r->backdrop);

    float W = (float)view->sw, H = (float)view->sh;
    rlCheckRenderBatchLimit(6);
    rlSetTexture(sourceTex);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(255, 255, 255, 255);
    rlTexCoord2f(0, 1);
    rlVertex2f(0, 0);
    rlTexCoord2f(0, 0);
    rlVertex2f(0, H);
    rlTexCoord2f(1, 0);
    rlVertex2f(W, H);
    rlTexCoord2f(0, 1);
    rlVertex2f(0, 0);
    rlTexCoord2f(1, 0);
    rlVertex2f(W, H);
    rlTexCoord2f(1, 1);
    rlVertex2f(W, 0);
    rlEnd();
    EndShaderMode();
}

/* Composite the offscreen `o` (already fully rendered) onto `destFbo`,
 * applying group opacity / blend / multiply+screen color / clipping mask. */
static void CompositeOffscreen(csmModel *model, Renderer *r, const View *view, bool maskingOn, int o, unsigned destFbo)
{
    const float *opa = csmGetOffscreenOpacities(model);
    const int *bm = csmGetOffscreenBlendModes(model);
    const csmVector4 *mul = csmGetOffscreenMultiplyColors(model);
    const csmVector4 *scr = csmGetOffscreenScreenColors(model);
    const csmFlags *cf = csmGetOffscreenConstantFlags(model);
    Offscreens *os = &r->os;

    int mg = maskingOn ? os->mgroup[o] : -1;
    csmVector4 mc = mul[o], sc = scr[o];
    float base[4] = { 1, 1, 1, opa[o] };
    float mulv[4] = { mc.X, mc.Y, mc.Z, 1 };
    float scrv[4] = { sc.X, sc.Y, sc.Z, 0 };
    float resv[2] = { (float)view->sw, (float)view->sh };

    /* Exotic color/alpha: route through the exact backdrop-sampling composite. */
    if (BlendIsExotic(bm[o]))
    {
        float invf = (cf[o] & csmIsInvertedMask) ? 1.0f : 0.0f;
        CompositeBackdrop(r, view, destFbo, os->bbox[o], bm[o] & 0xFF, (bm[o] >> 8) & 0xFF, base, mulv, scrv, mg, invf,
                          os->rt[o].texture.id);
        return;
    }

    /* Switch back to the destination target. */
    BindTarget(destFbo, view->sw, view->sh);
    rlDisableBackfaceCulling();
    /* The composite quad spans the screen but the group's content lives only in
     * its box, so clip the fill to it (BindTarget just flushed). */
    ScissorToOffscreen(os, o, view->sh);

    Shader sh = (mg >= 0) ? r->soffm : r->soff;
    BeginShaderMode(sh);
    ApplyExtendedBlend(bm[o]);
    if (mg >= 0)
    {
        SetShaderValue(sh, r->omBase, base, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->omMul, mulv, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->omScr, scrv, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->omRes, resv, SHADER_UNIFORM_VEC2);
        float invf = (cf[o] & csmIsInvertedMask) ? 1.0f : 0.0f;
        SetShaderValue(sh, r->omInv, &invf, SHADER_UNIFORM_FLOAT);
        float csel[4];
        ChannelVec(mg % GROUPS_PER_BUF, csel);
        SetShaderValue(sh, r->omSel, csel, SHADER_UNIFORM_VEC4);
        SetShaderValueTexture(sh, r->omMask, os->mcg.buf[mg / GROUPS_PER_BUF].texture);
    }
    else
    {
        SetShaderValue(sh, r->obBase, base, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->obMul, mulv, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, r->obScr, scrv, SHADER_UNIFORM_VEC4);
    }

    /* Full-screen quad sampling the offscreen RT. The RT is stored bottom-up,
     * so flip V. The destination target's own projection handles orientation. */
    Texture2D rt = os->rt[o].texture;
    float W = (float)view->sw, H = (float)view->sh;
    rlCheckRenderBatchLimit(6);
    rlSetTexture(rt.id);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(255, 255, 255, 255);
    /* two triangles covering [0,W]x[0,H], V flipped */
    rlTexCoord2f(0, 1);
    rlVertex2f(0, 0);
    rlTexCoord2f(0, 0);
    rlVertex2f(0, H);
    rlTexCoord2f(1, 0);
    rlVertex2f(W, H);
    rlTexCoord2f(0, 1);
    rlVertex2f(0, 0);
    rlTexCoord2f(1, 0);
    rlVertex2f(W, H);
    rlTexCoord2f(1, 1);
    rlVertex2f(W, 0);
    rlEnd();
    EndShaderMode();
}

static void DrawDrawableExotic(csmModel *model, Renderer *r, const View *view, bool maskingOn, int d, unsigned destFbo)
{
    int bb[4];
    DrawableScreenBbox(view, model, d, bb);
    if (bb[2] <= bb[0] || bb[3] <= bb[1]) return;

    /* Lay the drawable down in isolation (normal+over) into the scratch RT. */
    BindTarget(r->scratch.id, view->sw, view->sh);
    int x = bb[0], y = bb[1], w = bb[2] - bb[0], h = bb[3] - bb[1];
    rlEnableScissorTest();
    rlScissor(x, view->sh - (y + h), w, h);
    rlClearColor(0, 0, 0, 0);
    rlClearScreenBuffers();
    rlDisableBackfaceCulling();
    float resv[2] = { (float)view->sw, (float)view->sh };
    DrawOneDrawable(model, r, view, maskingOn, d, resv, 0 /* normal */);

    /* Composite the isolated drawable with its real blend (colors/opacity/mask
     * already baked into the scratch RT, so use identity here). */
    int ext = csmGetDrawableBlendModes(model)[d];
    float base[4] = { 1, 1, 1, 1 }, mul[4] = { 1, 1, 1, 1 }, scr[4] = { 0, 0, 0, 0 };
    CompositeBackdrop(r, view, destFbo, bb, ext & 0xFF, (ext >> 8) & 0xFF, base, mul, scr, -1, 0.0f,
                      r->scratch.texture.id);
}

static void RenderOffscreenMasks(csmModel *model, Renderer *r, const View *view)
{
    Offscreens *os = &r->os;
    if (os->mcg.count == 0) return;
    int dc = csmGetDrawableCount(model);
    const csmVector2 **pos = csmGetDrawableVertexPositions(model);
    const csmVector2 **uv = csmGetDrawableVertexUvs(model);
    const int *icount = csmGetDrawableIndexCounts(model);
    const unsigned short **idx = csmGetDrawableIndices(model);
    const int *texidx = csmGetDrawableTextureIndices(model);

    float s = DEFAULT_MASK_SCALE;
    View mv = *view;
    mv.zoom *= s;
    mv.panx *= s;
    mv.pany *= s;
    mv.sw = (int)(view->sw * s);
    mv.sh = (int)(view->sh * s);

    BeginShaderMode(r->smaskw);
    for (int b = 0; b < os->mcg.bufCount; b++)
    {
        BeginTextureMode(os->mcg.buf[b]);
        ClearBackground(BLANK);
        rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ONE, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
        rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
        int g0 = b * GROUPS_PER_BUF, g1 = g0 + GROUPS_PER_BUF;
        if (g1 > os->mcg.count) g1 = os->mcg.count;
        for (int g = g0; g < g1; g++)
        {
            float cmask[4];
            ChannelVec(g % GROUPS_PER_BUF, cmask);
            SetShaderValue(r->smaskw, r->wmask, cmask, SHADER_UNIFORM_VEC4);
            for (int mi = 0; mi < os->mcg.maskCount[g]; mi++)
            {
                int md = os->mcg.masks[g][mi];
                if (md < 0 || md >= dc) continue;
                int t = texidx[md] >= 0 ? r->textures[texidx[md]].id : 0;
                EmitDrawable(&mv, t, pos[md], uv[md], idx[md], icount[md]);
            }
            rlDrawRenderBatchActive();
        }
        EndTextureMode();
    }
    EndShaderMode();
    rlSetBlendMode(RL_BLEND_ALPHA);
}

static void RenderModelOffscreen(csmModel *model, Renderer *r, const View *view, bool maskingOn, const Options *opt,
                                 int *sorted)
{
    Offscreens *os = &r->os;
    int dc = csmGetDrawableCount(model);
    int oc = os->count;
    int total = dc + oc;
    const int *order = VIEWER_GET_RENDER_ORDERS(model);
    const float *opacity = csmGetDrawableOpacities(model);
    const csmFlags *dflags = csmGetDrawableDynamicFlags(model);
    const int *partParent = csmGetPartParentPartIndices(model);
    const int *drawPart = csmGetDrawableParentPartIndices(model);

    for (int i = 0; i < total; i++)
    {
        int ord = order[i];
        if (ord < 0 || ord >= total) continue;
        sorted[ord] = (i < dc) ? i : (dc + (i - dc)); /* same encoding below */
    }

    /* Mask pre-passes (drawable + offscreen) up front, before any RT is bound
     * as a sampler. */
    if (maskingOn)
    {
        RenderMasks(model, r, view);
        RenderOffscreenMasks(model, r, view);
    }

    /* Per-group: screen-space content box (for scissoring the clear/composite)
     * and whether the group is effectful (needs its own RT) or a pass-through
     * (draws straight into the parent target). */
    OffscreensComputeBboxes(os, model, view, maskingOn);

    unsigned rootFbo = (unsigned)rlGetActiveFramebuffer();
    int current = -1;             /* top of group stack (scope), or -1 */
    int activeScissor = -1;       /* group whose box the scissor matches */
    unsigned curTarget = rootFbo; /* FBO drawables currently render into */
    unsigned destFbo[MAX_OFFSCREENS];
    float resv[2] = { (float)view->sw, (float)view->sh };

    BindTarget(rootFbo, view->sw, view->sh);
    rlDisableBackfaceCulling();
    ScissorToOffscreen(os, -1, view->sh); /* full screen */

    /* Pop the current group: composite it (if effectful) onto its destination,
     * leaving curTarget = that destination. */
#define POP_CURRENT()                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (os->effectful[current])                                                                                    \
        {                                                                                                              \
            CompositeOffscreen(model, r, view, maskingOn, current, destFbo[current]);                                  \
            activeScissor = current; /* composite left the scissor at this box */                                      \
        }                                                                                                              \
        curTarget = destFbo[current];                                                                                  \
        current = os->parent[current];                                                                                 \
    } while (0)

    for (int p = 0; p < total; p++)
    {
        int obj = sorted[p];
        bool isOffscreen = obj >= dc;
        int oi = isOffscreen ? obj - dc : -1;
        int di = isOffscreen ? -1 : obj;

        if (!isOffscreen)
        {
            /* Drawable. Skip invisibles first (matches reference DrawDrawable). */
            if (!(dflags[di] & csmIsVisible) || opacity[di] <= 0.0f) continue;
            /* Submit-to-parent: pop groups that no longer contain this drawable. */
            while (current >= 0 && !DrawableInOffscreen(os, partParent, drawPart, di, current))
                POP_CURRENT();
            if (!DrawablePassesFilters(di, order[di], opt)) continue;
            /* Exotic-blend drawables (e.g. the HardLight hologram) need the exact
             * backdrop composite: render in isolation, then blend onto curTarget. */
            if (BlendIsExotic(csmGetDrawableBlendModes(model)[di]))
            {
                DrawDrawableExotic(model, r, view, maskingOn, di, curTarget);
                activeScissor = -2; /* composite left the scissor at the bbox */
                continue;
            }
            /* Match the scissor to the active group's box before drawing. */
            if (activeScissor != current)
            {
                rlDrawRenderBatchActive();
                ScissorToOffscreen(os, current, view->sh);
                activeScissor = current;
            }
            /* Draw into the active target (an effectful ancestor's RT, or screen). */
            DrawOneDrawable(model, r, view, maskingOn, di, resv, -1);
        }
        else
        {
            /* Group begin. Pop current groups until this one nests in it. */
            while (current >= 0 && os->parent[oi] != current && current != oi)
            {
                bool nested = false;
                int a = os->parent[oi];
                while (a >= 0)
                {
                    if (a == current)
                    {
                        nested = true;
                        break;
                    }
                    a = os->parent[a];
                }
                if (nested) break;
                POP_CURRENT();
            }
            destFbo[oi] = curTarget; /* where this group composites onto */
            if (os->effectful[oi])
            {
                /* Bind a fresh RT and clear it (scissored to the group box; the rest
                 * of the RT is never written or sampled). */
                BindTarget(os->rt[oi].id, view->sw, view->sh);
                ScissorToOffscreen(os, oi, view->sh);
                activeScissor = oi;
                rlClearColor(0, 0, 0, 0);
                rlClearScreenBuffers();
                rlDisableBackfaceCulling();
                curTarget = os->rt[oi].id;
            }
            /* Pass-through groups keep curTarget: their drawables draw straight
             * into the parent, with no RT, clear or composite. */
            current = oi;
        }
    }

    /* Composite any groups left on the stack up to the root. */
    while (current >= 0)
        POP_CURRENT();
#undef POP_CURRENT

    /* End on the root framebuffer with the screen projection so the UI / overlay
     * draw correctly (and lift the scissor). */
    BindTarget(rootFbo, view->sw, view->sh);
    rlDisableScissorTest();
    rlSetBlendMode(RL_BLEND_ALPHA);
}
#endif /* offscreens (v6) */

/* Draw the model in render order. `sorted` is scratch of length >= dc+oc. */
void RenderModel(csmModel *model, Renderer *r, const View *view, bool maskingOn, const Options *opt, int *sorted)
{
#if PSM_COMPAT_VERSION >= 0x06000000L
    if (r->os.count > 0 && !opt->forceFlat)
    {
        RenderModelOffscreen(model, r, view, maskingOn, opt, sorted);
        return;
    }
#endif
    RenderModelFlat(model, r, view, maskingOn, opt, sorted);
}
