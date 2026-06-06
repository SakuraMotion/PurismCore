/*
 * Purism Core: shared utilities for samples and tests
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__SAMPLES_COMMON_H
#define PSM__SAMPLES_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Portable aligned allocation. Returns a pointer aligned to at least
 * `alignment` bytes. Free with psm_aligned_free().
 */
static void *
psm_aligned_alloc(size_t alignment, size_t size)
{
  void *p = malloc(size + alignment + sizeof(void *));
  if (!p)
    return NULL;
  void *a = (void *)((((size_t)p + sizeof(void *))
      + alignment - 1) & ~(alignment - 1));
  ((void **)a)[-1] = p;
  return a;
}

static void
psm_aligned_free(void *p)
{
  if (p)
    free(((void **)p)[-1]);
}

/*
 * Read an entire file into an aligned buffer.
 * If nul_terminate is nonzero, appends a '\0' after the data
 * (useful for text/script files). The terminator is not
 * included in *out_size.
 * Returns NULL on failure (with a message on stderr).
 */
static void *
psm_read_file(const char *path, size_t *out_size,
    size_t alignment, int nul_terminate)
{
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Error: Cannot open '%s'\n", path);
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (size <= 0) {
    fprintf(stderr, "Error: Invalid file size for '%s'\n", path);
    fclose(f);
    return NULL;
  }

  size_t extra = nul_terminate ? 1 : 0;
  void *data = psm_aligned_alloc(alignment, (size_t)size + extra);
  if (!data) {
    fprintf(stderr, "Error: Cannot allocate %ld bytes\n", size);
    fclose(f);
    return NULL;
  }

  if (fread(data, 1, (size_t)size, f) != (size_t)size) {
    fprintf(stderr, "Error: Cannot read '%s'\n", path);
    psm_aligned_free(data);
    fclose(f);
    return NULL;
  }

  if (nul_terminate)
    ((char *)data)[size] = '\0';

  fclose(f);
  *out_size = (size_t)size;
  return data;
}

/*
 * Case-insensitive glob pattern matching.
 * Supports * (any sequence) and ? (any single character).
 */
static int
psm_glob_match(const char *pattern, const char *str)
{
  while (*pattern && *str) {
    if (*pattern == '*') {
      pattern++;
      if (!*pattern)
        return 1;
      while (*str) {
        if (psm_glob_match(pattern, str))
          return 1;
        str++;
      }
      return psm_glob_match(pattern, str);
    } else if (*pattern == '?') {
      pattern++;
      str++;
    } else {
      char p = *pattern, s = *str;
      if (p >= 'A' && p <= 'Z') p += 32;
      if (s >= 'A' && s <= 'Z') s += 32;
      if (p != s)
        return 0;
      pattern++;
      str++;
    }
  }
  while (*pattern == '*')
    pattern++;
  return !*pattern && !*str;
}

#endif /* PSM__SAMPLES_COMMON_H */
