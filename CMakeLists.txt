# Purism Core: CMake build
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.21)

# Decode version from PurismCore.h
file(STRINGS include/PurismCore.h _psm_ver_line REGEX "define[ \t]+PSM_TRUE_VERSION")
string(REGEX MATCH "0x[0-9A-Fa-f]+" _psm_ver "${_psm_ver_line}")
math(EXPR _psm_major "(${_psm_ver} >> 24) & 0xFF")
math(EXPR _psm_minor "(${_psm_ver} >> 16) & 0xFF")
math(EXPR _psm_patch "${_psm_ver} & 0xFFFF")

project(PurismCore
  VERSION ${_psm_major}.${_psm_minor}.${_psm_patch}
  DESCRIPTION "Live2D Cubism Core compatible library"
  LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(PURISM_CORE_ABI "v6" CACHE STRING "ABI to build: v6 (default) or v5")
set_property(CACHE PURISM_CORE_ABI PROPERTY STRINGS v6 v5)
set(PURISM_CORE_GIT_HASH "" CACHE STRING
  "Pin the baked git revision (default: derived from git at build time)")
option(PURISM_CORE_BUILD_SAMPLES "Build the moc3info/benchmark samples" OFF)
option(PURISM_CORE_BUILD_VIEWER  "Build the raylib viewer sample (desktop)" OFF)
option(PURISM_CORE_BUILD_TESTS   "Build the unit/conformance tests (ctest)" OFF)
option(PURISM_CORE_VERSION_RC
  "Embed a Windows version resource in the DLL (needs a working RC compiler)"
  ON)
set(PURISM_CORE_BIN2H "" CACHE FILEPATH
  "Pre-built host bin2h tool for cross-compile viewer embeds (optional)")
set(PURISM_CORE_RAYLIB_DIR "" CACHE PATH
  "Prebuilt raylib tree (include/ + lib/) for the viewer (optional)")
if(PURISM_CORE_ABI STREQUAL "v5")
  set(_psm_abi_def PSM_COMPAT_VERSION=0x05010000L)
  set(_psm_suffix "-v5")
else()
  set(_psm_abi_def "")
  set(_psm_suffix "")
endif()

set(PURISM_CORE_SOURCES
  src/core.c src/debug.c src/arena.c src/math2.c src/moc3.c src/verify.c
  src/model.c src/update.c src/param.c src/part.c src/deformer.c src/artmesh.c
  src/glue.c src/offscreen.c src/blendshape.c src/interpolate.c src/render.c)

add_library(PurismCore ${PURISM_CORE_SOURCES})
add_library(PurismCore::PurismCore ALIAS PurismCore)

target_compile_features(PurismCore PUBLIC c_std_11)
set_target_properties(PurismCore PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME "PurismCore${_psm_suffix}"
  VERSION   ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR})

# macOS dylib install_name: @rpath/<name> so consumers find it via rpath
if(APPLE)
  set_target_properties(PurismCore PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_WITH_INSTALL_NAME_DIR ON)
endif()

# Windows dll: remove MinGW-style `lib` prefix
if(WIN32)
  set_target_properties(PurismCore PROPERTIES PREFIX "")
endif()

target_include_directories(PurismCore
  PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(_psm_abi_def)
  target_compile_definitions(PurismCore PUBLIC ${_psm_abi_def})
endif()

if(NOT WIN32)
  target_link_libraries(PurismCore PUBLIC m)
endif()

set(_psm_githash_hdr "${CMAKE_CURRENT_BINARY_DIR}/purism_core_githash.h")
add_custom_target(purism_core_githash
  BYPRODUCTS "${_psm_githash_hdr}"
  COMMAND ${CMAKE_COMMAND}
    -DHDR=${_psm_githash_hdr} -DOVERRIDE=${PURISM_CORE_GIT_HASH}
    -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitHash.cmake
  COMMENT "Resolving git revision for csmGetExtendedVersionString()")
add_dependencies(PurismCore purism_core_githash)
if(MSVC)
  set(_psm_fi "/FI${_psm_githash_hdr}")
else()
  set(_psm_fi "-include" "${_psm_githash_hdr}")
endif()
set_source_files_properties(src/core.c PROPERTIES
  OBJECT_DEPENDS "${_psm_githash_hdr}"
  COMPILE_OPTIONS "${_psm_fi}")

if(WIN32 AND BUILD_SHARED_LIBS AND PURISM_CORE_VERSION_RC)
  target_compile_definitions(PurismCore PRIVATE PURISM_CORE_DLL)
  set(PSM_VER_MAJOR ${PROJECT_VERSION_MAJOR})
  set(PSM_VER_MINOR ${PROJECT_VERSION_MINOR})
  set(PSM_VER_PATCH ${PROJECT_VERSION_PATCH})
  set(PSM_VER_STRING "${PROJECT_VERSION}")
  if(PURISM_CORE_ABI STREQUAL "v5")
    set(PSM_COMPAT_STRING "5.1.0")
  else()
    set(PSM_COMPAT_STRING "6.0.1")
  endif()
  configure_file(src/version.rc.in
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

  set(_psm_res "${CMAKE_CURRENT_BINARY_DIR}/version.rc.res")
  add_custom_command(
    OUTPUT  "${_psm_res}"
    COMMAND "${CMAKE_RC_COMPILER}" /fo "${_psm_res}"
            "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    COMMENT "Compiling Windows version resource")
  add_custom_target(purism_core_rc DEPENDS "${_psm_res}")
  add_dependencies(PurismCore purism_core_rc)
  target_link_options(PurismCore PRIVATE "${_psm_res}")
endif()

install(TARGETS PurismCore EXPORT PurismCoreTargets
  RUNTIME       DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/PurismCore.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# pkg-config (reuse the Makefile's template).
set(PREFIX "${CMAKE_INSTALL_PREFIX}")
set(ABI "${PURISM_CORE_ABI}")
set(ABI_SUFFIX "${_psm_suffix}")
set(VERSION "${PROJECT_VERSION}")
configure_file(scripts/purismcore.pc.in
  "${CMAKE_CURRENT_BINARY_DIR}/purismcore${_psm_suffix}.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/purismcore${_psm_suffix}.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# CMake package: find_package(PurismCore) -> PurismCore::PurismCore.
set(_psm_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/PurismCore")
install(EXPORT PurismCoreTargets
  NAMESPACE PurismCore:: DESTINATION "${_psm_cmakedir}")
configure_package_config_file(cmake/PurismCoreConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/PurismCoreConfig.cmake"
  INSTALL_DESTINATION "${_psm_cmakedir}")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/PurismCoreConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/PurismCoreConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/PurismCoreConfigVersion.cmake"
  DESTINATION "${_psm_cmakedir}")

if(PURISM_CORE_BUILD_SAMPLES)
  foreach(s moc3info benchmark)
    add_executable(${s} src/samples/${s}.c)
    target_include_directories(${s} PRIVATE src src/samples)
    target_link_libraries(${s} PRIVATE PurismCore::PurismCore)
    target_compile_options(${s} PRIVATE
      $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wno-unused-function>)
  endforeach()
endif()

if(PURISM_CORE_BUILD_VIEWER)
  if(PURISM_CORE_RAYLIB_DIR)
    if(WIN32)
      add_library(raylib STATIC IMPORTED)
      set_target_properties(raylib PROPERTIES
        IMPORTED_LOCATION
        "${PURISM_CORE_RAYLIB_DIR}/lib/libraylib.a"
        INTERFACE_LINK_LIBRARIES "opengl32;gdi32;winmm")
    elseif(APPLE)
      add_library(raylib SHARED IMPORTED)
      set_target_properties(raylib PROPERTIES
        IMPORTED_LOCATION
        "${PURISM_CORE_RAYLIB_DIR}/lib/libraylib.dylib")
    else()
      add_library(raylib SHARED IMPORTED)
      set_target_properties(raylib PROPERTIES
        IMPORTED_LOCATION
        "${PURISM_CORE_RAYLIB_DIR}/lib/libraylib.so")
    endif()
    target_include_directories(raylib INTERFACE
      "${PURISM_CORE_RAYLIB_DIR}/include")
  else()
    find_package(raylib 6.0 QUIET)
    if(NOT raylib_FOUND)
      include(FetchContent)
      FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 6.0)
      set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      FetchContent_MakeAvailable(raylib)
    endif()
  endif()

  if(PURISM_CORE_BIN2H)
    set(_bin2h_cmd "${PURISM_CORE_BIN2H}")
    set(_bin2h_dep "${PURISM_CORE_BIN2H}")
  else()
    add_executable(bin2h scripts/bin2h.c)
    set(_bin2h_cmd "bin2h")
    set(_bin2h_dep "bin2h")
  endif()

  set(_embed_dir "${CMAKE_CURRENT_BINARY_DIR}/embed")
  file(MAKE_DIRECTORY "${_embed_dir}")
  file(GLOB _embed_inputs
    "${CMAKE_CURRENT_SOURCE_DIR}/src/samples/viewer/shaders/*.frag")
  list(APPEND _embed_inputs
    "${CMAKE_CURRENT_SOURCE_DIR}/src/samples/cascadia.fnt"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/samples/cascadia_0.png")
  set(_viewer_embeds "")
  foreach(input IN LISTS _embed_inputs)
    get_filename_component(_name "${input}" NAME)
    string(REPLACE "." "_" _sym "${_name}")
    set(_out "${_embed_dir}/${_name}.h")
    add_custom_command(OUTPUT "${_out}"
      COMMAND "${_bin2h_cmd}" "${input}" "${_sym}" "${_out}"
      DEPENDS "${_bin2h_dep}" "${input}" VERBATIM)
    list(APPEND _viewer_embeds "${_out}")
  endforeach()

  add_executable(viewer
    src/samples/viewer/io.c src/samples/viewer/blend.c
    src/samples/viewer/graphics.c src/samples/viewer/render.c
    src/samples/viewer/panel.c src/samples/viewer/viewer.c
    ${_viewer_embeds})
  target_include_directories(viewer PRIVATE
    src src/samples/viewer src/samples/vendor "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(viewer PRIVATE PurismCore::PurismCore raylib)

  if(PURISM_CORE_RAYLIB_DIR)
    if(NOT WIN32)
      set_target_properties(viewer PROPERTIES SKIP_BUILD_RPATH TRUE)
      if(APPLE)
        target_link_options(viewer PRIVATE "SHELL:-Wl,-rpath,@loader_path")
      else()
        target_link_options(viewer PRIVATE "SHELL:-Wl,-rpath,\$ORIGIN")
      endif()
    endif()
  else()
    if(APPLE)
      target_link_libraries(viewer PRIVATE
        "-framework Cocoa" "-framework IOKit"
        "-framework CoreVideo" "-framework OpenGL")
    elseif(WIN32)
      target_link_libraries(viewer PRIVATE opengl32 gdi32 winmm)
    else()
      find_package(OpenGL REQUIRED)
      find_package(X11 REQUIRED)
      set(THREADS_PREFER_PTHREAD_FLAG ON)
      find_package(Threads REQUIRED)
      target_link_libraries(viewer PRIVATE
        OpenGL::GL X11::X11 Threads::Threads ${CMAKE_DL_LIBS} rt m)
    endif()
  endif()

  target_compile_options(viewer PRIVATE
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wno-unused-function>)
endif()

if(PURISM_CORE_BUILD_TESTS)
  enable_testing()

  add_executable(purism_unit src/tests/unit.c ${PURISM_CORE_SOURCES})
  target_include_directories(purism_unit PRIVATE src include src/samples)
  if(_psm_abi_def)
    target_compile_definitions(purism_unit PRIVATE ${_psm_abi_def})
  endif()
  target_compile_options(purism_unit PRIVATE
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wno-unused-function>)
  if(NOT WIN32)
    target_link_libraries(purism_unit PRIVATE m)
  endif()
  add_test(NAME unit COMMAND purism_unit)

  add_executable(stageplay src/tests/stageplay.c)
  target_include_directories(stageplay PRIVATE src src/samples)
  target_link_libraries(stageplay PRIVATE PurismCore::PurismCore)
  if(NOT WIN32)
    target_link_libraries(stageplay PRIVATE m)
  endif()
  if(NOT CMAKE_HOST_WIN32)
    add_test(NAME conformance
      COMMAND ${CMAKE_COMMAND} -E env "STAGEPLAY=$<TARGET_FILE:stageplay>"
              sh "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-tests.sh"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
endif()
