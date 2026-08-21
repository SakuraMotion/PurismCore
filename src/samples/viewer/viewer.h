/*
 * Purism Core: sample model viewer declarations
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PURISM_VIEWER_H
#define PURISM_VIEWER_H

#include "raylib.h"
#include "rlgl.h"

#include "PurismCore.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* General limits. */
#define MAX_TEXTURES 64
#define MAX_CLIP_GROUPS 64
#define EMIT_CHUNK 6000
#define PANEL_W 340

#define PSM_GL_TEXTURE_2D 0x0DE1
#define PSM_GL_TEXTURE0 0x84C0

/* Subsampling for clipping masks. */
#define DEFAULT_MASK_SCALE 0.5f

/* Clip groups are channel packed. */
#define GROUPS_PER_BUF 4
#define MAX_MASK_BUFS ((MAX_CLIP_GROUPS + GROUPS_PER_BUF - 1) / GROUPS_PER_BUF)

#if PSM_COMPAT_VERSION >= 0x06000000L
#define MAX_OFFSCREENS 256
#endif

#if PSM_COMPAT_VERSION >= 0x06000000L
#define VIEWER_GET_RENDER_ORDERS csmGetRenderOrders
#else
#define VIEWER_GET_RENDER_ORDERS csmGetDrawableRenderOrders
#endif

/* Blend selector independent of the drawable constant-flag bits, so the same
 * GL factor setup can be shared by drawables (flags) and offscreens (which
 * carry an extended blend-mode integer instead). */
enum
{
    PSM_BLEND_NORMAL = 0,
    PSM_BLEND_ADDITIVE = 1,
    PSM_BLEND_MULTIPLICATIVE = 2
};

typedef struct
{
    char moc[512];
    char tex[MAX_TEXTURES][512];
    int texCount;
} Model3;

typedef struct
{
    void *mocBase, *modelBase;
    csmMoc *moc;
    csmModel *model;
} Core;

typedef struct
{
    float cx, cy; /* world-space center (y already flipped to screen-down) */
    float zoom;   /* pixels per world unit */
    float panx, pany;
    int sw, sh; /* screen size */
} View;

typedef struct
{
    int count;                         /* number of unique groups */
    const int *masks[MAX_CLIP_GROUPS]; /* mask indices (borrowed) */
    int maskCount[MAX_CLIP_GROUPS];
    int bufCount; /* ceil(count / 4) */
    RenderTexture2D buf[MAX_MASK_BUFS];
    int *of; /* [dc] group index, or -1 */
} ClipGroups;

#if PSM_COMPAT_VERSION >= 0x06000000L
typedef struct
{
    int count;                          /* offscreen count for this model */
    const int *owner;                   /* [count] owner PART index per offscreen */
    int parent[MAX_OFFSCREENS];         /* parent offscreen index, or -1 */
    RenderTexture2D rt[MAX_OFFSCREENS]; /* screen-sized target per offscreen */
    ClipGroups mcg;                     /* mask groups built from offscreen masks */
    int mgroup[MAX_OFFSCREENS];         /* mask group index per offscreen, or -1 */
    int bbox[MAX_OFFSCREENS][4];
    bool effectful[MAX_OFFSCREENS];
} Offscreens;
#endif /* offscreens (v6) */

typedef struct
{
    Texture2D textures[MAX_TEXTURES];
    int texCount;
    Shader sdraw, smask, smaskw;
    int locBase, locMul, locScr;                    /* sdraw */
    int mbase, mmul, mscr, mmask, mres, minv, msel; /* smask */
    int wmask;                                      /* smaskw */
    float maskScale;
    ClipGroups cg;
#if PSM_COMPAT_VERSION >= 0x06000000L
    Offscreens os;
    Shader soff, soffm;                                    /* offscreen composite shaders */
    int obBase, obMul, obScr;                              /* soff */
    int omBase, omMul, omScr, omMask, omRes, omInv, omSel; /* soffm */
    /* Exact backdrop-sampling blend (exotic color/alpha): shader + scratch RTs. */
    Shader sblend;
    int sbBlend, sbMask, sbRes, sbCmode, sbAmode, sbUsemask, sbInv, sbSel, sbBase, sbMul, sbScr;
    Texture2D backdrop;      /* copy of the destination for sampling */
    RenderTexture2D scratch; /* an exotic drawable rendered in isolation */
#endif
} Renderer;

typedef struct
{
    const char *path;
    const char *shotPath; /* render a few frames, save, exit */
    float shotZoom;       /* zoom multiplier, recenter on face */
    bool startNomask;
    bool forceFlat;  /* debug: bypass offscreen compositing */
    bool noVsync;    /* uncap frame rate (for measuring) */
    int bench;       /* render N frames, print avg ms, exit */
    float maskScale; /* mask buffer resolution fraction */
    /* debug filters */
    int maxOrder;     /* draw only renderOrder <= this */
    int onlyDrawable; /* draw only this drawable, or -1 */
    int hideIdx[64];  /* suppress these drawables */
    int hideCount;
    const char *setParamId[32]; /* force these parameters each frame */
    float setParamVal[32];
    int setParamCount;
} Options;

typedef struct
{
    int tab; /* 0 = Parameters, 1 = Parts */
    float scrollParam;
    float scrollPart;
    char filter[48]; /* search filter (substring, case-insensitive) */
    bool filterEdit;
    int editId; /* row whose value box is being typed in, or -1 */
    char editBuf[32];
    float uiScale; /* UI zoom */
} PanelState;

/* io.c */
bool ParseModel3(const char *path, Model3 *m3);
bool LoadCore(const char *mocPath, Core *c);
void FreeCore(Core *c);

/* blend.c */
void ApplyBlend(csmFlags flags);
#if PSM_COMPAT_VERSION >= 0x06000000L
bool BlendIsExotic(int extended);
bool BlendAlphaNeedsCoverage(int extended);
void ApplyExtendedBlend(int extended);
#endif

/* graphics.c */
void WorldToScreen(const View *v, float mx, float my, float *sx, float *sy);
void FitView(View *v, csmModel *model);
void EmitDrawable(const View *v, int texId, const csmVector2 *pos, const csmVector2 *uv, const unsigned short *idx,
                  int idxCount);
void ChannelVec(int c, float v[4]);
#if PSM_COMPAT_VERSION >= 0x06000000L
bool DrawableInOffscreen(const Offscreens *os, const int *partParent, const int *drawPart, int d, int o);
#endif
bool RendererInit(Renderer *r, const Model3 *m3, const char *dir, csmModel *model, int w, int h, float maskScale);
void RendererResize(Renderer *r, int w, int h);
void RendererFree(Renderer *r);

/* render.c */
void RenderModel(csmModel *model, Renderer *r, const View *view, bool maskingOn, const Options *opt, int *sorted);

/* panel.c */
Font LoadUiFont(void);
void DrawPanel(csmModel *m, Rectangle area, PanelState *ps);

#endif /* PURISM_VIEWER_H */
