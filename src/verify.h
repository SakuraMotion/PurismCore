/*
 * Purism Core: MOC3 load-time validation
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PSM__VERIFY_H
#define PSM__VERIFY_H

#include "private.h"
#include "moc3.h"

PSM__DEF int psm__verify_count_info(psm__u8 ver,
    const struct psm__count_info           *cnt);

PSM__DEF int psm__verify_sections(struct psm__sections *ms, psm__u8 *p,
    psm__u32 *offsets, psm_size n, psm_size off, psm__u8 ver, bool bounds);

PSM__DEF int psm__verify_idx(psm__u8 ver, const struct psm__sections *src);

#endif /* PSM__VERIFY_H */
