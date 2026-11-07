# 2:4 稀疏 VDOT 面积与时序评估

## 1. 综合流程

从 Chisel 生成 `VIAluFix` RTL，使用 `read_slang`、Yosys、`dfflibmap`、ABC 和 Nangate45 typical Liberty 完成标准单元映射，随后进行静态时序分析（STA）。评估对象为 `VIAluFix` 模块，未进行布局布线。

## 2. 标准单元映射

| 配置 | 单元数 | 面积（μm²） | 相对禁用稀疏功能 |
|---|---:|---:|---:|
| 禁用稀疏功能 | 38,136 | 48,587.028 | — |
| FastSel 优化前 | 38,789 | 49,517.496 | +1.915% |
| **FastSel** | **38,724** | **49,354.172** | **+1.579%** |

FastSel 相对优化前减少 **163.324 μm²（0.330%）** 映射面积。综合结果：[Nangate45 综合摘要](../results/vdot-s24/chisel-fastsel-ppa/nangate45/summary.txt)。

## 3. 逻辑规模与时序

独立技术无关综合比较：

| 配置 | Generic Cells | 最长逻辑路径（级） |
|---|---:|---:|
| FastSel 优化前 | 4,389 | 56 |
| FastSel | 4,409 | 50 |

FastSel 的最长拓扑路径减少 **6 级（10.7%）**，但拓扑级数不能直接换算为传播延迟。综合数据：[逻辑综合摘要](../results/vdot-s24/ppa-fastsel/summary.txt)。

完整 `VIAluFix` 在 **10 ns 理想时钟约束**下的 STA：

| 配置 | 最差 Setup Slack（ns） |
|---|---:|
| 禁用稀疏功能 | 1.62 |
| FastSel 优化前 | 2.99 |
| FastSel | 2.71 |

三种配置在该约束下均报告正 Slack。FastSel 相比优化前的最差 Slack 减少 **0.28 ns**，因此不能将其逻辑级数改善解读为整个模块的时序改善。

STA 报告：[禁用稀疏](../results/vdot-s24/chisel-fastsel-ppa/nangate45/sparse-off.sta.log) · [优化前](../results/vdot-s24/chisel-fastsel-ppa/nangate45/pre-fastsel.sta.log) · [FastSel](../results/vdot-s24/chisel-fastsel-ppa/nangate45/fastsel.sta.log)。文件校验值见[综合日志索引](../results/vdot-s24/PPA_REPORTS.md)。

## 4. 评估范围

面积为 Liberty 映射后的标准单元面积，STA 使用理想时钟、无布线寄生参数；不包含布线寄生参数和功耗分析。另有 Dense 归约结构的技术无关综合对比，详见 [`RESULTS.md`](RESULTS.md)。
