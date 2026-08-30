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

NEMU_JOBS=${NEMU_JOBS:-4}
XS_JOBS=${XS_JOBS:-3}
APP_JOBS=${APP_JOBS:-4}
JVM_HEAP=${JVM_HEAP:-16G}

RESULT_DIR="$PROJECT_ROOT/results"
mkdir -p "$RESULT_DIR"

for repo in "$NOOP_HOME" "$NEMU_HOME" "$AM_HOME"; do
    if [[ ! -d "$repo" ]]; then
        echo "ERROR: required directory is missing: $repo" >&2
        exit 1
    fi
done

for tool in make gcc g++ verilator; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: missing build tool: $tool" >&2
        echo "Enter the xs-env Nix development shell first." >&2
        exit 1
    fi
done

echo "=== Build configuration ==="
echo "xs-env:     $XS_ENV_PATH"
echo "NEMU jobs:  $NEMU_JOBS"
echo "XS jobs:    $XS_JOBS"
echo "App jobs:   $APP_JOBS"
echo "JVM heap:   $JVM_HEAP"

echo "=== Configure NEMU reference model ==="

make -C "$NEMU_HOME" \
    CC=gcc \
    CXX=g++ \
    LD=g++ \
    CCACHE= \
    riscv64-xs-ref_defconfig \
    2>&1 | tee "$RESULT_DIR/build-nemu-config.log"

echo "=== Build NEMU reference model ==="

make -C "$NEMU_HOME" \
    CC=gcc \
    CXX=g++ \
    LD=g++ \
    CCACHE= \
    -j"$NEMU_JOBS" \
    2>&1 | tee "$RESULT_DIR/build-nemu.log"

echo "=== Build Kunminghu V2 simulator ==="

make -C "$NOOP_HOME" \
    emu \
    CONFIG=KunminghuV2Config \
    RELEASE=1 \
    EMU_THREADS=2 \
    EMU_TRACE=fst \
    JVM_XMX="$JVM_HEAP" \
    -j"$XS_JOBS" \
    2>&1 | tee "$RESULT_DIR/build-xiangshan.log"

echo "=== Build validation workload ==="

make -C "$AM_HOME/apps/vdot-jade" clean

make -C "$AM_HOME/apps/vdot-jade" \
    ARCH=riscv64-xs \
    -j"$APP_JOBS" \
    2>&1 | tee "$RESULT_DIR/build-validation.log"

echo "=== Build benchmark workload ==="

make -C "$AM_HOME/apps/vdot-bench-jade" clean

make -C "$AM_HOME/apps/vdot-bench-jade" \
    ARCH=riscv64-xs \
    -j"$APP_JOBS" \
    2>&1 | tee "$RESULT_DIR/build-benchmark.log"

EMU="$NOOP_HOME/build/emu"
NEMU_SO="$NEMU_HOME/build/riscv64-nemu-interpreter-so"
VALIDATION_BIN="$AM_HOME/apps/vdot-jade/build/vdot-jade-riscv64-xs.bin"
BENCHMARK_BIN="$AM_HOME/apps/vdot-bench-jade/build/vdot-bench-jade-riscv64-xs.bin"

for output in \
    "$EMU" \
    "$NEMU_SO" \
    "$VALIDATION_BIN" \
    "$BENCHMARK_BIN"
do
    if [[ ! -f "$output" ]]; then
        echo "ERROR: expected output was not generated: $output" >&2
        exit 1
    fi

    ls -lh "$output"
done

echo "Build completed successfully."
