/*
 * Purism Core: MOC3 info dump utility (JSON output)
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "../../include/PurismCore.h"
#include "common.h"

static int g_decimal_places = -1;
static int g_full_output = 0;

#define JSON_MAX_DEPTH 64

static int g_indent = 0;
static int g_first_stack[JSON_MAX_DEPTH];
static int g_depth = 0;
static int g_first = 1;

static void
json_push(void)
{
  g_first_stack[g_depth++] = g_first;
  g_first = 1;
  g_indent++;
}

static void
json_pop(void)
{
  g_indent--;
  g_first = g_first_stack[--g_depth];
}

static void
json_indent(void)
{
  for (int i = 0; i < g_indent; i++)
    printf("  ");
}

static void
json_sep(void)
{
  if (!g_first)
    printf(",");
  printf("\n");
  g_first = 0;
}

static void
json_obj_open(const char *key)
{
  json_sep();
  json_indent();
  if (key)
    printf("\"%s\": {", key);
  else
    printf("{");
  json_push();
}

static void
json_obj_close(void)
{
  json_pop();
  printf("\n");
  json_indent();
  printf("}");
}

static void
json_arr_open(const char *key)
{
  json_sep();
  json_indent();
  if (key)
    printf("\"%s\": [", key);
  else
    printf("[");
  json_push();
}

static void
json_arr_close(void)
{
  json_pop();
  printf("\n");
  json_indent();
  printf("]");
}

/* Inline array open/close - no newlines between elements */
static void
json_arr_open_inline(const char *key)
{
  json_sep();
  json_indent();
  if (key)
    printf("\"%s\": [", key);
  else
    printf("[");
  /* We don't push here; caller manages comma manually */
}

static void
json_escape_str(const char *s)
{
  printf("\"");
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    switch (c) {
      case '"':  printf("\\\""); break;
      case '\\': printf("\\\\"); break;
      case '\b': printf("\\b"); break;
      case '\f': printf("\\f"); break;
      case '\n': printf("\\n"); break;
      case '\r': printf("\\r"); break;
      case '\t': printf("\\t"); break;
      default:
        if (c < 0x20) printf("\\u%04x", c);
        else printf("%c", c);
        break;
    }
  }
  printf("\"");
}

static void
json_format_float(float value)
{
  if (isnan(value) || isinf(value)) {
    printf("null");
    return;
  }
  if (g_decimal_places < 0) {
    printf("%g", (double)value);
  } else {
    printf("%.*f", g_decimal_places, (double)value);
  }
}

static void
json_str(const char *key, const char *val)
{
  json_sep();
  json_indent();
  printf("\"%s\": ", key);
  json_escape_str(val);
}

static void
json_int(const char *key, long long val)
{
  json_sep();
  json_indent();
  printf("\"%s\": %lld", key, val);
}

static void
json_uint(const char *key, unsigned long long val)
{
  json_sep();
  json_indent();
  printf("\"%s\": %llu", key, val);
}

static void
json_hex(const char *key, unsigned int val)
{
  json_sep();
  json_indent();
  char buf[16];
  snprintf(buf, sizeof(buf), "0x%08x", val);
  printf("\"%s\": \"%s\"", key, buf);
}

static void
json_float(const char *key, float val)
{
  json_sep();
  json_indent();
  printf("\"%s\": ", key);
  json_format_float(val);
}

static void
json_bool(const char *key, int val)
{
  json_sep();
  json_indent();
  printf("\"%s\": %s", key, val ? "true" : "false");
}

/* Emit an inline float array: [1.0, 2.0, ...] */
static void
json_float_arr(const char *key, const float *vals, int count)
{
  json_sep();
  json_indent();
  printf("\"%s\": [", key);
  for (int i = 0; i < count; i++) {
    if (i > 0) printf(", ");
    json_format_float(vals[i]);
  }
  printf("]");
}

/* Emit an inline int array */
static void
json_int_arr(const char *key, const int *vals, int count)
{
  json_sep();
  json_indent();
  printf("\"%s\": [", key);
  for (int i = 0; i < count; i++) {
    if (i > 0) printf(", ");
    printf("%d", vals[i]);
  }
  printf("]");
}

/* Emit an inline unsigned short array */
static void
json_ushort_arr_inline(const unsigned short *vals,
    int count)
{
  for (int i = 0; i < count; i++) {
    if (i > 0) printf(", ");
    printf("%u", vals[i]);
  }
}

static const char *
blend_mode_name(int mode)
{
  switch (mode) {
    case csmColorBlendType_Normal:             return "Normal";
    case csmColorBlendType_AddCompatible:      return "AddCompatible";
    case csmColorBlendType_MultiplyCompatible: return "MultiplyCompatible";
    case csmColorBlendType_Add:                return "Add";
    case csmColorBlendType_AddGlow:            return "AddGlow";
    case csmColorBlendType_Darken:             return "Darken";
    case csmColorBlendType_Multiply:           return "Multiply";
    case csmColorBlendType_ColorBurn:          return "ColorBurn";
    case csmColorBlendType_LinearBurn:         return "LinearBurn";
    case csmColorBlendType_Lighten:            return "Lighten";
    case csmColorBlendType_Screen:             return "Screen";
    case csmColorBlendType_ColorDodge:         return "ColorDodge";
    case csmColorBlendType_Overlay:            return "Overlay";
    case csmColorBlendType_SoftLight:          return "SoftLight";
    case csmColorBlendType_HardLight:          return "HardLight";
    case csmColorBlendType_LinearLight:        return "LinearLight";
    case csmColorBlendType_Hue:                return "Hue";
    case csmColorBlendType_Color:              return "Color";
    default:                                   return "Unknown";
  }
}

static const char *
moc_version_name(csmMocVersion ver)
{
  switch (ver) {
    case csmMocVersion_Unknown: return "Unknown";
    case csmMocVersion_30:      return "3.0";
    case csmMocVersion_33:      return "3.3";
    case csmMocVersion_40:      return "4.0";
    case csmMocVersion_42:      return "4.2";
    case csmMocVersion_50:      return "5.0";
    case csmMocVersion_53:      return "5.3";
    default:                    return "Unknown";
  }
}

static const char *
param_type_name(csmParameterType type)
{
  switch (type) {
    case csmParameterType_Normal:     return "Normal";
    case csmParameterType_BlendShape: return "BlendShape";
    default:                          return "Unknown";
  }
}


static void
json_const_flag_names(csmFlags flags)
{
  json_arr_open("constant_flag_names");
  if (flags & csmBlendAdditive) {
    json_sep();
    json_indent();
    json_escape_str("Additive");
  }
  if (flags & csmBlendMultiplicative) {
    json_sep();
    json_indent();
    json_escape_str("Multiplicative");
  }
  if (flags & csmIsDoubleSided) {
    json_sep();
    json_indent();
    json_escape_str("DoubleSided");
  }
  if (flags & csmIsInvertedMask) {
    json_sep();
    json_indent();
    json_escape_str("InvertedMask");
  }
  json_arr_close();
}

static void
json_dynamic_flag_names(csmFlags flags)
{
  json_arr_open("dynamic_flag_names");
  if (flags & csmIsVisible) {
    json_sep();
    json_indent();
    json_escape_str("Visible");
  }
  if (flags & csmVisibilityDidChange) {
    json_sep();
    json_indent();
    json_escape_str("VisibilityChanged");
  }
  if (flags & csmOpacityDidChange) {
    json_sep();
    json_indent();
    json_escape_str("OpacityChanged");
  }
  if (flags & csmDrawOrderDidChange) {
    json_sep();
    json_indent();
    json_escape_str("DrawOrderChanged");
  }
  if (flags & csmRenderOrderDidChange) {
    json_sep();
    json_indent();
    json_escape_str("RenderOrderChanged");
  }
  if (flags & csmVertexPositionsDidChange) {
    json_sep();
    json_indent();
    json_escape_str("VertexPosChanged");
  }
  if (flags & csmBlendColorDidChange) {
    json_sep();
    json_indent();
    json_escape_str("BlendColorChanged");
  }
  json_arr_close();
}


static void
log_handler(const char *message)
{
  fprintf(stderr, "[PurismCore] %s\n", message);
}

struct param_setting {
  char *pattern;
  char *value_str;
  float value;
  int is_pattern;
  int special;
};

#define SPECIAL_NUMERIC 0
#define SPECIAL_MIN     1
#define SPECIAL_MAX     2
#define SPECIAL_MID     3
#define SPECIAL_DEFAULT 4

static int
parse_special_value(const char *str, float *out_value)
{
  if (strcmp(str, "min") == 0) return SPECIAL_MIN;
  if (strcmp(str, "max") == 0) return SPECIAL_MAX;
  if (strcmp(str, "mid") == 0) return SPECIAL_MID;
  if (strcmp(str, "default") == 0) return SPECIAL_DEFAULT;
  *out_value = (float)atof(str);
  return SPECIAL_NUMERIC;
}


static void
print_usage(const char *prog)
{
  fprintf(stderr,
      "Usage: %s [-p pattern=value ...]"
      " [-P decimals] [-f] <file.moc3>\n", prog);
  fprintf(stderr, "\nOptions:\n");
  fprintf(stderr,
      "  -p pattern=value  Set parameter value"
      " before update\n");
  fprintf(stderr,
      "                    Pattern supports"
      " * and ? wildcards (case-insensitive)\n");
  fprintf(stderr,
      "                    Value can be:"
      " number, min, max, mid, default\n");
  fprintf(stderr,
      "  -P decimals       Number of decimal"
      " places for floats (default: auto)\n");
  fprintf(stderr,
      "  -f                Full output"
      " (don't collapse large arrays)\n");
  fprintf(stderr, "  -h                Show this help\n");
  fprintf(stderr, "\nExamples:\n");
  fprintf(stderr,
      "  %s -p '*AngleX*=30' model.moc3\n", prog);
  fprintf(stderr,
      "  %s -p 'ParamEyeLOpen=0'"
      " -p 'ParamEyeROpen=0' model.moc3\n", prog);
  fprintf(stderr,
      "  %s -p '*Angle*=max' model.moc3\n", prog);
}


int
main(int argc, char **argv)
{
  const char *moc_path = NULL;
  struct param_setting *param_settings = NULL;
  int param_count = 0;
  int param_capacity = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 ||
        strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-p") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: -p requires an argument\n");
        return 1;
      }
      i++;
      char *arg = argv[i];
      char *eq = strchr(arg, '=');
      if (!eq) {
        fprintf(stderr,
            "Error: Invalid parameter format"
            " '%s' (expected pattern=value)\n", arg);
        return 1;
      }

      if (param_count >= param_capacity) {
        param_capacity = param_capacity ? param_capacity * 2 : 8;
        param_settings = realloc(param_settings,
          sizeof(struct param_setting) * param_capacity);
      }

      *eq = '\0';
      param_settings[param_count].pattern = arg;
      param_settings[param_count].value_str = eq + 1;
      param_settings[param_count].is_pattern =
          (strchr(arg, '*') != NULL ||
          strchr(arg, '?') != NULL);
      param_settings[param_count].special =
          parse_special_value(eq + 1,
          &param_settings[param_count].value);
      param_count++;
    } else if (strcmp(argv[i], "-P") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: -P requires an argument\n");
        return 1;
      }
      i++;
      g_decimal_places = atoi(argv[i]);
      if (g_decimal_places < 0 || g_decimal_places > 15) {
        fprintf(stderr, "Error: -P must be 0-15\n");
        return 1;
      }
    } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--full") == 0) {
      g_full_output = 1;
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
      return 1;
    } else {
      if (moc_path) {
        fprintf(stderr, "Error: Multiple input files specified\n");
        return 1;
      }
      moc_path = argv[i];
    }
  }

  if (!moc_path) {
    print_usage(argv[0]);
    return 1;
  }

  csmSetLogFunction(log_handler);

  csmVersion version = csmGetVersion();

  size_t moc_size;
  void *moc_data = psm_read_file(moc_path, &moc_size, csmAlignofMoc, 0);
  if (!moc_data) {
    return 1;
  }

  csmMocVersion moc_ver = csmGetMocVersion(moc_data,
      (unsigned int)moc_size);
  int consistent = csmHasMocConsistency(moc_data,
      (unsigned int)moc_size);
  if (!consistent) {
    fprintf(stderr,
        "Error: MOC3 file failed consistency check\n");
    psm_aligned_free(moc_data);
    return 1;
  }

  csmMoc *moc = csmReviveMocInPlace(moc_data, (unsigned int)moc_size);
  if (!moc) {
    fprintf(stderr, "Error: Failed to revive MOC3\n");
    psm_aligned_free(moc_data);
    return 1;
  }

  unsigned int model_size = csmGetSizeofModel(moc);
  void *model_memory = psm_aligned_alloc(csmAlignofModel, model_size);
  if (!model_memory) {
    fprintf(stderr, "Error: Cannot allocate %u bytes for model\n", model_size);
    psm_aligned_free(moc_data);
    return 1;
  }

  csmModel *model = csmInitializeModelInPlace(moc, model_memory, model_size);
  if (!model) {
    fprintf(stderr, "Error: Failed to initialize model\n");
    psm_aligned_free(model_memory);
    psm_aligned_free(moc_data);
    return 1;
  }

  /* Apply parameter settings (log to stderr) */
  if (param_count > 0) {
    int model_param_count = csmGetParameterCount(model);
    const char** param_ids = csmGetParameterIds(model);
    float *param_values = csmGetParameterValues(model);
    const float *param_mins = csmGetParameterMinimumValues(model);
    const float *param_maxs = csmGetParameterMaximumValues(model);
    const float *param_defaults =
        csmGetParameterDefaultValues(model);

    for (int i = 0; i < param_count; i++) {
      int match_count = 0;
      for (int j = 0; j < model_param_count; j++) {
        int matches = 0;
        if (param_settings[i].is_pattern) {
          matches = psm_glob_match(
              param_settings[i].pattern,
              param_ids[j]);
        } else {
          matches = (strcmp(
              param_settings[i].pattern,
              param_ids[j]) == 0);
        }

        if (matches) {
          float value;
          switch (param_settings[i].special) {
            case SPECIAL_MIN: value = param_mins[j]; break;
            case SPECIAL_MAX: value = param_maxs[j]; break;
            case SPECIAL_MID:
              value = (param_mins[j] + param_maxs[j]) / 2.0f;
              break;
            case SPECIAL_DEFAULT: value = param_defaults[j]; break;
            default:
              value = param_settings[i].value;
              if (value < param_mins[j]) value = param_mins[j];
              if (value > param_maxs[j]) value = param_maxs[j];
              break;
          }
          fprintf(stderr, "Setting %s = %g (was %g)\n",
              param_ids[j], (double)value,
              (double)param_values[j]);
          param_values[j] = value;
          match_count++;
        }
      }
      if (match_count == 0 && !param_settings[i].is_pattern) {
        fprintf(stderr, "Warning: Parameter '%s' not found\n",
            param_settings[i].pattern);
      }
    }
  }

  csmUpdateModel(model);


  /* Root object */
  printf("{");
  json_push();

  /* version */
  {
    char ver_str[32];
    snprintf(ver_str, sizeof(ver_str), "%u.%u.%u",
             (version >> 24) & 0xff, (version >> 16) & 0xff, version & 0xffff);
    json_obj_open("version");
    json_str("purism_core", ver_str);
    json_hex("purism_core_hex", version);
    json_hex("true_version", csmGetTrueVersion());
    json_str("latest_moc",
        moc_version_name(csmGetLatestMocVersion()));
    json_obj_close();
  }

  /* file */
  {
    json_obj_open("file");
    json_str("path", moc_path);
    json_uint("size", (unsigned long long)moc_size);
    json_str("moc_version", moc_version_name(moc_ver));
    json_bool("consistent", consistent);
    json_obj_close();
  }

  /* model_size */
  json_uint("model_size", model_size);

  /* canvas */
  {
    csmVector2 canvas_size, canvas_origin;
    float pixels_per_unit;
    csmReadCanvasInfo(model, &canvas_size, &canvas_origin, &pixels_per_unit);

    json_obj_open("canvas");
    json_float("width", canvas_size.X);
    json_float("height", canvas_size.Y);
    {
      float origin[2] = { canvas_origin.X, canvas_origin.Y };
      json_float_arr("origin", origin, 2);
    }
    json_float("pixels_per_unit", pixels_per_unit);
    json_obj_close();
  }

  /* parameters */
  {
    int num_params = csmGetParameterCount(model);
    const char** param_ids = csmGetParameterIds(model);
    const csmParameterType *param_types =
        csmGetParameterTypes(model);
    const float *param_mins =
        csmGetParameterMinimumValues(model);
    const float *param_maxs =
        csmGetParameterMaximumValues(model);
    const float *param_defaults =
        csmGetParameterDefaultValues(model);
    const float *param_values =
        csmGetParameterValues(model);
    const int *param_repeats = csmGetParameterRepeats(model);
    const int *param_key_counts =
        csmGetParameterKeyCounts(model);
    const float **param_key_values =
        csmGetParameterKeyValues(model);

    json_arr_open("parameters");
    for (int i = 0; i < num_params; i++) {
      json_obj_open(NULL);
      json_int("index", i);
      json_str("id", param_ids[i]);
      json_str("type", param_type_name(param_types[i]));
      json_float("min", param_mins[i]);
      json_float("max", param_maxs[i]);
      json_float("default", param_defaults[i]);
      json_float("value", param_values[i]);
      json_bool("repeat",
          param_repeats ? param_repeats[i] : 0);
      if (param_key_counts && param_key_values) {
        json_int("key_count", param_key_counts[i]);
        if (param_key_counts[i] > 0 &&
          param_key_values[i]) {
          json_float_arr("keys",
              param_key_values[i],
              param_key_counts[i]);
        } else {
          json_sep();
          json_indent();
          printf("\"keys\": []");
        }
      }
      json_obj_close();
    }
    json_arr_close();
  }

  /* parts */
  {
    int num_parts = csmGetPartCount(model);
    const char** part_ids = csmGetPartIds(model);
    const float *part_opacities =
        csmGetPartOpacities(model);
    const int *part_parents =
        csmGetPartParentPartIndices(model);
    const int *part_offscreens =
        csmGetPartOffscreenIndices(model);

    json_arr_open("parts");
    for (int i = 0; i < num_parts; i++) {
      json_obj_open(NULL);
      json_int("index", i);
      json_str("id", part_ids[i]);
      json_float("opacity", part_opacities[i]);
      json_int("parent",
          part_parents ? part_parents[i] : -1);
      json_int("offscreen",
          part_offscreens ? part_offscreens[i] : -1);
      json_obj_close();
    }
    json_arr_close();
  }

  /* drawables */
  {
    int num_drawables = csmGetDrawableCount(model);
    const char** drawable_ids = csmGetDrawableIds(model);
    const csmFlags *const_flags =
        csmGetDrawableConstantFlags(model);
    const csmFlags *dyn_flags =
        csmGetDrawableDynamicFlags(model);
    const int *blend_modes =
        csmGetDrawableBlendModes(model);
    const int *tex_indices =
        csmGetDrawableTextureIndices(model);
    const int *draw_orders =
        csmGetDrawableDrawOrders(model);
    const int *render_orders = csmGetRenderOrders(model);
    const float *opacities =
        csmGetDrawableOpacities(model);
    const int *mask_counts =
        csmGetDrawableMaskCounts(model);
    const int** masks = csmGetDrawableMasks(model);
    const int *vertex_counts =
        csmGetDrawableVertexCounts(model);
    const csmVector2 **vertex_positions =
        csmGetDrawableVertexPositions(model);
    const csmVector2 **vertex_uvs =
        csmGetDrawableVertexUvs(model);
    const int *index_counts =
        csmGetDrawableIndexCounts(model);
    const unsigned short **indices =
        csmGetDrawableIndices(model);
    const csmVector4 *multiply_colors =
        csmGetDrawableMultiplyColors(model);
    const csmVector4 *screen_colors =
        csmGetDrawableScreenColors(model);
    const int *parent_parts =
        csmGetDrawableParentPartIndices(model);

    json_arr_open("drawables");
    for (int i = 0; i < num_drawables; i++) {
      json_obj_open(NULL);
      json_int("index", i);
      json_str("id", drawable_ids[i]);
      json_int("constant_flags", const_flags[i]);
      json_const_flag_names(const_flags[i]);
      json_int("dynamic_flags", dyn_flags[i]);
      json_dynamic_flag_names(dyn_flags[i]);
      json_str("blend_mode",
          blend_mode_name(blend_modes[i]));
      json_int("blend_mode_value",
          blend_modes[i]);
      json_int("texture_index",
          tex_indices[i]);
      json_int("draw_order", draw_orders[i]);
      json_int("render_order",
          render_orders[i]);
      json_float("opacity", opacities[i]);

      /* masks */
      if (mask_counts[i] > 0 && masks[i]) {
        json_int_arr("masks", masks[i],
            mask_counts[i]);
      } else {
        json_sep();
        json_indent();
        printf("\"masks\": []");
      }

      json_int("vertex_count",
          vertex_counts[i]);
      json_int("index_count",
          index_counts[i]);

      /* multiply_color */
      if (multiply_colors) {
        float mc[4] = {
            multiply_colors[i].X,
            multiply_colors[i].Y,
            multiply_colors[i].Z,
            multiply_colors[i].W };
        json_float_arr("multiply_color",
            mc, 4);
      }
      /* screen_color */
      if (screen_colors) {
        float sc[4] = {
            screen_colors[i].X,
            screen_colors[i].Y,
            screen_colors[i].Z,
            screen_colors[i].W };
        json_float_arr("screen_color",
            sc, 4);
      }

      if (parent_parts) {
        json_int("parent_part", parent_parts[i]);
      }

      /* vertices */
      if (vertex_positions[i] && vertex_counts[i] > 0) {
        int vc = vertex_counts[i];
        int show_all = g_full_output || vc <= 10;

        if (show_all) {
          json_arr_open("vertices");
          for (int v = 0; v < vc; v++) {
            json_obj_open(NULL);
            {
              float pos[2] = {
                  vertex_positions[i][v].X,
                  vertex_positions[i][v].Y };
              json_float_arr("pos", pos, 2);
            }
            if (vertex_uvs[i]) {
              float uv[2] = {
                  vertex_uvs[i][v].X,
                  vertex_uvs[i][v].Y };
              json_float_arr("uv", uv, 2);
            }
            json_obj_close();
          }
          json_arr_close();
        } else {
          /* Truncated: emit as object with metadata */
          int show_count = 10; /* 5 from start + 5 from end */
          json_obj_open("vertices");
          json_bool("truncated", 1);
          json_int("count", vc);
          json_int("shown", show_count);
          json_arr_open("data");
          for (int v = 0; v < 5; v++) {
            json_obj_open(NULL);
            {
              float pos[2] = {
                  vertex_positions[i][v].X,
                  vertex_positions[i][v].Y };
              json_float_arr("pos", pos, 2);
            }
            if (vertex_uvs[i]) {
              float uv[2] = {
                  vertex_uvs[i][v].X,
                  vertex_uvs[i][v].Y };
              json_float_arr("uv", uv, 2);
            }
            json_obj_close();
          }
          for (int v = vc - 5; v < vc; v++) {
            json_obj_open(NULL);
            {
              float pos[2] = {
                  vertex_positions[i][v].X,
                  vertex_positions[i][v].Y };
              json_float_arr("pos", pos, 2);
            }
            if (vertex_uvs[i]) {
              float uv[2] = {
                  vertex_uvs[i][v].X,
                  vertex_uvs[i][v].Y };
              json_float_arr("uv", uv, 2);
            }
            json_obj_close();
          }
          json_arr_close();
          json_obj_close();
        }
      }

      /* indices */
      if (indices[i] && index_counts[i] > 0) {
        int ic = index_counts[i];
        int show_all = g_full_output || ic <= 12;

        if (show_all) {
          json_arr_open_inline("indices");
          json_ushort_arr_inline(indices[i], ic);
          printf("]");
        } else {
          /* Truncated: emit as object */
          int show_count = 12; /* 6 from start + 6 from end */
          json_obj_open("indices");
          json_bool("truncated", 1);
          json_int("count", ic);
          json_int("shown", show_count);
          json_arr_open_inline("data");
          json_ushort_arr_inline(indices[i], 6);
          printf(", ");
          json_ushort_arr_inline(indices[i] + (ic - 6), 6);
          printf("]");
          /* Need to fix g_first state since we used inline */
          json_obj_close();
        }
      }

      json_obj_close();
    }
    json_arr_close();
  }

  /* offscreens */
  {
    int num_offscreens = csmGetOffscreenCount(model);

    json_arr_open("offscreens");
    if (num_offscreens > 0) {
      const int *offscreen_blend_modes =
          csmGetOffscreenBlendModes(model);
      const float *offscreen_opacities =
          csmGetOffscreenOpacities(model);
      const int *offscreen_owners =
          csmGetOffscreenOwnerIndices(model);
      const csmVector4 *offscreen_multiply =
          csmGetOffscreenMultiplyColors(model);
      const csmVector4 *offscreen_screen =
          csmGetOffscreenScreenColors(model);
      const int *offscreen_mask_counts =
          csmGetOffscreenMaskCounts(model);
      const int **offscreen_masks =
          csmGetOffscreenMasks(model);
      const csmFlags *offscreen_flags =
          csmGetOffscreenConstantFlags(model);

      for (int i = 0; i < num_offscreens; i++) {
        json_obj_open(NULL);
        json_int("index", i);
        if (offscreen_blend_modes) {
          json_str("blend_mode",
              blend_mode_name(
              offscreen_blend_modes[i]));
          json_int("blend_mode_value",
              offscreen_blend_modes[i]);
        }
        if (offscreen_opacities) {
          json_float("opacity",
              offscreen_opacities[i]);
        }
        if (offscreen_owners) {
          json_int("owner", offscreen_owners[i]);
        }
        if (offscreen_multiply) {
          float mc[4] = {
              offscreen_multiply[i].X,
              offscreen_multiply[i].Y,
              offscreen_multiply[i].Z,
              offscreen_multiply[i].W };
          json_float_arr("multiply_color",
              mc, 4);
        }
        if (offscreen_screen) {
          float sc[4] = {
              offscreen_screen[i].X,
              offscreen_screen[i].Y,
              offscreen_screen[i].Z,
              offscreen_screen[i].W };
          json_float_arr("screen_color",
              sc, 4);
        }
        if (offscreen_mask_counts &&
            offscreen_masks &&
            offscreen_mask_counts[i] > 0) {
          json_int_arr("masks",
              offscreen_masks[i],
              offscreen_mask_counts[i]);
        } else {
          json_sep();
          json_indent();
          printf("\"masks\": []");
        }
        if (offscreen_flags) {
          json_int("flags", offscreen_flags[i]);
        }
        json_obj_close();
      }
    }
    json_arr_close();
  }

  /* Close root */
  json_pop();
  printf("\n}\n");

  psm_aligned_free(model_memory);
  psm_aligned_free(moc_data);
  free(param_settings);

  return 0;
}
