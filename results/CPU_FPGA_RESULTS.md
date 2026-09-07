# 固定模型的 CPU / FPGA 基线结果

日期：2026-08-22  
模型：教程 Part 4/7 的 6-bit QKeras、约 75% 剪枝模型  
测试集：166,000 个样本

## 已完成内容

在不重新训练、不改变网络结构、权重、量化、剪枝或测试集的前提下，已将模型导出为同一份可移植 C 实现，并分别在以下平台运行：

- Ubuntu 22.04.5 主机：AMD Ryzen 5 5600，单线程固定 CPU 0；
- PYNQ-Z2：一个 Cortex-A9 核固定 CPU 0，650 MHz；
- PYNQ-Z2 FPGA：保留教程原始 bitstream，同时补测真正的 batch-one Python+AXI DMA 调用。

主机从项目开始至今一直是 Ubuntu 22.04，因此导师邮件中“安装 Linux”的建议已经满足，不需要安装双系统。

## 模型等价性

| 检查项 | 结果 |
|---|---:|
| QKeras 测试准确率 | 0.760566265060 |
| 可移植 C 测试准确率 | 0.760566265060 |
| C 与 QKeras 分类一致率 | 1.000000000000 |
| C 与 QKeras score MAE | 2.1305e-08 |
| C 与 QKeras最大 score 误差 | 0.00195557 |
| x86 与 ARM C 输出 | 逐字节相同 |

x86 和 ARM 的 C score 文件具有相同 SHA-256：`e1eda81bd82599b0df4f16c20d3cc17ee60af964e8303d63c8fe388e32311671`。这说明两台 CPU 测量的是同一模型，而不是两个近似实现。

## CPU 性能

所有 C 结果均使用相同的可移植编译参数：`-O3 -std=c11 -fno-fast-math -ffp-contract=off`，没有加入架构专用优化。输入、权重和输出缓冲区在计时前已驻留内存；文件加载、模型导出和正确性验证不计入 batch-one 延迟。每个平台运行 3 次，每次收集 20,000 个 batch-one 样本。

| 平台 | batch-one median，3 次均值 ± SD | p95，3 次均值 ± SD | 持续单线程吞吐，均值 ± SD | 单核利用率 |
|---|---:|---:|---:|---:|
| Ryzen 5 5600 | 0.9447 ± 0.0055 us | 1.2090 ± 0.1337 us | 1,072,718.7 ± 302.1 inf/s | 99.9959% |
| Cortex-A9 @ 650 MHz | 28.1850 ± 0.0250 us | 30.5378 ± 0.0523 us | 37,126.5 ± 9.0 inf/s | 99.9889% |

在这一公平的“同一 C、单线程、内存内推理”比较中，Ryzen 吞吐约为 Cortex-A9 的 28.89 倍，Cortex-A9 的 batch-one median 约为 Ryzen 的 29.84 倍。

## Ryzen 5 5600 包级能量

主机使用 Linux `perf 6.8.12` 暴露的 `power/energy-pkg/` Joule 计数器。CPU governor 为 `schedutil`、boost 开启；工作负载固定 CPU 0。进行了 3 组配对实验：每组先测 20 s 空闲，再测至少 20 s 的单核持续推理。每组负载包含约 21.7–22.4 million 次计时推理。

| 指标 | 3 次均值 ± 样本 SD |
|---|---:|
| 能量实验吞吐 | 1,103,090.2 ± 16,091.3 inf/s |
| 空闲 CPU package 功率 | 30.426 ± 1.200 W |
| 推理负载 CPU package 功率 | 48.737 ± 0.179 W |
| 总 package 能量 / inference | 44.189 ± 0.696 uJ |
| 配对空闲差分能量 / inference | 16.610 ± 1.385 uJ |
| 总 package 能效 | 约 22,630 inference/J |
| 空闲差分能效 | 约 60,205 inference/J |

`44.189 uJ` 是整颗 CPU package 在负载时的保守值，包含基线功耗和其他核心/uncore；`16.610 uJ` 是同一组空闲功耗扣除后的增量值，更能反映运行该模型带来的动态代价，但对系统后台活动更敏感。这不是整台电脑插座功耗，也没有包含电源转换、内存条和其他外设。

## 与 FPGA 的正确比较

| 路径 | 延迟/间隔 | 吞吐 | 功耗/能量状态 |
|---|---:|---:|---|
| Cortex-A9 可移植 C，batch one | median 28.185 us | 37,126.5 inf/s | 专用 XPE 输入已整理；暂无合规 XPE 输出 |
| FPGA HLS 核，不含软件/DMA | II = 0.95 us | 理论 1,052,632 inf/s | Vivado 1.492 W 混合估算：1.4174 uJ/inf |
| FPGA 教程流式整批调用 | 平均 1.4207 us/inf | 实测 703,899.9 inf/s | 与 1.492 W 混合：2.1196 uJ/inf |
| FPGA 教程 Python+DMA，真正 batch one | predict median 940.094 us | 不适合连续吞吐换算 | 软件、DMA 建立与同步开销占主导 |
| Ryzen 可移植 C，batch one | median 0.9447 us | 1,072,718.7 inf/s | package 总 44.189 uJ/inf；空闲差分 16.610 uJ/inf |

FPGA 内核本身的 0.95 us 间隔约比 Cortex-A9 的 C batch-one median 快 29.67 倍；教程流式 FPGA 吞吐约为 Cortex-A9 的 18.96 倍。另一方面，逐样本调用教程的 Python+DMA 接口时，中位数约 940 us，反而比内存内 Cortex-A9 C 调用慢 33.35 倍。这不是 FPGA 计算慢，而是 batch-one 时软件和 DMA 固定开销远大于 0.95 us 的硬件计算时间。

## PYNQ ARM 功耗说明

Vivado routed report 中的 PS7 `1.256 W` 是默认/vectorless 活动估算，不是 Cortex-A9 C 负载的专用功耗。机械地与 ARM 吞吐结合会得到 `33.83 uJ/inference`，只能列作临时参考，不能写成“实测”或“XPE 结果”。

准确的 XPE 填写表见 [XPE_INPUTS.md](XPE_INPUTS.md)。AMD 官方只支持在 Windows + Microsoft Excel 宏环境运行 7 Series/Zynq-7000 XPE 2019.1.2；当前 Ubuntu 22.04 环境不能生成受支持的工作簿结果。后续若使用导师的外部功率仪，应再补一组 PYNQ 整板空闲/负载差分数据。

## 可审计原始数据

- Ryzen 环境：[results/ryzen5_5600/system.txt](results/ryzen5_5600/system.txt)
- Ryzen 性能三次原始结果：[results/ryzen5_5600/benchmark_runs.csv](results/ryzen5_5600/benchmark_runs.csv)
- Ryzen 能量三组配对结果：[results/ryzen5_5600/energy_runs.csv](results/ryzen5_5600/energy_runs.csv)
- Cortex-A9 三次原始输出：[results/cortex_a9/](results/cortex_a9/)
- FPGA batch-one 三次原始输出：[results/fpga_batch_one/](results/fpga_batch_one/)
- 机器可读汇总：[results/summary.json](results/summary.json)
- 模型与数据哈希：[generated/manifest.json](generated/manifest.json)
