# Purism Core: zig CMake toolchain definition
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

if(NOT DEFINED ENV{ZIG_TARGET})
  message(FATAL_ERROR
    "zig-toolchain.cmake: ZIG_TARGET env var is required "
    "(e.g. x86_64-linux-gnu, aarch64-macos, x86_64-windows-gnu). "
    "It is set by each preset in CMakePresets.json.")
endif()

set(_zig_target "$ENV{ZIG_TARGET}")
find_program(ZIG_EXECUTABLE zig REQUIRED)

# Wrapper scripts in the build dir since CMake needs plain executable paths
# for CMAKE_C_COMPILER / CMAKE_AR / CMAKE_RANLIB.
set(_zig_tool_dir "${CMAKE_BINARY_DIR}/_zig-tools")
file(MAKE_DIRECTORY "${_zig_tool_dir}")

file(WRITE "${_zig_tool_dir}/zig-cc"
  "#!/bin/sh\nexec \"${ZIG_EXECUTABLE}\" cc -target ${_zig_target} \"$@\"\n")
file(WRITE "${_zig_tool_dir}/zig-cxx"
  "#!/bin/sh\nexec \"${ZIG_EXECUTABLE}\" c++ -target ${_zig_target} \"$@\"\n")
file(WRITE "${_zig_tool_dir}/zig-ar"
  "#!/bin/sh\nexec \"${ZIG_EXECUTABLE}\" ar \"$@\"\n")
file(WRITE "${_zig_tool_dir}/zig-ranlib"
  "#!/bin/sh\nexec \"${ZIG_EXECUTABLE}\" ranlib \"$@\"\n")
execute_process(COMMAND chmod +x
  "${_zig_tool_dir}/zig-cc" "${_zig_tool_dir}/zig-cxx"
  "${_zig_tool_dir}/zig-ar" "${_zig_tool_dir}/zig-ranlib")

set(CMAKE_C_COMPILER   "${_zig_tool_dir}/zig-cc")
set(CMAKE_CXX_COMPILER "${_zig_tool_dir}/zig-cxx")
set(CMAKE_AR           "${_zig_tool_dir}/zig-ar")
set(CMAKE_RANLIB       "${_zig_tool_dir}/zig-ranlib")

# Windows resource compiler. zig ships `zig rc` (LLVM-rc).
file(WRITE "${_zig_tool_dir}/zig-rc"
  "#!/bin/sh\nexec \"${ZIG_EXECUTABLE}\" rc \"$@\"\n")
execute_process(COMMAND chmod +x "${_zig_tool_dir}/zig-rc")
set(CMAKE_RC_COMPILER "${_zig_tool_dir}/zig-rc")

# Don't try to run cross binaries during configure.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Don't accidentally grab host executables for tools.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
