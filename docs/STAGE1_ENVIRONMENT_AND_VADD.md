# Stage 1 — XiangShan Environment and VADD Verification

## 1. Development Environment

The project uses XiangShan Kunminghu V2 with the `xs-env` development environment. The development environment was entered with:

```bash
cd ~/work/xiangshan-vdot/xs-env
nix develop
```

### Recorded versions

| Component | Version | Context |
|---|---|---|
| Host OS | Ubuntu 26.04.1 LTS | Host |
| Nix | 2.35.1 | Host |
| Host GCC | 15.2.0 | Host |
| Host Verilator | 5.051 development (`v5.050-102-ga6f4dd031`, modified) | Host; not the devshell version |
| Nix devshell Verilator | 5.048 (2026-04-26) | Devshell |
| Nix devshell GCC | 15.2.0 | Devshell |
| Nix devshell RISC-V Linux GCC | 15.2.0 | Devshell |
| Nix devshell OpenJDK | 21.0.12 | Devshell |
| Nix devshell Mill | 0.12.17 | Devshell |
| Nix devshell Clang | 21.1.8 | Devshell |
| `riscv64-unknown-elf-gcc` | 16.1.0 (`g6afcc4f6d`) | Accessible both in the checked host and devshell |
| `xs-env` revision | `8ef5238` | Local working-tree HEAD at time of inspection |
| `XiangShan` revision | `0255614d4` | Local working-tree HEAD at time of inspection; includes project development history |

The recorded Git revisions describe the **current working environment**, not necessarily the original clean upstream revisions. The submission repository separately pins upstream baseline revisions in `versions.env`.

## 2. Hello XiangShan Environment Test

Test source:

```text
xs-env/nexus-am/apps/hello-jade/hello.c
```

The source contains a `printf` statement in the requested format:

```text
hello xiangshan, I am Ruijue Luo, IP address: 10.172.202.240
```

The address in this line is a test-program literal, **not evidence of an automatically detected current IP address**.

Existing build artifacts were identified:

```text
xs-env/nexus-am/apps/hello-jade/build/hello-jade-riscv64-xs.bin
xs-env/nexus-am/apps/hello-jade/build/hello-jade-riscv64-xs.elf
xs-env/nexus-am/apps/hello-jade/build/hello-jade-riscv64-xs.txt
```

Reproduction command (from `~/work/xiangshan-vdot`):

```bash
xs-env/XiangShan/build/emu \
  -i xs-env/nexus-am/apps/hello-jade/build/hello-jade-riscv64-xs.bin \
  --diff xs-env/NEMU/build/riscv64-nemu-interpreter-so \
  2>&1 | tee cie2026-jade-vdot/results/stage1/hello-xiangshan.log
```

**Evidence to attach/check:** `results/stage1/hello-xiangshan.log` should contain the printed greeting and `Core 0: HIT GOOD TRAP`. Execution was reported successful in the session, but the log contents have not been independently inspected for this report.

## 3. VADD.VV Waveform Verification

The standalone test program is located at:

```text
xs-env/nexus-am/apps/vadd-wave-jade/vadd_wave.c
```

The disassembled vector addition instruction is:

```asm
80000168: 021101d7    vadd.vv v3, v1, v2
```

The test configures SEW=8 and executes eight active vector lanes. Its input vectors are:

```text
A = [1, 2, 3, 4, 5, 6, 7, 8]
B = [8, 7, 6, 5, 4, 3, 2, 1]
Expected = [9, 9, 9, 9, 9, 9, 9, 9]
```

The observed ALU output low 64 bits is `0x0909090909090909`, which corresponds to the eight expected 8-bit results. The test also reports `vadd.vv PASS: 8 lanes` and `Core 0: HIT GOOD TRAP` with RTL–NEMU Difftest enabled.

The waveform was generated with `--dump-wave-full` so that clock transitions, rather than only one clock edge, are captured:

```bash
cd ~/work/xiangshan-vdot
xs-env/XiangShan/build/emu \
  -i xs-env/nexus-am/apps/vadd-wave-jade/build/vadd-wave-jade-riscv64-xs.bin \
  --diff xs-env/NEMU/build/riscv64-nemu-interpreter-so \
  --dump-wave-full \
  --wave-path "$PWD/cie2026-jade-vdot/results/waveforms/vadd-full.fst" \
  -b 0 -e 10000 \
  2>&1 | tee cie2026-jade-vdot/results/vadd-wave-full.log
```

Waveform signals were inspected in the following execution-unit instance:

```text
TOP/SimTop/l_soc/core_with_l2/core/backend/inner_vfExuBlock/exus_2/VialuFix
```

| Signal | Observation |
|---|---|
| `io_in_valid` | Asserted around waveform timestamp 8970 ps |
| `io_in_bits_ctrl_vpu_vsew` | `0x00` (SEW=8) |
| `io_in_bits_ctrl_fuOpType` | `0x040` at the examined instruction |
| `io_in_bits_data_src_0` | Low 64 bits `0x0102030405060708` |
| `io_in_bits_data_src_1` | Low 64 bits `0x0807060504030201` |
| `io_out_valid` | Asserted around waveform timestamp 8972 ps |
| `io_out_bits_res_data` | Low 64 bits `0x0909090909090909` |
| `io_out_bits_ctrl_vecWen` | Asserted with the result |

The timestamp difference above is a **simulation trace interval**, not a standalone physical gate delay or reliable instruction-latency measurement. The waveform demonstrates the valid input/result sequence within the vector ALU. End-to-end functional correctness is corroborated by the vector store/readback test and Difftest pass.

### Evidence files

- `results/waveforms/vadd_execution.png` — captured GTKWave view of input/output activity.
- `results/vadd-wave-full.log` — VADD waveform simulation log.
- `results/vadd-wave-verification.log` — VADD verification log.

![VADD input/output waveform](../results/waveforms/vadd_execution.png)

## 4. Stage 1 Evidence Checklist

- [x] Identified devshell and host tool versions separately.
- [x] Identified Hello XiangShan source and prebuilt binary.
- [ ] Verify that the saved Hello XiangShan execution log contains both the required greeting and a good-trap result.
- [x] Executed and verified eight-lane VADD functional test and RTL–NEMU Difftest.
- [x] Captured and saved VADD execution-unit waveform screenshot.
- [ ] Attach a narrated screen recording of the environment/operation process if required by the contest submission system.

