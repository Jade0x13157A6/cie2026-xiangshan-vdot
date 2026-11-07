# Fused Booth + CSA 架构实验记录

## 实验定位

本目录记录 Dense VDOT 的 Fused Booth + CSA 归约结构实验。参赛硬件实现见 [`patches/s24/xiangshan.patch`](../../patches/s24/xiangshan.patch)。

原始 `vdot_fused_experiment.sh` 通过 radix-4 Booth 编码生成 8 个有符号 INT8 乘积的部分积行，再利用公共 19 位 CSA 压缩网络完成点积；生成独立 `VIAluFix` RTL 后运行 100,005 组测试，并以统一的 Yosys 流程比较逻辑单元数与拓扑深度。

## 实验文件

- `vdot_fused_experiment.sh`：Fused Booth 结构生成与测试脚本。
- `inputs/VIAluFix_hybrid.scala`：生成 Fused 版本所用的 Hybrid 输入源码。
- `inputs/VIAluFix_fused.scala`：实验脚本生成的 Fused Chisel 源码。
- `inputs/VdotCsaMain.scala`：独立执行单元的 Mill/Chisel 生成入口。
- `inputs/tb_vialufix_csa.sv`：Verilator 独立 RTL Testbench。
- `inputs/VIAluFix_*.scala`：其他存在的候选结构源码，当前归档：`mac`、`hybrid`、`fused`。
- `logs/fused-test.log.gz`：Verilator 测试记录（100,005 组通过）。
- `reports/`：各结构 Yosys 综合及逻辑路径分析日志。
- `SHA256SUMS`：相关文件的 SHA-256。

归档测试日志已无损压缩以减少仓库体积，需要查看原始文本可运行：`gzip -dc logs/fused-test.log.gz | less`。

## 综合比较

五种归约结构的技术无关 Yosys 综合结果：

| 结构 | `VIAluFix` 整模块 Generic Cells | VDOT 逻辑拓扑深度 |
|---|---:|---:|
| Balanced Tree | 48072 | 47 |
| CSA | 47336 | 52 |
| MAC Expression | 48179 | 47 |
| Hybrid CSA | 47672 | 47 |
| Fused Booth + CSA | 47786 | 56 |

Generic Cells 统计整个 `VIAluFix` 的技术无关逻辑单元；深度为拓扑级数。Dense 基准见 [`docs/RESULTS.md`](../../docs/RESULTS.md)，标准单元映射见 [`docs/S24_PPA.md`](../../docs/S24_PPA.md)。

## 实验脚本

`vdot_fused_experiment.sh` 使用独立分析工作区，并在运行过程中临时替换 `VIAluFix.scala` 生成 Fused Booth 结构。为避免影响其他 RTL 工作区，应在独立副本中运行。软件依赖包括 Nix/Mill、Verilator 与 Yosys。
