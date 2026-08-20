/*
 * Purism Core: sample model viewer entrypoint
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "viewer.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

static void usage(void)
{
    fprintf(stderr, "usage: viewer [options] <model.model3.json | model.moc3>\n"
                    "  --shot <file>          render a few frames, save PNG, exit\n"
                    "  --zoom <f>             (with --shot) zoom multiplier, recenter on face\n"
                    "  --nomask               start with masking disabled\n"
                    "  --flat                 debug: bypass offscreen compositing (flat path)\n"
                    "  --novsync              uncap frame rate (measure true ceiling)\n"
                    "  --maskscale <f>        mask buffer resolution fraction (default 0.5)\n"
                    "  --setparam NAME VAL    force a parameter each frame (repeatable)\n"
                    "  --hide N               suppress drawable N (repeatable)\n"
                    "  --only N               draw only drawable N\n"
                    "  --maxorder N           draw only drawables with renderOrder <= N\n"
                    "interactive: Tab panel  M masking  R reset view  wheel zoom  LMB pan\n"
                    "             drag a .model3.json or .moc3 onto the window to load it\n");
}

static void ResetOptions(Options *o)
{
    memset(o, 0, sizeof(*o));
    o->shotZoom = 1.0f;
    o->maskScale = DEFAULT_MASK_SCALE;
    o->maxOrder = 1 << 30;
    o->onlyDrawable = -1;
}

static bool ParseArgs(int argc, char **argv, Options *o)
{
    ResetOptions(o);
    for (int i = 1; i < argc; i++)
    {
        const char *a = argv[i];
        if (!strcmp(a, "--shot") && i + 1 < argc)
        {
            o->shotPath = argv[++i];
        }
        else if (!strcmp(a, "--zoom") && i + 1 < argc)
        {
            o->shotZoom = (float)atof(argv[++i]);
        }
        else if (!strcmp(a, "--nomask"))
        {
            o->startNomask = true;
        }
        else if (!strcmp(a, "--flat"))
        {
            o->forceFlat = true;
        }
        else if (!strcmp(a, "--bench") && i + 1 < argc)
        {
            o->bench = atoi(argv[++i]);
            o->noVsync = true;
        }
        else if (!strcmp(a, "--novsync"))
        {
            o->noVsync = true;
        }
        else if (!strcmp(a, "--maskscale") && i + 1 < argc)
        {
            o->maskScale = (float)atof(argv[++i]);
        }
        else if (!strcmp(a, "--maxorder") && i + 1 < argc)
        {
            o->maxOrder = atoi(argv[++i]);
        }
        else if (!strcmp(a, "--only") && i + 1 < argc)
        {
            o->onlyDrawable = atoi(argv[++i]);
        }
        else if (!strcmp(a, "--setparam") && i + 2 < argc && o->setParamCount < 32)
        {
            o->setParamId[o->setParamCount] = argv[++i];
            o->setParamVal[o->setParamCount] = (float)atof(argv[++i]);
            o->setParamCount++;
        }
        else if (!strcmp(a, "--hide") && i + 1 < argc && o->hideCount < 64)
        {
            o->hideIdx[o->hideCount++] = atoi(argv[++i]);
        }
        else if (a[0] == '-')
        {
            fprintf(stderr, "unknown option: %s\n", a);
            return false;
        }
        else
        {
            o->path = a;
        }
    }
    return o->path != NULL;
}

static int CountMasked(csmModel *m)
{
    int dc = csmGetDrawableCount(m);
    const int *mc = csmGetDrawableMaskCounts(m);
    int n = 0;
    for (int d = 0; d < dc; d++)
        if (mc[d] > 0) n++;
    return n;
}

static bool LoadModel3(const char *path, Model3 *m3, char *dir, size_t dirsz, Core *core)
{
    /* GetDirectoryPath omits the trailing slash for absolute paths but keeps
     * "./" for a bare filename; add a slash only when one isn't already there. */
    const char *gd = GetDirectoryPath(path);
    int gn = TextLength(gd);
    bool hasSep = gn > 0 && (gd[gn - 1] == '/' || gd[gn - 1] == '\\');
    snprintf(dir, dirsz, hasSep ? "%s" : "%s/", gd);

    char mocPath[768 + 512]; /* dir + moc field, both bounded */
    if (IsFileExtension(path, ".json"))
    {
        if (!ParseModel3(path, m3))
        {
            fprintf(stderr, "could not parse %s\n", path);
            return false;
        }
        snprintf(mocPath, sizeof(mocPath), "%s%s", dir, m3->moc);
    }
    else
    {
        memset(m3, 0, sizeof(*m3));
        snprintf(mocPath, sizeof(mocPath), "%s", path);
    }
    return LoadCore(mocPath, core);
}

typedef struct
{
    Options opt;
    Core core;
    Model3 m3;
    char dir[768];
    csmModel *model; /* == core.model, cached for the loop */
    Renderer rend;
    int *sorted; /* render-order scratch */
    View view;
    Font uiFont;
    PanelState ps;
    bool showPanel;
    bool maskingOn;
    int frame;
    double benchT0;
    bool quit; /* frame() sets this to stop the loop */
    const char *coreVerStr;
} App;

static bool ViewerLoad(App *a, const char *path)
{
    Model3 nm3;
    Core ncore;
    char ndir[sizeof(a->dir)];
    if (!LoadModel3(path, &nm3, ndir, sizeof(ndir), &ncore)) return false;

    Renderer nrend;
    if (!RendererInit(&nrend, &nm3, ndir, ncore.model, a->view.sw, a->view.sh, a->opt.maskScale))
    {
        fprintf(stderr, "out of memory\n");
        FreeCore(&ncore);
        return false;
    }
    int n = csmGetDrawableCount(ncore.model);
#if PSM_COMPAT_VERSION >= 0x06000000L
    n += csmGetOffscreenCount(ncore.model);
#endif
    int *nsorted = (int *)malloc(sizeof(int) * (n > 0 ? n : 1));
    if (!nsorted)
    {
        fprintf(stderr, "out of memory\n");
        RendererFree(&nrend);
        FreeCore(&ncore);
        return false;
    }

    /* commit: the new model is ready, so release the previous one (if any). */
    if (a->model)
    {
        RendererFree(&a->rend);
        FreeCore(&a->core);
        free(a->sorted);
    }
    a->m3 = nm3;
    a->core = ncore;
    a->model = ncore.model;
    memcpy(a->dir, ndir, sizeof(a->dir));
    a->rend = nrend;
    a->sorted = nsorted;
    FitView(&a->view, a->model);
    /* reset the model-specific panel state (param/part lists changed) */
    a->ps.editId = -1;
    a->ps.filterEdit = false;
    a->ps.filter[0] = '\0';
    a->ps.scrollParam = a->ps.scrollPart = 0.0f;
    SetWindowTitle(TextFormat("Purism Core viewer - %s", GetFileName(path)));
    return true;
}

#if defined(__EMSCRIPTEN__)
static char gPendingLoad[1024];

EMSCRIPTEN_KEEPALIVE void ViewerRequestLoad(const char *path)
{
    snprintf(gPendingLoad, sizeof gPendingLoad, "%s", path ? path : "");
}
#endif

/* One frame: resize, drag-and-drop, input, parameter push, render, overlay.
 * Self-contained (state via `a`) so it can be a browser main-loop callback. */
static void frame(App *a)
{
    a->frame++;

    /* resize: keep view + clip-group buffers in sync with the window */
    int nsw = GetScreenWidth(), nsh = GetScreenHeight();
    if (nsw != a->view.sw || nsh != a->view.sh)
    {
        a->view.sw = nsw;
        a->view.sh = nsh;
        if (a->model) RendererResize(&a->rend, nsw, nsh);
    }

#if defined(__EMSCRIPTEN__)
    /* file-picker / zip bridge (shell.html): a model was written to MEMFS and its
     * path handed to ViewerRequestLoad; load it here, on the loop thread. */
    if (gPendingLoad[0])
    {
        char p[sizeof gPendingLoad];
        memcpy(p, gPendingLoad, sizeof p);
        gPendingLoad[0] = '\0';
        if (!ViewerLoad(a, p)) TraceLog(LOG_WARNING, "load: could not load %s", p);
    }
#endif

    /* drag-and-drop: load a dropped .model3.json / .moc3. The current model is
     * kept if the dropped file fails to load (e.g. a stray image). */
    if (IsFileDropped())
    {
        FilePathList drop = LoadDroppedFiles();
        if (drop.count > 0 && !ViewerLoad(a, drop.paths[0]))
            TraceLog(LOG_WARNING, "drop: could not load %s", drop.paths[0]);
        UnloadDroppedFiles(drop);
    }

    /* Nothing loaded yet (the web build starts empty): prompt and wait. */
    if (!a->model)
    {
        BeginDrawing();
        ClearBackground((Color){ 30, 30, 36, 255 });
        DrawTextEx(a->uiFont, "Purism Core viewer", (Vector2){ 40, a->view.sh / 2.0f - 40 }, 30, 1, RAYWHITE);
        DrawTextEx(a->uiFont,
                   "Load a model with the button above, or drop a "
                   ".moc3 / .model3.json / .zip here.",
                   (Vector2){ 40, a->view.sh / 2.0f + 4 }, 18, 1, (Color){ 180, 180, 190, 255 });
        EndDrawing();
        return;
    }

    /* input */
    float pw = PANEL_W * a->ps.uiScale;
    if (pw > a->view.sw * 0.5f) pw = a->view.sw * 0.5f;
    Rectangle panel = { a->view.sw - pw, 0, pw, (float)a->view.sh };
    bool overPanel = a->showPanel && CheckCollisionPointRec(GetMousePosition(), panel);
    /* don't let model hotkeys fire while typing in a panel text/value box */
    bool typing = a->showPanel && (a->ps.filterEdit || a->ps.editId >= 0);
    if (!typing)
    {
        if (IsKeyPressed(KEY_TAB)) a->showPanel = !a->showPanel;
        if (IsKeyPressed(KEY_M)) a->maskingOn = !a->maskingOn;
        if (IsKeyPressed(KEY_R)) FitView(&a->view, a->model);
    }
    if (!overPanel && !typing)
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) a->view.zoom *= (wheel > 0 ? 1.1f : 1.0f / 1.1f);
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            Vector2 md = GetMouseDelta();
            a->view.panx += md.x;
            a->view.pany += md.y;
        }
    }

    /* forced parameters (--setparam) */
    if (a->opt.setParamCount > 0)
    {
        int ppc = csmGetParameterCount(a->model);
        const char **pids = csmGetParameterIds(a->model);
        float *pvals = csmGetParameterValues(a->model);
        for (int s = 0; s < a->opt.setParamCount; s++)
            for (int p = 0; p < ppc; p++)
                if (strcmp(pids[p], a->opt.setParamId[s]) == 0)
                {
                    pvals[p] = a->opt.setParamVal[s];
                    break;
                }
    }

    csmUpdateModel(a->model);

    BeginDrawing();
    ClearBackground((Color){ 30, 30, 36, 255 });
    RenderModel(a->model, &a->rend, &a->view, a->maskingOn, &a->opt, a->sorted);

    if (a->showPanel) DrawPanel(a->model, panel, &a->ps);
    DrawTextEx(a->uiFont,
               "Tab: panel   M: masking   R: reset view   "
               "wheel: zoom   LMB drag: pan   drop a model to load it",
               (Vector2){ 10, a->view.sh - 30 }, 18, 1, RAYWHITE);
    DrawTextEx(a->uiFont, a->coreVerStr, (Vector2){ 10, a->view.sh - 56 }, 18, 1, RAYWHITE);
    char fps[32];
    snprintf(fps, sizeof(fps), "%d FPS", GetFPS());
    DrawTextEx(a->uiFont, fps, (Vector2){ 10, 8 }, 20, 1, (Color){ 0, 228, 48, 255 });
    EndDrawing();

    /* --bench: warm up a few frames, then time a fixed count and exit. */
    if (a->opt.bench > 0)
    {
        if (a->frame == 4) a->benchT0 = GetTime();
        if (a->frame >= 4 + a->opt.bench)
        {
            double ms = (GetTime() - a->benchT0) * 1000.0 / a->opt.bench;
            printf("bench: %.3f ms/frame  (%.1f fps)  over %d frames\n", ms, 1000.0 / ms, a->opt.bench);
            a->quit = true;
            return;
        }
    }

    /* --shot: let a couple of frames settle, capture, then exit. */
    if (a->opt.shotPath && a->frame >= 3)
    {
        TakeScreenshot(a->opt.shotPath);
        a->quit = true;
        return;
    }
}

int main(int argc, char **argv)
{
    static App app;

#if defined(__EMSCRIPTEN__)
    (void)argc;
    (void)argv;
    ResetOptions(&app.opt);
#else
    if (!ParseArgs(argc, argv, &app.opt))
    {
        usage();
        return 2;
    }
#endif

    csmSetLogFunction(NULL);
    SetTraceLogLevel(LOG_WARNING); /* quiet raylib's per-file INFO chatter */

    app.coreVerStr = csmGetExtendedVersionString();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | (app.opt.noVsync ? 0 : FLAG_VSYNC_HINT));
    InitWindow(1280, 720, "Purism Core viewer");
    if (!app.opt.noVsync) SetTargetFPS(60);

    app.uiFont = LoadUiFont();
    bool uiFontCustom = (app.uiFont.texture.id != GetFontDefault().texture.id);

    app.view.sw = GetScreenWidth();
    app.view.sh = GetScreenHeight();
    app.showPanel = true;
    app.maskingOn = !app.opt.startNomask;
    app.ps.editId = -1;
    app.ps.uiScale = 1.0f;

#if !defined(__EMSCRIPTEN__)
    if (!ViewerLoad(&app, app.opt.path))
    {
        if (uiFontCustom) UnloadFont(app.uiFont);
        CloseWindow();
        return 1;
    }
    if (app.opt.shotZoom != 1.0f)
    {
        /* Zoom toward the face: lift the center to the upper part of the model. */
        app.view.zoom *= app.opt.shotZoom;
        app.view.pany = app.view.sh * 0.33f * app.opt.shotZoom;
    }
#endif

#if defined(__EMSCRIPTEN__)
    (void)uiFontCustom;
    emscripten_set_main_loop_arg((em_arg_callback_func)frame, &app, 0, 1);
    return 0;
#else
    while (!WindowShouldClose() && !app.quit)
        frame(&app);

    free(app.sorted);
    RendererFree(&app.rend);
    if (uiFontCustom) UnloadFont(app.uiFont);
    CloseWindow();
    FreeCore(&app.core);
    return 0;
#endif
}
