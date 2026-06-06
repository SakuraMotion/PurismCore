/*
 * Purism Core: libFuzzer harness
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "PurismCore.h"
#include "../samples/common.h"

static int g_initialized = 0;

static void
null_log(const char *msg)
{
  (void)msg;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
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
    for (int i = 0; i < param_count && i < (int)size / 4; i++) {
      uint32_t r;
      memcpy(&r, data + i * 4 % size, sizeof(r));
      float t = (float)(r % 1000) / 999.0f;
      values[i] = mins[i] + t * (maxs[i] - mins[i]);
    }
    csmResetDrawableDynamicFlags(model);
    csmUpdateModel(model);
  }

  psm_aligned_free(model_buf);
  psm_aligned_free(moc_buf);
  return 0;
}
