/*
 * Purism Core: model runtime structures
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__MODEL_H
#define PSM__MODEL_H

#include "private.h"
#include "moc3.h"

struct psm__color3 {
  psm__f32 *r, *g, *b;
};

struct psm__model;

struct psm__interp {
  psm__i32 object_count;
  psm__i32 *max_blend;
  psm__i32 tmp_len;
  psm__f32 *tmp;
  psm__i32 *blend_count;
  psm__f32 *weights;
};

struct psm__axis {
  psm__i32 key_count;
  psm__f32 *keys;
  psm__i32 idx;
  psm__f32 weight;
  bool out_of_range;
  bool idx_dirty;
  bool weight_dirty;
};

struct psm__blend_axis {
  psm__i32 key_count;
  psm__f32 *keys;
  psm__i32 base_key_idx;
  psm__i32 idx;
  psm__f32 weight;
  bool idx_dirty;
  bool weight_dirty;
};

struct psm__binding {
  struct psm__axis **axes;
  psm__i32 axis_count;
  psm__i32 max_blend;
  psm__i32 blend_count;
  psm__i32 *keyform_idx;
  psm__f32 *weights;
  bool idx_dirty;
  bool weight_dirty;
  bool out_of_range;
};

struct psm__blend_constraint {
  struct psm__param *param;
  psm__f32 *keys;
  psm__f32 *weights;
  psm__i32 count;
  psm__f32 weight;
};

struct psm__blend_binding {
  struct psm__blend_axis *axis;
  psm__i32 key_src_offset;
  psm__i32 blend_count;
  psm__i32 keyform_idx[2];
  psm__f32 weights[2];
  bool idx_dirty;
  bool weight_dirty;
  psm__i32 constraint_count;
  struct psm__blend_constraint **constraints;
  psm__f32 weight;
};

struct psm__part {
  struct psm__binding *binding;
  psm__i32 parent_part_idx;
  bool local_enable;
};

struct psm__part_keydata {
  struct psm__interp interp;
  psm__f32 *draw_order;
};

struct psm__parts {
  psm__i32 count;
  struct psm__part *items;
  struct psm__part_keydata keydata;
  bool *enable;
  psm__i32 *draw_order;
  psm__f32 *opacity;
  psm__f32 *input_opacity;
  psm__i32 *offscreen_src_idx;
};

struct psm__deformer_node {
  struct psm__binding *binding;
  psm__i32 parent_part_idx;
  psm__i32 parent_deformer_idx;
  psm__i32 type;
  psm__i32 local_idx;
  bool local_enable;
};

struct psm__warp {
  struct psm__binding *binding;
  psm__i32 row;
  psm__i32 column;
  bool quad_transform;
  psm__i32 vertex_count;
};

struct psm__rotation {
  struct psm__binding *binding;
  psm__f32 base_angle;
};

struct psm__warp_keydata {
  struct psm__interp interp;
  psm__f32 *opacity;
  psm__f32 **pos;
  struct psm__color3 mul_color;
  struct psm__color3 scr_color;
};

struct psm__warps {
  psm__i32 count;
  struct psm__warp *items;
  struct psm__warp_keydata keydata;
  bool *enable;
  psm__f32 *opacity;
  psm__f32 **pos;
  psm__f32 *mul_color;
  psm__f32 *scr_color;
};

struct psm__rotation_keydata {
  struct psm__interp interp;
  psm__f32 *opacity;
  psm__f32 *angle;
  psm__f32 *origin_x;
  psm__f32 *origin_y;
  psm__f32 *scale;
  struct psm__color3 mul_color;
  struct psm__color3 scr_color;
};

struct psm__rotations {
  psm__i32 count;
  struct psm__rotation *items;
  struct psm__rotation_keydata keydata;
  bool *enable;
  psm__f32 *opacity;
  psm__f32 *scale;
  psm__f32 *origin_x;
  psm__f32 *origin_y;
  psm__f32 *angle;
  psm__i32 *reflect_x;
  psm__i32 *reflect_y;
  psm__f32 *mul_color;
  psm__f32 *scr_color;
};

struct psm__deformers {
  struct psm__warps warps;
  struct psm__rotations rotations;
  psm__i32 count;
  struct psm__deformer_node *nodes;
  bool *enable;
  psm__f32 *opacity;
  psm__f32 *scale;
  psm__f32 *mul_color;
  psm__f32 *scr_color;
};

struct psm__art_mesh {
  struct psm__binding *binding;
  psm__i32 parent_part_idx;
  psm__i32 parent_deformer_idx;
  bool local_enable;
  psm__i32 vertex_count;
};

struct psm__art_mesh_keydata {
  struct psm__interp interp;
  psm__f32 *opacity;
  psm__f32 *draw_order;
  psm__f32 **pos;
  struct psm__color3 mul_color;
  struct psm__color3 scr_color;
};

struct psm__art_meshes {
  psm__i32 count;
  struct psm__art_mesh *meshes;
  struct psm__art_mesh_keydata keydata;
  bool *enable;
  bool state_changed;
  psm__u8  *const_flags;
  psm__u8  *change_flags;
  psm__i32 *blend_mode;
  psm__i32 *draw_order;
  psm__f32 **pos;
  psm__f32 *opacity;
  psm__f32 *mul_color;
  psm__f32 *scr_color;
  psm__i32 *last_render_order;
  psm__i32 *last_draw_order;
  psm__f32 *last_opacity;
  psm__f32 *last_mul_color;
  psm__f32 *last_scr_color;
};

struct psm__draw_item {
  psm__i32 object_type;
  psm__i32 object_idx;
  psm__i32 group_idx;
  psm__i32 draw_order;
};

struct psm__draw_group {
  psm__i32 total_count;
  psm__i32 count;
  psm__i32 cursor;
  psm__i32 max_order;
  psm__i32 min_order;
  psm__i32 order_level;
  struct psm__draw_item *items;
};

struct psm__draw_sort {
  psm__i32 *first;
  psm__i32 *next;
  psm__i32 *last;
};

struct psm__draw_groups {
  psm__i32 count;
  struct psm__draw_group *groups;
  struct psm__draw_sort sort;
};

struct psm__glue {
  struct psm__binding *binding;
  psm__i32 mesh_idx0;
  psm__i32 mesh_idx1;
  psm__i32 glue_info_count;
  bool local_enable;
  psm__f32 *weights;
  psm__u16 *pos_idx;
};

struct psm__glue_keydata {
  struct psm__interp interp;
  psm__f32 *intensity;
};

struct psm__glues {
  psm__i32 count;
  struct psm__glue *items;
  struct psm__glue_keydata keydata;
  psm__f32 *intensity;
};

struct psm__offscreen {
  struct psm__binding *binding;
  bool *owner_enable;
  psm__i32 *keyform_idx;
};

struct psm__offscreen_keydata {
  struct psm__interp interp;
  psm__f32 *opacity;
  struct psm__color3 mul_color;
  struct psm__color3 scr_color;
};

struct psm__offscreens {
  psm__i32 count;
  struct psm__offscreen *surfaces;
  struct psm__offscreen_keydata keydata;
  bool *enable;
  psm__f32 *opacity;
  psm__f32 *mul_color;
  psm__f32 *scr_color;
};

struct psm__param {
  psm__i32 type;
  psm__f32 range[2];
  psm__f32 range_length;
  bool repeat;
  psm__f32 snap_eps;
  psm__f32 interp_eps;
  psm__f32 value;
  bool dirty;
  struct psm__axis *axes;
  psm__i32 axis_count;
  struct psm__blend_axis *blend_axes;
  psm__i32 blend_axis_count;
};

struct psm__params {
  psm__i32 count;
  struct psm__param *items;
  psm__i32 *type;
  psm__f32 *input_value;
};

struct psm__axes {
  psm__i32 count;
  struct psm__axis *items;
};

struct psm__bindings {
  psm__i32 count;
  struct psm__binding *items;
};

struct psm__blend_shape {
  psm__i32 target_idx;
  psm__i32 axis_count;
  struct psm__blend_binding *bindings;
};

struct psm__blend_shapes {
  psm__i32 count;
  struct psm__blend_shape *items;
};

struct psm__blend_constraints {
  psm__i32 count;
  struct psm__blend_constraint *items;
};

struct psm__blend_axes {
  psm__i32 count;
  struct psm__blend_axis *items;
};

struct psm__blend_bindings {
  psm__i32 count;
  struct psm__blend_binding *items;
};

struct psm__param_ext {
  psm__f32 **keys;
  psm__i32 *key_counts;
};

struct psm__model {
  const struct psm__moc3_data *source;
  struct psm__parts parts;
  struct psm__deformers deformers;
  struct psm__art_meshes art_meshes;
  struct psm__draw_groups draw_groups;
  struct psm__glues glues;
  struct psm__offscreens offscreens;
  struct psm__params params;
  struct psm__axes axes;
  struct psm__bindings bindings;
  struct psm__blend_constraints blend_constraints;
  struct psm__blend_axes blend_axes;
  struct psm__blend_bindings blend_bindings;
  struct psm__blend_shapes bs_parts;
  struct psm__blend_shapes bs_warps;
  struct psm__blend_shapes bs_rotations;
  struct psm__blend_shapes bs_art_meshes;
  struct psm__blend_shapes bs_glues;
  struct psm__blend_shapes bs_offscreens;
  struct psm__param_ext param_ext;
  psm__i32 *render_order;
  bool force_update;
  bool y_reversed;
};

#endif /* PSM__MODEL_H */
