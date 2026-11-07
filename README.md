# 2026 CIE 全国 RISC-V 高水平创新及应用大赛——XiangShan VDOT 指令扩展

## 项目简介

本项目基于 XiangShan 昆明湖 V2，在向量整数执行单元 `VIAluFix` 中实现 **INT8 Dense VDOT 与 2:4 结构化稀疏 VDOT**。两种指令共享 8 个有符号 INT8 乘法器；稀疏运算采用 FastSel 选择逻辑与 CSA 归约网络。项目覆盖指令译码、RTL、NEMU 参考模型、差分测试、应用性能与标准单元综合。

## 指令与硬件实现

| 功能 | 说明 |
|---|---|
| Dense VDOT | `vdot.vv` 对 8 对 INT8 元素求点积，输出 32 位结果。 |
| 2:4 Sparse VDOT | `VDOT_S24_VV` 每组 4 个权重保留 2 个非零值；4 组计算产生两个 32 位部分和。 |
| 共享计算单元 | 根据操作类型选择激活值，复用乘法器和 CSA 网络，结果经原有流水线写回。 |
| 软件支持 | NEMU 参考实现、nexus-am 测试以及 PairAcc INT8 MLP 应用。 |

实现涉及 XiangShan、Yunsuan、Rocket Chip、NEMU 和 nexus-am，完整补丁位于 [`patches/s24/`](patches/s24/)。指令格式、数据通路与异常处理见 [`docs/S24.md`](docs/S24.md)。

## 测试与性能

XiangShan + NEMU Difftest 验证了指令执行；Dense 回归测试为 **76/76 PASS**。应用采用 2:4 稀疏权重的 **64→32→10 INT8 MLP**，测试 32 个样本：预测结果与 Python INT8 参考模型 **32/32 一致**，分类结果 **31/32 正确**，仿真以 `HIT GOOD TRAP` 结束。

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

稀疏功能相对未启用时的 `VIAluFix` **增加 1.579% 映射面积**；FastSel 相对优化前**减少 0.330% 面积**。STA 使用 10 ns 理想时钟约束；FastSel 的全模块最差 Slack 未优于优化前。以上为布局布线前的模块级估计，不代表整颗处理器的物理 PPA。详见 [`docs/S24_PPA.md`](docs/S24_PPA.md)。

## 复现方法

在 `xs-env` 与 `cie2026-jade-vdot` 同级目录中，先将 `xs-env` 及其子仓库检出到 [`patches/s24/`](patches/s24/) 所列基础提交并初始化子模块，再运行：

```bash
bash cie2026-jade-vdot/scripts/apply-s24-patches.sh "$(pwd)/xs-env"
```

完成 XiangShan 仿真器和 NEMU 的构建后执行应用测试：

```bash
cd xs-env
nix develop -c bash ../cie2026-jade-vdot/software/VDOT_Real_INT8_PAIRACC/run.sh
```

补丁脚本检查版本和 SHA256；应用脚本编译测试程序并运行 Difftest。处理器与 NEMU 需要单独构建，完整的全新环境重建尚未验证。

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
