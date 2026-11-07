# 2026 CIE 全国 RISC-V 高水平创新及应用大赛——XiangShan VDOT 指令扩展

## 项目简介

本项目基于 XiangShan 昆明湖 V2，在向量整数执行单元 `VIAluFix` 中实现 **INT8 Dense VDOT 与 2:4 结构化稀疏 VDOT**。两种指令共享 8 个有符号 INT8 乘法器；稀疏运算采用 FastSel 选择逻辑与 CSA 归约网络。项目覆盖指令译码、RTL、NEMU 参考模型、差分测试、应用性能与标准单元综合。

## 技术报告

[环境与 VADD 验证](docs/STAGE1_REPORT.md) · [VDOT 设计与功能测试](docs/STAGE2_REPORT.md) · [NEMU 协同仿真及性能分析](docs/STAGE3_REPORT.md)

## 指令与硬件实现

| 功能 | 说明 |
|---|---|
| Dense VDOT | `vdot.vv` 对 8 对 INT8 元素求点积，输出 32 位结果。 |
| 2:4 Sparse VDOT | `VDOT_S24_VV` 每组 4 个权重保留 2 个非零值；4 组计算产生两个 32 位部分和。 |
| 共享计算单元 | 根据操作类型选择激活值，复用乘法器和 CSA 网络，结果经原有流水线写回。 |
| 软件支持 | NEMU 参考实现、nexus-am 测试以及 PairAcc INT8 MLP 应用。 |

实现涉及 XiangShan、Yunsuan、Rocket Chip、NEMU 和 nexus-am，完整补丁位于 [`patches/s24/`](patches/s24/)。指令格式、数据通路与异常处理见 [`docs/S24.md`](docs/S24.md)。

## 测试与性能

XiangShan + NEMU Difftest 验证了指令执行：Dense 回归 **76/76 PASS**，全零边界测试 **2/2 PASS**。应用采用 2:4 稀疏权重的 **64→32→10 INT8 MLP**，测试 32 个样本：预测结果与 Python INT8 参考模型 **32/32 一致**，分类结果 **31/32 正确**，仿真以 `HIT GOOD TRAP` 结束。

标准 RVV 点积内核为 **13.349 cycles/dot**，自定义 Dense VDOT 为 **9.015 cycles/dot**，测得 **1.480×** 加速（五轮取最小周期数）。详见 [性能分析](docs/STAGE3_REPORT.md)。

| 应用实现 | 32 个样本的周期数（5 轮测量中位数） |
|---|---:|
| Specialized | 19,387 |
| **PairAcc** | **19,308** |

PairAcc 相比 Specialized 减少约 **0.41%** 周期。两者在同一次测试中比较；该指标不是 LLM 推理吞吐率。应用代码和运行记录见 [`software/VDOT_Real_INT8_PAIRACC/`](software/VDOT_Real_INT8_PAIRACC/) 与 [`results/vdot-s24/real-int8-pairacc/summary.txt`](results/vdot-s24/real-int8-pairacc/summary.txt)。

## 综合与时序

基于 Chisel 生成的 `VIAluFix`，使用 Yosys/ABC 和 Nangate45 标准单元库完成映射与 STA：

| 配置 | 标准单元数 | 映射面积（μm²） | 最差 Setup Slack（ns） |
|---|---:|---:|---:|
| 禁用稀疏功能 | 38,136 | 48,587.028 | 1.62 |
| FastSel 优化前 | 38,789 | 49,517.496 | 2.99 |
| **FastSel** | **38,724** | **49,354.172** | **2.71** |

稀疏功能相对未启用时的 `VIAluFix` **增加 1.579% 映射面积**；FastSel 相对优化前**减少 0.330% 面积**。STA 使用 10 ns 理想时钟约束；FastSel 的全模块最差 Slack 未优于优化前。上述面积与时序结果为布局布线前的 `VIAluFix` 模块评估。详见 [`docs/S24_PPA.md`](docs/S24_PPA.md)。

## 复现方法

本项目使用 `xs-env`（包含 XiangShan、NEMU 和 nexus-am）及本仓库的补丁和应用源码。构建时使用 Nix 开发环境。下列命令在**已具备对应基础 Git 提交的工作区**中执行；基础提交版本见本节末尾。

### 1. 准备工作区

目录结构如下，`xs-env` 与参赛仓库必须位于同一级目录：

```text
workspace/
├── xs-env/
└── cie2026-jade-vdot/
```

参赛仓库可以通过 `git clone https://github.com/Jade0x13157A6/cie2026-xiangshan-vdot.git` 获取。`xs-env` 官方地址为 `https://github.com/OpenXiangShan/xs-env.git`，但还必须具有 [`patches/s24/`](patches/s24/) 中记录的基础提交及已初始化的子模块，不能直接使用未固定版本的上游 `HEAD`。

环境依赖：Git、Nix（启用 flakes）、具备足够内存和磁盘空间的 Linux 主机。开发时的工具版本参见 [`docs/STAGE1_ENVIRONMENT_AND_VADD.md`](docs/STAGE1_ENVIRONMENT_AND_VADD.md)。

### 2. 应用补丁

从上述 `workspace/` 目录执行：

```bash
bash cie2026-jade-vdot/scripts/apply-s24-patches.sh "$(pwd)/xs-env"
```

脚本检查 `xs-env` 和五个子仓库的基础提交、补丁 SHA256 与适用状态；如果补丁已存在则跳过。如果基础版本不匹配，脚本会报错停止，不应强制应用。

### 3. 构建处理器及 NEMU

```bash
cd xs-env
nix develop -c bash -c 'source ./env.sh && bash ../cie2026-jade-vdot/scripts/build.sh "$PWD"'
```

`build.sh` 会配置并编译 NEMU、生成和编译 Kunminghu V2 的 Verilator 仿真器，同时构建 Dense 指令验证程序。处理器构建资源消耗较大。

### 4. 执行 INT8 MLP 测试

继续在 `xs-env/` 目录执行：

```bash
export VDOT_WORKSPACE="$(cd .. && pwd)"
nix develop -c bash ../cie2026-jade-vdot/software/VDOT_Real_INT8_PAIRACC/run.sh
```

`run.sh` 编译 PairAcc 应用并在 XiangShan + NEMU 上运行 Difftest，完成 3 次预热和 5 轮性能测量。结果保存在 `results/vdot-s24/real-int8-pairacc/`：

- `run.log` 应包含 `S24-PAIRACC ALL PASS` 与 `HIT GOOD TRAP`；
- `summary.txt` 给出 Specialized 与 PairAcc 的周期数。

### 源码基线

补丁对应的源码提交记录在 [`patches/s24/*-base.txt`](patches/s24/) 中。构建前需确保 `xs-env` 及其子仓库处于这些提交；本仓库保存对应的修改补丁，不包含完整的上游处理器源码。

## 目录说明

| 路径 | 内容 |
|---|---|
| [`patches/s24/`](patches/s24/) | 指令及软件补丁、基础版本 |
| [`docs/S24.md`](docs/S24.md) | 指令定义与 RTL 实现 |
| [`docs/S24_PPA.md`](docs/S24_PPA.md) | 面积、逻辑综合与 STA |
| [`software/VDOT_Real_INT8_PAIRACC/`](software/VDOT_Real_INT8_PAIRACC/) | INT8 MLP 应用与测试脚本 |
| [`results/vdot-s24/`](results/vdot-s24/) | 综合和仿真记录 |
| [`docs/STAGE1_ENVIRONMENT_AND_VADD.md`](docs/STAGE1_ENVIRONMENT_AND_VADD.md) | 环境与 VADD 验证 |

Dense 归约结构的 Balanced Tree、Hybrid CSA、Fused Booth 等对比实验保留在 [`docs/DESIGN.md`](docs/DESIGN.md)、[`docs/RESULTS.md`](docs/RESULTS.md) 和 [`experiments/`](experiments/fused_booth/)。`source/` 与 Dense 实验脚本仅供结构对比参考。

参考项目：[XiangShan](https://github.com/OpenXiangShan/XiangShan) · [xs-env](https://github.com/OpenXiangShan/xs-env) · [NEMU](https://github.com/OpenXiangShan/NEMU)
