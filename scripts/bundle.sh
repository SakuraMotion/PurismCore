#!/bin/sh
# Generate single-file bundle from source files.
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

# Usage: ./scripts/bundle.sh > dist/PurismCoreBundle.h

set -e
cd "$(dirname "$0")/.."

cat << 'HEADER'
/*
 * PurismCoreBundle.h - Single-file Purism Core library
 *
 * Usage:
 *   #include "PurismCoreBundle.h"
 *
 * In exactly ONE .c file, define the implementation:
 *   #define PURISM_CORE_IMPLEMENTATION
 *   #include "PurismCoreBundle.h"
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */
#ifndef PURISM_CORE_BUNDLE_H
#define PURISM_CORE_BUNDLE_H
HEADER

sed -e '/^#ifndef PURISM_CORE_H$/d' \
  -e '/^#define PURISM_CORE_H$/d' \
  include/PurismCore.h | sed '$ { /^#endif/d }'

echo ""
echo "#ifdef PURISM_CORE_IMPLEMENTATION"
echo ""

# Internal headers + source files, stripping include guards
# and internal #include "..." directives
for f in \
  src/private.h src/error.h src/debug.h src/arena.h \
  src/array.h src/math2.h src/moc3.h src/model.h \
  src/gather.h src/interpolate.h src/artmesh.h \
  src/blendshape.h src/deformer.h src/glue.h \
  src/offscreen.h src/param.h src/part.h \
  src/render.h src/update.h \
  src/core.c src/debug.c src/arena.c \
  src/math2.c src/moc3.c src/model.c src/update.c \
  src/param.c src/part.c src/deformer.c src/artmesh.c \
  src/glue.c src/offscreen.c src/blendshape.c \
  src/interpolate.c src/render.c
do
  echo "/* file: $(basename $f) */"
  sed -e '/^#ifndef PSM__.*_H$/d' \
  -e '/^#define PSM__.*_H$/d' \
  -e '/^#endif.*PSM__.*_H/d' \
  -e '/^#include *".*"/d' \
  "$f"
  echo ""
done

echo "#endif /* PURISM_CORE_IMPLEMENTATION */"
echo ""
echo "#endif /* PURISM_CORE_BUNDLE_H */"
