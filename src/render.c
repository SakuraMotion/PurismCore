/*
 * Purism Core: render order calculation and dynamic flags
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "private.h"
#include "array.h"
#include "debug.h"
#include "moc3.h"
#include "model.h"
#include "render.h"

PSM__DEF void
psm__sort_render_order(struct psm__model *m)
{
  struct psm__draw_groups *dog = &m->draw_groups;

  psm__i32 group_count = dog->count;
  if (group_count <= 0)
    return;

  struct psm__draw_group *groups = dog->groups;
  if (!groups)
    return;

  struct psm__art_meshes *am = &m->art_meshes;
  struct psm__parts      *pt = &m->parts;

  psm__i32 *am_draw = am->draw_order;
  psm__i32 *pt_draw = pt->draw_order;
  bool     *am_en = am->enable;
  bool     *pt_en = pt->enable;
  psm__i32  am_cnt = am->count;

  /* First assign draw orders to items */
  for (psm__i32 gi = 0; gi < group_count; gi++) {
    struct psm__draw_group *c = &groups[gi];

    psm__i32 n = c->count;
    if (n <= 0)
      continue;
    struct psm__draw_item *it = c->items;
    if (!it)
      continue;

    for (psm__i32 j = 0; j < n; j++) {
      struct psm__draw_item *item = &it[j];

      psm__i32 oi = item->object_idx;

      if (item->object_type == 1) {
        if (pt_en[oi])
          item->draw_order = pt_draw[oi];
        else
          item->draw_order = c->min_order;
      } else {
        if (am_en[oi])
          item->draw_order = am_draw[oi];
        else
          item->draw_order = c->min_order;
      }
    }
  }

  /* Now time for sorting and render order assignment */
  psm__i32 *render_order = m->render_order;
  psm__u8   ver = m->source->header->version;

  struct psm__draw_sort *srt = &dog->sort;

  psm__i32 *first = srt->first;
  psm__i32 *last = srt->last;
  psm__i32 *next = srt->next;

  if (!first || !last || !next)
    return;

  /* Compute max values from source for bounds checking */
  psm__i32 max_level = 0, max_items = 0;

  struct psm__sections   *ms = m->source->sections;
  struct psm__count_info *cnt = ms->count_info;

  if (cnt->draw_groups > 0 && ms->draw_group_src.obj_len &&
      ms->draw_group_src.max_order && ms->draw_group_src.min_order) {
    for (psm__i32 i = 0; i < cnt->draw_groups; i++) {
      psm__i32 gc = ms->draw_group_src.obj_len[i];
      psm__i32 hi = ms->draw_group_src.max_order[i];
      psm__i32 lo = ms->draw_group_src.min_order[i];
      psm__i32 lv = psm__safe_order_level(hi, lo);
      if (lv > max_level) max_level = lv;
      if (gc > max_items) max_items = gc;
    }
  }

  if (max_level <= 0 || max_items <= 0)
    return;

  for (psm__i32 gi = 0; gi < group_count; gi++) {
    struct psm__draw_group *c = &groups[gi];

    psm__i32 olevel = c->order_level;
    psm__i32 n = c->count;

    if (olevel <= 0 || n <= 0)
      continue;
    if (olevel > max_level || n > max_items)
      continue;

    /* Clear sorting buckets */
    if ((psm_size)olevel > (psm_size)-1 / sizeof(psm__i32))
      continue;
    psm_size olsz = (psm_size)olevel * sizeof(psm__i32);
    memset(first, 0xFF, olsz);
    memset(last, 0xFF, olsz);

    if ((psm_size)n > (psm_size)-1 / sizeof(psm__i32))
      continue;
    memset(next, 0xFF, (psm_size)n * sizeof(psm__i32));

    /* Bucket items by relative draw order */
    struct psm__draw_item *it = c->items;
    if (!it)
      continue;
    for (psm__i32 j = 0; j < n; j++) {
      psm__i32 rel =
          (psm__i32)((psm__u32)it[j].draw_order - (psm__u32)c->min_order);
      rel = psm__clamp_idx(rel, olevel);

      if (last[rel] == -1)
        first[rel] = j;
      else
        next[last[rel]] = j;
      last[rel] = j;
    }

    /* Walk buckets in order and assign render positions */
    psm__i32 pos = c->cursor;
    bool     has_offscr = (ver >= csmMocVersion_53);

    for (psm__i32 oi = 0; oi < olevel; oi++) {
      psm__i32 di = first[oi];
      while (di != -1 && di < n) {
        struct psm__draw_item *item = &it[di];

        psm__i32 obj = item->object_idx;
        psm__i32 grp = item->group_idx;

        if (item->object_type == 1) {
          if (has_offscr) {
            psm__i32 oidx = ms->part_src.offscreen_idx[obj];
            if (oidx >= 0)
              render_order[am_cnt + oidx] = pos++;
          }
          /* F3: a part draw item (type 1) always has a valid child group;
           * self_group_idx >= 0 is proved at load (psm__verify_idx). */
          if (grp < group_count) {
            struct psm__draw_group *nc = &groups[grp];
            nc->cursor = pos;
            pos += nc->total_count;
          }
        } else {
          render_order[obj] = pos++;
        }

        psm__i32 ni = next[di];
        if (ni <= di)
          break;
        di = ni;
      }
    }
  }
}

#if PSM_COMPAT_VERSION >= 0x06000000L
PSMDEF const int *
csmGetRenderOrders(const csmModel *model)
{
  return ((const struct psm__model *)model)->render_order;
}
#else
PSMDEF const int *
csmGetDrawableRenderOrders(const csmModel *model)
{
  return ((const struct psm__model *)model)->render_order;
}
#endif
