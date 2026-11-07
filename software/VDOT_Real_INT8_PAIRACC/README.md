# PairAcc：INT8 MLP 部分和累加优化

本目录包含参赛工程的完整 INT8 MLP 应用及其构建、验证脚本。模型使用 2:4 结构化稀疏权重，网络为 64→32→10，共测试 32 个样本。

PairAcc 仅调整第 0 层的部分和累加顺序：先计算 `v24 += v16`，再计算 `v8 += v24`，替代原来的连续 `v8 += v24` 和 `v8 += v16`。该变化**不修改 VDOT 指令、RTL 或 NEMU**。测试在计时之外对点积、中间结果及预测进行参考值检查；运行方式为 3 轮预热、5 轮测量，并交替比较 Specialized 与 PairAcc。

同条件记录的周期数：Specialized **19,387 cycles**，PairAcc **19,308 cycles**（32 个样本）。这些结果来自 XiangShan + NEMU 仿真，不是主机运行性能。

## 运行方法

从包含 `xs-env` 与 `cie2026-jade-vdot` 的工作区执行：

```bash
cd xs-env
nix develop -c bash ../cie2026-jade-vdot/software/VDOT_Real_INT8_PAIRACC/run.sh
```

运行前应已按照根目录 README 应用参赛硬件补丁，并构建 XiangShan 仿真器和 NEMU 参考库。该脚本负责复制已保存的应用源码、编译应用并运行 Difftest，**不负责重新构建处理器或 NEMU**。默认工作区为 `$HOME/work/xiangshan-vdot`，也可通过环境变量 `VDOT_WORKSPACE` 修改。
