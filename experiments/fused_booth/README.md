# Fused Booth + CSA 架构实验记录

## 实验定位

该目录归档的是**第五种候选结构 Fused Booth + CSA** 的独立实验资料，**不是正式提交的 Hybrid CSA RTL 版本**。最终提交使用仓库根目录 `patches/03-xiangshan-vdot.patch` 中的 Hybrid CSA 数据通路。

原始 `vdot_fused_experiment.sh` 通过 radix-4 Booth 编码生成 8 个有符号 INT8 乘积的部分积行，再利用公共 19 位 CSA 压缩网络完成点积；生成独立 `VIAluFix` RTL 后运行 100,005 组测试，并以统一的 Yosys 流程比较逻辑单元数与拓扑深度。

## 已归档内容

- `vdot_fused_experiment.sh`：实验当时使用的**原始脚本**（未重新运行或改写）。
- `inputs/VIAluFix_hybrid.scala`：生成 Fused 版本所用的 Hybrid 输入源码。
- `inputs/VIAluFix_fused.scala`：实验脚本生成的 Fused Chisel 源码。
- `inputs/VdotCsaMain.scala`：独立执行单元的 Mill/Chisel 生成入口。
- `inputs/tb_vialufix_csa.sv`：Verilator 独立 RTL Testbench。
- `inputs/VIAluFix_*.scala`：其他存在的候选结构源码，当前归档：`mac`、`hybrid`、`fused`。
- `logs/fused-test.log`：实际 Verilator 测试输出（检查含 `PASS: 100005 actual VIAluFix RTL tests`）。
- `reports/`：存在于本地分析目录的各结构 Yosys 原始统计和路径分析日志。
- `SHA256SUMS`：本目录所有归档原始文件的 SHA-256。

## 综合比较

下表优先从本次实际归档的 Yosys 日志解析；未找到原始日志时不填入 README 中已有的数字。

| 结构 | `VIAluFix` 整模块 Generic Cells | VDOT 逻辑拓扑深度 |
|---|---:|---:|
| Balanced Tree | 48072 | 47 |
| CSA | 47336 | 52 |
| MAC Expression | 48179 | 47 |
| Hybrid CSA | 47672 | 47 |
| Fused Booth + CSA | 47786 | 56 |

**解释范围：** Generic Cells 是技术无关 Yosys 综合后的整个 `VIAluFix` 模块单元数，**不是仅 VDOT 的新增面积，更不是 ASIC 物理面积**；逻辑深度是拓扑级数，不是传播延迟。此实验只比较候选结构，最终 Hybrid 的整机 Difftest 与性能结果以仓库 `results/hybrid/` 和 `docs/RESULTS.md` 为准。

## 原始实验命令与局限

原脚本基于开发时固定的本地工作区结构，依赖 `~/work/xiangshan-vdot/csa-analysis`、`xs-env/XiangShan`、Nix/Mill、Verilator 和 Yosys。它会**临时替换**本地 `VIAluFix.scala` 以生成 Fused RTL，正常退出时通过 trap 恢复，**不应直接作为参赛作品的默认构建命令运行**。建议在独立的研究工作区和完成备份的情况下复做。

原脚本运行方式（仅供研究复做，非正式项目构建）：

```bash
bash experiments/fused_booth/vdot_fused_experiment.sh
```

归档脚本不会运行该命令，也不会编辑任何 `xs-env` 下的文件。本目录不包括体积较大的、可从 Chisel 重新生成的 Verilog/FIRRTL 中间产物。

### 没有找到的可选原始文件

- `inputs/VIAluFix_balanced.scala`
- `inputs/VIAluFix_csa.scala`

如果这里列出日志，说明其原件本次未从本地工作区找到；这不代表测试未发生，但不能把未找到的文件说成已经归档。
