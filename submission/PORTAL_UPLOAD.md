# 山海核：大赛官网分阶段提交索引

本项目在 GitHub 上为**一个工程**。官网按三个阶段分别收取文档和视频；官方 XiangShanLab PR 只用于提交加密后的作品仓库信息，不替代官网上传。

| 官网文档位置 | 阶段报告 | 对应代码与运行结果 |
|---|---|---|
| 第一阶段 | [`docs/STAGE1_REPORT.md`](../docs/STAGE1_REPORT.md) | `results/stage1/hello-xiangshan.log`、`results/vadd-wave-full.log`、`results/waveforms/vadd_execution.png` |
| 第二阶段 | [`docs/STAGE2_REPORT.md`](../docs/STAGE2_REPORT.md) | `patches/s24/`、`results/validation-difftest.log`、`source/nexus-am/apps/vdot-jade/`、`results/stage2/zero-difftest.log` |
| 第三阶段 | [`docs/STAGE3_REPORT.md`](../docs/STAGE3_REPORT.md) | `patches/s24/nemu.patch`、`results/benchmark-rvv-difftest.log`、`docs/S24_PPA.md` |

以上三个 Markdown 的 PDF 排版件在 `submission/pdf/`，可供官网文档上传。PDF 是报告的排版副本，完整工程复现步骤仍统一放在根目录 `README.md`。

## 视频上传位置

- **第一阶段**：开发环境部署与 Hello / VADD 操作过程录屏。
- **第二阶段**：指令语义、译码、执行单元数据通路、正负数及边界测试讲解录屏。
- **第三阶段**：NEMU–RTL 协同仿真、VADD/VDOT 波形及 RVV/VDOT 加速比分析录屏。

这三个视频需由实际操作录制，**本工程没有生成或声称已上传视频**。首次部署的完整原始操作历史也不能通过现有日志倒推生成。

## 分值与报告位置

| 评审项目 | 分值 | 报告位置 |
|---|---:|---|
| 阶段一交付完成度 | 15 | 阶段一报告第 1.1–1.3 节 |
| 阶段二交付完成度 | 20 | 阶段二报告第 2.1–2.3 节 |
| 阶段三交付完成度 | 25 | 阶段三报告第 3.1–3.3 节 |
| 技术路线合理性 | 10 | 阶段二报告第 2.2 节 |
| 功能验证有效性 | 10 | 阶段二报告第 2.3 节；阶段三报告第 3.1 节 |
| 微架构优化分析 | 5 | 阶段三报告第 3.5 节 |
| 软硬件协同 | 5 | 阶段三报告第 3.1 节 |
| 设计优化独特性 | 5 | 阶段二报告第 2.2 节；阶段三报告第 3.5 节 |
| 扩展应用潜力 | 5 | 阶段三报告第 3.4、3.6 节 |

## 正式提交

作品仓库：`https://github.com/Jade0x13157A6/cie2026-xiangshan-vdot`。队伍：**山海核**。先按官网阶段分别上传文档与视频；再依照官方 `XiangShanLab` 的 `SUBMISSION_GUIDE.md` 在 Fork 中生成 `01_参赛选手提交区/山海核/submission.asc` 并创建 PR。注意只提交加密的 `submission.asc`，不要把含联系人邮箱的原始 `my_submission.txt` 上传到公开仓库。
