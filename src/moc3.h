/*
 * Purism Core: MOC3 format structures
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__MOC3_H
#define PSM__MOC3_H

#include "private.h"

struct psm__moc3_header;
struct psm__moc3_data;
struct psm__sections;
struct psm__count_info;
struct psm__canvas_info;

struct psm__moc3_header {
  char magic[4];
  psm__u8 version;
  psm__u8 endian_flag;
  psm__u8 _reserved1[2];
  struct psm__moc3_data *data;
  psm__u8 _reserved2[56 - sizeof(void *)];
};

psm__static_assert(sizeof(struct psm__moc3_header) == 64,
    "MOC3 header must be 64 bytes");

struct psm__moc3_data {
  struct psm__moc3_header *header;
  psm__u32 *offsets;
  struct psm__sections *sections;
};

struct psm__id {
  char data[64];
};

struct psm__count_info {
  psm__i32 parts;
  psm__i32 deformers;
  psm__i32 warps;
  psm__i32 rotations;
  psm__i32 art_meshes;
  psm__i32 parameters;
  psm__i32 part_kf;
  psm__i32 warp_kf;
  psm__i32 rotation_kf;
  psm__i32 art_mesh_kf;
  psm__i32 kf_pos;
  psm__i32 key_table_indices;
  psm__i32 bindings;
  psm__i32 key_tables;
  psm__i32 keys;
  psm__i32 uvs;
  psm__i32 indices;
  psm__i32 masks;
  psm__i32 draw_groups;
  psm__i32 draw_items;
  psm__i32 glues;
  psm__i32 glue_info;
  psm__i32 glue_kf;
  psm__i32 kf_mul_colors;
  psm__i32 kf_scr_colors;
  psm__i32 blend_key_tables;
  psm__i32 blend_bindings;
  psm__i32 bs_warps;
  psm__i32 bs_art_meshes;
  psm__i32 bs_constraint_idx;
  psm__i32 bs_constraints;
  psm__i32 bs_constraint_vals;
  psm__i32 bs_parts;
  psm__i32 bs_rotations;
  psm__i32 bs_glues;
  psm__i32 offscreens;
  psm__i32 offscreen_kf;
  psm__i32 bs_offscreens;
  psm__i32 _reserved;
};

struct psm__canvas_info {
  psm__f32 pix_per_unit;
  psm__f32 origin_x;
  psm__f32 origin_y;
  psm__f32 width;
  psm__f32 height;
  psm__u8 flag;
};

struct psm__part_src {
  const char **id_runtime;
  struct psm__id *id;
  psm__i32 *binding_idx;
  psm__i32 *keyform_offset;
  psm__i32 *key_count;
  psm__i32 *visible;
  psm__i32 *enable;
  psm__i32 *parent_part_idx;
  psm__i32 *offscreen_idx;
};

struct psm__deformer_src {
  const char **id_runtime;
  struct psm__id *id;
  psm__i32 *binding_idx;
  psm__i32 *visible;
  psm__i32 *enable;
  psm__i32 *parent_part_idx;
  psm__i32 *parent_deformer_idx;
  psm__i32 *type;
  psm__i32 *local_idx;
};

struct psm__warp_src {
  psm__i32 *binding_idx;
  psm__i32 *keyform_offset;
  psm__i32 *key_count;
  psm__i32 *key_color_offset;
  psm__i32 *vertex_count;
  psm__i32 *row;
  psm__i32 *column;
  psm__i32 *quad_transform;
};

struct psm__rotation_src {
  psm__i32 *binding_idx;
  psm__i32 *keyform_offset;
  psm__i32 *key_count;
  psm__i32 *key_color_offset;
  psm__f32 *base_angle;
};

struct psm__art_mesh_src {
  const char **id_runtime;
  const psm__f32 **uv_runtime;
  const psm__u16 **position_idx_runtime;
  const psm__i32 **drawable_mask_runtime;
  void *id;
  psm__i32 *binding_idx;
  psm__i32 *keyform_offset;
  psm__i32 *key_count;
  psm__i32 *key_color_offset;
  psm__i32 *visible;
  psm__i32 *enable;
  psm__i32 *parent_part_idx;
  psm__i32 *parent_deformer_idx;
  psm__i32 *texture_no;
  psm__u8 *drawable_flag;
  psm__i32 *blend_mode;
  psm__i32 *vertex_count;
  psm__i32 *uv_begin;
  psm__i32 *indices_begin;
  psm__i32 *indices_count;
  psm__i32 *mask_begin;
  psm__i32 *mask_count;
};

struct psm__param_src {
  const char **id_runtime;
  struct psm__id *id;
  psm__f32 *maximum_value;
  psm__f32 *minimum_value;
  psm__f32 *default_value;
  psm__i32 *repeat;
  psm__i32 *decimal_places;
  psm__i32 *type;
  psm__i32 *key_table_begin;
  psm__i32 *key_table_count;
  psm__i32 *blend_key_table_begin;
  psm__i32 *blend_key_table_count;
};

struct psm__glue_src {
  const char **id_runtime;
  struct psm__id *id;
  psm__i32 *binding_idx;
  psm__i32 *keyform_offset;
  psm__i32 *key_count;
  psm__i32 *art_mesh_index_a;
  psm__i32 *art_mesh_index_b;
  psm__i32 *info_begin;
  psm__i32 *info_count;
};

struct psm__part_key_src {
  psm__f32 *draw_order;
  psm__i32 *key_idx;
};

struct psm__warp_key_src {
  psm__f32 *opacity;
  psm__i32 *key_pos_offset;
  psm__i32 *key_mul_color_offset;
  psm__i32 *key_scr_color_offset;
};

struct psm__rotation_key_src {
  psm__f32 *opacity;
  psm__f32 *angle;
  psm__f32 *origin_x;
  psm__f32 *origin_y;
  psm__f32 *scale;
  psm__i32 *reflect_x;
  psm__i32 *reflect_y;
  psm__i32 *key_mul_color_offset;
  psm__i32 *key_scr_color_offset;
};

struct psm__art_mesh_key_src {
  psm__f32 *opacity;
  psm__f32 *draw_order;
  psm__i32 *key_pos_offset;
  psm__i32 *key_mul_color_offset;
  psm__i32 *key_scr_color_offset;
};

struct psm__glue_key_src {
  psm__f32 *intensity;
};

struct psm__key_pos_src {
  psm__f32 *xy;
};

struct psm__key_table_idx_src {
  psm__i32 *index;
};

struct psm__binding_src {
  psm__i32 *key_table_idx_begin;
  psm__i32 *key_table_idx_count;
};

struct psm__key_table_src {
  psm__i32 *keys_begin;
  psm__i32 *keys_count;
};

struct psm__keys_src {
  psm__f32 *key;
};

struct psm__uv_src {
  psm__f32 *xy;
};

struct psm__pos_idx_src {
  psm__u16 *index;
};

struct psm__mask_src {
  psm__i32 *art_mesh_idx;
};

struct psm__draw_group_src {
  psm__i32 *obj_begin;
  psm__i32 *obj_count;
  psm__i32 *obj_total_count;
  psm__i32 *max_order;
  psm__i32 *min_order;
};

struct psm__draw_group_obj_src {
  psm__i32 *type;
  psm__i32 *index;
  psm__i32 *self_group_idx;
};

struct psm__glue_info_src {
  psm__f32 *weight;
  psm__u16 *position_idx;
};

struct psm__param_keys_src {
  const psm__f32 **key_runtime;
  psm__i32 *keys_begin;
  psm__i32 *keys_count;
};

struct psm__blend_key_table_src {
  psm__i32 *keys_begin;
  psm__i32 *keys_count;
  psm__i32 *base_key_idx;
};

struct psm__blend_binding_src {
  psm__i32 *axis_idx;
  psm__i32 *key_bs_begin;
  psm__i32 *key_bs_count;
  psm__i32 *bs_constraint_idx_begin;
  psm__i32 *bs_constraint_idx_count;
};

struct psm__blend_src {
  psm__i32 *target_idx;
  psm__i32 *bs_binding_begin;
  psm__i32 *bs_binding_count;
};

struct psm__blend_constraint_idx_src {
  psm__i32 *constraint_idx;
};

struct psm__blend_constraint_src {
  psm__i32 *parameter_idx;
  psm__i32 *value_begin;
  psm__i32 *value_count;
};

struct psm__blend_constraint_val_src {
  psm__f32 *key;
  psm__f32 *weight;
};

struct psm__offscreen_src {
  const psm__i32 **drawable_mask_runtime;
  psm__i32 *owner_idx;
  psm__u8 *drawable_flag;
  psm__i32 *blend_mode;
  psm__i32 *mask_begin;
  psm__i32 *mask_count;
};

struct psm__key_color_src {
  psm__f32 *r;
  psm__f32 *g;
  psm__f32 *b;
};

struct psm__offscreen_key_src {
  psm__f32 *opacity;
  psm__i32 *key_mul_color_offset;
  psm__i32 *key_scr_color_offset;
};

struct psm__sections {
  struct psm__moc3_data source;
  struct psm__count_info *count_info;
  struct psm__canvas_info *canvas_info;

  struct psm__part_src part_src;
  struct psm__deformer_src deformer_src;
  struct psm__warp_src warp_src;
  struct psm__rotation_src rotation_src;
  struct psm__art_mesh_src art_mesh_src;

  struct psm__param_src param_src;
  struct psm__param_keys_src param_keys_src;

  struct psm__part_key_src part_key_src;
  struct psm__warp_key_src warp_key_src;
  struct psm__rotation_key_src rotation_key_src;
  struct psm__art_mesh_key_src art_mesh_key_src;

  struct psm__key_pos_src key_pos_src;
  struct psm__key_table_src key_table_src;
  struct psm__key_table_idx_src key_table_idx_src;
  struct psm__binding_src binding_src;

  struct psm__blend_key_table_src blend_key_table_src;
  struct psm__blend_binding_src blend_binding_src;
  struct psm__blend_src bs_part_src;
  struct psm__blend_src bs_warp_src;
  struct psm__blend_src bs_rotation_src;
  struct psm__blend_src bs_art_mesh_src;
  struct psm__blend_src bs_glue_src;
  struct psm__blend_constraint_idx_src blend_constraint_idx_src;
  struct psm__blend_constraint_src blend_constraint_src;
  struct psm__blend_constraint_val_src blend_constraint_val_src;

  struct psm__keys_src keys_src;
  struct psm__uv_src uv_src;
  struct psm__pos_idx_src indices_src;
  struct psm__mask_src mask_src;

  struct psm__draw_group_src draw_group_src;
  struct psm__draw_group_obj_src draw_group_obj_src;

  struct psm__glue_src glue_src;
  struct psm__glue_info_src glue_info_src;
  struct psm__glue_key_src glue_key_src;

  struct psm__key_color_src kf_mul_color_src;
  struct psm__key_color_src kf_scr_color_src;

  struct psm__offscreen_src offscreen_src;
  struct psm__offscreen_key_src offscreen_key_src;
  struct psm__blend_src bs_offscreen_src;
};

/* MOC3 v1-v5: 160 section offsets */
struct psm__moc3_data_v52 {
  struct psm__moc3_header header;
  psm__u32 offsets[160];
  struct psm__sections sections;
};

/* MOC3 v6+: 480 section offsets */
struct psm__moc3_data_v53 {
  struct psm__moc3_header header;
  psm__u32 offsets[480];
  struct psm__sections sections;
};

/*
 * FOREACH macros for sections member initialization
 * S(TYPE, MEMBER, COUNT) - static count (compile-time constant)
 * D(TYPE, MEMBER, CNT_MEMBER) - dynamic count from count_info
 */
#define PSM__SECTIONS_V30(S, D) \
  S(struct psm__count_info, count_info, 1) \
  S(struct psm__canvas_info, canvas_info, 1) \
  D(const char *, part_src.id_runtime, parts) \
  D(struct psm__id, part_src.id, parts) \
  D(psm__i32, part_src.binding_idx, parts) \
  D(psm__i32, part_src.keyform_offset, parts) \
  D(psm__i32, part_src.key_count, parts) \
  D(psm__i32, part_src.visible, parts) \
  D(psm__i32, part_src.enable, parts) \
  D(psm__i32, part_src.parent_part_idx, parts) \
  D(const char *, deformer_src.id_runtime, deformers) \
  D(struct psm__id, deformer_src.id, deformers) \
  D(psm__i32, deformer_src.binding_idx, deformers) \
  D(psm__i32, deformer_src.visible, deformers) \
  D(psm__i32, deformer_src.enable, deformers) \
  D(psm__i32, deformer_src.parent_part_idx, deformers) \
  D(psm__i32, deformer_src.parent_deformer_idx, deformers) \
  D(psm__i32, deformer_src.type, deformers) \
  D(psm__i32, deformer_src.local_idx, deformers) \
  D(psm__i32, warp_src.binding_idx, warps) \
  D(psm__i32, warp_src.keyform_offset, warps) \
  D(psm__i32, warp_src.key_count, warps) \
  D(psm__i32, warp_src.vertex_count, warps) \
  D(psm__i32, warp_src.row, warps) \
  D(psm__i32, warp_src.column, warps) \
  D(psm__i32, rotation_src.binding_idx, rotations) \
  D(psm__i32, rotation_src.keyform_offset, rotations) \
  D(psm__i32, rotation_src.key_count, rotations) \
  D(psm__f32, rotation_src.base_angle, rotations) \
  D(const char *, art_mesh_src.id_runtime, art_meshes) \
  D(const psm__f32 *, art_mesh_src.uv_runtime, art_meshes) \
  D(const psm__u16 *, art_mesh_src.position_idx_runtime, art_meshes) \
  D(const psm__i32 *, art_mesh_src.drawable_mask_runtime, art_meshes) \
  D(struct psm__id, art_mesh_src.id, art_meshes) \
  D(psm__i32, art_mesh_src.binding_idx, art_meshes) \
  D(psm__i32, art_mesh_src.keyform_offset, art_meshes) \
  D(psm__i32, art_mesh_src.key_count, art_meshes) \
  D(psm__i32, art_mesh_src.visible, art_meshes) \
  D(psm__i32, art_mesh_src.enable, art_meshes) \
  D(psm__i32, art_mesh_src.parent_part_idx, art_meshes) \
  D(psm__i32, art_mesh_src.parent_deformer_idx, art_meshes) \
  D(psm__i32, art_mesh_src.texture_no, art_meshes) \
  D(psm__u8, art_mesh_src.drawable_flag, art_meshes) \
  D(psm__i32, art_mesh_src.vertex_count, art_meshes) \
  D(psm__i32, art_mesh_src.uv_begin, art_meshes) \
  D(psm__i32, art_mesh_src.indices_begin, art_meshes) \
  D(psm__i32, art_mesh_src.indices_count, art_meshes) \
  D(psm__i32, art_mesh_src.mask_begin, art_meshes) \
  D(psm__i32, art_mesh_src.mask_count, art_meshes) \
  D(const char *, param_src.id_runtime, parameters) \
  D(struct psm__id, param_src.id, parameters) \
  D(psm__f32, param_src.maximum_value, parameters) \
  D(psm__f32, param_src.minimum_value, parameters) \
  D(psm__f32, param_src.default_value, parameters) \
  D(psm__i32, param_src.repeat, parameters) \
  D(psm__i32, param_src.decimal_places, parameters) \
  D(psm__i32, param_src.key_table_begin, parameters) \
  D(psm__i32, param_src.key_table_count, parameters) \
  D(psm__f32, part_key_src.draw_order, part_kf) \
  D(psm__f32, warp_key_src.opacity, warp_kf) \
  D(psm__i32, warp_key_src.key_pos_offset, warp_kf) \
  D(psm__f32, rotation_key_src.opacity, rotation_kf) \
  D(psm__f32, rotation_key_src.angle, rotation_kf) \
  D(psm__f32, rotation_key_src.origin_x, rotation_kf) \
  D(psm__f32, rotation_key_src.origin_y, rotation_kf) \
  D(psm__f32, rotation_key_src.scale, rotation_kf) \
  D(psm__i32, rotation_key_src.reflect_x, rotation_kf) \
  D(psm__i32, rotation_key_src.reflect_y, rotation_kf) \
  D(psm__f32, art_mesh_key_src.opacity, art_mesh_kf) \
  D(psm__f32, art_mesh_key_src.draw_order, art_mesh_kf) \
  D(psm__i32, art_mesh_key_src.key_pos_offset, art_mesh_kf) \
  D(psm__f32, key_pos_src.xy, kf_pos) \
  D(psm__i32, key_table_idx_src.index, key_table_indices) \
  D(psm__i32, binding_src.key_table_idx_begin, bindings) \
  D(psm__i32, binding_src.key_table_idx_count, bindings) \
  D(psm__i32, key_table_src.keys_begin, key_tables) \
  D(psm__i32, key_table_src.keys_count, key_tables) \
  D(psm__f32, keys_src.key, keys) \
  D(psm__f32, uv_src.xy, uvs) \
  D(psm__u16, indices_src.index, indices) \
  D(psm__i32, mask_src.art_mesh_idx, masks) \
  D(psm__i32, draw_group_src.obj_begin, draw_groups) \
  D(psm__i32, draw_group_src.obj_count, draw_groups) \
  D(psm__i32, draw_group_src.obj_total_count, draw_groups) \
  D(psm__i32, draw_group_src.max_order, draw_groups) \
  D(psm__i32, draw_group_src.min_order, draw_groups) \
  D(psm__i32, draw_group_obj_src.type, draw_items) \
  D(psm__i32, draw_group_obj_src.index, draw_items) \
  D(psm__i32, draw_group_obj_src.self_group_idx, draw_items) \
  D(const char *, glue_src.id_runtime, glues) \
  D(struct psm__id, glue_src.id, glues) \
  D(psm__i32, glue_src.binding_idx, glues) \
  D(psm__i32, glue_src.keyform_offset, glues) \
  D(psm__i32, glue_src.key_count, glues) \
  D(psm__i32, glue_src.art_mesh_index_a, glues) \
  D(psm__i32, glue_src.art_mesh_index_b, glues) \
  D(psm__i32, glue_src.info_begin, glues) \
  D(psm__i32, glue_src.info_count, glues) \
  D(psm__f32, glue_info_src.weight, glue_info) \
  D(psm__u16, glue_info_src.position_idx, glue_info) \
  D(psm__f32, glue_key_src.intensity, glue_kf)

#define PSM__SECTIONS_V33(S, D) \
  D(psm__i32, warp_src.quad_transform, warps)

#define PSM__SECTIONS_V42(S, D) \
  D(const psm__f32 *, param_keys_src.key_runtime, parameters) \
  D(psm__i32, param_keys_src.keys_begin, parameters) \
  D(psm__i32, param_keys_src.keys_count, parameters) \
  D(psm__i32, warp_src.key_color_offset, warps) \
  D(psm__i32, rotation_src.key_color_offset, rotations) \
  D(psm__i32, art_mesh_src.key_color_offset, art_meshes) \
  D(psm__f32, kf_mul_color_src.r, kf_mul_colors) \
  D(psm__f32, kf_mul_color_src.g, kf_mul_colors) \
  D(psm__f32, kf_mul_color_src.b, kf_mul_colors) \
  D(psm__f32, kf_scr_color_src.r, kf_scr_colors) \
  D(psm__f32, kf_scr_color_src.g, kf_scr_colors) \
  D(psm__f32, kf_scr_color_src.b, kf_scr_colors) \
  D(psm__i32, param_src.type, parameters) \
  D(psm__i32, param_src.blend_key_table_begin, parameters) \
  D(psm__i32, param_src.blend_key_table_count, parameters) \
  D(psm__i32, blend_key_table_src.keys_begin, blend_key_tables) \
  D(psm__i32, blend_key_table_src.keys_count, blend_key_tables) \
  D(psm__i32, blend_key_table_src.base_key_idx, blend_key_tables) \
  D(psm__i32, blend_binding_src.axis_idx, blend_bindings) \
  D(psm__i32, blend_binding_src.key_bs_begin, blend_bindings) \
  D(psm__i32, blend_binding_src.key_bs_count, blend_bindings) \
  D(psm__i32, blend_binding_src.bs_constraint_idx_begin, blend_bindings) \
  D(psm__i32, blend_binding_src.bs_constraint_idx_count, blend_bindings) \
  D(psm__i32, bs_warp_src.target_idx, bs_warps) \
  D(psm__i32, bs_warp_src.bs_binding_begin, bs_warps) \
  D(psm__i32, bs_warp_src.bs_binding_count, bs_warps) \
  D(psm__i32, bs_art_mesh_src.target_idx, bs_art_meshes) \
  D(psm__i32, bs_art_mesh_src.bs_binding_begin, bs_art_meshes) \
  D(psm__i32, bs_art_mesh_src.bs_binding_count, bs_art_meshes) \
  D(psm__i32, blend_constraint_idx_src.constraint_idx, bs_constraint_idx) \
  D(psm__i32, blend_constraint_src.parameter_idx, bs_constraints) \
  D(psm__i32, blend_constraint_src.value_begin, bs_constraints) \
  D(psm__i32, blend_constraint_src.value_count, bs_constraints) \
  D(psm__f32, blend_constraint_val_src.key, bs_constraint_vals) \
  D(psm__f32, blend_constraint_val_src.weight, bs_constraint_vals)

#define PSM__SECTIONS_V50(S, D) \
  D(psm__i32, warp_key_src.key_mul_color_offset, warp_kf) \
  D(psm__i32, warp_key_src.key_scr_color_offset, warp_kf) \
  D(psm__i32, rotation_key_src.key_mul_color_offset, rotation_kf) \
  D(psm__i32, rotation_key_src.key_scr_color_offset, rotation_kf) \
  D(psm__i32, art_mesh_key_src.key_mul_color_offset, art_mesh_kf) \
  D(psm__i32, art_mesh_key_src.key_scr_color_offset, art_mesh_kf) \
  D(psm__i32, bs_part_src.target_idx, bs_parts) \
  D(psm__i32, bs_part_src.bs_binding_begin, bs_parts) \
  D(psm__i32, bs_part_src.bs_binding_count, bs_parts) \
  D(psm__i32, bs_rotation_src.target_idx, bs_rotations) \
  D(psm__i32, bs_rotation_src.bs_binding_begin, bs_rotations) \
  D(psm__i32, bs_rotation_src.bs_binding_count, bs_rotations) \
  D(psm__i32, bs_glue_src.target_idx, bs_glues) \
  D(psm__i32, bs_glue_src.bs_binding_begin, bs_glues) \
  D(psm__i32, bs_glue_src.bs_binding_count, bs_glues)

#define PSM__SECTIONS_V53(S, D) \
  D(psm__i32, part_src.offscreen_idx, parts) \
  D(psm__i32, art_mesh_src.blend_mode, art_meshes) \
  D(const psm__i32 *, offscreen_src.drawable_mask_runtime, offscreens) \
  D(psm__i32, offscreen_src.owner_idx, offscreens) \
  D(psm__u8, offscreen_src.drawable_flag, offscreens) \
  D(psm__i32, offscreen_src.blend_mode, offscreens) \
  D(psm__i32, offscreen_src.mask_begin, offscreens) \
  D(psm__i32, offscreen_src.mask_count, offscreens) \
  D(psm__i32, part_key_src.key_idx, part_kf) \
  D(psm__f32, offscreen_key_src.opacity, offscreen_kf) \
  D(psm__i32, offscreen_key_src.key_mul_color_offset, offscreen_kf) \
  D(psm__i32, offscreen_key_src.key_scr_color_offset, offscreen_kf) \
  D(psm__i32, bs_offscreen_src.target_idx, bs_offscreens) \
  D(psm__i32, bs_offscreen_src.bs_binding_begin, bs_offscreens) \
  D(psm__i32, bs_offscreen_src.bs_binding_count, bs_offscreens)

static inline struct psm__moc3_data *
psm__moc_to_data(const csmMoc *moc)
{
  return ((struct psm__moc3_header *)moc)->data;
}

#endif /* PSM__MOC3_H */
