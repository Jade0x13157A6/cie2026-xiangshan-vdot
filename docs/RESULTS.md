# Verification and Performance Results

## 1. Platform

XiangShan Kunminghu V2 (`KunminghuV2Config`), Verilator 5.048, modified NEMU reference model, RV64GCV application workloads. RTL and NEMU run under differential testing.

## 2. Stage 1 — Vector addition waveform

The dedicated `vadd.vv v3, v1, v2` workload uses instruction encoding `0x021101d7` at PC `0x80000168`. The test confirms eight active INT8 lanes: `vadd.vv PASS: 8 lanes`; RTL–NEMU simulation terminates with `HIT GOOD TRAP`.

The waveform was captured with `--dump-wave-full` in `results/waveforms/vadd-full.fst`; the regular `--dump-wave` mode sampled only one clock phase and was not suitable for clock-edge analysis.

![VADD execution-unit waveform](../results/waveforms/vadd_execution.png)

Observed in `backend/inner_vfExuBlock/exus_2/VialuFix`:

| Signal | Observation |
|---|---|
| `io_in_valid` | asserted near trace timestamp 8970 ps |
| `io_in_bits_ctrl_fuOpType` | `0x040` |
| `io_in_bits_ctrl_vpu_vsew` | `0x00` (SEW=8) |
| `io_in_bits_data_src_0`, `src_1` | contain the two eight-element input sequences |
| `io_out_valid` | asserted near trace timestamp 8972 ps |
| `io_out_bits_res_data` | low 64 bits `0x0909090909090909` |
| `io_out_bits_ctrl_vecWen` | asserted (write-enable carried with result) |

The output matches elementwise addition of `[1,2,3,4,5,6,7,8]` and `[8,7,6,5,4,3,2,1]`. The timestamps are trace sampling units, **not physical gate delays**. This view demonstrates the functional-unit result; it does **not independently prove architectural commit timing**.

## 3. Stage 2 — Custom VDOT verification

The custom `vdot.vv` uses 8 signed INT8 products and a balanced reduction tree to return a signed 32-bit dot product. RTL–NEMU differential testing passed **76/76** cases: 4 directed cases (positive, negative, maximum, minimum), 8 lane-routing cases, and 64 fixed-seed randomized cases (`0x013157a6`). See `results/validation-difftest.log`.

## 4. Stage 3 — Standard RVV baseline and comparison

The standard RVV baseline uses `vle8.v`, `vwmul.vv`, `vwredsum.vs`, and `vmv.x.s`. The VDOT path uses two vector loads, custom `vdot.vv`, and extraction of its **32-bit** result using `SEW=32` before `vmv.x.s`. Both implementations include configuration, loads and result extraction in the timed loop.

Both paths passed four cross-implementation directed cases:

| Case | Scalar | RVV | VDOT |
|---|---:|---:|---:|
| Positive | 120 | 120 | 120 |
| Negative | -120 | -120 | -120 |
| Maximum | 131072 | 131072 | 131072 |
| Minimum | -130048 | -130048 | -130048 |

Benchmark settings: 1000 dot products per timing round, 5 rounds, 32 warm-up calls; best cycle count reported per implementation, with measurement order rotated. Source arrays are volatile; auto-vectorization disabled. Both programs ran with NEMU Difftest enabled and exited via `HIT GOOD TRAP`.

| Implementation | Total cycles / 1000 dots | Cycles / dot | Speedup vs VDOT |
|---|---:|---:|---:|
| Scalar C | 24,846 | 24.846 | 2.756× |
| Standard RVV | 13,349 | 13.349 | **1.480×** |
| Custom VDOT | 9,015 | 9.015 | 1.000× |

**Official vector-instruction baseline comparison: 13,349 / 9,015 = 1.480× speedup**, equivalent to approximately 32.5% fewer measured cycles. This is a dot-product **kernel microbenchmark**, not isolated instruction latency and not whole-LLM tokens/s. Source log: `results/benchmark-rvv-difftest.log`.

## 5. Scope and limits

- Fixed SEW=8, LMUL=1, VL=8, unmasked dot product.
- No full LLM inference performance claim.
- No ASIC PPA claim. Preliminary generic Yosys cells for the VDOT-equipped `VIAluFix` are not a baseline-versus-modified area delta.
- Simulation waveforms and testcase pass do not replace a comprehensive microarchitectural timing signoff.
