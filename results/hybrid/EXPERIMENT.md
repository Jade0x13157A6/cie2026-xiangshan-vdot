# Dense/Hybrid CSA 归约结构实验验证记录

本文件记录 Dense VDOT 的 Hybrid CSA 结构实验。稀疏 VDOT 的实现见 [`patches/s24/`](../../patches/s24/) 与 [`docs/S24.md`](../../docs/S24.md)。

- **实验输入：** 本地分析工作区中的 `VIAluFix_hybrid.scala`。
- **输入源码 SHA256：** `02dd34723d9d65f488f1081d28d6b33047b556271762c3405722a39706865575`。
- **归档 Dense 实验补丁 SHA256：** `eee2983b0238f4370b51e521aa589eb5066549e08b9fb0e63726c3d2b2b84cc4`。
- **独立生成 RTL 测试：** 100,005 组通过；当时的原始调试日志体积较大，未归入此目录。
- **整机编译：** 重新生成 XiangShan RTL，并重新编译 Verilator 仿真器后执行 Difftest。
- **整机 RTL–NEMU 验证：** 76/76 组通过，见 [`validation-difftest.log`](validation-difftest.log)。
- **Dense 微基准：** Scalar 24.846、标准 RVV 13.349、Hybrid VDOT 9.015 cycles/dot，见 [`benchmark-rvv-difftest.log`](benchmark-rvv-difftest.log)。
- **五种结构的技术无关综合比较：** 见 [`docs/RESULTS.md`](../../docs/RESULTS.md)。

上述性能来自 Dense 点积微基准，综合比较属于技术无关评估。