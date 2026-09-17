# CIE 2026 XiangShan VDOT

基于香山昆明湖 V2 的 RISC-V 自定义有符号 INT8 向量点积指令。

## 项目内容

本项目新增：

- XiangShan `vdot.vv` 指令译码
- 8 路有符号 INT8 并行乘法器
- 平衡加法树
- 32 位点积结果写回
- NEMU 参考模型
- RTL–NEMU 差分验证
- 正确性测试和性能测试

## 指令功能

```text
vdot.vv vd, vs2, vs1

vd[31:0] = sum(vs2[i] * vs1[i]), i = 0..7
vd[127:32] = 0
```

当前支持：

- `SEW = 8`
- `LMUL = 1`
- 8 个 signed INT8 元素
- signed INT32 结果
- 不支持 mask

示例机器码：

```text
vdot.vv v3, v1, v2
.word 0xe61101d7
```

## 验证结果

| 项目 | 结果 |
|---|---:|
| 定向边界测试 | 4/4 通过 |
| Lane 接线测试 | 8/8 通过 |
| 固定种子随机测试 | 64/64 通过 |
| RTL–NEMU 差分测试 | 通过 |
| 总测试数量 | 76 |

## 性能结果

| 实现 | 每次点积周期 |
|---|---:|
| 标量 C | 24.862 cycles |
| 自定义 VDOT | 8.203 cycles |

```text
加速比：3.030×
```

## 目录结构

```text
patches/    各上游仓库的源码补丁
scripts/    应用补丁、构建和测试脚本
results/    差分测试与性能测试结果
```

## 使用方法

假设本项目与 `xs-env` 位于同一个目录：

```bash
./cie2026-jade-vdot/scripts/apply-patches.sh "$(pwd)/xs-env"
./cie2026-jade-vdot/scripts/build.sh "$(pwd)/xs-env"
./cie2026-jade-vdot/scripts/test.sh "$(pwd)/xs-env"
```

构建命令需要在 xs-env 的 Nix 开发环境中执行。

## 上游项目

- https://github.com/OpenXiangShan/xs-env
- https://github.com/OpenXiangShan/XiangShan
- https://github.com/OpenXiangShan/NEMU
- https://github.com/OpenXiangShan/nexus-am

# CIE 2026 XiangShan VDOT [English Version]

A custom signed INT8 vector dot-product instruction for XiangShan Kunminghu V2.

## Implementation

- Added `vdot.vv` instruction decoding to XiangShan
- Implemented eight parallel signed INT8 multipliers
- Used a balanced adder tree for accumulation
- Added 32-bit result write-back
- Extended the NEMU reference model
- Added RTL–NEMU differential verification

## Instruction

```text
vdot.vv vd, vs2, vs1

vd[31:0] = sum(vs2[i] * vs1[i]), i = 0..7
vd[127:32] = 0
```

Current configuration:

- `SEW = 8`
- `LMUL = 1`
- Eight signed INT8 elements
- Signed INT32 result
- Masking is not supported

## Verification Results

| Test | Result |
|---|---:|
| Directed boundary tests | 4/4 passed |
| Lane-connectivity tests | 8/8 passed |
| Fixed-seed randomized tests | 64/64 passed |
| RTL–NEMU differential test | Passed |
| Total tests | 76 |

## Performance Results

| Implementation | Cycles per dot product |
|---|---:|
| Scalar C | 24.862 |
| Custom VDOT | 8.203 |

**Speedup: 3.03×**

## Repository Structure

```text
patches/    Source patches for the upstream repositories
scripts/    Patch, build, and test scripts
results/    Verification and performance results
```

## Usage

Place this repository and `xs-env` in the same parent directory:

```bash
./cie2026-xiangshan-vdot/scripts/apply-patches.sh "$(pwd)/xs-env"
./cie2026-xiangshan-vdot/scripts/build.sh "$(pwd)/xs-env"
./cie2026-xiangshan-vdot/scripts/test.sh "$(pwd)/xs-env"
```

The build commands must be executed in the `xs-env` Nix development environment.
