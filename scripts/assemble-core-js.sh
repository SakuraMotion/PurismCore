#!/bin/sh
# Purism Core: assemble compatible Emscripten JS module
#
# Usage: assemble-core-js.sh <wrapper> <module> <tail>
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT
set -eu

wrapper="$1"
module="$2"
tail="$3"

cat "$wrapper"
echo ''
echo '/* Embedded Emscripten module (emcc-generated, MODULARIZE) */'
awk '
  {
    i = index($0, "if(typeof exports===")
    if (i) { s = substr($0, 1, i - 1); sub(/[ \t;]+$/, "", s); printf "%s;\n", s; exit }
    print
  }' "$module"
echo ''
cat "$tail"
