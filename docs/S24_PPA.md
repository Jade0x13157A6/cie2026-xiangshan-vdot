# S24 Sparse VDOT — Area and Timing Evidence

## 1. Scope and synthesis flow

This report distinguishes the final **S24 2:4 sparse VDOT** from the
previous dense-only VDOT/Hybrid CSA experiment. The mapped results use the
**VIAluFix module** generated from Chisel, not the entire XiangShan processor.

The mapped-cell experiment used `read_slang`, Yosys, `dfflibmap`, ABC and
`NangateOpenCellLibrary_typical.lib`. These are **Liberty-mapped standard-cell
area estimates**, not placed-and-routed physical area. The clock/critical-path
logs are retained separately as evidence; no physical timing closure or
measured silicon Fmax is claimed.

## 2. Nangate45 standard-cell mapped area

| Configuration | Cells | Mapped area (um²) |
|---|---:|---:|
| Sparse disabled | 38,136 | 48,587.028 |
| Pre-FastSel | 38,789 | 49,517.496 |
| **Final FastSel** | **38,724** | **49,354.172** |

Relative to *Sparse disabled*, **FastSel adds 767.144 um² (+1.579%)**
for the **entire VIAluFix block**. This is not +1.579% chip area.

Relative to *Pre-FastSel*, **FastSel saves 163.324 um² (−0.330%)**.

Original mapped-area source:
[`nangate45/summary.txt`](../results/vdot-s24/chisel-fastsel-ppa/nangate45/summary.txt).

## 3. Generic synthesis / selector experiment

The standalone ABC technology-independent comparison reports:

| Configuration | Generic cells | Longest topological path levels |
|---|---:|---:|
| Pre-FastSel | 4,389 | 56 |
| FastSel | 4,409 | 50 |

FastSel changes the generic count by **+20 (+0.46%)** and reduces the
reported topological path by **6 logic levels (−10.7%)**. Generic logic
levels are **not** timing in nanoseconds and do **not** imply an equivalent
percentage reduction in the clock period.

Source: [`ppa-fastsel/summary.txt`](../results/vdot-s24/ppa-fastsel/summary.txt).
The earlier `ppa-generic/summary.txt` recorded `cells=None`, so it must not
be quoted as a numeric gate count. It also used a standalone dense reference,
not a separately generated full XiangShan processor.

## 4. STA evidence and limitations

The following **mapped, pre-layout VIAluFix** static-timing numbers are
extracted from the original Nangate45 logs. They use a **10 ns ideal clock**
constraint; they are not post-route clock-frequency measurements.

| Mapped configuration | Worst reported setup slack (ns) |
|---|---:|
| Sparse disabled | 1.62 |
| Pre-FastSel | 2.99 |
| FastSel | 2.71 |

All three reported worst setup slacks are positive under this constraint.
Although FastSel reduces **mapped area** and the **standalone generic logic
levels**, its whole-module worst setup slack is **0.28 ns smaller**
than Pre-FastSel in this STA run. Therefore, FastSel must **not** be claimed
to improve the global mapped timing of VIAluFix. The metric is unplaced,
with ideal clocks, and includes paths unrelated to Sparse VDOT.

Original whole-module STA evidence:

- [`sparse-off.sta.log`](../results/vdot-s24/chisel-fastsel-ppa/nangate45/sparse-off.sta.log)
- [`pre-fastsel.sta.log`](../results/vdot-s24/chisel-fastsel-ppa/nangate45/pre-fastsel.sta.log)
- [`fastsel.sta.log`](../results/vdot-s24/chisel-fastsel-ppa/nangate45/fastsel.sta.log)

VDOT-specific tagged paths are also retained alongside the full reports.
No placement, routing, post-route setup timing, switching-activity-based
power, or ASIC sign-off is claimed.

## 5. Correctness and throughput are independent evidence

Functionality was verified with XiangShan + NEMU Difftest and an INT8 MLP
software workload. Best reported PairAcc result is **19,308 cycles / 32
samples**, compared with **19,387 cycles** for Specialized in the same run.
The 2:4 S24 mapped-area data cannot be interpreted as a measured
processor-wide power/performance/area trade-off by itself.

Benchmark evidence:
[`real-int8-pairacc/summary.txt`](../results/vdot-s24/real-int8-pairacc/summary.txt),
[`real-int8-pairacc/run.log.gz`](../results/vdot-s24/real-int8-pairacc/run.log.gz).

The evidence files and their SHA-256 hashes are indexed in
[`PPA_EVIDENCE_MANIFEST.md`](../results/vdot-s24/PPA_EVIDENCE_MANIFEST.md).
