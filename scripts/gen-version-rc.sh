#!/bin/sh
# Generate version.rc from version.rc.in and PurismCore.h
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

# Usage: ./scripts/gen-version-rc.sh [compat_version_hex]
#   compat_version_hex: e.g. 0x05010000 (default: from PurismCore.h)
#
# Reads PSM_TRUE_VERSION from include/PurismCore.h,
# substitutes into src/version.rc.in, writes to stdout.

set -e

cd "$(dirname "$0")/.."

HEADER="include/PurismCore.h"
TEMPLATE="src/version.rc.in"

# Extract PSM_TRUE_VERSION
TRUE_VER=$(grep '#define PSM_TRUE_VERSION' "$HEADER" | awk '{print $3}' | tr -d 'L')

# Parse version components: 0xMMmmPPPP
MAJOR=$(printf '%d' $(( ($TRUE_VER >> 24) & 0xFF )))
MINOR=$(printf '%d' $(( ($TRUE_VER >> 16) & 0xFF )))
PATCH=$(printf '%d' $(( $TRUE_VER & 0xFFFF )))
VER_STRING="$MAJOR.$MINOR.$PATCH"

# Compat version (from argument or header default)
if [ -n "$1" ]; then
  COMPAT_VER="$1"
else
  COMPAT_VER=$(grep '#define PSM_COMPAT_VERSION' "$HEADER" | head -1 | awk '{print $3}' | tr -d 'L')
fi

C_MAJOR=$(printf '%d' $(( ($COMPAT_VER >> 24) & 0xFF )))
C_MINOR=$(printf '%d' $(( ($COMPAT_VER >> 16) & 0xFF )))
C_PATCH=$(printf '%d' $(( $COMPAT_VER & 0xFFFF )))
COMPAT_STRING="$C_MAJOR.$C_MINOR.$C_PATCH"

sed \
  -e "s/@PSM_VER_MAJOR@/$MAJOR/g" \
  -e "s/@PSM_VER_MINOR@/$MINOR/g" \
  -e "s/@PSM_VER_PATCH@/$PATCH/g" \
  -e "s/@PSM_VER_STRING@/$VER_STRING/g" \
  -e "s/@PSM_COMPAT_STRING@/$COMPAT_STRING/g" \
  "$TEMPLATE"
