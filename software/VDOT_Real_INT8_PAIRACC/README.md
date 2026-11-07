# PairAcc：INT8 MLP 累加优化

两层 **64→32→10**、2:4 稀疏 INT8 MLP，测试 32 个样本。PairAcc 将第 0 层的部分和累加顺序调整为 `v24 += v16`、`v8 += v24`，无需修改 RTL 或 NEMU。

同轮测试采用 3 次预热、5 轮测量；Specialized 为 **19,387 cycles**，PairAcc 为 **19,308 cycles**。输出与 Python INT8 参考模型 32/32 一致。

构建与运行步骤统一见[根目录 README 的「复现方法」](../../README.md#复现方法)。
