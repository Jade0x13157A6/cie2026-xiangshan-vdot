# 源代码镜像（便于直接阅读）

此目录提供参赛作品涉及的 **12 个完整源文件**，便于评委直接查看 Scala/Chisel、C 与 Makefile 内容。

- `xiangshan/`：XiangShan 向量指令译码与 `VIAluFix` Hybrid CSA 数据通路。
- `yunsuan/`：操作码和类型定义。
- `rocket-chip/`：自定义指令编码。
- `nemu/`：差分测试参考模型。
- `nexus-am/`：VADD 波形测试、VDOT 正确性测试与 RVV 性能基准。

**来源和一致性：** 这些文件是由 `versions.env` 中记录的上游固定版本，加上对应 `patches/` 的最终补丁生成的源码镜像。`source/` 不用于替代上游仓库，不是独立可构建工程。

**正式复现方式：** 使用根目录 `scripts/apply-patches.sh` 将补丁应用到固定版本的 `xs-env`，再使用 `scripts/build.sh` 和 `scripts/test.sh` 构建并验证。请勿直接将 `source/` 当作编译输入。
