#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)
PROJECT_ROOT=$(
    cd "$SCRIPT_DIR/.."
    pwd
)

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /absolute/path/to/xs-env" >&2
    exit 2
fi

XS_ENV_PATH=$1
NOOP_HOME="$XS_ENV_PATH/XiangShan"
NEMU_HOME="$XS_ENV_PATH/NEMU"
AM_HOME="$XS_ENV_PATH/nexus-am"

EMU="$NOOP_HOME/build/emu"
NEMU_SO="$NEMU_HOME/build/riscv64-nemu-interpreter-so"
VALIDATION_BIN="$AM_HOME/apps/vdot-jade/build/vdot-jade-riscv64-xs.bin"
BENCHMARK_BIN="$AM_HOME/apps/vdot-bench-jade/build/vdot-bench-jade-riscv64-xs.bin"

RESULT_DIR="$PROJECT_ROOT/results"
VALIDATION_LOG="$RESULT_DIR/validation-difftest.log"
BENCHMARK_LOG="$RESULT_DIR/benchmark-difftest.log"
SUMMARY="$RESULT_DIR/latest-summary.txt"

mkdir -p "$RESULT_DIR"

for input in \
    "$EMU" \
    "$NEMU_SO" \
    "$VALIDATION_BIN" \
    "$BENCHMARK_BIN"
do
    if [[ ! -f "$input" ]]; then
        echo "ERROR: required file is missing: $input" >&2
        echo "Run scripts/build.sh first." >&2
        exit 1
    fi
done

echo "=== Run 76-case RTL-NEMU validation ==="

set +e
"$EMU" \
    --diff="$NEMU_SO" \
    -C 10000000 \
    -i "$VALIDATION_BIN" \
    2>&1 | tee "$VALIDATION_LOG"
VALIDATION_STATUS=${PIPESTATUS[0]}
set -e

if [[ "$VALIDATION_STATUS" -ne 0 ]]; then
    echo "ERROR: validation returned $VALIDATION_STATUS" >&2
    exit "$VALIDATION_STATUS"
fi

grep -q "Difftest enabled" "$VALIDATION_LOG"
grep -q "vdot.vv lane routing PASS: 8 cases" "$VALIDATION_LOG"
grep -q "vdot.vv deterministic random PASS: 64 cases" "$VALIDATION_LOG"
grep -q "vdot.vv test PASS: 76 total cases" "$VALIDATION_LOG"
grep -q "HIT GOOD TRAP" "$VALIDATION_LOG"

echo "=== Run scalar-versus-VDOT benchmark ==="

set +e
"$EMU" \
    --diff="$NEMU_SO" \
    -C 100000000 \
    -i "$BENCHMARK_BIN" \
    2>&1 | tee "$BENCHMARK_LOG"
BENCHMARK_STATUS=${PIPESTATUS[0]}
set -e

if [[ "$BENCHMARK_STATUS" -ne 0 ]]; then
    echo "ERROR: benchmark returned $BENCHMARK_STATUS" >&2
    exit "$BENCHMARK_STATUS"
fi

grep -q "Difftest enabled" "$BENCHMARK_LOG"
grep -q "correctness: scalar=120 vdot=120 expected=120" \
    "$BENCHMARK_LOG"
grep -q "vdot benchmark PASS" "$BENCHMARK_LOG"
grep -q "HIT GOOD TRAP" "$BENCHMARK_LOG"

{
    echo "CIE 2026 XiangShan VDOT test summary"
    echo
    echo "Validation:"
    grep -E \
        "lane routing PASS|deterministic random PASS|test PASS" \
        "$VALIDATION_LOG"
    echo
    echo "Performance:"
    grep -E \
        "^correctness:|^scalar:|^vdot  :|^vdot speedup:|benchmark PASS" \
        "$BENCHMARK_LOG"
} | tee "$SUMMARY"

echo
echo "All RTL-NEMU validation and benchmark checks passed."
echo "Summary: $SUMMARY"
