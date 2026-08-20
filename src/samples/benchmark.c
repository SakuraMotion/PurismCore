/*
 * Purism Core: micro-benchmark
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <dirent.h>

#include "../../include/PurismCore.h"
#include "common.h"

static uint64_t
now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/*
 * Run `fn(ctx)` repeatedly (in batches, to amortize the clock read) until at
 * least target_ns has elapsed, then return mean nanoseconds per call.
 * `*iters_out` receives the iteration count actually run.
 */
static double
time_loop(uint64_t target_ns, void (*fn)(void *), void *ctx,
    uint64_t *iters_out)
{
  const int batch = 8;
  /* warm up */
  for (int i = 0; i < 4; i++) fn(ctx);

  uint64_t iters = 0;
  uint64_t start = now_ns(), elapsed = 0;
  do {
    for (int b = 0; b < batch; b++) fn(ctx);
    iters += batch;
    elapsed = now_ns() - start;
  } while (elapsed < target_ns && iters < 200000000ull);

  if (iters_out) *iters_out = iters;
  return (double)elapsed / (double)iters;
}

struct ctx {
  const uint8_t *raw;     /* pristine file bytes */
  size_t         n;       /* file size */
  void          *work;    /* reusable revive buffer (n bytes, aligned) */
  csmMoc        *moc;     /* live moc (for init/update phases) */
  void          *model_buf;
  unsigned       model_sz;
  csmModel      *model;   /* live model (for update phases) */
  uint64_t       counter; /* drives param animation */
};

static void
do_memcpy(void *p)
{
  struct ctx *c = p;
  memcpy(c->work, c->raw, c->n);
}

static void
do_revive(void *p)
{
  struct ctx *c = p;
  memcpy(c->work, c->raw, c->n);
  volatile csmMoc *m = csmReviveMocInPlace(c->work, (unsigned)c->n);
  (void)m;
}

static void
do_init(void *p)
{
  struct ctx *c = p;
  unsigned sz = csmGetSizeofModel(c->moc);
  volatile csmModel *m =
      csmInitializeModelInPlace(c->moc, c->model_buf, sz);
  (void)m;
}

static void
do_animate(void *p)
{
  struct ctx *c = p;
  int pc = csmGetParameterCount(c->model);
  float *v = csmGetParameterValues(c->model);
  const float *mn = csmGetParameterMinimumValues(c->model);
  const float *mx = csmGetParameterMaximumValues(c->model);
  uint64_t t = c->counter++;
  for (int i = 0; i < pc; i++) {
    /* cheap deterministic sweep across [min,max] */
    float f = (float)(((t + (uint64_t)i) * 2654435761u) % 1000) / 999.0f;
    v[i] = mn[i] + f * (mx[i] - mn[i]);
  }
  csmResetDrawableDynamicFlags(c->model);
  csmUpdateModel(c->model);
}

static void
do_idle(void *p)
{
  struct ctx *c = p;
  csmUpdateModel(c->model);
}

static int g_models;
static double g_sum_revive, g_sum_init, g_sum_anim, g_sum_idle;
static uint64_t g_total_bytes, g_total_verts;

static void
fmt_time(double ns, char *out, size_t cap)
{
  if (ns < 1000.0)            snprintf(out, cap, "%.0f ns", ns);
  else if (ns < 1000000.0)   snprintf(out, cap, "%.2f us", ns / 1e3);
  else                       snprintf(out, cap, "%.3f ms", ns / 1e6);
}

static void
run_one(const char *path, uint64_t budget_ns)
{
  size_t n = 0;
  void *raw = psm_read_file(path, &n, csmAlignofMoc, 0);
  if (!raw) return;

  void *work = psm_aligned_alloc(csmAlignofMoc, n);
  void *probe = psm_aligned_alloc(csmAlignofMoc, n);
  memcpy(probe, raw, n);
  csmMoc *moc = csmReviveMocInPlace(probe, (unsigned)n);
  if (!moc) {
    fprintf(stderr, "  skip (revive failed): %s\n", path);
    psm_aligned_free(raw); psm_aligned_free(work); psm_aligned_free(probe);
    return;
  }
  unsigned model_sz = csmGetSizeofModel(moc);
  void *model_buf = psm_aligned_alloc(csmAlignofModel, model_sz);
  csmModel *model = csmInitializeModelInPlace(moc, model_buf, model_sz);
  if (!model) {
    fprintf(stderr, "  skip (init failed): %s\n", path);
    psm_aligned_free(raw); psm_aligned_free(work);
    psm_aligned_free(probe); psm_aligned_free(model_buf);
    return;
  }

  int dc = csmGetDrawableCount(model);
  int pc = csmGetParameterCount(model);
  int parts = csmGetPartCount(model);
  const int *vcs = csmGetDrawableVertexCounts(model);
  uint64_t verts = 0;
  for (int i = 0; i < dc; i++) verts += (vcs[i] > 0) ? (uint64_t)vcs[i] : 0;

  struct ctx c = {0};
  c.raw = raw; c.n = n; c.work = work;
  c.moc = moc; c.model_buf = model_buf; c.model_sz = model_sz; c.model = model;

  uint64_t it;
  double t_copy   = time_loop(budget_ns / 2, do_memcpy, &c, &it);
  double t_revive = time_loop(budget_ns, do_revive, &c, &it);
  double revive   = t_revive - t_copy; if (revive < 0) revive = 0;
  double init     = time_loop(budget_ns, do_init, &c, &it);
  double anim     = time_loop(budget_ns, do_animate, &c, &it);
  double idle     = time_loop(budget_ns, do_idle, &c, &it);

  char b1[32], b2[32], b3[32], b4[32];
  fmt_time(revive, b1, sizeof b1);
  fmt_time(init, b2, sizeof b2);
  fmt_time(anim, b3, sizeof b3);
  fmt_time(idle, b4, sizeof b4);

  const char *base = strrchr(path, '/');
  base = base ? base + 1 : path;

  printf("\n%s\n", base);
  printf("  size %zu KB | drawables %d | params %d | parts %d | vertices %llu\n",
      n / 1024, dc, pc, parts, (unsigned long long)verts);
  printf("  revive (parse+validate) : %-10s  %.2f GB/s\n",
      b1, revive > 0 ? (double)n / revive : 0.0);   /* bytes/ns == GB/s */
  printf("  init   (build model)    : %-10s\n", b2);
  printf("  animate (full recompute): %-10s  %.0f fps\n",
      b3, anim > 0 ? 1e9 / anim : 0.0);
  printf("  idle   (no change)      : %-10s  %.0f fps\n",
      b4, idle > 0 ? 1e9 / idle : 0.0);

  g_models++;
  g_sum_revive += revive; g_sum_init += init;
  g_sum_anim += anim; g_sum_idle += idle;
  g_total_bytes += n; g_total_verts += verts;

  psm_aligned_free(raw); psm_aligned_free(work);
  psm_aligned_free(probe); psm_aligned_free(model_buf);
}

static int
ends_moc3(const char *s)
{
  size_t l = strlen(s);
  return l > 5 && strcmp(s + l - 5, ".moc3") == 0;
}

static void
run_path(const char *path, uint64_t budget_ns)
{
  DIR *d = opendir(path);
  if (d) {
    struct dirent *e; char p[4096];
    while ((e = readdir(d))) {
      if (!ends_moc3(e->d_name)) continue;
      snprintf(p, sizeof p, "%s/%s", path, e->d_name);
      run_one(p, budget_ns);
    }
    closedir(d);
  } else {
    run_one(path, budget_ns);
  }
}

static void
null_log(const char *m) { (void)m; }

int
main(int argc, char **argv)
{
  uint64_t budget_ns = 200ull * 1000000ull;   /* 200 ms per phase */

  csmSetLogFunction(null_log);
  printf("Purism Core benchmark  (ABI %#08x)\n", (unsigned)csmGetVersion());

  int any = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
      budget_ns = (uint64_t)strtoull(argv[++i], NULL, 10) * 1000000ull;
    } else if (strcmp(argv[i], "-h") == 0) {
      printf("usage: benchmark [-t ms_per_phase] <file.moc3 | dir> ...\n");
      return 0;
    } else {
      run_path(argv[i], budget_ns);
      any = 1;
    }
  }

  if (!any) {
    fprintf(stderr, "no input; usage: benchmark [-t ms] <file.moc3 | dir> ...\n");
    return 1;
  }

  if (g_models > 1) {
    char b1[32], b2[32], b3[32], b4[32];
    fmt_time(g_sum_revive / g_models, b1, sizeof b1);
    fmt_time(g_sum_init / g_models, b2, sizeof b2);
    fmt_time(g_sum_anim / g_models, b3, sizeof b3);
    fmt_time(g_sum_idle / g_models, b4, sizeof b4);
    printf("\n=== mean over %d models ===\n", g_models);
    printf("  revive  %-10s   init  %-10s\n", b1, b2);
    printf("  animate %-10s   idle  %-10s\n", b3, b4);
    printf("  total %llu KB parsed, %llu vertices\n",
        (unsigned long long)(g_total_bytes / 1024),
        (unsigned long long)g_total_verts);
  }
  return 0;
}
