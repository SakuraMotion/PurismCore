/*
 * Purism Core: sample model viewer I/O
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include "viewer.h"

static void *AlignedBlob(size_t align, size_t size, void **outBase)
{
    void *base = malloc(size + align);
    if (!base)
    {
        *outBase = NULL;
        return NULL;
    }
    *outBase = base;
    return (void *)(((size_t)base + align - 1) & ~(align - 1));
}

static bool JsonStringValue(const char *buf, const char *key, char *dst, int dstsz)
{
    int ki = TextFindIndex(buf, TextFormat("\"%s\"", key));
    if (ki < 0) return false;
    const char *p = buf + ki;
    int colon = TextFindIndex(p, ":");
    if (colon < 0) return false;
    p += colon;
    int q1 = TextFindIndex(p, "\""); /* opening quote of the value */
    if (q1 < 0) return false;
    p += q1 + 1;
    int q2 = TextFindIndex(p, "\""); /* closing quote -> value length */
    if (q2 < 0) return false;
    if (q2 > dstsz - 1) q2 = dstsz - 1;
    TextCopy(dst, TextSubtext(p, 0, q2));
    return true;
}

bool ParseModel3(const char *path, Model3 *m3)
{
    char *buf = LoadFileText(path); /* raylib: NUL-terminated text */
    if (!buf) return false;

    memset(m3, 0, sizeof(*m3));
    bool ok = JsonStringValue(buf, "Moc", m3->moc, sizeof(m3->moc));

    /* Textures: find the [ ... ] array, then pull each "quoted" path until ']'.
     * Scanned in place with TextFindIndex (no fixed-size split buffer), so a
     * long texture list is not truncated; each value is bounded on copy. */
    int ti = TextFindIndex(buf, "\"Textures\"");
    if (ti >= 0)
    {
        const char *p = buf + ti;
        int lb = TextFindIndex(p, "[");
        int rb = TextFindIndex(p, "]");
        if (lb >= 0 && rb > lb)
        {
            const char *end = p + rb;
            const char *q = p + lb + 1;
            while (m3->texCount < MAX_TEXTURES)
            {
                int o = TextFindIndex(q, "\""); /* opening quote */
                if (o < 0 || q + o >= end) break;
                q += o + 1;
                int c = TextFindIndex(q, "\""); /* closing quote */
                if (c < 0 || q + c > end) break;
                int len = c < (int)sizeof(m3->tex[0]) ? c : (int)sizeof(m3->tex[0]) - 1;
                TextCopy(m3->tex[m3->texCount++], TextSubtext(q, 0, len));
                q += c + 1;
            }
            if (m3->texCount >= MAX_TEXTURES && TextFindIndex(q, "\"") >= 0 && q + TextFindIndex(q, "\"") < end)
                TraceLog(LOG_WARNING, "more than %d textures; extras ignored", MAX_TEXTURES);
        }
    }
    UnloadFileText(buf);
    return ok;
}

bool LoadCore(const char *mocPath, Core *c)
{
    memset(c, 0, sizeof(*c));
    int sz;
    unsigned char *file = LoadFileData(mocPath, &sz); /* raylib */
    if (!file)
    {
        fprintf(stderr, "cannot read moc: %s\n", mocPath);
        return false;
    }
    void *mocMem = AlignedBlob(csmAlignofMoc, (size_t)sz, &c->mocBase);
    if (!mocMem)
    {
        fprintf(stderr, "out of memory\n");
        UnloadFileData(file);
        return false;
    }
    memcpy(mocMem, file, (size_t)sz);
    UnloadFileData(file);

    c->moc = csmReviveMocInPlace(mocMem, (unsigned)sz);
    if (!c->moc)
    {
        fprintf(stderr, "revive failed: %s\n", csmGetErrorString(csmGetMocError((const csmMoc *)mocMem)));
        FreeCore(c); /* free what we allocated (matters for repeated drops) */
        return false;
    }
    unsigned modelSz = csmGetSizeofModel(c->moc);
    void *modelMem = AlignedBlob(csmAlignofModel, modelSz, &c->modelBase);
    if (!modelMem)
    {
        fprintf(stderr, "out of memory\n");
        FreeCore(c);
        return false;
    }
    c->model = csmInitializeModelInPlace(c->moc, modelMem, modelSz);
    if (!c->model)
    {
        fprintf(stderr, "init failed: %s\n", csmGetErrorString(csmGetMocError(c->moc)));
        FreeCore(c);
        return false;
    }
    return true;
}

void FreeCore(Core *c)
{
    free(c->mocBase);
    free(c->modelBase);
}
