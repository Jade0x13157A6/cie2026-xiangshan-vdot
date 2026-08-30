# VDOT 验证与性能结果

## 1. 测试平台

| 项目 | 配置 |
|---|---|
| 主机系统 | Ubuntu 22.04 x86-64 |
| CPU 线程 | 8 |
| 内存 | 约 16 GiB |
| Swap | 16 GiB |
| 处理器配置 | KunminghuV2Config |
| RTL 仿真 | Verilator 5.048 |
| 参考模型 | NEMU RV64 XiangShan |
| 数据类型 | signed INT8 × signed INT8 → INT32 |

## 2. 正确性测试

### 定向边界测试

| 测试 | 实际值 | 期望值 |
|---|---:|---:|
| positive | 120 | 120 |
| negative | -36 | -36 |
| maximum | 131072 | 131072 |
| minimum | -130048 | -130048 |

每组测试还检查写回结果高 96 位是否全部为零。

### Lane 接线测试

测试逐次只激活一个 lane，用于发现：

- lane 顺序错误
- byte 切片错误
- lane 丢失或重复
- 符号扩展错误

```text
vdot.vv lane routing PASS: 8 cases
```

### 固定种子随机测试

使用固定种子 `0x013157a6` 生成 64 组输入。期望结果由普通 C 标量循环独立计算。

```text
vdot.vv deterministic random PASS: 64 cases
vdot.vv test PASS: 76 total cases
```

## 3. RTL–NEMU 差分测试

验证程序和性能程序都加载修改后的 NEMU 参考模型：

```text
The first instruction of core 0 has commited. Difftest enabled.
```

两个程序都正常运行到：

```text
Core 0: HIT GOOD TRAP
```

说明 XiangShan RTL 与 NEMU 的体系结构状态一致。

## 4. 性能测试方法

性能测试比较：

1. 普通标量 C 点积。
2. 自定义 `vdot.vv` 点积。

测试条件：

- 两者读取相同的两个 8 元素 INT8 数组。
- 关闭编译器自动向量化。
- 输入声明为 `volatile`，防止常量折叠。
- 两个内核均使用 `noinline`。
- 预热 32 次。
- 每轮运行 1000 次。
- 测量 5 轮并取最小周期数。
- 交替改变两个实现的测量顺序。

反汇编确认标量版本使用：

```text
lbu + 符号扩展 + mulw + addw
```

标量版本没有使用向量指令。

VDOT 版本使用：

```text
vsetivli
vle8.v
vle8.v
.word 0xe61101d7
vmv.x.s
```

## 5. 性能结果

| 实现 | 1000 次总周期 | 每次点积周期 |
|---|---:|---:|
| 标量 C | 24862 | 24.862 |
| 自定义 VDOT | 8203 | 8.203 |

计算加速比：

```text
24.862 / 8.203 = 3.030×
```

最终结果：

```text
vdot speedup: 3.030x
vdot benchmark PASS
```

## 6. 结果解释

`8.203 cycles/dot` 是完整点积内核的周期，其中包含：

- 向量配置
- 两次向量加载
- VDOT 指令
- 结果移动到标量寄存器
- 循环与函数调用的摊销开销

因此它不是只计算乘加树组合延迟得到的理想数字，而是实际程序测得的端到端结果。
