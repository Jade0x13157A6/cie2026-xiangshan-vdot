# Dense VDOT 归约结构实验源码镜像

本目录保存 Dense VDOT 结构实验的源码快照。当前指令扩展补丁见 [`patches/s24/`](../patches/s24/)。

- `xiangshan/`：XiangShan 向量指令译码与 `VIAluFix` Hybrid CSA 数据通路。
- `yunsuan/`：操作码和类型定义。
- `rocket-chip/`：自定义指令编码。
- `nemu/`：差分测试参考模型。
- `nexus-am/`：VADD 波形测试、VDOT 正确性测试与 RVV 性能基准。

文件来源：`versions.env` 中的基础版本及 `patches/` 中的 Dense 实验补丁。源码快照用于结构对照。

构建工程使用 [`scripts/apply-s24-patches.sh`](../scripts/apply-s24-patches.sh) 应用修改补丁。
