# CIE 2026 — XiangShan Kunminghu V2 Custom VDOT

Implementation of an eight-lane signed INT8 dot-product instruction, `vdot.vv`, in the XiangShan vector integer ALU. The project covers the three published CIE stages: environment/waveform verification, RTL design and functional tests, and NEMU differential testing plus a standard-RVV performance baseline.

## Instruction

`vdot.vv vd, vs2, vs1`: sum eight signed INT8 products into signed INT32; clear upper bits of the 128-bit destination. Supported: SEW=8, LMUL=1, VL=8, unmasked. Example: `.word 0xe61101d7` (`vdot.vv v3, v1, v2`).

## Key results

| Evidence | Result |
|---|---|
| VDOT directed + lane + randomized tests | **76/76 PASS** |
| RTL–NEMU Difftest | PASS (`HIT GOOD TRAP`) |
| VADD functional test | 8 lanes PASS; waveform captured |
| Scalar C | 24.846 cycles/dot |
| Standard RVV (`vwmul.vv` + `vwredsum.vs`) | 13.349 cycles/dot |
| Custom VDOT | **9.015 cycles/dot** |
| Standard RVV → VDOT speedup | **1.480×** |

Timing measures a complete dot-product kernel (vector configuration, loads, arithmetic, extraction and loop overhead), **not** single-instruction latency or end-to-end LLM inference. See [detailed results](docs/RESULTS.md) and [hardware design](docs/DESIGN.md).

## Repository layout

- `patches/` — source changes for Yunsuan, rocket-chip, XiangShan, NEMU, nexus-am.
- `scripts/` — reproducible patch application, build and test commands.
- `docs/DESIGN.md` — ISA encoding, hardware datapath, constraints.
- `docs/RESULTS.md` — directed/random tests, RVV comparison, VADD waveform analysis.
- `results/` — simulator logs, waveforms and evidence.

## Reproduce

Use the pinned upstream revisions in `versions.env`, in the XiangShan `xs-env` Nix development shell:

```bash
# Execute from the directory containing both xs-env and cie2026-jade-vdot
./cie2026-jade-vdot/scripts/apply-patches.sh "$(pwd)/xs-env"
./cie2026-jade-vdot/scripts/build.sh "$(pwd)/xs-env"
./cie2026-jade-vdot/scripts/test.sh "$(pwd)/xs-env"
```

`test.sh` runs the VDOT correctness program and benchmark binary. Before running updated benchmark sources in an existing workspace, recompile `xs-env/nexus-am/apps/vdot-bench-jade` (the test script does not rebuild binaries). The dedicated VADD executable can be built with `make -C xs-env/nexus-am/apps/vadd-wave-jade ARCH=riscv64-xs`; waveform capture requires `--dump-wave-full`.

Upstream: [xs-env](https://github.com/OpenXiangShan/xs-env) · [XiangShan](https://github.com/OpenXiangShan/XiangShan) · [NEMU](https://github.com/OpenXiangShan/NEMU).
