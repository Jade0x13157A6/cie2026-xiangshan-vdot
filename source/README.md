# Dense VDOT 归约结构实验源码镜像

此目录保存 Dense VDOT 归约结构实验涉及的 **12 个完整源文件**，便于查看当时的 Scala/Chisel、C 与 Makefile 内容。**这不是参赛方案的最终 S24 源码；实际参赛代码以 [`patches/s24/`](../patches/s24/) 为准。**

- `xiangshan/`：XiangShan 向量指令译码与 `VIAluFix` Hybrid CSA 数据通路。
- `yunsuan/`：操作码和类型定义。
- `rocket-chip/`：自定义指令编码。
- `nemu/`：差分测试参考模型。
- `nexus-am/`：VADD 波形测试、VDOT 正确性测试与 RVV 性能基准。

**来源和一致性：** 这些文件是由 `versions.env` 中记录的 Dense 实验基础版本，加上对应的 Dense 实验补丁生成的源码镜像。`source/` 不用于替代上游仓库，不是独立可构建工程。

**参赛工程的复现入口：** 请使用根目录 [`scripts/apply-s24-patches.sh`](../scripts/apply-s24-patches.sh) 及 README 中的说明。`scripts/apply-patches.sh`、`scripts/build.sh`、`scripts/test.sh` 为 Dense 归约实验保留，勿将 `source/` 当作参赛硬件的编译输入。
