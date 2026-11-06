#!/usr/bin/env bash
set -euo pipefail
ROOT="${VDOT_WORKSPACE:-$HOME/work/xiangshan-vdot}"
ROOT="$(realpath "$ROOT")"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP="$ROOT/xs-env/nexus-am/apps/vdot-real-int8-pairacc"
OUT="$ROOT/cie2026-jade-vdot/results/vdot-s24/real-int8-pairacc"
EMU="$ROOT/xs-env/XiangShan/build/emu"
NEMU="$ROOT/xs-env/NEMU/build/riscv64-nemu-interpreter-so"
mkdir -p "$APP" "$OUT"
for file in Makefile vdot_real_int8_opt.c real_weights.h; do
  [[ -s "$HERE/bench/$file" ]] || { echo "ERROR: missing $file" >&2; exit 1; }
  if [[ -f "$APP/$file" ]] && ! cmp -s "$HERE/bench/$file" "$APP/$file"; then
    echo "ERROR: existing $APP/$file differs; refusing overwrite" >&2; exit 1
  fi
  cp "$HERE/bench/$file" "$APP/$file"
done
[[ -s "$EMU" && -s "$NEMU" ]] || { echo 'ERROR: emu or NEMU missing' >&2; exit 1; }
export AM_HOME="$ROOT/xs-env/nexus-am"
echo "=== Verifying U32 requantization against IEEE float32 reference ==="
python3 "$HERE/verify_quant.py" | tee "$OUT/quant-proof.txt"
echo '=== Building 2-mode A/B: Specialized vs pairwise layer0 accumulation ==='
if ! make -C "$APP" ARCH=riscv64-xs -j2 > "$OUT/build.log" 2>&1; then
  tail -n 100 "$OUT/build.log" >&2; exit 1
fi
BIN="$APP/build/vdot-real-int8-pairacc-riscv64-xs.bin"
[[ -s "$BIN" ]] || { echo "ERROR: missing $BIN" >&2; exit 1; }
sha256sum "$APP/Makefile" "$APP/vdot_real_int8_opt.c" "$APP/real_weights.h" "$BIN" > "$OUT/sha256.txt"
if [[ "${1:-}" == '--build-only' ]]; then echo "BUILD PASS: $BIN"; exit 0; fi
echo '=== XiangShan + NEMU Difftest: 2 modes; 3 warmup + 5 measured rounds ==='
if ! "$EMU" --diff="$NEMU" -C 150000000 -i "$BIN" > "$OUT/run.log" 2>&1; then
  tail -n 100 "$OUT/run.log" >&2; exit 1
fi
grep -E 'Difftest enabled|S24-PAIRACC|HIT GOOD TRAP|HIT BAD TRAP|ABORT' "$OUT/run.log" || true
if grep -Eq 'S24-PAIRACC FAIL|HIT BAD TRAP|Core 0: ABORT|mtval different|difftest failed' "$OUT/run.log"; then
  echo 'ERROR: failure marker in run.log' >&2; exit 1
fi
grep -q 'S24-PAIRACC ALL PASS' "$OUT/run.log" || { echo 'ERROR: no ALL PASS' >&2; exit 1; }
grep -q 'HIT GOOD TRAP' "$OUT/run.log" || { echo 'ERROR: no GOOD TRAP' >&2; exit 1; }
python3 "$HERE/summarize.py" "$OUT/run.log" | tee "$OUT/summary.txt"
echo "RESULTS: $OUT"
