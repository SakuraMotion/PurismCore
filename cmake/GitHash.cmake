# Purism Core: extract git revision
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

if(OVERRIDE)
  set(hash "${OVERRIDE}")
else()
  execute_process(
    COMMAND git describe --always --dirty --tags
    WORKING_DIRECTORY "${SRCDIR}"
    OUTPUT_VARIABLE hash OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0 OR hash STREQUAL "")
    set(hash "unknown")
  endif()
endif()

set(content "#define PSM_GIT_HASH \"${hash}\"\n")
if(EXISTS "${HDR}")
  file(READ "${HDR}" old)
else()
  set(old "")
endif()
if(NOT old STREQUAL content)
  file(WRITE "${HDR}" "${content}")
endif()
