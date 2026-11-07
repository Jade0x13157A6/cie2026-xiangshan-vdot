# Dense VDOT 归约结构实验源码镜像

本目录为 Dense VDOT 归约结构实验的源码快照。参赛代码见 [`patches/s24/`](../patches/s24/)。

- `xiangshan/`：XiangShan 向量指令译码与 `VIAluFix` Hybrid CSA 数据通路。
- `yunsuan/`：操作码和类型定义。
- `rocket-chip/`：自定义指令编码。
- `nemu/`：差分测试参考模型。
- `nexus-am/`：VADD 波形测试、VDOT 正确性测试与 RVV 性能基准。

文件来源：`versions.env` 中的基础版本及 `patches/` 中的 Dense 实验补丁。`source/` 不用于替代上游仓库，不是独立可构建工程。

参赛工程通过 [`scripts/apply-s24-patches.sh`](../scripts/apply-s24-patches.sh) 应用补丁；`source/` 不作为编译输入。
