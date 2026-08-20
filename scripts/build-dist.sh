#!/bin/sh
# Cross-compile Purism Core SDK distribution via CMake presets + zig cc.
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

# Output layout:
#
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
#     dll/linux/x86_64/libPurismCore.so       (.so.1, .so.1.1.0 symlinks)
#     dll/macos/x86_64/libPurismCore.1.dylib  (+ .dylib symlink)
#     dll/macos/arm64/libPurismCore.1.dylib
#     dll/macos/universal/libPurismCore.1.dylib
#     dll/windows/x86_64/PurismCore.dll
#     dll/windows/x86/PurismCore.dll
#     dll/windows/arm64/PurismCore.dll
#     bin/linux/x86_64/viewer  + libraylib.so*    (rpath $ORIGIN)
#     bin/linux/arm64/viewer   + libraylib.so*
#     bin/macos/x86_64/viewer  + libraylib.dylib  (rpath @loader_path)
#     bin/macos/arm64/viewer   + libraylib.dylib
#     bin/macos/universal/Viewer.app/  (lipo'd; Contents/{Info.plist,MacOS/})
#     bin/windows/x86_64/viewer.exe    (static raylib -- self-contained)
#     bin/windows/x86/viewer.exe
#     bundle/PurismCoreBundle.h
#
# v5 compat builds go under lib-v5/ and dll-v5/ (Windows only; no viewer).
#
# Usage:
#   ./scripts/build-dist.sh              # full matrix
#   ./scripts/build-dist.sh linux        # linux x86_64 + arm64
#   ./scripts/build-dist.sh macos        # macos x86_64 + arm64 + universal
#   ./scripts/build-dist.sh windows      # windows x86_64 + x86 + arm64, v5+v6
#   ./scripts/build-dist.sh zig-cross-linux-x86_64 # single target (one preset)
#
# Viewer (bin/): opt-in via RAYLIB_DIR_<PRESET> env vars pointing at an
# extracted raylib v6.0 release archive (include/ + lib/). Mapping:
#   RAYLIB_DIR_LINUX_AMD64   -> zig-cross-linux-x86_64
#   RAYLIB_DIR_LINUX_ARM64   -> zig-cross-linux-arm64
#   RAYLIB_DIR_MACOS         -> zig-cross-macos-x86_64 AND zig-cross-macos-arm64 (universal raylib)
#   RAYLIB_DIR_WINDOWS_AMD64 -> zig-cross-windows-x86_64
#   RAYLIB_DIR_WINDOWS_X86   -> zig-cross-windows-x86
#   (zig-cross-windows-arm64 viewer: no raylib v6.0 mingw archive ships for it -- skip.)

set -e
cd "$(dirname "$0")/.."

DIST="${DISTDIR:-dist/sdk}"
TMP="build/_cmake-dist"

# Per-target metadata
# Format: `<preset-name> <os-name> <arch-name> <raylib-envvar-or-empty>`
TARGETS=$(cat <<EOF
zig-cross-linux-x86_64    linux    x86_64  RAYLIB_DIR_LINUX_AMD64
zig-cross-linux-arm64     linux    arm64   RAYLIB_DIR_LINUX_ARM64
zig-cross-macos-x86_64    macos    x86_64  RAYLIB_DIR_MACOS
zig-cross-macos-arm64     macos    arm64   RAYLIB_DIR_MACOS
zig-cross-windows-x86_64  windows  x86_64  RAYLIB_DIR_WINDOWS_AMD64
zig-cross-windows-x86     windows  x86     RAYLIB_DIR_WINDOWS_X86
zig-cross-windows-arm64   windows  arm64
EOF
)

# Iterate target tuples line-by-line, calling the given cb with 4 args.
each_target() {
  cb="$1"; pat="$2"
  echo "$TARGETS" | while IFS= read -r line; do
    [ -z "$line" ] && continue
    set -- $line
    preset="$1"; os="$2"; arch="$3"; raylib_env="$4"
    case "$pat" in
      all)              : ;;
      "$preset"|"$os")  : ;;
      *)                continue ;;
    esac
    "$cb" "$preset" "$os" "$arch" "$raylib_env"
  done
}

# Output dirs for a given target's library + (optionally) viewer.
libdir_for()  { echo "lib/$1/$2"; } # <os> <arch>
dlldir_for()  { echo "dll/$1/$2"; } # (no viewer here, see bindir_for)
bindir_for()  { echo "bin/$1/$2"; } # <os> <arch>

# build one (target, ABI, flavor) combination
# Args: <preset> <ABI (v5|v6)> <shared (1|0)>
# Echoes the resulting build dir to stdout (last line).
build_one() {
  preset="$1" abi="$2" shared="$3"
  dir="$TMP/$preset-$abi-$(if [ "$shared" = 1 ]; then echo shared; else echo static; fi)"
  rm -rf "$dir"
  cmake --preset "$preset" -DPURISM_CORE_ABI="$abi" \
        -DBUILD_SHARED_LIBS=$([ "$shared" = 1 ] && echo ON || echo OFF) \
        -B "$dir" >/dev/null 2>&1
  cmake --build "$dir" -j"$(nproc)" >/dev/null 2>&1
  echo "$dir"
}

# build the library artifacts for one target
# Args: <preset> <os> <arch> <raylib_env> <ABI (v5|v6)>
build_libs() {
  preset="$1"; os="$2"; arch="$3"; _re="$4"; abi="$5"
  # CMake's OUTPUT_NAME is PurismCore${_psm_suffix}, where _psm_suffix is
  # "-v5" for v5 and "" for v6 -- so the produced filenames carry -v5 for v5
  # builds. The dist layout convention puts -v5 in the *directory* (lib-v5/
  # dll-v5/), never the filename, so strip the suffix when copying.
  suf=""
  if [ "$abi" = v5 ]; then suf="-v5"; fi

  echo "  [$preset] static ($abi)"
  dir="$(build_one "$preset" "$abi" 0)"
  outlibdir="$DIST/$(libdir_for "$os" "$arch")"
  if [ "$abi" = v5 ]; then outlibdir="$DIST/lib-v5/$os/$arch"; fi
  mkdir -p "$outlibdir"
  # Linux/macOS produce libPurismCore<suf>.a; Windows produces PurismCore<suf>.a
  # (PREFIX ""). The bare-name destination matches the original Cubism Core
  # convention (no -v5 in the filename). When suf is empty (v6), the strip
  # step is a literal copy.
  strip_suf() {  # <src> <destdir>
    src="$1"; dest="$2"
    if [ -n "$suf" ]; then
      cp -a "$src" "$dest/$(basename "$src" | sed "s/$suf//")"
    else
      cp -a "$src" "$dest/"
    fi
  }
  for src in "$dir"/libPurismCore$suf.a "$dir"/PurismCore$suf.a; do
    [ -f "$src" ] && strip_suf "$src" "$outlibdir"
  done

  echo "  [$preset] shared ($abi)"
  dir="$(build_one "$preset" "$abi" 1)"
  outdlldir="$DIST/$(dlldir_for "$os" "$arch")"
  if [ "$abi" = v5 ]; then outdlldir="$DIST/dll-v5/$os/$arch"; fi
  mkdir -p "$outdlldir"
  # Copy the produced shared lib *and any soname symlinks*, stripping the
  # -v5 suffix where present (only Windows v5 builds carry it in the filename):
  #   Linux:   libPurismCore.so + .so.1 + .so.1.x.y    (no suf for v6-only)
  #   macOS:   libPurismCore.dylib + .1.dylib + .1.x.y.dylib
  #   Windows: PurismCore.dll + PurismCore-v5.dll
  if [ "$os" = windows ]; then
    src="$dir/PurismCore$suf.dll"
    if [ -f "$src" ]; then
      if [ -n "$suf" ]; then
        cp -a "$src" "$outdlldir/$(basename "$src" | sed "s/$suf//")"
      else
        cp -a "$src" "$outdlldir/"
      fi
    fi
  else
    # Linux/macOS only ship v6 (suf is "" here), so the suffix-strip is a no-op.
    cp -a "$dir"/libPurismCore$suf.so*     "$outdlldir/" 2>/dev/null || true
    cp -a "$dir"/libPurismCore$suf.*dylib  "$outdlldir/" 2>/dev/null || true
  fi
}

# build the viewer for one target
# Args: <preset> <os> <arch> <raylib_env>
build_viewer() {
  preset="$1"; os="$2"; arch="$3"; raylib_env="$4"

  # windows-arm64: no mingw raylib archive -> intentional skip.
  if [ -z "$raylib_env" ]; then
    echo "  [$preset] viewer skipped (no raylib for this target)"
    return 0
  fi
  eval "raylib_dir=\${$raylib_env:-}"
  if [ -z "$raylib_dir" ]; then
    echo "  [$preset] viewer skipped ($raylib_env not set)"
    return 0
  fi
  if [ ! -d "$raylib_dir/include" ] || [ ! -d "$raylib_dir/lib" ]; then
    # The var was set, so the viewer was requested: a bad dir is a packaging
    # error (would ship a release without viewers), not an opt-out. Fail.
    echo "error: [$preset] $raylib_env is set but $raylib_dir" >&2
    echo "       lacks include/ or lib/ -- unset $raylib_env to skip the viewer" >&2
    exit 1
  fi

  # Host-build bin2h so the cross-configure can use it (CMake's
  # add_executable(bin2h) would otherwise target the cross target).
  make -s build/bin2h >/dev/null 2>&1
  bin2h_path="$PWD/build/bin2h"

  dir="$TMP/$preset-viewer"
  rm -rf "$dir"
  echo "  [$preset] viewer (raylib $raylib_dir)"
  cmake --preset "$preset" -DPURISM_CORE_ABI=v6 -DBUILD_SHARED_LIBS=OFF \
        -DPURISM_CORE_BUILD_VIEWER=ON \
        -DPURISM_CORE_BIN2H="$bin2h_path" \
        -DPURISM_CORE_RAYLIB_DIR="$raylib_dir" \
        -B "$dir" >/dev/null 2>&1
  cmake --build "$dir" --target viewer -j"$(nproc)" >/dev/null 2>&1

  outbindir="$DIST/$(bindir_for "$os" "$arch")"
  mkdir -p "$outbindir"
  if [ "$os" = windows ]; then
    cp -a "$dir"/viewer.exe "$outbindir/"
  else
    cp -a "$dir"/viewer "$outbindir/"
    # Stage raylib next to the binary so the rpath ($ORIGIN / @loader_path)
    # finds it. Absolute-path copies keep the soname symlinks intact.
    if [ "$os" = macos ]; then
      cp -a "$raylib_dir"/lib/libraylib*.dylib "$outbindir/" 2>/dev/null || true
    else
      cp -a "$raylib_dir"/lib/libraylib.so* "$outbindir/" 2>/dev/null || true
    fi
  fi
}

# Info.plist for the universal macOS .app
write_info_plist() {
  out="$1"
  ver_hex=$(sed -n 's/^#define PSM_TRUE_VERSION[[:space:]]*\(0x[0-9A-Fa-f]*\).*/\1/p' include/PurismCore.h)
  v_major=$(( ($ver_hex >> 24) & 0xFF ))
  v_minor=$(( ($ver_hex >> 16) & 0xFF ))
  v_patch=$((  $ver_hex        & 0xFFFF ))
  v_string="$v_major.$v_minor.$v_patch"
  sed "s/@PSM_VER_STRING@/$v_string/g" src/samples/viewer/Info.plist.in > "$out"
}

# Build the universal Viewer.app from per-arch macOS viewers + their raylib.
build_macos_universal_app() {
  x86_dir="$DIST/bin/macos/x86_64"
  arm_dir="$DIST/bin/macos/arm64"
  if [ ! -f "$x86_dir/viewer" ] || [ ! -f "$arm_dir/viewer" ]; then
    echo "  [macos universal] viewer skipped (per-arch viewers missing)"
    return 0
  fi
  echo "  [macos universal] Viewer.app"
  app="$DIST/bin/macos/universal/Viewer.app"
  rm -rf "$app"
  mkdir -p "$app/Contents/MacOS"
  # lipo may be absent on non-macOS dev hosts; tolerate so the rest of dist
  # still completes (the .app skeleton + Info.plist land without the binary).
  lipo -create "$x86_dir/viewer" "$arm_dir/viewer" \
       -output "$app/Contents/MacOS/viewer" 2>/dev/null || \
    echo "  [macos universal] lipo unavailable -- .app lacks the universal binary"
  cp -a "$x86_dir"/libraylib*.dylib "$app/Contents/MacOS/" 2>/dev/null || true
  write_info_plist "$app/Contents/Info.plist"
}

rm -rf "$DIST" "$TMP"
mkdir -p "$DIST"

FILTER="${1:-all}"

# Build the library artifacts (static + shared) for every matching target.
# Windows gets both v6 and v5 ABIs; Linux/macOS ship v6 only.
build_libs_for() {
  preset="$1"; os="$2"; arch="$3"; re="$4"
  if [ "$os" = windows ]; then
    build_libs "$preset" "$os" "$arch" "$re" v6
    build_libs "$preset" "$os" "$arch" "$re" v5
  else
    build_libs "$preset" "$os" "$arch" "$re" v6
  fi
}
each_target build_libs_for "$FILTER"

# Viewer (opt-in via RAYLIB_DIR_<PRESET> env vars).
each_target build_viewer "$FILTER"

# macOS universal fat binaries (lib + dll + Viewer.app)
case "$FILTER" in
  all|macos|zig-cross-macos-x86_64|zig-cross-macos-arm64)
    if [ -f "$DIST/lib/macos/x86_64/libPurismCore.a" ] && \
       [ -f "$DIST/lib/macos/arm64/libPurismCore.a" ]; then
      echo "  [macos universal] lib + dll"
      mkdir -p "$DIST/lib/macos/universal" "$DIST/dll/macos/universal"
      lipo -create \
        "$DIST/lib/macos/x86_64/libPurismCore.a" \
        "$DIST/lib/macos/arm64/libPurismCore.a" \
        -output "$DIST/lib/macos/universal/libPurismCore.a" 2>/dev/null || true
      # The versioned dylib name tracks PSM_TRUE_VERSION; resolve it by glob
      # (the libPurismCore{.dylib,.N.dylib} symlinks don't match *.*.*).
      dylib_ver=$(basename "$DIST"/dll/macos/x86_64/libPurismCore.*.*.*.dylib)
      if [ -f "$DIST/dll/macos/x86_64/$dylib_ver" ]; then
        lipo -create \
          "$DIST/dll/macos/x86_64/$dylib_ver" \
          "$DIST/dll/macos/arm64/$dylib_ver" \
          -output "$DIST/dll/macos/universal/$dylib_ver" \
          2>/dev/null || true
        # Recreate the versioned + bare dylib symlinks for the universal
        # slice (only when lipo actually produced the fat dylib).
        if [ -f "$DIST/dll/macos/universal/$dylib_ver" ]; then
          vnums=${dylib_ver#libPurismCore.}     # 1.x.y.dylib
          vnums=${vnums%.dylib}                 # 1.x.y
          ( cd "$DIST/dll/macos/universal" && \
            ln -sf "$dylib_ver" "libPurismCore.${vnums%%.*}.dylib" && \
            ln -sf "libPurismCore.${vnums%%.*}.dylib" libPurismCore.dylib )
        fi
      fi
      build_macos_universal_app
    fi
    ;;
esac

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

rm -rf "$TMP"

echo ""
echo "=== Done ==="
find "$DIST" -type f -not -path '*/obj/*' | sort | sed 's|^|  |'
