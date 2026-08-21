#!/bin/sh
# Assemble a single-file amalgamation from a template (default src/bundle.c.in).
#
# Each `#include "PATH"` line in the template is inlined: PATH is resolved
# relative to the template's directory, and the file's contents are emitted with
# its own quoted `#include "..."` lines dropped (their targets are bundled
# elsewhere, in order, by the template). System `#include <...>` lines and
# everything else (guards, code, comments) are kept verbatim. Non-include lines
# of the template pass through unchanged, so the template doubles as the bundle's
# skeleton.
#
# Usage: bundle.sh [template] > dist/PurismCoreBundle.h
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT
set -eu
cd "$(dirname "$0")/.."

tpl="${1:-src/bundle.c.in}"
tpldir=$(dirname "$tpl")

while IFS= read -r line || [ -n "$line" ]; do
  case "$line" in
  '#include "'*)
    path=${line#*\"}      # drop up to the opening quote
    path=${path%%\"*}     # drop from the closing quote
    echo "/* ===== $path ===== */"
    grep -v '^[[:space:]]*#[[:space:]]*include[[:space:]]*"' "$tpldir/$path"
    ;;
  *)
    printf '%s\n' "$line"
    ;;
  esac
done < "$tpl"
