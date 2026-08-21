/*
 * Purism Core: sample model viewer control panel
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "viewer.h"
/* UI font baked in by bin2h (see the Makefile): cascadia_fnt (the .fnt text) +
 * cascadia_0_png (the grayscale atlas). */
#include "embed/cascadia.fnt.h"
#include "embed/cascadia_0.png.h"

#define RAYGUI_IMPLEMENTATION
#include "../vendor/raygui.h"

/* Copy the next '\n'-terminated line of *pp into buf (bounded), advancing *pp
 * past the newline. Returns 0 at end of text. */
static int FontNextLine(const char **pp, char *buf, int bufsz)
{
    const char *p = *pp;
    if (!*p) return 0;
    int n = 0;
    while (*p && *p != '\n')
    {
        if (n < bufsz - 1) buf[n++] = *p;
        p++;
    }
    buf[n] = '\0';
    if (*p == '\n') p++;
    *pp = p;
    return 1;
}

Font LoadUiFont(void)
{
    Font font = { 0 };

    Image img = LoadImageFromMemory(".png", (const unsigned char *)cascadia_0_png, (int)cascadia_0_png_size);
    if (img.data == NULL)
    {
        Font f = GetFontDefault();
        GuiSetFont(f);
        return f;
    }
    /* The atlas is 8-bit grayscale where the gray value is the glyph mask; raylib
     * turns that into GRAY+ALPHA (gray=0xff, alpha=mask) so it composites right. */
    if (img.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
    {
        int count = img.width * img.height;
        unsigned char *src = (unsigned char *)img.data;
        unsigned char *ga = (unsigned char *)RL_MALLOC((size_t)count * 2);
        for (int i = 0; i < count; i++)
        {
            ga[i * 2] = 0xff;
            ga[i * 2 + 1] = src[i];
        }
        UnloadImage(img);
        img.data = ga;
        img.format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
        img.mipmaps = 1;
    }
    font.texture = LoadTextureFromImage(img);
    UnloadImage(img);

    /* Parse the .fnt: baseSize from `common lineHeight`, then the `char` lines.
     * (Single page; mirrors raylib's LoadBMFont field order.) */
    const char *p = (const char *)cascadia_fnt;
    char line[256];
    int fontSize = 0, base = 0, sw = 0, sh = 0, glyphCount = 0;
    FontNextLine(&p, line, sizeof line); /* info ...      (skip) */
    FontNextLine(&p, line, sizeof line); /* common lineHeight=.. */
    const char *s = strstr(line, "lineHeight");
    if (s) sscanf(s, "lineHeight=%i base=%i scaleW=%i scaleH=%i", &fontSize, &base, &sw, &sh);
    FontNextLine(&p, line, sizeof line); /* page file=".." (skip) */
    FontNextLine(&p, line, sizeof line); /* chars count=..      */
    s = strstr(line, "count");
    if (s) sscanf(s, "count=%i", &glyphCount);
    (void)base;
    (void)sw;
    (void)sh;

    font.baseSize = fontSize;
    font.glyphCount = glyphCount;
    font.glyphPadding = 0;
    font.glyphs = (GlyphInfo *)RL_MALLOC((size_t)glyphCount * sizeof(GlyphInfo));
    font.recs = (Rectangle *)RL_MALLOC((size_t)glyphCount * sizeof(Rectangle));

    for (int i = 0; i < glyphCount; i++)
    {
        int id = 0, x = 0, y = 0, w = 0, h = 0, ox = 0, oy = 0, ax = 0, pg = 0;
        FontNextLine(&p, line, sizeof line);
        sscanf(line,
               "char id=%i x=%i y=%i width=%i height=%i xoffset=%i yoffset=%i"
               " xadvance=%i page=%i",
               &id, &x, &y, &w, &h, &ox, &oy, &ax, &pg);
        font.recs[i] = (Rectangle){ (float)x, (float)y, (float)w, (float)h };
        font.glyphs[i].value = id;
        font.glyphs[i].offsetX = ox;
        font.glyphs[i].offsetY = oy;
        font.glyphs[i].advanceX = ax;
        font.glyphs[i].image = (Image){ 0 }; /* unused for drawing; freed safely */
    }

    if (font.texture.id == 0)
    {
        UnloadFont(font);
        Font f = GetFontDefault();
        GuiSetFont(f);
        return f;
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    GuiSetFont(font);
    return font;
}

/* case-insensitive substring test ("" matches everything) */
static bool CiContains(const char *hay, const char *needle)
{
    if (!needle[0]) return true;
    for (const char *h = hay; *h; h++)
    {
        const char *a = h, *b = needle;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
        {
            a++;
            b++;
        }
        if (!*b) return true;
    }
    return false;
}

/* One row: id label, a slider, and a value box you can click and type into.
 * `id` identifies the row for edit-focus tracking (unique within a tab). */
static void ValueRow(Rectangle r, const char *label, float *value, float mn, float mx, int id, PanelState *ps)
{
    float lh = r.height * 0.5f;
    float vbw = lh * 3.2f;
    GuiLabel((Rectangle){ r.x, r.y, r.width, lh }, label);
    GuiSliderBar((Rectangle){ r.x, r.y + lh, r.width - vbw - 4, lh }, NULL, NULL, value, mn, mx);

    Rectangle vb = { r.x + r.width - vbw, r.y + lh, vbw, lh };
    bool edit = (ps->editId == id);
    char tmp[32], *txt;
    if (edit)
    {
        txt = ps->editBuf; /* persistent: holds the in-progress text */
    }
    else
    {
        snprintf(tmp, sizeof(tmp), "%.3f", *value);
        txt = tmp; /* transient: display only */
    }
    if (GuiValueBoxFloat(vb, NULL, txt, value, edit))
    {
        if (edit)
        {
            ps->editId = -1; /* Enter / click-away -> commit */
        }
        else
        {
            ps->editId = id; /* click -> start editing this box */
            snprintf(ps->editBuf, sizeof(ps->editBuf), "%.3f", *value);
        }
    }
}

/* A filtered, scrollable list of value rows. mins/maxs may be NULL (range
 * defaults to [0,1], used for part opacities). */
static void ValueList(Rectangle area, PanelState *ps, float *scroll, int n, const char **ids, float *vals,
                      const float *mins, const float *maxs)
{
    float s = ps->uiScale;
    float rowH = 44 * s;

    int shown = 0;
    for (int i = 0; i < n; i++)
        if (CiContains(ids[i], ps->filter)) shown++;

    Rectangle content = { 0, 0, area.width - 16, shown * rowH + 8 };
    Vector2 sc = { 0, *scroll };
    Rectangle view;
    GuiScrollPanel(area, NULL, content, &sc, &view);
    *scroll = sc.y;

    BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
    float x = area.x + 8, w = area.width - 24;
    float y = area.y + 4 + *scroll;
    for (int i = 0; i < n; i++)
    {
        if (!CiContains(ids[i], ps->filter)) continue;
        /* skip rows scrolled out of view (cheap; keeps 100+ params fast) */
        if (y + rowH >= view.y && y <= view.y + view.height)
        {
            float mn = mins ? mins[i] : 0.0f;
            float mx = maxs ? maxs[i] : 1.0f;
            ValueRow((Rectangle){ x, y, w, rowH - 8 * s }, ids[i], &vals[i], mn, mx, i, ps);
        }
        y += rowH;
    }
    EndScissorMode();
}

void DrawPanel(csmModel *m, Rectangle area, PanelState *ps)
{
    float s = ps->uiScale;
    GuiSetStyle(DEFAULT, TEXT_SIZE, (int)(16 * s));
    GuiPanel(area, NULL);

    float pad = 8 * s, lh = 24 * s, gap = 4 * s, btn = lh;
    float x = area.x + pad, w = area.width - 2 * pad;
    float y = area.y + pad;

    /* search filter + UI-scale buttons */
    if (GuiTextBox((Rectangle){ x, y, w - 2 * btn - 2 * gap, lh }, ps->filter, sizeof(ps->filter), ps->filterEdit))
        ps->filterEdit = !ps->filterEdit;
    if (GuiButton((Rectangle){ x + w - 2 * btn - gap, y, btn, lh }, "-")) ps->uiScale = s > 0.75f ? s - 0.1f : 0.7f;
    if (GuiButton((Rectangle){ x + w - btn, y, btn, lh }, "+")) ps->uiScale = s < 2.15f ? s + 0.1f : 2.2f;
    y += lh + gap;

    /* tab switch (toggle group, not a tab bar) */
    int prev = ps->tab;
    GuiToggleGroup((Rectangle){ x, y, (w - gap) / 2, lh }, "Parameters;Parts", &ps->tab);
    if (ps->tab != prev) ps->editId = -1; /* drop edit focus when switching tabs */
    y += lh + gap;

    /* reset button (context-sensitive) */
    if (GuiButton((Rectangle){ x, y, w, lh }, ps->tab == 0 ? "Reset parameters to defaults" : "Reset part opacities"))
    {
        if (ps->tab == 0)
        {
            int pc = csmGetParameterCount(m);
            const float *def = csmGetParameterDefaultValues(m);
            float *v = csmGetParameterValues(m);
            for (int i = 0; i < pc; i++)
                v[i] = def[i];
        }
        else
        {
            int pc = csmGetPartCount(m);
            float *o = csmGetPartOpacities(m);
            for (int i = 0; i < pc; i++)
                o[i] = 1.0f;
        }
        ps->editId = -1;
    }
    y += lh + gap;

    Rectangle content = { area.x, y, area.width, area.y + area.height - y };
    if (ps->tab == 0)
    {
        ValueList(content, ps, &ps->scrollParam, csmGetParameterCount(m), csmGetParameterIds(m),
                  csmGetParameterValues(m), csmGetParameterMinimumValues(m), csmGetParameterMaximumValues(m));
    }
    else
    {
        ValueList(content, ps, &ps->scrollPart, csmGetPartCount(m), csmGetPartIds(m), csmGetPartOpacities(m), NULL,
                  NULL);
    }
}
