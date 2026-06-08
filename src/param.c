/*
 * Purism Core: parameter handling
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <string.h>
#include "private.h"
#include "array.h"
#include "debug.h"
#include "math2.h"
#include "moc3.h"
#include "model.h"
#include "param.h"

struct psm__key_search_result {
  psm__i32 index;   /* Key index (lower bound of segment) */
  psm__f32 weight;  /* Interpolation weight within segment [0,1] */
  bool is_outside;  /* Value is outside key range */
  bool needs_check; /* Need to check previous out_of_range state */
};

static struct psm__key_search_result
psm__find_key_segment(psm__f32 value, const psm__f32 *keys, psm__i32 key_count,
                      psm__f32 snap_eps, psm__f32 interp_epsilon)
{
  struct psm__key_search_result r = {0, 0.0f, false, false};

  if (key_count <= 0) {
    r.needs_check = true;
    return r;
  }

  if (key_count == 1) {
    psm__f32 key0 = keys[0];
    r.is_outside = (value <= key0 - snap_eps) || (value >= key0 + snap_eps);
    r.needs_check = !r.is_outside;
    return r;
  }

  /* key_count >= 2 */
  psm__f32 key0 = keys[0], key1;

  /* Before first key */
  if (value < key0 - snap_eps) {
    r.is_outside = true;
    return r;
  }

  /* Snapped to first key */
  if (value < key0 + snap_eps) {
    r.needs_check = true;
    return r;
  }

  /* Check second key */
  key1 = keys[1];
  if (value < key1 - snap_eps) {
    /* Between first and second key */
    psm__f32 key_diff = key1 - key0;
    if (key_diff >= interp_epsilon)
      r.weight = (value - key0) / key_diff;
    return r;
  }
  if (value < key1 + snap_eps) {
    /* Snapped to second key */
    r.index = 1;
    r.needs_check = true;
    return r;
  }

  /* Search remaining keys */
  for (psm__i32 k = 2; k < key_count; k++) {
    key0 = key1;
    key1 = keys[k];
    if (value < key1 - snap_eps) {
      /* Between key0 and key1 */
      r.index = k - 1;
      psm__f32 key_diff = key1 - key0;
      if (key_diff >= interp_epsilon)
        r.weight = (value - key0) / key_diff;
      return r;
    }
    if (value < key1 + snap_eps) {
      /* Snapped to this key */
      r.index = k;
      r.needs_check = true;
      return r;
    }
  }

  /* After last key */
  r.index = key_count - 1;
  r.is_outside = true;
  return r;
}

PSM__DEF void
psm__resolve_params(struct psm__params *parameters)
{
  psm__i32 count = parameters->count;
  if (count <= 0)
    return;

  struct psm__param *params = parameters->items;
  psm__f32 *input_value = parameters->input_value;

  for (psm__i32 i = 0; i < count; i++) {
    psm__f32 userland_value = input_value[i];
    psm__f32 new_value;

    if (params[i].repeat) {
      psm__f32 range_min = params[i].range[0],
                    range_length = params[i].range_length;

      psm__f32 normalized = (userland_value - range_min) / range_length;
      psm__f32 wrapped = normalized - floorf(normalized);
      new_value = wrapped * range_length + range_min;

      if (params[i].value != new_value) {
        params[i].value = new_value;
        params[i].dirty = 1;
      } else {
        params[i].dirty = 0;
      }
    } else {
      psm__f32 range_min = params[i].range[0], range_max = params[i].range[1];
      new_value = psm__clamp_f32(userland_value, range_min, range_max);

      if (params[i].value != new_value) {
        params[i].value = new_value;
        params[i].dirty = 1;
      } else {
        params[i].dirty = 0;
      }

      input_value[i] = new_value;
    }
  }
}

PSM__DEF void
psm__resolve_key_tables(struct psm__model *m)
{
  psm__i32 param_count = m->params.count;
  struct psm__param *param_items = m->params.items;
  if (!param_items || param_count <= 0)
    return;

  for (psm__i32 i = 0; i < param_count; i++) {
    struct psm__param *param = &param_items[i];

    if (param->type != csmParameterType_Normal)
      continue;
    if (!param->dirty && !m->force_update) {
      /*
       * Unchanged parameter: clear dirty flags on all its bindings
       * so downstream keyform updates don't re-evaluate them.
       */
      psm__i32 bc = param->key_table_len;
      struct psm__key_table *bs = param->key_tables;
      if (bs) {
        for (psm__i32 j = 0; j < bc; j++) {
          bs[j].idx_dirty = 0;
          bs[j].weight_dirty = 0;
        }
      }
      continue;
    }

    psm__i32 binding_count = param->key_table_len;
    struct psm__key_table *bindings = param->key_tables;
    if (!bindings || binding_count <= 0)
      continue;
    psm__f32 value = param->value;
    psm__f32 snap_eps = param->snap_eps, interp_eps = param->interp_eps;

    for (psm__i32 j = 0; j < binding_count; j++) {
      struct psm__key_table *binding = &bindings[j];
      if (!binding->keys || binding->key_count <= 0)
        continue;

      struct psm__key_search_result r = psm__find_key_segment(value,
          binding->keys, binding->key_count, snap_eps, interp_eps);

      bool idx_dirty, weight_dirty;
      if (!r.is_outside && binding->out_of_range) {
        /* Transition from outside to inside: force dirty and clear outside */
        r.is_outside = false;
        idx_dirty = 1;
        weight_dirty = 1;
      } else {
        /* Normal case (including outside→outside): check value changes */
        idx_dirty = (binding->idx != r.index);
        weight_dirty = (binding->weight != r.weight);
        if (weight_dirty)
          idx_dirty = r.weight == 0.0f || binding->weight == 0.0f || idx_dirty;
      }

      binding->idx_dirty = idx_dirty;
      binding->weight_dirty = weight_dirty;
      binding->idx = r.index;
      binding->weight = r.weight;
      binding->out_of_range = r.is_outside;
    }
  }
}

PSM__DEF void
psm__resolve_blend_key_tables(struct psm__model *m)
{
  psm__u8 version = m->source->header->version;
  if (version < csmMocVersion_42)
    return;

  psm__i32 param_count = m->params.count;
  if (param_count <= 0)
    return;

  struct psm__param *params = m->params.items;
  if (!params)
    return;
  bool force_update = m->force_update;

  for (psm__i32 param_i = 0; param_i < param_count; param_i++) {
    if (params[param_i].type != csmParameterType_BlendShape)
      continue;

    psm__i32 bs_count = params[param_i].blend_key_table_len;
    if (bs_count <= 0)
      continue;

    struct psm__blend_key_table *blend_key_tables = params[param_i].blend_key_tables;
    if (!blend_key_tables)
      continue;
    psm__f32 value = params[param_i].value;

    if (force_update || params[param_i].dirty) {
      for (psm__i32 bs_i = 0; bs_i < bs_count; bs_i++) {
        psm__i32 key_count = blend_key_tables[bs_i].key_count;
        psm__u32 index = 0;
        psm__f32 weight = 0.0f;

        if (key_count >= 2) {
          psm__f32 *keys = blend_key_tables[bs_i].keys;
          if (keys && value > keys[0]) {
            /* Find upper bound: first key > value */
            for (index = 1;
                index < (psm__u32)key_count && value >= keys[index]; index++) {}
            index--;
            if (index < (psm__u32)key_count - 1)
              weight = (value - keys[index]) / (keys[index + 1] - keys[index]);
          }
        }

        psm__u32 old_index = blend_key_tables[bs_i].idx;
        psm__f32 old_weight = blend_key_tables[bs_i].weight;
        bool idx_dirty = (old_index != index),
                    weight_dirty = (old_weight != weight);
        if (weight_dirty)
          idx_dirty = weight == 0.0f ||
              old_weight == 0.0f || old_index != index;

        blend_key_tables[bs_i].idx_dirty = idx_dirty;
        blend_key_tables[bs_i].weight_dirty = weight_dirty;
        blend_key_tables[bs_i].weight = weight;
        blend_key_tables[bs_i].idx = index;
      }
    } else {
      for (psm__i32 bs_i = 0; bs_i < bs_count; bs_i++) {
        blend_key_tables[bs_i].idx_dirty = 0;
        blend_key_tables[bs_i].weight_dirty = 0;
      }
    }
  }
}

PSM__DEF void
psm__resolve_bindings(struct psm__model *m)
{
  psm__i32 count = m->bindings.count;
  if (count <= 0)
    return;

  struct psm__binding *binds = m->bindings.items;
  if (!binds)
    return;
  bool force_update = m->force_update;

  struct psm__key_table *kt_base = m->key_tables.items;
  struct psm__key_table *kt_end = kt_base + m->key_tables.count;

  for (psm__i32 bi = 0; bi < count; bi++) {
    psm__i32 binding_count = binds[bi].key_table_len;
    struct psm__key_table **bindings = binds[bi].key_tables;

    bool idx_dirty = false;
    bool weight_dirty = false;
    bool out_of_range = false;
    psm__u32 active_binding_count = 0;

    /* Skip if no bindings */
    if (!bindings) {
      binds[bi].idx_dirty = 0;
      binds[bi].weight_dirty = 0;
      binds[bi].out_of_range = true;
      continue;
    }

    for (psm__i32 i = 0; i < binding_count; i++) {
      struct psm__key_table *binding = bindings[i];

      /* Validate pointer is within expected range */
      if (binding < kt_base || binding >= kt_end) {
        out_of_range = true; /* corrupted ptr */
        break;
      }

      if (binding->out_of_range) {
        out_of_range = true;
        idx_dirty = 0;
        weight_dirty = 0;
        break;
      }

      weight_dirty |= binding->weight_dirty;
      idx_dirty |= binding->idx_dirty;

      if (binding->weight != 0.0f) {
        active_binding_count++;
      }
    }

    if (out_of_range) {
      binds[bi].idx_dirty = 0;
      binds[bi].weight_dirty = 0;
      binds[bi].out_of_range = true;
      continue;
    }

    if (force_update) {
      idx_dirty = 1;
      weight_dirty = 1;
    }

    if (!(idx_dirty || weight_dirty)) {
      binds[bi].idx_dirty = 0;
      binds[bi].weight_dirty = 0;
      binds[bi].out_of_range = false;
      continue;
    }

    psm__u32 blend_count = 1u << active_binding_count;
    binds[bi].blend_count = blend_count;

    if (active_binding_count == 31) {
      binds[bi].idx_dirty = idx_dirty;
      binds[bi].weight_dirty = weight_dirty;
      binds[bi].out_of_range = false;
      continue;
    }

    /* Skip if keyform_idx or weights arrays are missing */
    if (!binds[bi].keyform_idx || !binds[bi].weights) {
      binds[bi].idx_dirty = 0;
      binds[bi].weight_dirty = 0;
      binds[bi].out_of_range = true;
      continue;
    }

    memset(binds[bi].keyform_idx, 0, blend_count * sizeof(psm__i32));
    for (psm__i32 j = 0; j < (psm__i32)blend_count; j++) {
      binds[bi].weights[j] = 1.0f;
    }

    /*
     * Combo builder. Produces all 2^N keyform
     * index + weight combinations from N key tables.
     * index_stride tracks the key counts.
     * combo_stride tracks which bit selects upper vs lower
     * keyform for each active key table.
     */
    psm__u32 index_stride = 1, combo_stride = 1;

    for (psm__i32 i = 0; i < binding_count; i++) {
      struct psm__key_table *binding = bindings[i];
      psm__i32 index = binding->idx;
      psm__i32 key_count = binding->key_count;
      psm__f32 weight = binding->weight;
      psm__i32 index_offset = index * index_stride;

      if (weight != 0.0f) {
        psm__i32 next_index_offset = (index + 1) * index_stride;
        psm__f32 inv_weight = 1.0f - weight;

        for (psm__i32 j = 0; j < (psm__i32)blend_count; j++) {
          if ((j & combo_stride) == 0) {
            binds[bi].keyform_idx[j] += index_offset;
            binds[bi].weights[j] *= inv_weight;
          } else {
            binds[bi].keyform_idx[j] += next_index_offset;
            binds[bi].weights[j] *= weight;
          }
        }

        combo_stride *= 2;
      } else {
        for (psm__i32 j = 0; j < (psm__i32)blend_count; j++) {
          binds[bi].keyform_idx[j] += index_offset;
        }
      }

      index_stride *= key_count;
    }

    binds[bi].idx_dirty = idx_dirty;
    binds[bi].weight_dirty = weight_dirty;
    binds[bi].out_of_range = false;
  }
}

PSM__DEF void
psm__resolve_blend_bindings(struct psm__model *m)
{
  psm__u8 version = m->source->header->version;
  if (version < csmMocVersion_42)
    return;

  psm__i32 count = m->blend_bindings.count;
  if (count <= 0)
    return;

  struct psm__blend_binding *binds = m->blend_bindings.items;
  if (!binds)
    return;
  bool force_update = m->force_update;

  for (psm__i32 bi = 0; bi < count; bi++) {
    struct psm__blend_key_table *binding = binds[bi].key_table;
    if (!binding)
      continue;

    bool weight_dirty = force_update || binding->weight_dirty;
    bool idx_dirty = force_update || binding->idx_dirty;

    if (weight_dirty || idx_dirty) {
      psm__f32 weight = binding->weight;
      psm__i32 base_key_idx = binding->base_key_idx;
      psm__i32 next_key_index = binding->idx;

      /* Handle special case: weight != 0 and at base key */
      if (weight != 0.0f && next_key_index == base_key_idx) {
        idx_dirty = 1;
        weight_dirty = 1;
        binds[bi].blend_count = 1;
        binds[bi].weights[0] = weight;
        binds[bi].weights[1] = 1.0f - weight;
        next_key_index++;
        binds[bi].keyform_idx[0] = next_key_index;
        binds[bi].keyform_idx[1] = next_key_index + 1;
      } else {
        /* Set combination count */
        if (weight == 0.0f)
          binds[bi].blend_count = next_key_index != base_key_idx;
        else
          binds[bi].blend_count =
              (next_key_index + 1 == base_key_idx) ? 1 : 2;

        /* Update weights if weight changed */
        if (weight_dirty) {
          binds[bi].weights[0] = 1.0f - weight;
          binds[bi].weights[1] = weight;
        }

        /* Update indices if index changed */
        if (idx_dirty) {
          binds[bi].keyform_idx[0] = next_key_index;
          binds[bi].keyform_idx[1] = next_key_index + 1;
        }
      }
    }

    /* Process constraints */
    psm__i32 constraint_count = binds[bi].constraint_count;
    psm__f32 weight = 1.0f;

    for (psm__i32 i = 0; i < constraint_count; i++) {
      struct psm__blend_constraint *constraint = binds[bi].constraints[i];
      struct psm__param *param = constraint->param;
      psm__f32 constraint_weight = 1.0f;

      if (param != NULL && (force_update || param->dirty)) {
        psm__i32 key_count = constraint->count;
        psm__f32 *keys = constraint->keys, *weights = constraint->weights;

        if (key_count >= 2) {
          psm__f32 value = param->value;
          if (value > keys[0]) {
            /* Find upper bound */
            psm__i32 idx;
            for (idx = 1; idx < key_count && value >= keys[idx]; idx++) {}
            idx--;
            if (idx < key_count - 1) {
              psm__f32 t = (value - keys[idx]) / (keys[idx + 1] - keys[idx]);
              constraint_weight = weights[idx] * (1.0f - t) +
                  weights[idx + 1] * t;
            } else {
              constraint_weight = weights[key_count - 1];
            }
          } else {
            constraint_weight = weights[0];
          }
        } else if (key_count == 1) {
          constraint_weight = weights[0];
        }
        constraint->weight = constraint_weight;
      } else if (param != NULL) {
        constraint_weight = constraint->weight;
      } else {
        constraint->weight = constraint_weight;
      }

      weight = fminf(weight, constraint_weight);
    }

    binds[bi].weight = weight;
    binds[bi].idx_dirty = idx_dirty;
    binds[bi].weight_dirty = weight_dirty;
  }
}

PSMDEF int
csmGetParameterCount(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->params.count;
}

PSMDEF const char **
csmGetParameterIds(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->param_src.id_runtime;
}

PSMDEF const csmParameterType *
csmGetParameterTypes(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const csmParameterType *)m->params.type;
}

PSMDEF const float *
csmGetParameterMinimumValues(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->param_src.minimum_value;
}

PSMDEF const float *
csmGetParameterMaximumValues(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->param_src.maximum_value;
}

PSMDEF const float *
csmGetParameterDefaultValues(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->source->sections->param_src.default_value;
}

PSMDEF float *
csmGetParameterValues(csmModel *model)
{
  struct psm__model *m = (struct psm__model *)model;
  return m->params.input_value;
}

#if PSM_COMPAT_VERSION >= 0x06000000L
PSMDEF const int *
csmGetParameterRepeats(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const int *)m->source->sections->param_src.repeat;
}
#endif

PSMDEF const int *
csmGetParameterKeyCounts(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return m->param_keys.key_counts;
}

PSMDEF const float **
csmGetParameterKeyValues(const csmModel *model)
{
  const struct psm__model *m = (const struct psm__model *)model;
  return (const float **)m->param_keys.keys;
}
