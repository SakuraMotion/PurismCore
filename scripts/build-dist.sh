#!/bin/sh
# Cross-compile Purism Core SDK distribution.
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

# Output layout:
#   dist/sdk/
#     include/PurismCore.h
#     include/Live2DCubismCore.h
#     lib/linux/x86_64/libPurismCore.a
#     lib/linux/arm64/libPurismCore.a
#     lib/macos/x86_64/libPurismCore.a
#     lib/macos/arm64/libPurismCore.a
#     lib/macos/universal/libPurismCore.a
#     lib/windows/x86_64/PurismCore.a
#     lib/windows/x86/PurismCore.a
#     lib/windows/arm64/PurismCore.a
#     dll/linux/x86_64/libPurismCore.so
#     dll/macos/x86_64/libPurismCore.dylib
#     dll/macos/arm64/libPurismCore.dylib
#     dll/macos/universal/libPurismCore.dylib
#     dll/windows/x86_64/PurismCore.dll
#     dll/windows/x86/PurismCore.dll
#     dll/windows/arm64/PurismCore.dll
#     bundle/PurismCoreBundle.h
#
# v5 compat builds go under lib-v5/ and dll-v5/ (Windows only).
#
# Usage:
#   ./scripts/build-dist.sh          # full matrix
#   ./scripts/build-dist.sh linux    # linux only
#   ./scripts/build-dist.sh windows  # windows only

set -e
cd "$(dirname "$0")/.."

DIST="${DISTDIR:-dist/sdk}"

build() {
  target="$1" outlib="$2" outdll="$3" compat="$4"
  opts="--out build/_dist_tmp"

  [ -n "$compat" ] && opts="$opts $compat"

  if [ -n "$outdll" ]; then
    opts="$opts --both"
  else
    opts="$opts --static"
  fi

  ./scripts/zig-build.sh "$target" $opts

  mkdir -p "$DIST/$outlib"
  cp build/_dist_tmp/libPurismCore.a "$DIST/$outlib/" 2>/dev/null || true

  if [ -n "$outdll" ]; then
    mkdir -p "$DIST/$outdll"
    cp build/_dist_tmp/PurismCore.dll "$DIST/$outdll/" 2>/dev/null || true
    cp build/_dist_tmp/libPurismCore.so "$DIST/$outdll/" 2>/dev/null || true
    cp build/_dist_tmp/libPurismCore.dylib "$DIST/$outdll/" 2>/dev/null || true
  fi

  rm -rf build/_dist_tmp
}

# ── main ──

rm -rf "$DIST"
mkdir -p "$DIST"

FILTER="${1:-all}"

# Linux
if [ "$FILTER" = all ] || [ "$FILTER" = linux ]; then
  echo "=== Linux ==="
  build linux-x64   lib/linux/x86_64 dll/linux/x86_64 --v6
  build linux-arm64 lib/linux/arm64  ""               --v6
fi

# macOS
if [ "$FILTER" = all ] || [ "$FILTER" = macos ]; then
  echo "=== macOS ==="
  build macos-x64   lib/macos/x86_64 dll/macos/x86_64 --v6
  build macos-arm64 lib/macos/arm64  dll/macos/arm64  --v6

  # Universal binary (fat dylib)
  mkdir -p "$DIST/dll/macos/universal"
  lipo -create \
    "$DIST/dll/macos/x86_64/libPurismCore.dylib" \
    "$DIST/dll/macos/arm64/libPurismCore.dylib" \
    -output "$DIST/dll/macos/universal/libPurismCore.dylib" \
    2>/dev/null || true

  # Universal static library
  mkdir -p "$DIST/lib/macos/universal"
  lipo -create \
    "$DIST/lib/macos/x86_64/libPurismCore.a" \
    "$DIST/lib/macos/arm64/libPurismCore.a" \
    -output "$DIST/lib/macos/universal/libPurismCore.a" \
    2>/dev/null || true
fi

# Windows (v6 + v5)
if [ "$FILTER" = all ] || [ "$FILTER" = windows ]; then
  echo "=== Windows (v6) ==="
  build win-x64   lib/windows/x86_64   dll/windows/x86_64   --v6
  build win-x86   lib/windows/x86      dll/windows/x86      --v6
  build win-arm64 lib/windows/arm64    dll/windows/arm64    --v6

  echo "=== Windows (v5) ==="
  build win-x64   lib-v5/windows/x86_64  dll-v5/windows/x86_64  --v5
  build win-x86   lib-v5/windows/x86     dll-v5/windows/x86     --v5
  build win-arm64 lib-v5/windows/arm64   dll-v5/windows/arm64   --v5
fi

# Headers
echo "=== Headers ==="
mkdir -p "$DIST/include"
cp include/PurismCore.h include/Live2DCubismCore.h "$DIST/include/"

# Bundle
echo "=== Bundle ==="
mkdir -p "$DIST/bundle"
./scripts/bundle.sh > "$DIST/bundle/PurismCoreBundle.h"

# Documentation
echo "=== Docs ==="
for f in LICENSE README.md docs/*.md docs/*.txt; do
  [ -f "$f" ] && cp "$f" "$DIST/"
done

echo ""
echo "=== Done ==="
find "$DIST" -type f -not -path '*/obj/*' | sort | sed 's|^|  |'
