#!/usr/bin/env bash
# Run inside xs-env's Nix devshell: nix develop -c bash ../cie2026-jade-vdot/scripts/run-stage2-zero.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${VDOT_WORKSPACE:-$(cd "$HERE/.." && pwd)}"
XS="$WORKSPACE/xs-env"
APP="$XS/nexus-am/apps/vdot-zero-jade"
SOURCE="$HERE/software/vdot-zero"
OUT="$HERE/results/stage2"
EMU="$XS/XiangShan/build/emu"
NEMU="$XS/NEMU/build/riscv64-nemu-interpreter-so"
[[ -s "$EMU" && -s "$NEMU" ]] || { echo 'ERROR: XiangShan emu or NEMU not built' >&2; exit 1; }
mkdir -p "$APP" "$OUT"
for file in Makefile vdot_zero.c; do
  if [[ -f "$APP/$file" ]] && ! cmp -s "$SOURCE/$file" "$APP/$file"; then
    echo "ERROR: $APP/$file differs; refusing to overwrite" >&2; exit 1
  fi
  cp "$SOURCE/$file" "$APP/$file"
done
export AM_HOME="$XS/nexus-am"
make -C "$APP" ARCH=riscv64-xs -j2 > "$OUT/zero-build.log" 2>&1 || {
  tail -n 80 "$OUT/zero-build.log" >&2; exit 1;
}
BIN="$APP/build/vdot-zero-jade-riscv64-xs.bin"
[[ -s "$BIN" ]] || { echo "ERROR: binary missing: $BIN" >&2; exit 1; }
set +e
"$EMU" --diff="$NEMU" -C 10000000 -i "$BIN" > "$OUT/zero-difftest.log" 2>&1
rc=$?
set -e
if [[ $rc -ne 0 ]] || ! grep -q 'Difftest enabled' "$OUT/zero-difftest.log" || \
   ! grep -q 'vdot.vv zero PASS: 2 cases' "$OUT/zero-difftest.log" || \
   ! grep -q 'HIT GOOD TRAP' "$OUT/zero-difftest.log"; then
  echo "ERROR: zero test failed; see $OUT/zero-difftest.log" >&2
  tail -n 30 "$OUT/zero-difftest.log" >&2
  exit 1
fi
grep -E 'Difftest enabled|vdot.vv zero PASS|HIT GOOD TRAP' "$OUT/zero-difftest.log"
echo "PASS: $OUT/zero-difftest.log"
