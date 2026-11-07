# 2026 CIE 全国 RISC-V 高水平创新及应用大赛——XiangShan 自定义 VDOT 指令

## 项目概述

本参赛工程在 **XiangShan 昆明湖 V2（KunminghuV2Config）** 的向量整数执行单元 `VIAluFix` 中实现 INT8 点积指令，支持常规 Dense VDOT 和 **2:4 结构化稀疏 VDOT**。两种运算共用八个有符号 INT8 乘法器，稀疏运算在乘法器前通过掩码选择激活值，并使用 CSA 归约网络计算结果。最终硬件采用 **FastSel 掩码选择逻辑**；软件使用 PairAcc 对完整两层 INT8 MLP 进行测试。

**这是同一个参赛工程。** 仓库保留的 Hybrid CSA、Balanced Tree、Fused Booth 等材料是设计探索和对比实验，不是分别提交的实现，也不代表当前 RTL 的最终结果。

## 指令与实现

| 功能 | 说明 |
|---|---|
| Dense VDOT | `vdot.vv`：八组有符号 INT8 乘积求和，写入 `vd[31:0]`，其余位清零。 |
| 2:4 Sparse VDOT | `VDOT_S24_VV`（RTL 操作类型为 `vdots24_vv`）：两组独立的有符号点积部分和写入 `vd[31:0]`、`vd[63:32]`，高位清零。 |
| 稀疏数据 | `vs1[63:0]` 为 8 个激活值；`vs2[63:0]` 为 8 个非零权重；`vs2[79:64]` 为四个 2:4 掩码。 |
| 稀疏指令约束 | `SEW=e8`、`LMUL=m1`、`VL=8`、`vstart=0`，每个 4-bit 掩码必须恰有两位为 1。 |
| 硬件路径 | Rocket Chip 指令编码 → XiangShan `VecDecoder` → Yunsuan 操作类型 → `VIAluFix` → 流水线寄存与写回。 |
| 参考模型 | NEMU 实现相同运算语义，配合 XiangShan 进行 RTL–NEMU Difftest。 |

硬件与参考模型通过 [`patches/s24/`](patches/s24/) 中五份补丁交付：XiangShan、Yunsuan、Rocket Chip、NEMU 和 nexus-am。设计细节见 [指令实现说明](docs/S24.md)。

## 关键验证和性能结果

- **功能验证：** 记录了 Dense 功能回归、稀疏运算测试及完整应用的 RTL–NEMU Difftest；PairAcc 应用以 `HIT GOOD TRAP` 正常结束。
- **应用正确性：** 32/32 样本的输出与 Python INT8 参考模型一致；分类正确率为 31/32。
- **应用配置：** 两层 INT8 MLP，网络规模 64→32→10，32 个样本，2:4 结构化稀疏权重。
- **相同测试中的周期数：** PairAcc **19,308 cycles**；Specialized **19,387 cycles**。结果为 3 次预热、5 轮测量的统计值。

上述指标是完整小型 MLP 应用在仿真处理器上的周期数，**不是大语言模型推理吞吐率**。此前的标准 RVV 点积微基准采用不同测试负载，不应拿来计算本次完整应用的同条件加速比。实验记录见 [应用测试结果](results/vdot-s24/real-int8-pairacc/summary.txt) 和 [指令实现说明](docs/S24.md)。

## 标准单元面积和时序评估

对 Chisel 生成的 `VIAluFix` 进行 Yosys/ABC 综合及 Nangate45 标准单元映射，得到：

| 配置 | 标准单元数量 | 映射面积（μm²） | 10 ns 理想时钟下的最差 Setup Slack（ns） |
|---|---:|---:|---:|
| 禁用稀疏功能 | 38,136 | 48,587.028 | 1.62 |
| FastSel 优化前 | 38,789 | 49,517.496 | 2.99 |
| **FastSel（参赛硬件）** | **38,724** | **49,354.172** | **2.71** |

FastSel 相比禁用稀疏功能的 `VIAluFix` 增加 **767.144 μm²（+1.579%）** 面积；相比优化前减少 **163.324 μm²（−0.330%）**。独立技术无关综合中，选择逻辑优化使最长拓扑路径从 56 级降到 50 级；但是 **FastSel 的全模块最差 Slack 并未优于优化前**。以上面积属于未布局布线的标准单元映射结果，不是整颗 CPU 的物理面积，也不代表流片后的功耗或 Fmax。

详细流程、原始 STA 日志与限制见 [PPA 评估报告](docs/S24_PPA.md) 和 [证据校验清单](results/vdot-s24/PPA_EVIDENCE_MANIFEST.md)。

## 工程复现入口

以下是**本参赛方案唯一的默认补丁入口**。在 `xs-env` 与 `cie2026-jade-vdot` 同级的目录执行，并确保 `xs-env` 与其子仓库已检出 [`patches/s24/`](patches/s24/) 中 `*-base.txt` 指定的基础提交并初始化子模块。

```bash
bash cie2026-jade-vdot/scripts/apply-s24-patches.sh "$(pwd)/xs-env"
```

脚本校验基础 Git 提交、补丁 SHA256 和补丁适用性，然后应用五份补丁。**脚本不负责克隆、检出上游版本或重新构建仿真器。** 需要先使用现有 XiangShan/NEMU 构建流程生成仿真器与参考模型；然后运行应用：

```bash
cd xs-env
nix develop -c bash ../cie2026-jade-vdot/software/VDOT_Real_INT8_PAIRACC/run.sh
```

`run.sh` 编译应用、执行 XiangShan + NEMU Difftest 并检查结果。补丁已在记录的基础提交上完成适用性验证，但**未宣称完成全新机器上的端到端重建**。

## 仓库导航

| 目录 | 用途 |
|---|---|
| [`patches/s24/`](patches/s24/) | 参赛硬件与软件的五份源码补丁、基础提交、SHA256。 |
| [`scripts/apply-s24-patches.sh`](scripts/apply-s24-patches.sh) | 参赛方案的补丁复现入口。 |
| [`software/VDOT_Real_INT8_PAIRACC/`](software/VDOT_Real_INT8_PAIRACC/) | PairAcc 应用源码、构建与验证脚本。 |
| [`docs/S24.md`](docs/S24.md) | 指令语义、数据通路与应用验证说明。 |
| [`docs/S24_PPA.md`](docs/S24_PPA.md) | Nangate45 面积、STA 和比较条件。 |
| [`results/vdot-s24/`](results/vdot-s24/) | 应用测试、综合与时序证据。 |
| [`docs/STAGE1_ENVIRONMENT_AND_VADD.md`](docs/STAGE1_ENVIRONMENT_AND_VADD.md) | 环境与 VADD 波形验证。 |
| [`docs/DESIGN.md`](docs/DESIGN.md)、[`docs/RESULTS.md`](docs/RESULTS.md)、[`experiments/fused_booth/`](experiments/fused_booth/) | Dense VDOT 归约结构探索和历史对比数据。 |
| [`source/`](source/README.md)、`versions.env`、`scripts/apply-patches.sh` | Dense 归约结构实验的源码镜像及留存脚本，不是参赛工程的默认复现入口。 |

## 适用范围

本工程面向固定参数的 INT8 Dense/2:4 Sparse 点积运算；未实现通用掩码与任意 `VL` 的完整向量语义，也未添加编译器 intrinsic。未开展标准单元布局布线、功耗测量或完整 LLM 推理吞吐率测试。

参考项目：[XiangShan](https://github.com/OpenXiangShan/XiangShan) · [xs-env](https://github.com/OpenXiangShan/xs-env) · [NEMU](https://github.com/OpenXiangShan/NEMU)
