#!/usr/bin/env bash
# Inspection of CURRENT environment only; this cannot reconstruct historical install logs.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${VDOT_WORKSPACE:-$(cd "$HERE/.." && pwd)}"
OUT="$HERE/results/stage1/environment-inspection.log"
mkdir -p "$(dirname "$OUT")"
{
 echo 'XiangShan environment inspection (current machine, not historical setup log)'
 date -u '+UTC: %Y-%m-%dT%H:%M:%SZ'
 uname -a
 if command -v lsb_release >/dev/null; then lsb_release -a 2>&1; fi
 for bin in git nix gcc verilator java mill clang; do
   printf '\n=== %s ===\n' "$bin"
   if command -v "$bin" >/dev/null 2>&1; then
     case "$bin" in
       git) git --version ;;
       nix) nix --version ;;
       gcc) gcc --version | head -1 ;;
       verilator) verilator --version ;;
       java) java -version 2>&1 | head -3 ;;
       mill) mill --version 2>&1 | head -3 ;;
       clang) clang --version | head -1 ;;
     esac
   else echo 'not available in current shell'; fi
 done
 for rel in xs-env xs-env/XiangShan xs-env/XiangShan/yunsuan xs-env/XiangShan/rocket-chip xs-env/NEMU xs-env/nexus-am; do
    echo; echo "=== $rel ==="
    if [[ -d "$WORKSPACE/$rel" ]]; then
      git -C "$WORKSPACE/$rel" rev-parse HEAD || true
      git -C "$WORKSPACE/$rel" remote get-url origin || true
    else echo 'MISSING'; fi
 done
} > "$OUT"
echo "WROTE: $OUT"
