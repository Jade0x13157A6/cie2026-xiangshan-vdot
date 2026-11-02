# VDOT 指令设计说明

## 1. 设计目标

本项目把 8 次有符号 INT8 乘法和 7 次加法合并成一条自定义向量指令，并集成到香山昆明湖 V2 的向量整数 ALU。

## 2. 指令语义

```text
vdot.vv vd, vs2, vs1
```

计算过程：

```text
product[i] = signed_int8(vs2[i]) * signed_int8(vs1[i])
sum = product[0] + product[1] + ... + product[7]
```

写回结果：

```text
vd[31:0]   = 32 位点积结果
vd[127:32] = 0
```

当前软件约定：

| 参数 | 要求 |
|---|---|
| SEW | e8 |
| LMUL | m1 |
| vl | 8 |
| vstart | 0 |
| mask | 不支持，vm 必须为 1 |

## 3. 指令编码

| 字段 | 位 | 值 |
|---|---:|---|
| funct6 | 31:26 | `111001` |
| vm | 25 | `1` |
| vs2 | 24:20 | 源向量 2 |
| vs1 | 19:15 | 源向量 1 |
| funct3 | 14:12 | `000` |
| vd | 11:7 | 目标向量 |
| opcode | 6:0 | `1010111` |

示例：

```text
vdot.vv v3, v1, v2
.word 0xe61101d7
```

`funct6=111001` 与 NEMU 中预留的 `vdot` 编码一致，并避免与 `vwsmaccus` 的 `111111` 编码冲突。

## 4. 译码路径

```text
Instructions.scala
    ↓
VecDecoder
    ↓
FuType.vialuF
    ↓
VialuFixType.vdot_vv
    ↓
内部操作码 0x078
    ↓
VIAluFix
```

## 5. 硬件数据通路（最终 Hybrid CSA）

最终实现仍然计算低 8 个 signed INT8 元素的点积，不修改指令编码或写回语义。

每个 signed INT8 乘积独立生成后，先符号扩展至 19 位，再拆成两组（每组 4 个乘积）。每组利用两级 3:2 carry-save compressor 压缩为两个操作数，经普通加法器得到组内和；最后两个组内和相加，符号扩展为低 32 位结果，目标寄存器高 96 位清零。

```text
8 signed INT8 products
          |
    +-----+-----+
    |           |
  4 products  4 products
    |           |
  CSA 3:2    CSA 3:2
    |           |
  CSA 3:2    CSA 3:2
    |           |
  final add  final add
    +-----+-----+
          |
      final add
          |
    32-bit signed result
```

19 位中间结果足以覆盖 8 组 signed INT8 乘积的全部可能和。`VIAluFix` 的原有流水线接口、VDOT 译码和 NEMU 参考模型均保持不变。该选择来自对 Balanced Tree、CSA、MAC、Hybrid 和 Fused Booth + CSA 五种实现的同条件 Yosys 综合对比。测量见 `docs/RESULTS.md` 第 6 节。

## 6. 流水线与写回

`VIAluFix` 是单周期流水功能单元。

实现使用寄存器保存：

```text
outIsVdot
outVdotResult
```

它们把 VDOT 操作类型和点积结果对齐到原有写回周期。

最终写回选择：

```text
VDOT 指令 → outVdotResult
其他指令 → 原有 VIAluFix 结果
```

当 `SEW != e8` 或 `LMUL != m1` 时，硬件产生非法指令异常。

## 7. NEMU 参考模型

NEMU 使用独立的顺序 C 模型：

1. 读取两个源向量的低 8 个 signed INT8 元素。
2. 顺序执行 8 次乘法累加。
3. 写入目标向量低 32 位。
4. 清零目标向量高 96 位。
5. 与 XiangShan RTL 逐条退休指令比较。

## 8. 当前限制

- 固定计算 8 个 INT8 元素。
- 不支持 mask。
- 尚未添加汇编器 mnemonic，因此测试使用 `.word`。
- 尚未进行综合后的面积、功耗和时序评估。
- 尚未加入编译器 intrinsic。
