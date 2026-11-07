# 2:4 稀疏 VDOT：面积与时序评估

## 1. 评估对象和流程

综合对象是从 Chisel 生成的完整 `VIAluFix` 模块，不是整颗 XiangShan 处理器。流程使用 `read_slang`、Yosys、`dfflibmap`、ABC 和 `NangateOpenCellLibrary_typical.lib` 完成标准单元映射，并使用 STA 报告组合路径与 Setup Slack。

映射面积来源于 Liberty 标准单元库，**尚未进行布局布线**。不能将其解释为整颗芯片的物理面积、最终最高工作频率或功耗。保留的 Dense/Hybrid 归约结构技术无关综合属于设计探索，不能与本节的 Nangate45 面积视作同一量纲。

## 2. Nangate45 标准单元映射面积

| 配置 | 标准单元数量 | 映射面积（μm²） |
|---|---:|---:|
| 禁用稀疏功能 | 38,136 | 48,587.028 |
| FastSel 优化前 | 38,789 | 49,517.496 |
| **FastSel（参赛方案）** | **38,724** | **49,354.172** |

相对于禁用稀疏功能的 `VIAluFix`，加入稀疏运算及 FastSel 后的面积增量是 **767.144 μm²（+1.579%）**。该百分比的分母是 `VIAluFix`，不是整个 XiangShan 处理器。

FastSel 相对于优化前减少 **163.324 μm²（−0.330%）** 的映射面积。

原始统计：[`nangate45/summary.txt`](../results/vdot-s24/chisel-fastsel-ppa/nangate45/summary.txt)。

## 3. 独立逻辑综合与选择器比较

使用相同 Yosys/ABC 技术无关综合设置，对 FastSel 优化前后独立模块的逻辑规模和拓扑最长路径进行比较：

| 配置 | Generic Cells | 最长拓扑路径（级） |
|---|---:|---:|
| FastSel 优化前 | 4,389 | 56 |
| FastSel | 4,409 | 50 |

优化后单元数 **+20（+0.46%）**，最长拓扑路径 **−6 级（−10.7%）**。**这不是以 ns 为单位的时序结果，也不代表时钟周期缩短 10.7%。**

原始记录：[`ppa-fastsel/summary.txt`](../results/vdot-s24/ppa-fastsel/summary.txt)。另一组独立对比中的 `ppa-generic/summary.txt` 将 `cells` 记录为 `None`，不能把该字段当作有效的单元数量；其中的 Dense 参考是独立重建的计算模块，而非另一颗完整处理器。

## 4. STA 测量结果与限制

以下结果来自 Nangate45 映射后的 `VIAluFix` STA 日志，采用 **10 ns 理想时钟约束**，未进行布局布线，也未考虑真实时钟树和布线寄生参数。

| 配置 | 报告的最差 Setup Slack（ns） |
|---|---:|
| 禁用稀疏功能 | 1.62 |
| FastSel 优化前 | 2.99 |
| FastSel | 2.71 |

三组报告的 Setup Slack 均为正值。但相对于优化前，FastSel 的全模块最差 Slack **减少 0.28 ns**。因此只能说 FastSel 减少了映射面积及独立逻辑测试中的拓扑深度，**不能说它改善了 `VIAluFix` 的全模块最差时序**。报告中的最差路径还可能经过与 VDOT 无关的现有向量操作逻辑。

原始 STA 日志：

- [`sparse-off.sta.log`](../results/vdot-s24/chisel-fastsel-ppa/nangate45/sparse-off.sta.log)
- [`pre-fastsel.sta.log`](../results/vdot-s24/chisel-fastsel-ppa/nangate45/pre-fastsel.sta.log)
- [`fastsel.sta.log`](../results/vdot-s24/chisel-fastsel-ppa/nangate45/fastsel.sta.log)

针对 VDOT 输出寄存器筛选的路径日志也保留在同一目录。上述数据不构成布局布线后的时序签核、功耗估计或芯片 Fmax 测量。

## 5. 与功能和应用性能评估的关系

XiangShan + NEMU Difftest 用于验证指令执行与参考模型一致性，PairAcc 用于测量 2:4 INT8 MLP 的仿真运行周期。PairAcc 结果为 **19,308 cycles / 32 个样本**，同一次对比中的 Specialized 为 **19,387 cycles**。标准单元面积与应用周期属于不同评价维度，不能直接据此声称整颗处理器的功耗或物理 PPA 改善。

应用证据：[`summary.txt`](../results/vdot-s24/real-int8-pairacc/summary.txt) 与 [`run.log.gz`](../results/vdot-s24/real-int8-pairacc/run.log.gz)。原始综合和 STA 文件、字节数及 SHA256 记录在 [`PPA_EVIDENCE_MANIFEST.md`](../results/vdot-s24/PPA_EVIDENCE_MANIFEST.md)。
