/*
 * Purism Core: Emscripten/WASM platform glue
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifdef __EMSCRIPTEN__

#  include <stdlib.h>
#  include <stdint.h>
#  include <emscripten.h>

#  include "PurismCore.h"

static void *
psm_js_aligned_alloc(unsigned int alignment, unsigned int size)
{
  if (alignment < sizeof(void *))
    alignment = sizeof(void *);

  size_t total = (size_t)size + (size_t)alignment + sizeof(void *);
  void  *raw = malloc(total);
  if (!raw)
    return NULL;

  uintptr_t base = (uintptr_t)raw + sizeof(void *);
  uintptr_t aligned = (base + (alignment - 1)) & ~(uintptr_t)(alignment - 1);

  ((void **)aligned)[-1] = raw;

  return (void *)aligned;
}

static void
psm_js_aligned_free(void *ptr)
{
  if (!ptr)
    return;
  free(((void **)ptr)[-1]);
}

EMSCRIPTEN_KEEPALIVE
void *
csmMallocMoc(unsigned int mocSize)
{
  return psm_js_aligned_alloc(csmAlignofMoc, mocSize);
}

EMSCRIPTEN_KEEPALIVE
void *
csmMallocModelAndInitialize(csmMoc *moc)
{
  unsigned int size = csmGetSizeofModel(moc);
  if (!size)
    return NULL;
  void *memory = psm_js_aligned_alloc(csmAlignofModel, size);
  if (!memory)
    return NULL;
  csmModel *model = csmInitializeModelInPlace(moc, memory, size);
  if (!model) {
    psm_js_aligned_free(memory);
    return NULL;
  }
  return model;
}

EMSCRIPTEN_KEEPALIVE
void *
csmMalloc(unsigned int size)
{
  return psm_js_aligned_alloc(csmAlignofModel, size);
}

EMSCRIPTEN_KEEPALIVE
void
csmFree(void *memory)
{
  psm_js_aligned_free(memory);
}

EMSCRIPTEN_KEEPALIVE
void
csmInitializeAmountOfMemory(unsigned int size)
{
  (void)size;
}

#endif /* __EMSCRIPTEN__ */
