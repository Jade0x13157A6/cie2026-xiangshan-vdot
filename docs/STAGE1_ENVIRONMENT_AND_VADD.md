# 第一阶段——香山开发环境与 VADD 验证

## 1. 开发环境

本项目使用香山（XiangShan）昆明湖 V2 处理器及 `xs-env` 开发环境。进入开发环境的命令为：

```bash
cd ~/work/xiangshan-vdot/xs-env
nix develop
```

### 开发工具版本

| 组件 | 版本 | 环境说明 |
|---|---|---|
| 主机操作系统 | Ubuntu 26.04.1 LTS | 主机 |
| Nix | 2.35.1 | 主机 |
| 主机 GCC | 15.2.0 | 主机 |
| 主机 Verilator | 5.051 development（`v5.050-102-ga6f4dd031`） | 主机环境 |
| Nix devshell Verilator | 5.048（2026-04-26） | devshell |
| Nix devshell GCC | 15.2.0 | devshell |
| Nix devshell RISC-V Linux GCC | 15.2.0 | devshell |
| Nix devshell OpenJDK | 21.0.12 | devshell |
| Nix devshell Mill | 0.12.17 | devshell |
| Nix devshell Clang | 21.1.8 | devshell |
| `riscv64-unknown-elf-gcc` | 16.1.0（`g6afcc4f6d`） | 检查时主机和 devshell 均可使用 |
| `xs-env` 提交版本 | `8ef5238` | 本地工作区提交版本 |
| `XiangShan` 提交版本 | `0255614d4` | 本地工作区提交版本 |

各仓库的补丁应用基线见 [`patches/s24/`](../patches/s24/)。

## 2. Hello XiangShan 环境测试

测试源码路径：

```text
xs-env/nexus-am/apps/hello-jade/hello.c
```

Hello 程序输出：

```text
hello xiangshan, I am Ruijue Luo, IP address: 10.172.202.240
```

IP 地址作为测试标识写入程序。

构建输出：

```text
xs-env/nexus-am/apps/hello-jade/build/hello-jade-riscv64-xs.bin
xs-env/nexus-am/apps/hello-jade/build/hello-jade-riscv64-xs.elf
xs-env/nexus-am/apps/hello-jade/build/hello-jade-riscv64-xs.txt
```

复现命令（从 `~/work/xiangshan-vdot` 执行）：

```bash
xs-env/XiangShan/build/emu \
  -i xs-env/nexus-am/apps/hello-jade/build/hello-jade-riscv64-xs.bin \
  --diff xs-env/NEMU/build/riscv64-nemu-interpreter-so \
  2>&1 | tee cie2026-jade-vdot/results/stage1/hello-xiangshan.log
```

**运行日志：** `results/stage1/hello-xiangshan.log` 包含 Hello XiangShan 的问候输出、`Difftest enabled` 和 `HIT GOOD TRAP`；这是已保存日志的核查结果。

## 3. VADD.VV 波形验证

独立测试程序路径：

```text
xs-env/nexus-am/apps/vadd-wave-jade/vadd_wave.c
```

反汇编得到的向量加法指令为：

```asm
80000168: 021101d7    vadd.vv v3, v1, v2
```

测试设置 `SEW=8`，执行 8 个有效向量元素的加法。输入与预期结果如下：

```text
A = [1, 2, 3, 4, 5, 6, 7, 8]
B = [8, 7, 6, 5, 4, 3, 2, 1]
Expected = [9, 9, 9, 9, 9, 9, 9, 9]
```

观察到 ALU 输出低 64 位为 `0x0909090909090909`，与预期的 8 个 8-bit 结果一致。测试同时报告 `vadd.vv PASS: 8 lanes` 和 `Core 0: HIT GOOD TRAP`，并启用了 RTL–NEMU Difftest。

为捕获完整时钟翻转，而不只是单一时钟边沿，波形使用 `--dump-wave-full` 生成：

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

波形分析所使用的执行单元实例路径：

```text
TOP/SimTop/l_soc/core_with_l2/core/backend/inner_vfExuBlock/exus_2/VialuFix
```

| 信号 | 观测结果 |
|---|---|
| `io_in_valid` | 在波形时间戳约 8970 ps 时有效 |
| `io_in_bits_ctrl_vpu_vsew` | `0x00`（SEW=8） |
| `io_in_bits_ctrl_fuOpType` | 所检查指令对应 `0x040` |
| `io_in_bits_data_src_0` | 低 64 位为 `0x0102030405060708` |
| `io_in_bits_data_src_1` | 低 64 位为 `0x0807060504030201` |
| `io_out_valid` | 在波形时间戳约 8972 ps 时有效 |
| `io_out_bits_res_data` | 低 64 位为 `0x0909090909090909` |
| `io_out_bits_ctrl_vecWen` | 与结果一起有效 |

波形时间戳用于定位仿真信号，并非物理门延迟。功能正确性由向量存储回读与 Difftest 结果验证。

### 测试记录

- `results/waveforms/vadd_execution.png`：GTKWave 中的输入/输出波形截图。
- `results/vadd-wave-full.log`：VADD 波形仿真日志。
- `results/vadd-wave-verification.log`：VADD 功能验证日志。

![VADD 输入与输出波形](../results/waveforms/vadd_execution.png)
