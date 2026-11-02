#!/usr/bin/env bash
# CIE 2026 XiangShan VDOT: isolated radix-4 Booth partial-product CSA experiment.
# Only VIAluFix.scala is temporarily replaced; original is restored before tests.
set -Eeuo pipefail

ROOT="${HOME}/work/xiangshan-vdot"
XS="${ROOT}/xs-env/XiangShan"
RTL="${XS}/src/main/scala/xiangshan/backend/fu/wrapper/VIAluFix.scala"
DIR="${ROOT}/csa-analysis"
SAVED="${DIR}/generated-rtl/VIAluFix_hybrid.scala"
FUSED="${DIR}/generated-rtl/VIAluFix_fused.scala"
GEN="${XS}/build/vdot-csa"
LOG="${DIR}/logs"
REPORTS="${DIR}/reports"
TB="${DIR}/testbench/tb_vialufix_csa.sv"
OBJ="${DIR}/testbench/obj_dir_fused"

cd "${ROOT}"
mkdir -p "${DIR}/generated-rtl" "${LOG}" "${REPORTS}"
for path in "$RTL" "$SAVED" "$TB" "${XS}/src/main/scala/top/VdotCsaMain.scala"; do
  if [[ ! -s "$path" ]]; then echo "ERROR: missing $path" >&2; exit 1; fi
done
if ! cmp -s "$RTL" "$SAVED"; then
  echo "ERROR: current VIAluFix.scala does not match saved Hybrid. Not changing it." >&2
  exit 1
fi
for executable in python3 nix verilator yosys; do
  command -v "$executable" >/dev/null || { echo "ERROR: missing $executable" >&2; exit 1; }
done

printf '\n[1/4] Constructing signed radix-4 Booth + fused CSA variant...\n'
python3 - "$SAVED" "$FUSED" <<'PY'
import sys
from pathlib import Path
source = Path(sys.argv[1]).read_text()
start_tag = "  private val vdotProducts ="
end_tag = "  private val vdotSum32 ="
assert source.count(start_tag) == 1, "Could not uniquely locate existing multiplier block"
assert source.count(end_tag) == 1, "Could not uniquely locate vdotSum32"
start = source.index(start_tag)
end = source.index(end_tag, start)
replacement = '''  // Experimental fused 8x signed-INT8 dot product.
  // Each 8x8 multiply is represented by four signed radix-4 Booth rows.
  // All 32 shifted rows feed one shared 19-bit carry-save compressor tree.
  private val fusedWidth = 19

  private val fusedRows = (0 until 8).flatMap { i =>
    val a = vs1(8 * i + 7, 8 * i).asSInt.pad(20)
    val b = vs2(8 * i + 7, 8 * i)
    val twiceA = a << 1
    val negA = -a
    val negTwiceA = -twiceA

    (0 until 4).map { j =>
      val code = if (j == 0) {
        Cat(b(1), b(0), 0.U(1.W))
      } else {
        Cat(b(2 * j + 1), b(2 * j), b(2 * j - 1))
      }
      val digit = MuxLookup(code, 0.S(20.W), Seq(
        1.U -> a,         2.U -> a,
        3.U -> twiceA,    4.U -> negTwiceA,
        5.U -> negA,      6.U -> negA
      ))
      val shifted = (digit << (2 * j)).asUInt
      shifted(fusedWidth - 1, 0)
    }
  }

  private def fusedCsa3(a: UInt, b: UInt, c: UInt): (UInt, UInt) = {
    val sum = a ^ b ^ c
    val carry = ((a & b) | (a & c) | (b & c)) << 1
    (sum(fusedWidth - 1, 0), carry(fusedWidth - 1, 0))
  }

  private def fusedCompress(rows: Seq[UInt]): Seq[UInt] = {
    if (rows.size <= 2) rows
    else {
      val next = rows.grouped(3).toSeq.flatMap {
        case Seq(a, b, c) =>
          val (sum, carry) = fusedCsa3(a, b, c)
          Seq(sum, carry)
        case remaining => remaining
      }
      fusedCompress(next)
    }
  }

  private val fusedCompressed = fusedCompress(fusedRows.toSeq)
  private val fusedResult = fusedCompressed(0) +& fusedCompressed(1)
  private val vdotSum = fusedResult(fusedWidth - 1, 0).asSInt

'''
Path(sys.argv[2]).write_text(source[:start] + replacement + source[end:])
print(f"Fused Chisel generated: {sys.argv[2]}")
PY

# Protect user's live code. Restore it whether generation succeeds or fails.
BACKUP="$(mktemp /tmp/VIAluFix_fused_backup.XXXXXXXX)"
cp -- "$RTL" "$BACKUP"
restore_source() {
  if [[ -f "$BACKUP" ]]; then
    cp -- "$BACKUP" "$RTL"
    rm -f -- "$BACKUP"
    echo "Hybrid source restored."
  fi
}
trap restore_source EXIT INT TERM
cp -- "$FUSED" "$RTL"
rm -f -- "${GEN}/VIAluFix.v" "${GEN}/VIAluFix.fir"

printf '\n[2/4] Elaborating standalone VIAluFix using existing Nix/Mill generator...\n'
# Nix may return a nonzero status even when the generator emitted files.
GEN_STATUS=0
(
  cd "${ROOT}/xs-env"
  nix develop -c bash -c 'cd XiangShan && mill -i xiangshan.runMain top.VdotCsaMain'
) > "${LOG}/fused-elaboration.log" 2>&1 || GEN_STATUS=$?

if [[ ! -s "${GEN}/VIAluFix.v" || ! -s "${GEN}/VIAluFix.fir" ]]; then
  echo "ERROR: Fused RTL was not generated (Nix/Mill status $GEN_STATUS)." >&2
  tail -35 "${LOG}/fused-elaboration.log" >&2
  exit 1
fi
cp -- "${GEN}/VIAluFix.v" "${DIR}/generated-rtl/VIAluFix_fused.v"
cp -- "${GEN}/VIAluFix.fir" "${DIR}/generated-rtl/VIAluFix_fused.fir"
restore_source
trap - EXIT INT TERM
cmp -s "$RTL" "$SAVED" || { echo "ERROR: Hybrid restoration mismatch" >&2; exit 1; }
if [[ "$GEN_STATUS" -ne 0 ]]; then
  echo "NOTE: Nix/Mill exit code $GEN_STATUS, but fresh RTL/FIR were generated."
fi

printf '\n[3/4] Verilating and running 100,005 real-RTL tests...\n'
if ! verilator --lint-only -Wno-fatal --top-module VIAluFix \
     "${DIR}/generated-rtl/VIAluFix_fused.v" \
     > "${LOG}/fused-lint.log" 2>&1; then
  tail -35 "${LOG}/fused-lint.log" >&2; exit 1
fi
if ! verilator --binary --timing -Wno-fatal -j 2 \
     --top-module tb_vialufix_csa --Mdir "$OBJ" \
     "${DIR}/generated-rtl/VIAluFix_fused.v" "$TB" \
     > "${LOG}/fused-test-build.log" 2>&1; then
  tail -35 "${LOG}/fused-test-build.log" >&2; exit 1
fi
if ! "${OBJ}/Vtb_vialufix_csa" > "${LOG}/fused-test.log" 2>&1; then
  tail -35 "${LOG}/fused-test.log" >&2; exit 1
fi
if ! grep -q 'PASS: 100005 actual VIAluFix RTL tests' "${LOG}/fused-test.log"; then
  echo "ERROR: testbench did not report 100005 PASS" >&2
  tail -30 "${LOG}/fused-test.log" >&2
  exit 1
fi
grep 'PASS: 100005 actual VIAluFix RTL tests' "${LOG}/fused-test.log"

printf '\n[4/4] Running identical Yosys technology-independent benchmarks...\n'
if ! yosys -Q -T -l "${REPORTS}/fused-actual-yosys.log" -p '
  read_slang csa-analysis/generated-rtl/VIAluFix_fused.v --top VIAluFix
  hierarchy -check -top VIAluFix
  synth -top VIAluFix -flatten
  stat
  ltp -noff
' > /dev/null 2>&1; then
  tail -35 "${REPORTS}/fused-actual-yosys.log" >&2; exit 1
fi
if ! yosys -Q -T -l "${REPORTS}/fused-vdot-path.log" -p '
  read_slang csa-analysis/generated-rtl/VIAluFix_fused.v --top VIAluFix
  hierarchy -check -top VIAluFix
  synth -top VIAluFix -flatten
  select -module VIAluFix
  select -set vdot_cone w:outVdotResult %ci2:+$_DFFE_PP_[Q,D] %cie*
  ltp -noff @vdot_cone
' > /dev/null 2>&1; then
  tail -35 "${REPORTS}/fused-vdot-path.log" >&2; exit 1
fi

printf '\n===== Four existing variants + fused =====\n'
printf '%-11s  %-16s  %s\n' 'Version' 'Generic cells' 'VDOT logic depth'
for version in balanced csa mac hybrid fused; do
  synth_log="${REPORTS}/${version}-actual-yosys.log"
  path_log="${REPORTS}/${version}-vdot-path.log"
  cells='?'; depth='?'
  if [[ -f "$synth_log" ]]; then
    cells="$(grep -E '^[[:space:]]+[0-9]+ cells$' "$synth_log" | tail -1 | awk '{print $1}' || true)"
  fi
  if [[ -f "$path_log" ]]; then
    depth="$(grep 'Longest topological path' "$path_log" | tail -1 | sed -n 's/.*length=\([0-9]*\).*/\1/p' || true)"
  fi
  printf '%-11s  %-16s  %s\n' "$version" "${cells:-?}" "${depth:-?}"
done
printf '\nPASS: Fused isolated RTL test and synthesis finished. Original Hybrid source preserved.\n'
printf 'Results: %s/fused-test.log, %s/fused-actual-yosys.log, %s/fused-vdot-path.log\n' "$LOG" "$REPORTS" "$REPORTS"
