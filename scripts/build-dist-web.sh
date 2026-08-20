#!/bin/sh
# Cross-compile Purism Core SDK distribution using Emscripten
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

# Output layout:
#
#   dist/sdk-web/
#     Core/purismcore.js
#     Samples/Viewer/viewer.html + viewer.js + viewer.wasm
#
# v5 compat builds go under Core-v5/ (no viewer).
#
# Usage:
#   ./scripts/build-dist-web.sh
#
# Viewer (bin/): opt-in via RAYLIB_WEB_DIR env var.

set -e
cd "$(dirname "$0")/.."

DIST="${DISTDIR:-dist/sdk-web}"

rm -rf "$DIST"
mkdir -p "$DIST"

mkdir -p "$DIST/Core" "$DIST/Core-v5" "$DIST/Samples/Viewer"

make wasm-all
cp build/purismcore.js "$DIST/Core/"
cp build/purismcore-v5.js "$DIST/Core-v5/purismcore.js"

if [ ! -z "$RAYLIB_WEB_DIR" ]; then
  make viewer-web
  cp build/viewer.html build/viewer.js build/viewer.wasm "$DIST/Samples/Viewer/"
fi

# Documentation
echo "=== Docs ==="
for f in LICENSE README.md docs/*.md docs/SDKINFO-WEB.txt; do
  [ -f "$f" ] && cp "$f" "$DIST/"
done

rm -rf "$TMP"

echo ""
echo "=== Done ==="
find "$DIST" -type f -not -path '*/obj/*' | sort | sed 's|^|  |'
