# 2026 CIE 全国 RISC-V 高水平创新及应用大赛——香山昆明湖 V2 自定义 VDOT 指令

## 项目概述

本项目面向边缘 AI 中的 INT8 点积计算，在香山（XiangShan）昆明湖 V2 处理器的向量整数执行单元中实现自定义 `vdot.vv` 指令，并完成指令译码、硬件执行、NEMU 参考模型、差分验证及性能评估。在基础实现之上，对五种乘积归约网络进行同条件综合比较，最终选择 **Hybrid Carry-Save Adder（CSA）** 结构。

本项目的性能数据来自完整点积内核的周期测量，而不是单条指令延迟，也不代表端到端大语言模型推理速度。

## 指令功能与接口

```text
vdot.vv vd, vs2, vs1
```

一次指令读取两个向量源操作数的低 8 个有符号 INT8 元素，计算：

```math
\mathrm{VDOT} = \sum_{i=0}^{7} \mathrm{int8}(vs2_i)\times\mathrm{int8}(vs1_i)
```

计算结果写入目标向量寄存器低 32 位，目标寄存器高 96 位清零。当前实现支持 `SEW=e8`、`LMUL=m1`、`vl=8`、`vstart=0` 和不使用掩码（`vm=1`）的情况。

由于尚未添加汇编器助记符支持，软件测试使用指令机器码，例如：

```asm
.word 0xe61101d7  # vdot.vv v3, v1, v2
```

## 设计实现与优化

指令经过 XiangShan 向量译码路径进入 `VIAluFix`，执行单元计算并寄存点积结果，随后按照原有流水线周期写回。NEMU 中使用独立的 C 参考模型进行逐指令差分验证。

最终 **Hybrid CSA** 数据通路将 8 个有符号 INT8 乘积分为两组，每组 4 个乘积。每组先经过两级 3:2 CSA 压缩，再通过普通加法器求和，最后相加得到完整点积结果。该改动不改变自定义指令编码、运算语义或执行单元的流水线接口。

在相同的 Yosys 技术无关综合流程下，对五种结构进行比较：

| 归约结构 | `VIAluFix` 整模块 Generic Cells | 相对 Balanced Tree | VDOT 逻辑拓扑深度 |
|---|---:|---:|---:|
| Balanced Tree | 48,072 | 基线 | 47 |
| CSA | **47,336** | −1.53% | 52 |
| MAC Expression | 48,179 | +0.22% | 47 |
| **Hybrid CSA（最终选择）** | **47,672** | **−0.83%** | **47** |
| Fused Booth + CSA | 47,786 | −0.59% | 56 |

**指标说明：** Generic Cells 统计的是整个 `VIAluFix` 模块，并非 VDOT 新增电路的独立面积；逻辑深度为 Yosys 的拓扑级数，并非布局布线后的实际传播延迟。现有数据不能作为 ASIC 物理面积、功耗或最高频率结论。

## 功能验证与性能结果

最终 Hybrid 版本完成了以下验证：

- **独立 RTL 功能测试：100,005 / 100,005 PASS**，使用从 Chisel 生成的实际 Verilog。
- **整机 RTL–NEMU Difftest：76 / 76 PASS**，包含定向测试、8 组数据通道检查及 64 组固定随机种子测试。
- **完整处理器 RTL 重新生成、Verilator 仿真器重新编译**，测试以 `HIT GOOD TRAP` 正常结束。
- **Hybrid 性能复测**：保持与基础实现相同的点积内核周期结果。

| 实现 | 每 1000 次点积总周期 | 每次点积周期 | 相对 Hybrid VDOT 的周期比值 |
|---|---:|---:|---:|
| Scalar C | 24,846 | 24.846 | 2.756× |
| 标准 RVV（`vwmul.vv` + `vwredsum.vs`） | 13,349 | 13.349 | **1.480×** |
| **自定义 Hybrid VDOT** | **9,015** | **9.015** | **1.000×** |

性能测试包含向量配置、加载、计算、结果提取及循环开销；每轮运行 1,000 次点积，共测试 5 轮并取最佳周期数。**相较标准 RVV，Hybrid VDOT 点积内核加速 1.480×，周期数减少约 32.5%。** 该结果不应表述为完整 LLM 推理加速。

## 源代码快速浏览

评委可直接查看 [`source/xiangshan/`](source/xiangshan/src/main/scala/xiangshan/backend/fu/wrapper/VIAluFix.scala) 中的最终 Hybrid CSA 硬件实现，以及 [`source/`](source/README.md) 内的其余 Scala/Chisel、NEMU C 参考模型及测试源文件。该目录是根据固定上游版本和正式补丁还原的可阅读源码镜像；实际复现仍以 `patches/` 和 `scripts/` 为准。

## 仓库结构

| 路径 | 内容 |
|---|---|
| `patches/` | Yunsuan、Rocket Chip、XiangShan、NEMU 和 nexus-am 的源码补丁及校验值 |
| `scripts/` | 补丁应用、构建与测试脚本 |
| `docs/DESIGN.md` | 中文指令与硬件设计说明 |
| `docs/RESULTS.md` | 五种结构的综合比较、Difftest 与性能结果 |
| `results/hybrid/` | 最终 Hybrid 整机 Difftest 和 Benchmark 日志 |
| `results/` | 其他实验日志、波形及证据材料 |
| `versions.env` | 上游仓库的固定版本信息 |

## 复现方法

建议使用 `versions.env` 指定的上游版本，在 `xs-env` 的 Nix 开发环境中构建。以下命令从同时包含 `xs-env` 和本项目仓库的目录执行：

```bash
./cie2026-jade-vdot/scripts/apply-patches.sh "$(pwd)/xs-env"
./cie2026-jade-vdot/scripts/build.sh "$(pwd)/xs-env"
./cie2026-jade-vdot/scripts/test.sh "$(pwd)/xs-env"
```

`test.sh` 会运行 VDOT 正确性验证和基准程序；如果在已有工作区更新了 Benchmark 源代码，请先重新编译 `xs-env/nexus-am/apps/vdot-bench-jade`，因为测试脚本不会自动重编译二进制。重建完整处理器可能需要较大的内存。

## 适用范围与限制

本版本仅支持固定的 8-lane 有符号 INT8 点积，不支持向量掩码、通用 `vl`/`vstart` 处理或编译器 intrinsic。五种结构的比较是技术无关的逻辑综合评估，未完成标准单元映射后的 ASIC PPA 分析，也未开展完整 LLM 推理吞吐测试。

## 参考项目

[XiangShan](https://github.com/OpenXiangShan/XiangShan) · [xs-env](https://github.com/OpenXiangShan/xs-env) · [NEMU](https://github.com/OpenXiangShan/NEMU)
