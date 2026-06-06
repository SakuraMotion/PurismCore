#!/bin/sh
# Cross-build Purism Core with zig cc.
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

# Usage:
#   ./scripts/zig-build.sh <target> [options]
#
# Targets:
#   linux-x64    linux-arm64
#   macos-x64    macos-arm64
#   win-x64      win-x86      win-arm64
#   native       (host platform, same as plain make)
#
# Options:
#   --v5         Build with v5 compat (default for Windows)
#   --v6         Build with v6 compat (default for non-Windows)
#   --dll        Build shared library / DLL
#   --static     Build static library (default)
#   --both       Build both static and shared
#   --out DIR    Output directory (default: build/<target>)
#
# Examples:
#   ./scripts/zig-build.sh win-x64 --both
#   ./scripts/zig-build.sh linux-arm64 --dll
#   ./scripts/zig-build.sh macos-arm64

set -e

cd "$(dirname "$0")/.."

SOURCES="
  src/core.c src/debug.c src/arena.c
  src/math2.c src/moc3.c src/model.c src/update.c
  src/param.c src/part.c src/deformer.c src/artmesh.c
  src/glue.c src/offscreen.c src/blendshape.c
  src/interpolate.c src/render.c
"

usage() {
  sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
  exit 1
}

[ $# -lt 1 ] && usage

TARGET="$1"; shift
ZIG_TARGET=""
IS_WINDOWS=0
IS_MACOS=0
COMPAT=""
BUILD_STATIC=0
BUILD_SHARED=0
OUTDIR=""

case "$TARGET" in
  linux-x64)    ZIG_TARGET="x86_64-linux-gnu" ;;
  linux-arm64)  ZIG_TARGET="aarch64-linux-gnu" ;;
  macos-x64)    ZIG_TARGET="x86_64-macos" ;;
  macos-arm64)  ZIG_TARGET="aarch64-macos"; IS_MACOS=1 ;;
  win-x64)      ZIG_TARGET="x86_64-windows-gnu"; IS_WINDOWS=1 ;;
  win-x86)      ZIG_TARGET="x86-windows-gnu"; IS_WINDOWS=1 ;;
  win-arm64)    ZIG_TARGET="aarch64-windows-gnu"; IS_WINDOWS=1 ;;
  native)       ZIG_TARGET="" ;;
  *)            echo "Unknown target: $TARGET"; usage ;;
esac

while [ $# -gt 0 ]; do
  case "$1" in
  --v5)     COMPAT="-DPSM_COMPAT_VERSION=0x05010000L" ;;
  --v6)     COMPAT="-DPSM_COMPAT_VERSION=0x06000001L" ;;
  --dll)    BUILD_SHARED=1 ;;
  --static) BUILD_STATIC=1 ;;
  --both)   BUILD_STATIC=1; BUILD_SHARED=1 ;;
  --out)    OUTDIR="$2"; shift ;;
  *)        echo "Unknown option: $1"; usage ;;
  esac
  shift
done

# Defaults
if [ $BUILD_STATIC -eq 0 ] && [ $BUILD_SHARED -eq 0 ]; then
  BUILD_STATIC=1
fi
if [ -z "$COMPAT" ]; then
  if [ $IS_WINDOWS -eq 1 ]; then
  COMPAT="-DPSM_COMPAT_VERSION=0x05010000L"
  else
  COMPAT="-DPSM_COMPAT_VERSION=0x06000001L"
  fi
fi
if [ -z "$OUTDIR" ]; then
  OUTDIR="build/$TARGET"
fi

CC="${ZIG:-zig} cc"
AR="${ZIG:-zig} ar"
CFLAGS="-O2 -I./include -I./src -fPIC $COMPAT"
if [ -n "$ZIG_TARGET" ]; then
  CFLAGS="-target $ZIG_TARGET $CFLAGS"
fi

OBJDIR="$OUTDIR/obj"
mkdir -p "$OBJDIR" "$OUTDIR"

echo "target:  $TARGET ($ZIG_TARGET)"
echo "compat:  $COMPAT"
echo "output:  $OUTDIR"
echo ""

# Compile objects
for src in $SOURCES; do
  obj="$OBJDIR/$(basename "$src" .c).o"
  $CC $CFLAGS -c "$src" -o "$obj"
done

# Static library
if [ $BUILD_STATIC -eq 1 ]; then
  $AR rcs "$OUTDIR/libPurismCore.a" "$OBJDIR"/*.o
  echo "static:  $OUTDIR/libPurismCore.a"
fi

# Shared library
if [ $BUILD_SHARED -eq 1 ]; then
  if [ $IS_WINDOWS -eq 1 ]; then
  # Windows DLL with version resource
  RC_OBJ=""
  if [ -f src/version.rc.in ]; then
  COMPAT_HEX=$(echo "$COMPAT" | grep -o '0x[0-9a-fA-F]*' || echo "")
  ./scripts/gen-version-rc.sh $COMPAT_HEX > "$OBJDIR/version.rc"
  RC="${ZIG:-zig} rc"
  RC_OBJ="$OBJDIR/version.res"
  $RC /fo "$RC_OBJ" "$OBJDIR/version.rc" 2>/dev/null || RC_OBJ=""
  fi
  $CC -target "$ZIG_TARGET" -shared -O2 \
  $COMPAT -DPURISM_CORE_DLL -I./include -I./src \
  -o "$OUTDIR/PurismCore.dll" \
  $SOURCES $RC_OBJ -lm
  echo "dll:     $OUTDIR/PurismCore.dll"
  elif [ $IS_MACOS -eq 1 ]; then
  $CC -target "$ZIG_TARGET" -shared -dynamiclib -O2 \
  $COMPAT -I./include -I./src \
  -o "$OUTDIR/libPurismCore.dylib" \
  $SOURCES -lm
  echo "dylib:   $OUTDIR/libPurismCore.dylib"
  else
  $CC $CFLAGS -shared \
  -o "$OUTDIR/libPurismCore.so" \
  $SOURCES -lm
  echo "so:      $OUTDIR/libPurismCore.so"
  fi
fi

echo "done."
