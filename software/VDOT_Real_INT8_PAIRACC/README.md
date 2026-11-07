# PairAcc：INT8 MLP 累加优化

两层 **64→32→10**、2:4 稀疏 INT8 MLP，测试 32 个样本。PairAcc 将第 0 层的部分和累加顺序调整为 `v24 += v16`、`v8 += v24`，无需修改 RTL 或 NEMU。

同轮测试采用 3 次预热、5 轮测量；Specialized 为 **19,387 cycles**，PairAcc 为 **19,308 cycles**。输出与 Python INT8 参考模型 32/32 一致。

## 运行

在包含 `xs-env` 和 `cie2026-jade-vdot` 的工作区执行：

```bash
cd xs-env
nix develop -c bash ../cie2026-jade-vdot/software/VDOT_Real_INT8_PAIRACC/run.sh
```

运行前需完成硬件补丁应用并构建 XiangShan 仿真器及 NEMU。`run.sh` 负责构建应用和执行 Difftest；可通过 `VDOT_WORKSPACE` 设置工作区路径。
