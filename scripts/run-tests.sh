#!/bin/sh
# Purism Core: test runner
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

STAGEPLAY="${STAGEPLAY:-./build/stageplay}"
TEST_DATA="${TEST_DATA:-testdata/moc3}"
REF_DIR="${REF_DIR:-testdata/ref}"
SCENARIOS_DIR="scripts/scenarios"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

VERBOSE=0
QUICK=0
GENERATE=0
SCENARIO_FILTER=""
while [ $# -gt 0 ]; do
  case $1 in
    -v|--verbose) VERBOSE=1; shift ;;
    -q|--quick) QUICK=1; shift ;;
    -g|--generate) GENERATE=1; shift ;;
    -s|--scenario) SCENARIO_FILTER="$2"; shift 2 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

if [ ! -x "$STAGEPLAY" ]; then
  if [ $GENERATE -eq 1 ]; then
    echo "Error: Stageplay binary not found at $STAGEPLAY"
    echo "Set STAGEPLAY=./path/to/binary"
    exit 1
  fi
  echo "Building stageplay..."
  make stageplay || { echo "Error: Failed to build stageplay"; exit 1; }
fi

mkdir -p "$REF_DIR"

if [ -n "$SCENARIO_FILTER" ]; then
  SCENARIOS="$SCENARIOS_DIR/${SCENARIO_FILTER}.tcl"
  if [ ! -f "$SCENARIOS" ]; then
    echo "Error: Scenario '$SCENARIO_FILTER' not found"
    exit 1
  fi
else
  SCENARIOS=$(find "$SCENARIOS_DIR" -name "*.tcl" -type f | sort)
fi

if [ -z "$SCENARIOS" ]; then
  echo "Error: No .tcl scenario files found in $SCENARIOS_DIR"
  exit 1
fi

TOTAL=0
PASS=0
FAIL=0
SKIP=0

run_test_set() {
  data_dir="$1"

  if [ ! -d "$data_dir" ]; then
    echo "Warning: Test data directory not found: $data_dir"
    return
  fi

  if [ $QUICK -eq 1 ]; then
    moc3_files=$(find "$data_dir" -name "*.moc3" -type f | head -1)
  else
    moc3_files=$(find "$data_dir" -name "*.moc3" -type f | sort)
  fi

  if [ -z "$moc3_files" ]; then
    echo "Warning: No .moc3 files found in $data_dir"
    return
  fi

  moc3_count=$(echo "$moc3_files" | wc -l)
  scenario_count=$(echo "$SCENARIOS" | wc -l)

  if [ $GENERATE -eq 1 ]; then
    echo "[$data_dir] Generating ($moc3_count models x $scenario_count scenarios)..."
  else
    echo "[$data_dir] Running ($moc3_count models x $scenario_count scenarios)..."
  fi
  echo ""

  for moc3_file in $moc3_files; do
    rel_path=$(echo "$moc3_file" | sed "s|^$data_dir/||")
    base_name=$(echo "$rel_path" | tr '/' '_' | sed 's/.moc3$//')

    for scenario in $SCENARIOS; do
      scenario_name=$(basename "$scenario" .tcl)
      TOTAL=$((TOTAL + 1))
      out_name="${base_name}_${scenario_name}.txt"
      ref_file="$REF_DIR/$out_name"

      printf "  [%d] %s (%s) ... " "$TOTAL" "$rel_path" "$scenario_name"

      if [ $GENERATE -eq 1 ]; then
        if $STAGEPLAY -D "model_path=$moc3_file" \
            -e 'load_model $model_path' \
            -o "$ref_file" "$scenario" 2>/dev/null; then
          printf "${GREEN}OK${NC}\n"
          PASS=$((PASS + 1))
        else
          printf "${RED}FAILED${NC}\n"
          FAIL=$((FAIL + 1))
        fi
      else
        if [ ! -f "$ref_file" ]; then
          printf "${YELLOW}SKIP${NC} (no reference)\n"
          SKIP=$((SKIP + 1))
          continue
        fi

        if [ $VERBOSE -eq 1 ]; then
          if $STAGEPLAY -D "model_path=$moc3_file" \
              -e 'load_model $model_path' \
              -i "$ref_file" -v "$scenario" 2>&1; then
            printf "${GREEN}PASS${NC}\n"
            PASS=$((PASS + 1))
          else
            printf "${RED}FAIL${NC}\n"
            FAIL=$((FAIL + 1))
          fi
        else
          if $STAGEPLAY -D "model_path=$moc3_file" \
              -e 'load_model $model_path' \
              -i "$ref_file" "$scenario" 2>/dev/null; then
            printf "${GREEN}PASS${NC}\n"
            PASS=$((PASS + 1))
          else
            printf "${RED}FAIL${NC}\n"
            FAIL=$((FAIL + 1))
          fi
        fi
      fi
    done
  done
}

OLD_IFS="$IFS"
IFS=':'
for data_dir in $TEST_DATA; do
  IFS="$OLD_IFS"
  [ -z "$data_dir" ] && continue
  run_test_set "$data_dir"
done
IFS="$OLD_IFS"

echo ""
if [ $GENERATE -eq 1 ]; then
  echo "Results: $PASS generated, $FAIL failed (total: $TOTAL)"
else
  echo "Results: $PASS passed, $FAIL failed, $SKIP skipped (total: $TOTAL)"
fi

[ $FAIL -gt 0 ] && exit 1
exit 0
