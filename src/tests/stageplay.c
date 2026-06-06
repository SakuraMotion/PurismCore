/*
 * Purism Core: Tcl-based model testing tool
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef CORE_HEADER
#include CORE_HEADER
#else
#include "../../include/PurismCore.h"
#endif

#include "../samples/common.h"

#if defined(PSM_COMPAT_VERSION) && PSM_COMPAT_VERSION < 0x06000000L
#error "stageplay requires PSM_COMPAT_VERSION >= 0x06000000 (Cubism Core 6+)"
#endif

#define TCL_DISABLE_PUTS
#define TEST
#include "partcl.h"

static float g_tolerance = 0.00015f;
static int g_verbose = 0;
static FILE *g_output = NULL;

static unsigned int g_lcg_state = 1;

/* Verify mode state */
static int g_verify_mode = 0;
static FILE *g_reference = NULL;
static int g_line_number = 0;
static int g_mismatch_count = 0;
static int g_total_comparisons = 0;
static char g_ref_line[4096];

/* Model state */
typedef struct {
  void *moc_data;
  size_t moc_size;
  csmMoc *moc;
  void *model_mem;
  csmModel *model;
} model_ctx;

static model_ctx g_model = {0};

static unsigned int
lcg_next(void)
{
  g_lcg_state = g_lcg_state * 1664525u + 1013904223u;
  return g_lcg_state;
}

/* Returns a float in [0, 1). */
static float
lcg_float(void)
{
  return (float)(lcg_next() >> 8) / 16777216.0f;
}

static void
output_line(const char *line)
{
  if (g_verify_mode && g_reference) {
    if (fgets(g_ref_line, sizeof(g_ref_line),
        g_reference)) {
      size_t len = strlen(g_ref_line);
      if (len > 0 && g_ref_line[len-1] == '\n')
        g_ref_line[len-1] = '\0';

      g_line_number++;
      g_total_comparisons++;

      int mismatch = 0;
      if (strcmp(line, g_ref_line) != 0) {
        char ref_copy[4096], got_copy[4096];
        snprintf(ref_copy, sizeof(ref_copy),
            "%s", g_ref_line);
        snprintf(got_copy, sizeof(got_copy),
            "%s", line);

        char *ref_tok = strtok(ref_copy, "\t");
        char *got_saveptr;
        char *got_tok = strtok_r(got_copy,
            "\t", &got_saveptr);

        while (ref_tok && got_tok && !mismatch) {
          char *ref_end, *got_end;
          double ref_val = strtod(ref_tok, &ref_end);
          double got_val = strtod(got_tok, &got_end);

          if (*ref_end == '\0' && *got_end == '\0' &&
              ref_tok != ref_end && got_tok != got_end) {
            if (fabs(ref_val - got_val) >
              g_tolerance)
              mismatch = 1;
          } else {
            if (strcmp(ref_tok, got_tok) != 0)
              mismatch = 1;
          }

          ref_tok = strtok(NULL, "\t");
          got_tok = strtok_r(NULL, "\t",
              &got_saveptr);
        }

        if ((ref_tok && !got_tok) ||
            (!ref_tok && got_tok))
          mismatch = 1;
      }

      if (mismatch) {
        g_mismatch_count++;
        if (g_verbose) {
          fprintf(stderr, "MISMATCH line %d:\n",
              g_line_number);
          fprintf(stderr, "  expected: %s\n", g_ref_line);
          fprintf(stderr, "  got:      %s\n", line);
        }
      }
    } else {
      g_mismatch_count++;
      if (g_verbose)
        fprintf(stderr,
            "EXTRA line %d: %s\n",
            g_line_number + 1, line);
    }
  }

  if (g_output)
    fprintf(g_output, "%s\n", line);
}

/* Format helpers */
static char g_fmt_buf[4096];
static int g_fmt_pos;

static void
fmt_start(void)
{
  g_fmt_pos = 0;
  g_fmt_buf[0] = '\0';
}

static void
fmt_str(const char *s)
{
  g_fmt_pos += snprintf(g_fmt_buf + g_fmt_pos,
      sizeof(g_fmt_buf) - g_fmt_pos, "%s", s);
}

static void
fmt_tab(void)
{
  fmt_str("\t");
}

static void
fmt_int(int v)
{
  g_fmt_pos += snprintf(g_fmt_buf + g_fmt_pos,
      sizeof(g_fmt_buf) - g_fmt_pos, "%d", v);
}

static void
fmt_float(float v)
{
  g_fmt_pos += snprintf(g_fmt_buf + g_fmt_pos,
      sizeof(g_fmt_buf) - g_fmt_pos, "%.6g", (double)v);
}

static void
fmt_emit(void)
{
  output_line(g_fmt_buf);
}

static void
free_model(void)
{
  if (g_model.model_mem) {
    psm_aligned_free(g_model.model_mem);
    g_model.model_mem = NULL;
  }
  if (g_model.moc_data) {
    psm_aligned_free(g_model.moc_data);
    g_model.moc_data = NULL;
  }
  g_model.moc = NULL;
  g_model.model = NULL;
}

static int
load_model(const char *path)
{
  free_model();

  g_model.moc_data = psm_read_file(path, &g_model.moc_size, 64, 0);
  if (!g_model.moc_data) {
    fprintf(stderr, "Error: Cannot read '%s'\n", path);
    return 0;
  }

  g_model.moc = csmReviveMocInPlace(g_model.moc_data,
      (unsigned int)g_model.moc_size);
  if (!g_model.moc) {
    fprintf(stderr,
        "Error: Invalid MOC3 file '%s'\n", path);
    psm_aligned_free(g_model.moc_data);
    g_model.moc_data = NULL;
    return 0;
  }

  unsigned int model_size = csmGetSizeofModel(g_model.moc);
  g_model.model_mem = psm_aligned_alloc(64, model_size);
  if (!g_model.model_mem) {
    fprintf(stderr,
        "Error: Cannot allocate model (%u bytes)\n",
        model_size);
    psm_aligned_free(g_model.moc_data);
    g_model.moc_data = NULL;
    return 0;
  }

  g_model.model = csmInitializeModelInPlace(g_model.moc,
      g_model.model_mem, model_size);
  if (!g_model.model) {
    fprintf(stderr, "Error: Cannot initialize model\n");
    free_model();
    return 0;
  }

  return 1;
}

/* Set parameters matching a glob pattern. Returns match count. */
static int
set_parameters(const char *pattern, const char *value_str)
{
  csmModel *model = g_model.model;
  if (!model) return 0;

  int count = csmGetParameterCount(model);
  const char **ids = csmGetParameterIds(model);
  float *values = csmGetParameterValues(model);
  const float *mins = csmGetParameterMinimumValues(model);
  const float *maxs = csmGetParameterMaximumValues(model);
  const float *defaults =
      csmGetParameterDefaultValues(model);

  int is_pattern =
      (strchr(pattern, '*') || strchr(pattern, '?'));
  int match_count = 0;

  for (int i = 0; i < count; i++) {
    int matches = is_pattern ? psm_glob_match(pattern, ids[i])
                             : (strcmp(pattern, ids[i]) == 0);
    if (!matches) continue;

    float value;
    if (strcmp(value_str, "min") == 0) {
      value = mins[i];
    } else if (strcmp(value_str, "max") == 0) {
      value = maxs[i];
    } else if (strcmp(value_str, "mid") == 0) {
      value = (mins[i] + maxs[i]) / 2.0f;
    } else if (strcmp(value_str, "default") == 0) {
      value = defaults[i];
    } else {
      value = (float)atof(value_str);
      if (value < mins[i]) value = mins[i];
      if (value > maxs[i]) value = maxs[i];
    }

    if (g_verbose)
      fprintf(stderr, "set %s = %g (was %g)\n",
          ids[i], value, values[i]);

    values[i] = value;
    match_count++;
  }

  return match_count;
}

static void
dump_version(void)
{
  csmVersion version = csmGetVersion();
  csmMocVersion latest = csmGetLatestMocVersion();

  fmt_start();
  fmt_str("V");
  fmt_tab(); fmt_int((version >> 24) & 0xff);
  fmt_tab(); fmt_int((version >> 16) & 0xff);
  fmt_tab(); fmt_int(version & 0xffff);
  fmt_tab(); fmt_int(0);
  fmt_tab(); fmt_int(latest);
  fmt_emit();
}

static void
dump_canvas(void)
{
  if (!g_model.model) return;
  csmVector2 size, origin;
  float ppu;
  csmReadCanvasInfo(g_model.model, &size, &origin, &ppu);

  fmt_start();
  fmt_str("C");
  fmt_tab(); fmt_float(size.X);
  fmt_tab(); fmt_float(size.Y);
  fmt_tab(); fmt_float(origin.X);
  fmt_tab(); fmt_float(origin.Y);
  fmt_tab(); fmt_float(ppu);
  fmt_emit();
}

static void
dump_parameters(void)
{
  if (!g_model.model) return;
  csmModel *model = g_model.model;
  int count = csmGetParameterCount(model);
  const char **ids = csmGetParameterIds(model);
  const float *mins = csmGetParameterMinimumValues(model);
  const float *maxs = csmGetParameterMaximumValues(model);
  const float *defaults =
      csmGetParameterDefaultValues(model);
  const float *values =
      csmGetParameterValues(model);

  fmt_start();
  fmt_str("PC");
  fmt_tab();
  fmt_int(count);
  fmt_emit();

  for (int i = 0; i < count; i++) {
    fmt_start();
    fmt_str("P");
    fmt_tab(); fmt_int(i);
    fmt_tab(); fmt_str(ids[i]);
    fmt_tab(); fmt_float(values[i]);
    fmt_tab(); fmt_float(mins[i]);
    fmt_tab(); fmt_float(maxs[i]);
    fmt_tab(); fmt_float(defaults[i]);
    fmt_emit();
  }
}

static void
dump_parts(void)
{
  if (!g_model.model) return;
  csmModel *model = g_model.model;
  int count = csmGetPartCount(model);
  const char **ids = csmGetPartIds(model);
  const float *opacities = csmGetPartOpacities(model);
  const int *parents =
      csmGetPartParentPartIndices(model);

  fmt_start();
  fmt_str("TC");
  fmt_tab();
  fmt_int(count);
  fmt_emit();

  for (int i = 0; i < count; i++) {
    fmt_start();
    fmt_str("T");
    fmt_tab(); fmt_int(i);
    fmt_tab(); fmt_str(ids[i]);
    fmt_tab(); fmt_float(opacities[i]);
    fmt_tab(); fmt_int(parents ? parents[i] : -1);
    fmt_emit();
  }
}

static void
dump_drawables(void)
{
  if (!g_model.model) return;
  csmModel *model = g_model.model;
  int count = csmGetDrawableCount(model);
  const char **ids = csmGetDrawableIds(model);
  const float *opacities = csmGetDrawableOpacities(model);
  const int *render_orders =
      csmGetRenderOrders(model);
  const int *draw_orders =
      csmGetDrawableDrawOrders(model);
  const int *vertex_counts =
      csmGetDrawableVertexCounts(model);
  const int *blend_modes =
      csmGetDrawableBlendModes(model);

  fmt_start();
  fmt_str("DC");
  fmt_tab();
  fmt_int(count);
  fmt_emit();

  for (int i = 0; i < count; i++) {
    fmt_start();
    fmt_str("D");
    fmt_tab(); fmt_int(i);
    fmt_tab(); fmt_str(ids[i]);
    fmt_tab(); fmt_float(opacities[i]);
    fmt_tab();
    fmt_int(render_orders ? render_orders[i] : i);
    fmt_tab();
    fmt_int(draw_orders ? draw_orders[i] : 0);
    fmt_tab(); fmt_int(vertex_counts[i]);
    fmt_tab(); fmt_int(blend_modes ? blend_modes[i] : 0);
    fmt_emit();
  }
}

static void
dump_vertices(void)
{
  if (!g_model.model) return;
  csmModel *model = g_model.model;
  int count = csmGetDrawableCount(model);
  const int *vertex_counts =
      csmGetDrawableVertexCounts(model);
  const csmVector2 **positions =
      csmGetDrawableVertexPositions(model);

  for (int i = 0; i < count; i++) {
    int vcount = vertex_counts[i];
    const csmVector2 *pos = positions[i];
    for (int v = 0; v < vcount; v++) {
      fmt_start();
      fmt_str("X");
      fmt_tab(); fmt_int(i);
      fmt_tab(); fmt_int(v);
      fmt_tab(); fmt_float(pos[v].X);
      fmt_tab(); fmt_float(pos[v].Y);
      fmt_emit();
    }
  }
}

static void
dump_colors(void)
{
  if (!g_model.model) return;
  csmModel *model = g_model.model;
  int count = csmGetDrawableCount(model);
  const csmVector4 *mult =
      csmGetDrawableMultiplyColors(model);
  const csmVector4 *screen =
      csmGetDrawableScreenColors(model);

  if (!mult || !screen) return;

  for (int i = 0; i < count; i++) {
    fmt_start();
    fmt_str("M");
    fmt_tab(); fmt_int(i);
    fmt_tab(); fmt_float(mult[i].X);
    fmt_tab(); fmt_float(mult[i].Y);
    fmt_tab(); fmt_float(mult[i].Z);
    fmt_tab(); fmt_float(mult[i].W);
    fmt_emit();

    fmt_start();
    fmt_str("S");
    fmt_tab(); fmt_int(i);
    fmt_tab(); fmt_float(screen[i].X);
    fmt_tab(); fmt_float(screen[i].Y);
    fmt_tab(); fmt_float(screen[i].Z);
    fmt_tab(); fmt_float(screen[i].W);
    fmt_emit();
  }
}

static void
dump_all(void)
{
  dump_version();
  dump_canvas();
  dump_parameters();
  dump_parts();
  dump_drawables();
  dump_vertices();
}

/* Helper: return a Tcl result string */
static int
tcl_ok(struct tcl *tcl, const char *s)
{
  return tcl_result(tcl, FNORMAL,
      tcl_alloc(s, strlen(s)));
}

static int
tcl_ok_int(struct tcl *tcl, int v)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", v);
  return tcl_ok(tcl, buf);
}

static int
tcl_ok_float(struct tcl *tcl, float v)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "%.6g", (double)v);
  return tcl_ok(tcl, buf);
}

static int
tcl_err(struct tcl *tcl, const char *msg)
{
  fprintf(stderr, "Error: %s\n", msg);
  return tcl_result(tcl, FERROR,
      tcl_alloc(msg, strlen(msg)));
}

/* load_model <path> */
static int
cmd_load_model(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *path = tcl_list_at(args, 1);
  int ok = load_model(tcl_string(path));
  tcl_free(path);
  return ok ? tcl_ok(tcl, "1") :
      tcl_err(tcl, "failed to load model");
}

/* update */
static int
cmd_update(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg; (void)args;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  csmUpdateModel(g_model.model);
  if (g_verbose) fprintf(stderr, "update\n");
  return tcl_ok(tcl, "");
}

/* reset_flags */
static int
cmd_reset_flags(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg; (void)args;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  csmResetDrawableDynamicFlags(g_model.model);
  return tcl_ok(tcl, "");
}

/* set_param <pattern> <value> */
static int
cmd_set_param(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *pattern = tcl_list_at(args, 1);
  tcl_value_t *value = tcl_list_at(args, 2);
  int n = set_parameters(tcl_string(pattern),
      tcl_string(value));
  tcl_free(pattern);
  tcl_free(value);
  return tcl_ok_int(tcl, n);
}

/* get_param <name_or_index> */
static int
cmd_get_param(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *name = tcl_list_at(args, 1);
  const char *s = tcl_string(name);

  csmModel *model = g_model.model;
  int count = csmGetParameterCount(model);
  float *values = csmGetParameterValues(model);

  /* Try as index first */
  char *end;
  long idx = strtol(s, &end, 10);
  if (*end == '\0' && idx >= 0 && idx < count) {
    tcl_free(name);
    return tcl_ok_float(tcl, values[idx]);
  }

  /* Try as name */
  const char **ids = csmGetParameterIds(model);
  for (int i = 0; i < count; i++) {
    if (strcmp(s, ids[i]) == 0) {
      tcl_free(name);
      return tcl_ok_float(tcl, values[i]);
    }
  }

  tcl_free(name);
  return tcl_err(tcl, "parameter not found");
}

/* get_param_count */
static int
cmd_get_param_count(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg; (void)args;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  return tcl_ok_int(tcl,
      csmGetParameterCount(g_model.model));
}

/* get_param_id <index> */
static int
cmd_get_param_id(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *idx_str = tcl_list_at(args, 1);
  int idx = atoi(tcl_string(idx_str));
  tcl_free(idx_str);
  int count =
      csmGetParameterCount(g_model.model);
  if (idx < 0 || idx >= count)
    return tcl_err(tcl, "index out of range");
  return tcl_ok(tcl,
      csmGetParameterIds(g_model.model)[idx]);
}

/* get_part_count */
static int
cmd_get_part_count(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg; (void)args;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  return tcl_ok_int(tcl,
      csmGetPartCount(g_model.model));
}

/* get_part_opacity <index> */
static int
cmd_get_part_opacity(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *idx_str = tcl_list_at(args, 1);
  int idx = atoi(tcl_string(idx_str));
  tcl_free(idx_str);
  int count = csmGetPartCount(g_model.model);
  if (idx < 0 || idx >= count)
    return tcl_err(tcl, "index out of range");
  return tcl_ok_float(tcl,
      csmGetPartOpacities(g_model.model)[idx]);
}

/* set_part_opacity <index> <value> */
static int
cmd_set_part_opacity(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *idx_str = tcl_list_at(args, 1);
  tcl_value_t *val_str = tcl_list_at(args, 2);
  int idx = atoi(tcl_string(idx_str));
  float val = (float)atof(tcl_string(val_str));
  tcl_free(idx_str);
  tcl_free(val_str);
  int count = csmGetPartCount(g_model.model);
  if (idx < 0 || idx >= count)
    return tcl_err(tcl, "index out of range");
  csmGetPartOpacities(g_model.model)[idx] = val;
  return tcl_ok(tcl, "");
}

/* get_drawable_count */
static int
cmd_get_drawable_count(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg; (void)args;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  return tcl_ok_int(tcl,
      csmGetDrawableCount(g_model.model));
}

/* get_drawable_opacity <index> */
static int
cmd_get_drawable_opacity(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *idx_str = tcl_list_at(args, 1);
  int idx = atoi(tcl_string(idx_str));
  tcl_free(idx_str);
  int count =
      csmGetDrawableCount(g_model.model);
  if (idx < 0 || idx >= count)
    return tcl_err(tcl, "index out of range");
  return tcl_ok_float(tcl,
      csmGetDrawableOpacities(
      g_model.model)[idx]);
}

/* get_drawable_vertex <drawable_idx> <vertex_idx> */
static int
cmd_get_drawable_vertex(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *d_str = tcl_list_at(args, 1);
  tcl_value_t *v_str = tcl_list_at(args, 2);
  int d = atoi(tcl_string(d_str));
  int v = atoi(tcl_string(v_str));
  tcl_free(d_str);
  tcl_free(v_str);

  csmModel *model = g_model.model;
  int dcount = csmGetDrawableCount(model);
  if (d < 0 || d >= dcount)
    return tcl_err(tcl,
        "drawable index out of range");
  const int *vcounts =
      csmGetDrawableVertexCounts(model);
  if (v < 0 || v >= vcounts[d])
    return tcl_err(tcl,
        "vertex index out of range");
  const csmVector2 **positions =
      csmGetDrawableVertexPositions(model);
  char buf[128];
  snprintf(buf, sizeof(buf), "%.6g %.6g",
      (double)positions[d][v].X,
      (double)positions[d][v].Y);
  return tcl_ok(tcl, buf);
}

/* get_render_order <index> */
static int
cmd_get_render_order(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *idx_str = tcl_list_at(args, 1);
  int idx = atoi(tcl_string(idx_str));
  tcl_free(idx_str);
  const int *orders =
      csmGetRenderOrders(g_model.model);
  if (!orders)
    return tcl_err(tcl, "no render orders");
  return tcl_ok_int(tcl, orders[idx]);
}

/* get_dynamic_flags <index> */
static int
cmd_get_dynamic_flags(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *idx_str = tcl_list_at(args, 1);
  int idx = atoi(tcl_string(idx_str));
  tcl_free(idx_str);
  int count =
      csmGetDrawableCount(g_model.model);
  if (idx < 0 || idx >= count)
    return tcl_err(tcl, "index out of range");
  const csmFlags *flags =
      csmGetDrawableDynamicFlags(g_model.model);
  return tcl_ok_int(tcl, flags[idx]);
}

/* get_mul_color <index> - returns "R G B A" */
static int
cmd_get_mul_color(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *idx_str = tcl_list_at(args, 1);
  int idx = atoi(tcl_string(idx_str));
  tcl_free(idx_str);
  int count =
      csmGetDrawableCount(g_model.model);
  if (idx < 0 || idx >= count)
    return tcl_err(tcl, "index out of range");
  const csmVector4 *mc =
      csmGetDrawableMultiplyColors(g_model.model);
  char buf[128];
  snprintf(buf, sizeof(buf),
      "%.6g %.6g %.6g %.6g",
      (double)mc[idx].X, (double)mc[idx].Y,
      (double)mc[idx].Z, (double)mc[idx].W);
  return tcl_ok(tcl, buf);
}

/* get_scr_color <index> - returns "R G B A" */
static int
cmd_get_scr_color(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *idx_str = tcl_list_at(args, 1);
  int idx = atoi(tcl_string(idx_str));
  tcl_free(idx_str);
  int count =
      csmGetDrawableCount(g_model.model);
  if (idx < 0 || idx >= count)
    return tcl_err(tcl, "index out of range");
  const csmVector4 *sc =
      csmGetDrawableScreenColors(g_model.model);
  char buf[128];
  snprintf(buf, sizeof(buf),
      "%.6g %.6g %.6g %.6g",
      (double)sc[idx].X, (double)sc[idx].Y,
      (double)sc[idx].Z, (double)sc[idx].W);
  return tcl_ok(tcl, buf);
}

/* get_key_count <index> */
static int
cmd_get_key_count(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *idx_str = tcl_list_at(args, 1);
  int idx = atoi(tcl_string(idx_str));
  tcl_free(idx_str);
  int count =
      csmGetParameterCount(g_model.model);
  if (idx < 0 || idx >= count)
    return tcl_err(tcl, "index out of range");
  const int *kc =
      csmGetParameterKeyCounts(g_model.model);
  return tcl_ok_int(tcl, kc[idx]);
}

/* bitand <a> <b> */
static int
cmd_bitand(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *a = tcl_list_at(args, 1);
  tcl_value_t *b = tcl_list_at(args, 2);
  int r = atoi(tcl_string(a)) &
      atoi(tcl_string(b));
  tcl_free(a); tcl_free(b);
  return tcl_ok_int(tcl, r);
}

/* assert_eq <actual> <expected> <msg> */
static int
cmd_assert_eq(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *actual = tcl_list_at(args, 1);
  tcl_value_t *expected = tcl_list_at(args, 2);
  tcl_value_t *msg = tcl_list_at(args, 3);
  if (strcmp(tcl_string(actual),
      tcl_string(expected)) != 0) {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "ASSERT FAILED: %s:"
        " got '%s' expected '%s'",
        tcl_string(msg), tcl_string(actual),
        tcl_string(expected));
    tcl_free(actual);
    tcl_free(expected);
    tcl_free(msg);
    return tcl_err(tcl, buf);
  }
  tcl_free(actual);
  tcl_free(expected);
  tcl_free(msg);
  return tcl_ok(tcl, "");
}

/* assert_neq <actual> <not_expected> <msg> */
static int
cmd_assert_neq(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *actual = tcl_list_at(args, 1);
  tcl_value_t *not_exp = tcl_list_at(args, 2);
  tcl_value_t *msg = tcl_list_at(args, 3);
  if (strcmp(tcl_string(actual),
      tcl_string(not_exp)) == 0) {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "ASSERT FAILED: %s:"
        " got '%s' (should differ)",
        tcl_string(msg), tcl_string(actual));
    tcl_free(actual);
    tcl_free(not_exp);
    tcl_free(msg);
    return tcl_err(tcl, buf);
  }
  tcl_free(actual);
  tcl_free(not_exp);
  tcl_free(msg);
  return tcl_ok(tcl, "");
}

/* tolerance <float> */
static int
cmd_tolerance(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *val = tcl_list_at(args, 1);
  g_tolerance = (float)atof(tcl_string(val));
  tcl_free(val);
  if (g_verbose)
    fprintf(stderr, "tolerance = %g\n",
        g_tolerance);
  return tcl_ok(tcl, "");
}

/* emit <line> - emit a line for verification */
static int
cmd_emit(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *line = tcl_list_at(args, 1);
  output_line(tcl_string(line));
  tcl_free(line);
  return tcl_ok(tcl, "");
}

/* dump <section> */
static int
cmd_dump(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");
  tcl_value_t *section = tcl_list_at(args, 1);
  const char *s = tcl_string(section);

  if (strcmp(s, "version") == 0)
    dump_version();
  else if (strcmp(s, "canvas") == 0)
    dump_canvas();
  else if (strcmp(s, "parameters") == 0)
    dump_parameters();
  else if (strcmp(s, "parts") == 0)
    dump_parts();
  else if (strcmp(s, "drawables") == 0)
    dump_drawables();
  else if (strcmp(s, "vertices") == 0)
    dump_vertices();
  else if (strcmp(s, "colors") == 0)
    dump_colors();
  else if (strcmp(s, "all") == 0)
    dump_all();
  else {
    tcl_free(section);
    return tcl_err(tcl, "unknown dump section");
  }

  tcl_free(section);
  return tcl_ok(tcl, "");
}

/* version - return library version string */
static int
cmd_version(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg; (void)args;
  csmVersion v = csmGetVersion();
  char buf[32];
  snprintf(buf, sizeof(buf), "%d.%d.%d",
      (v >> 24) & 0xff, (v >> 16) & 0xff, v & 0xffff);
  return tcl_ok(tcl, buf);
}

/* psm_glob_match <pattern> <string> */
static int
cmd_glob_match(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *pat = tcl_list_at(args, 1);
  tcl_value_t *str = tcl_list_at(args, 2);
  int r = psm_glob_match(tcl_string(pat),
      tcl_string(str));
  tcl_free(pat);
  tcl_free(str);
  return tcl_ok_int(tcl, r);
}

/* fmt_float <value> */
static int
cmd_fmt_float(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *val = tcl_list_at(args, 1);
  float f = (float)atof(tcl_string(val));
  tcl_free(val);
  return tcl_ok_float(tcl, f);
}

/* Custom puts - goes through output_line */
static int
cmd_puts(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *text = tcl_list_at(args, 1);
  output_line(tcl_string(text));
  return tcl_result(tcl, FNORMAL, text);
}

/* srand <seed> - seed the LCG */
static int
cmd_srand(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  tcl_value_t *val = tcl_list_at(args, 1);
  g_lcg_state =
      (unsigned int)atoi(tcl_string(val));
  tcl_free(val);
  return tcl_ok(tcl, "");
}

/* rand [max] - return random int [0, max-1] */
static int
cmd_rand(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg;
  int n = tcl_list_length(args);
  unsigned int r = lcg_next();
  if (n >= 2) {
    tcl_value_t *max_str = tcl_list_at(args, 1);
    int max = atoi(tcl_string(max_str));
    tcl_free(max_str);
    if (max > 0) r = r % (unsigned int)max;
  }
  return tcl_ok_int(tcl, (int)r);
}

/* rand_float - return random float in [0, 1) */
static int
cmd_rand_float(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg; (void)args;
  return tcl_ok_float(tcl, lcg_float());
}

/*
 * randomize_params [pattern] - set matching params
 * to random values in their range. If no pattern
 * given, randomizes all normal parameters.
 */
static int
cmd_randomize_params(struct tcl *tcl,
    tcl_value_t *args, void *arg)
{
  (void)arg;
  if (!g_model.model)
    return tcl_err(tcl, "no model loaded");

  const char *pattern = "*";
  tcl_value_t *pat_val = NULL;
  if (tcl_list_length(args) >= 2) {
    pat_val = tcl_list_at(args, 1);
    pattern = tcl_string(pat_val);
  }

  csmModel *model = g_model.model;
  int count = csmGetParameterCount(model);
  const char **ids = csmGetParameterIds(model);
  float *values = csmGetParameterValues(model);
  const float *mins = csmGetParameterMinimumValues(model);
  const float *maxs =
      csmGetParameterMaximumValues(model);
  int is_glob =
      (strchr(pattern, '*') || strchr(pattern, '?'));
  int match_count = 0;

  for (int i = 0; i < count; i++) {
    int matches = is_glob ? psm_glob_match(pattern, ids[i])
                          : (strcmp(pattern, ids[i]) == 0);
    if (!matches) continue;

    float lo = mins[i], hi = maxs[i];
    volatile float t = lcg_float();
    volatile float range = hi - lo;
    float value = lo + t * range;
    if (g_verbose)
      fprintf(stderr,
          "randomize %s = %g"
          " (range [%g, %g])\n",
          ids[i], value, lo, hi);
    values[i] = value;
    match_count++;
  }

  if (pat_val) tcl_free(pat_val);
  return tcl_ok_int(tcl, match_count);
}

/* No-op command for # comments */
static int
cmd_comment(struct tcl *tcl, tcl_value_t *args,
    void *arg)
{
  (void)arg; (void)args;
  return tcl_result(tcl, FNORMAL,
      tcl_alloc("", 0));
}

/* Log handler */
static void
log_handler(const char *message)
{
  if (g_verbose)
    fprintf(stderr, "[CSM] %s\n", message);
}

static void
register_commands(struct tcl *tcl)
{
  /* Comments */
  tcl_register(tcl, "#",
      cmd_comment, 0, NULL);
  /* Model lifecycle */
  tcl_register(tcl, "load_model",
      cmd_load_model, 2, NULL);
  tcl_register(tcl, "update",
      cmd_update, 1, NULL);
  tcl_register(tcl, "reset_flags",
      cmd_reset_flags, 1, NULL);

  /* Parameter access */
  tcl_register(tcl, "set_param",
      cmd_set_param, 3, NULL);
  tcl_register(tcl, "get_param",
      cmd_get_param, 2, NULL);
  tcl_register(tcl, "get_param_count",
      cmd_get_param_count, 1, NULL);
  tcl_register(tcl, "get_param_id",
      cmd_get_param_id, 2, NULL);

  /* Part access */
  tcl_register(tcl, "get_part_count",
      cmd_get_part_count, 1, NULL);
  tcl_register(tcl, "get_part_opacity",
      cmd_get_part_opacity, 2, NULL);
  tcl_register(tcl, "set_part_opacity",
      cmd_set_part_opacity, 3, NULL);

  /* Drawable access */
  tcl_register(tcl, "get_drawable_count",
      cmd_get_drawable_count, 1, NULL);
  tcl_register(tcl, "get_drawable_opacity",
      cmd_get_drawable_opacity, 2, NULL);
  tcl_register(tcl, "get_drawable_vertex",
      cmd_get_drawable_vertex, 3, NULL);
  tcl_register(tcl, "get_render_order",
      cmd_get_render_order, 2, NULL);
  tcl_register(tcl, "get_dynamic_flags",
      cmd_get_dynamic_flags, 2, NULL);
  tcl_register(tcl, "get_mul_color",
      cmd_get_mul_color, 2, NULL);
  tcl_register(tcl, "get_scr_color",
      cmd_get_scr_color, 2, NULL);
  tcl_register(tcl, "get_key_count",
      cmd_get_key_count, 2, NULL);

  /* Assertions and bitwise */
  tcl_register(tcl, "assert_eq",
      cmd_assert_eq, 4, NULL);
  tcl_register(tcl, "assert_neq",
      cmd_assert_neq, 4, NULL);
  tcl_register(tcl, "bitand",
      cmd_bitand, 3, NULL);

  /* Output */
  tcl_register(tcl, "tolerance",
      cmd_tolerance, 2, NULL);
  tcl_register(tcl, "emit",
      cmd_emit, 2, NULL);
  tcl_register(tcl, "dump",
      cmd_dump, 2, NULL);
  tcl_register(tcl, "puts",
      cmd_puts, 2, NULL);

  /* Random */
  tcl_register(tcl, "srand",
      cmd_srand, 2, NULL);
  tcl_register(tcl, "rand",
      cmd_rand, 0, NULL);
  tcl_register(tcl, "rand_float",
      cmd_rand_float, 1, NULL);
  tcl_register(tcl, "randomize_params",
      cmd_randomize_params, 0, NULL);

  /* Utility */
  tcl_register(tcl, "version",
      cmd_version, 1, NULL);
  tcl_register(tcl, "psm_glob_match",
      cmd_glob_match, 3, NULL);
  tcl_register(tcl, "fmt_float",
      cmd_fmt_float, 2, NULL);
}

static void
print_usage(const char *prog)
{
  fprintf(stderr,
      "Usage: %s [-v] [-D var=value]"
      " [-o output] [-i input]\n"
      "       [-e script]"
      " [script.tcl] [args...]\n\n"
      "Options:\n"
      "  -v            Verbose output\n"
      "  -D var=value  Set a Tcl variable\n"
      "  -o output     Write output to file\n"
      "  -i input      Verify mode:"
      " compare with input\n"
      "  -e script     Run script text\n"
      "  -h            Show this help\n\n"
      "  script.tcl    Tcl script file\n"
      "  args...       Arguments as"
      " $argv list\n", prog);
}

/* Parsed -D definitions */
#define MAX_DEFINES 64
static struct {
  const char *name;
  const char *value;
} g_defines[MAX_DEFINES];
static int g_define_count = 0;

int
main(int argc, char **argv)
{
  const char *script_path = NULL;
  const char *output_path = NULL;
  const char *input_path = NULL;
  const char *eval_script = NULL;
  int first_positional = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 ||
        strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]); return 0;
    } else if (strcmp(argv[i], "-v") == 0) {
      g_verbose = 1;
    } else if (strcmp(argv[i], "-D") == 0 &&
        i + 1 < argc) {
      const char *def = argv[++i];
      const char *eq = strchr(def, '=');
      if (eq && g_define_count < MAX_DEFINES) {
        /* Parse name=value later at Tcl init */
        g_defines[g_define_count].name = def;
        g_defines[g_define_count].value = eq + 1;
        g_define_count++;
      } else if (!eq) {
        fprintf(stderr,
            "Error: -D requires"
            " var=value format\n");
        return 1;
      }
    } else if (strcmp(argv[i], "-o") == 0 &&
        i + 1 < argc) {
      output_path = argv[++i];
    } else if (strcmp(argv[i], "-i") == 0 &&
        i + 1 < argc) {
      input_path = argv[++i];
    } else if (strcmp(argv[i], "-e") == 0 &&
        i + 1 < argc) {
      eval_script = argv[++i];
    } else if (argv[i][0] == '-') {
      fprintf(stderr,
          "Error: Unknown option '%s'\n",
          argv[i]);
      return 1;
    } else {
      /* First positional argument is the script file */
      first_positional = i;
      break;
    }
  }

  if (first_positional) {
    script_path = argv[first_positional];
  }

  if (!script_path && !eval_script) {
    print_usage(argv[0]);
    return 1;
  }

  /* Set up output mode */
  if (input_path) {
    g_verify_mode = 1;
    g_reference = fopen(input_path, "r");
    if (!g_reference) {
      fprintf(stderr,
          "Error: Cannot open input file"
          " '%s'\n", input_path);
      return 1;
    }
    if (output_path) {
      g_output = fopen(output_path, "w");
      if (!g_output) {
        fprintf(stderr,
          "Error: Cannot open output"
          " file '%s'\n", output_path);
        return 1;
      }
    }
  } else if (output_path) {
    g_output = fopen(output_path, "w");
    if (!g_output) {
      fprintf(stderr,
          "Error: Cannot open output"
          " file '%s'\n", output_path);
      return 1;
    }
  } else {
    g_output = stdout;
  }

  csmSetLogFunction(log_handler);

  /* Initialize Tcl and register commands */
  struct tcl tcl;
  tcl_init(&tcl);
  register_commands(&tcl);

  /* Set -D variables */
  for (int i = 0; i < g_define_count; i++) {
    const char *def = g_defines[i].name;
    const char *eq = strchr(def, '=');
    size_t name_len = (size_t)(eq - def);
    tcl_value_t *name = tcl_alloc(def, name_len);
    tcl_value_t *val = tcl_alloc(
        g_defines[i].value,
        strlen(g_defines[i].value));
    tcl_var(&tcl, name, val);
    tcl_free(name);
  }

  /* Build $argv list from remaining positional arguments */
  {
    tcl_value_t *argv_list = tcl_list_alloc();
    int args_start = first_positional ?
        first_positional + 1 : argc;
    for (int i = args_start; i < argc; i++) {
      tcl_value_t *arg = tcl_alloc(argv[i],
          strlen(argv[i]));
      argv_list =
          tcl_list_append(argv_list, arg);
      tcl_free(arg);
    }
    tcl_value_t *name = tcl_alloc("argv", 4);
    tcl_var(&tcl, name, tcl_dup(argv_list));
    tcl_free(name);
    tcl_list_free(argv_list);
  }

  /* Execute -e script if given */
  int r = FNORMAL;
  if (eval_script) {
    r = tcl_eval(&tcl, eval_script,
        strlen(eval_script) + 1);
    if (r == FERROR) {
      fprintf(stderr,
          "Error: -e script failed: %s\n",
          tcl_length(tcl.result) ?
          tcl_string(tcl.result) :
          "(unknown)");
    }
  }

  /* Read and execute script file */
  if (script_path && r != FERROR) {
    size_t script_size;
    void *script_data = psm_read_file(
        script_path, &script_size, 1, 1);
    if (!script_data) {
      fprintf(stderr,
          "Error: Cannot read script '%s'\n",
          script_path);
      tcl_destroy(&tcl);
      free_model();
      return 1;
    }

    r = tcl_eval(&tcl, (const char *)script_data,
        script_size + 1);
    if (r == FERROR) {
      fprintf(stderr,
          "Error: Script execution"
          " failed: %s\n",
          tcl_length(tcl.result) ?
          tcl_string(tcl.result) :
          "(unknown)");
    }
    psm_aligned_free(script_data);
  }

  /* Clean up */
  tcl_destroy(&tcl);
  free_model();

  if (g_output && g_output != stdout)
    fclose(g_output);

  /* Report verify results */
  if (g_verify_mode) {
    while (fgets(g_ref_line, sizeof(g_ref_line),
        g_reference)) {
      g_mismatch_count++;
      if (g_verbose) {
        size_t len = strlen(g_ref_line);
        if (len > 0 && g_ref_line[len-1] == '\n')
          g_ref_line[len-1] = '\0';
        fprintf(stderr, "MISSING line: %s\n",
            g_ref_line);
      }
    }
    fclose(g_reference);

    if (g_mismatch_count > 0) {
      fprintf(stderr,
          "FAIL: %d mismatches out of"
          " %d comparisons\n",
          g_mismatch_count,
          g_total_comparisons);
      return 1;
    } else {
      fprintf(stderr,
          "PASS: %d comparisons matched\n",
          g_total_comparisons);
      return 0;
    }
  }

  return (r == FERROR) ? 1 : 0;
}
