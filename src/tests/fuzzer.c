/*
 * Purism Core: libFuzzer harness
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "PurismCore.h"
#include "../samples/common.h"

#ifdef PSM_DEBUG_MALLOC
void psm__dbg_free_all(void);
#endif

static int g_initialized = 0;

static void
null_log(const char *msg)
{
  (void)msg;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
#ifdef PSM_DEBUG_MALLOC
  psm__dbg_free_all();
#endif

  if (!g_initialized) {
    csmSetLogFunction(null_log);
    g_initialized = 1;
  }
  if (size == 0 || size > 64 * 1024 * 1024) return 0;

  void *moc_buf = psm_aligned_alloc(csmAlignofMoc, size);
  if (!moc_buf) return 0;
  memcpy(moc_buf, data, size);

  csmMoc *moc = csmReviveMocInPlace(moc_buf, (unsigned int)size);
  if (!moc) {
    psm_aligned_free(moc_buf);
    return 0;
  }

  unsigned int model_size = csmGetSizeofModel(moc);
  if (model_size == 0 || model_size > 256 * 1024 * 1024) {
    psm_aligned_free(moc_buf);
    return 0;
  }

  void *model_buf = psm_aligned_alloc(csmAlignofModel, model_size);
  if (!model_buf) {
    psm_aligned_free(moc_buf);
    return 0;
  }

  csmModel *model = csmInitializeModelInPlace(moc, model_buf, model_size);
  if (!model) {
    psm_aligned_free(model_buf);
    psm_aligned_free(moc_buf);
    return 0;
  }

  csmResetDrawableDynamicFlags(model);
  csmUpdateModel(model);

  int param_count = csmGetParameterCount(model);
  if (param_count > 0 && size >= 4) {
    float *values = csmGetParameterValues(model);
    const float *mins = csmGetParameterMinimumValues(model);
    const float *maxs = csmGetParameterMaximumValues(model);
    for (int pass = 0; pass < 6; pass++) {
      for (int i = 0; i < param_count; i++) {
        uint32_t r;
        /* read 4 bytes from a varying in-bounds offset (size >= 4 here) */
        size_t off = ((size_t)i * 4u + (size_t)pass * 2654435761u)
            % (size - 3);
        memcpy(&r, data + off, sizeof(r));
        r ^= (uint32_t)pass * 0x9e3779b9u;

        /* sweep extremes/out-of-range on some passes to exercise clamping */
        float t = (pass & 1) ? (float)(int32_t)r / 64.0f
                             : (float)(r % 1000) / 999.0f;
        values[i] = mins[i] + t * (maxs[i] - mins[i]);
      }
      csmResetDrawableDynamicFlags(model);
      csmUpdateModel(model);
    }
  }

  psm_aligned_free(model_buf);
  psm_aligned_free(moc_buf);
  return 0;
}
