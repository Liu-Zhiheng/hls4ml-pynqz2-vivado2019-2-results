# hls4ml 教程：Vivado 2019.2 与 PYNQ-Z2 完成报告

日期：2026-08-22  
目标板：PYNQ-Z2（`xc7z020-clg400-1`）  
主机系统：Ubuntu 22.04.5 LTS（项目从开始至今一直使用此系统）  
工具链：Xilinx Vivado / Vivado HLS 2019.2，Python 3.10.16，hls4ml 1.2.0

## 结论

已按下载的 [hls4ml-tutorial](https://github.com/fastmachinelearning/hls4ml-tutorial) 教程源文件完成全部要求范围：

1. 完整执行 Tutorial 1：数据读取、网络训练、hls4ml 转换、定点仿真和 Vivado HLS C 综合。
2. 执行 Part 7a 明确依赖的 Part 4 训练单元，生成教程原设定的 6-bit QKeras、75% 稀疏模型 `model_3/KERAS_check_best_model.h5`。
3. 完整执行 Tutorial 7a：为 PYNQ-Z2 转换模型、C 仿真、HLS 综合、导出 IP、Vivado block design、综合、实现、布线、生成 bitstream 和打包。
4. 将部署包传至 PYNQ-Z2，完整执行 Part 7b 硬件推理，将 `y_hw.npy` 取回主机并完整执行 Part 7c 验证。
5. 在不改变模型的前提下完成可移植 C 基线：在 Ryzen 5 5600 和 PYNQ-Z2 Cortex-A9 上做全量数值验证、batch-one 延迟和持续吞吐，并在 Ryzen 上完成包级能量计数；同时补测 FPGA 真正 batch-one 的 Python+DMA 调用。

没有添加算法，也没有改动数据集、网络结构、训练轮数、量化、剪枝或 hls4ml 配置。原 Notebook 均保留不动；只创建了本地执行副本。Tutorial 1 原文件面向 Vitis，而本机按项目要求只有 Vivado 2019.2，因此执行副本仅把工具入口从 `Vitis`/`XILINX_VITIS` 对应到 `Vivado`/`XILINX_VIVADO`。

## 执行结果

| 阶段 | 结果 |
|---|---:|
| Tutorial 1 Keras 测试准确率 | 0.758205 |
| Tutorial 1 hls4ml 定点测试准确率 | 0.758090 |
| Part 7 模型 QKeras 测试准确率 | 0.760566 |
| Part 7 模型 hls4ml CPU 定点测试准确率 | 0.759807 |
| Part 7 模型 PYNQ-Z2 测试准确率 | 0.760277 |
| PYNQ-Z2 与 hls4ml CPU 分类结果一致率 | 0.994096 |
| PYNQ-Z2 与 QKeras 分类结果一致率 | 0.992470 |
| Part 7 测试样本数 | 166,000 |

QKeras、hls4ml CPU 和 PYNQ-Z2 输出形状均为 `(166000, 5)`，所有预测结果均为有限数值。最终保留的六个执行 Notebook 中，全部非空代码单元均有执行记录，未保存任何 Python 异常。

### Tutorial 1 HLS 结果

教程原目标器件为 `xcu250-figd2104-2L-e`：

| 指标 | 结果 |
|---|---:|
| HLS 目标 / 估算时钟周期 | 5.000 ns / 4.292 ns |
| 延迟 | 12 cycles（60 ns） |
| 启动间隔 II | 1 cycle |
| BRAM18K / DSP / FF / LUT | 4 / 2118 / 12562 / 109753 |

### PYNQ-Z2 HLS 与实现结果

| 指标 | 结果 |
|---|---:|
| HLS AXI wrapper 延迟 / 间隔 | 95 / 95 cycles |
| HLS 目标 / 估算时钟周期 | 5.000 ns / 4.367 ns |
| 实现时钟 | 100 MHz（10.000 ns） |
| Routed WNS / TNS | +0.213 ns / 0.000 ns |
| Routed WHS / THS | +0.024 ns / 0.000 ns |
| Slice LUTs | 20,668 / 53,200（38.85%） |
| Slice Registers | 23,097 / 106,400（21.71%） |
| Block RAM Tiles | 7 / 140（5.00%） |
| DSPs | 9 / 220（4.09%） |

所有用户时序约束满足；实现后路由没有失败网络或未布线网络，bitstream 生成成功。Vivado 在创建 PYNQ board preset 时给出了 DDR DQS 和 AXI 宽度相关的非阻塞警告，但没有阻止综合、实现、DRC、布线或 bitstream 生成。

## 能效估算与板端实测

Vivado 实现后的时钟为 100 MHz，HLS AXI wrapper 的一次推理启动间隔为 95 cycles，因此不计 DMA、Python 和操作系统开销时：

- 理论推理间隔：`95 / 100 MHz = 0.95 us`
- 理论吞吐率：约 `1,052,632 inference/s`
- Vivado 实现后估算的片上总功耗：`1.492 W`（动态 `1.355 W`，静态 `0.137 W`）
- 估算单次推理能量：`1.492 W × 0.95 us = 1.4174 uJ/inference`
- 估算能效：约 `705,517 inference/J`，等价于 `705,517 inference/s/W`

这是设计报告给出的纯硬件理论值。Vivado power report 的置信度为 **Medium**，没有仿真活动文件，使用的是 vectorless/default activity；总功耗包含 PS7 估算，但不代表整块开发板、稳压损耗和外设功耗。

### PYNQ-Z2 实测

板端环境为 PYNQ 3.1.1、Python 3.10.4。部署包 SHA-256 与主机一致，overlay 下载后 FPGA manager 状态为 `operating`。Part 7b 的教程原始 `profile=True` 输出为：

- 166,000 个样本总用时：`0.235829 s`
- 实测吞吐率：`703,899.86 inference/s`
- 批处理平均时间：约 `1.4207 us/inference`
- 实测吞吐为纯硬件理论上限的约 `66.87%`，差异包含缓冲区复制、DMA 和软件调用开销

板卡没有在本次教程中提供实测功率数据。若仅把实测吞吐与 Vivado 的 `1.492 W` 片上功耗估算结合，则得到约 `2.1196 uJ/inference`、`471,783 inference/J`；这是“实测吞吐 + 估算功耗”的混合估计，不能当作整板实测能耗。

## 固定模型的 CPU 基线扩展

导师要求的第一阶段扩展已经实际运行。所用可移植 C 保持教程模型的 `16-64-32-32-5` 网络、训练权重、6-bit QKeras 量化和原有剪枝不变；x86 与 ARM 使用相同的可移植编译参数，没有加入新算法或架构专用优化。全量 166,000 个样本上，C 准确率为 `0.760566265060`，与 QKeras 分类一致率为 `1.0`；x86 与 ARM 的输出文件逐字节相同。

| 平台 | batch-one median（3 次均值） | 持续吞吐（3 次均值） |
|---|---:|---:|
| Ryzen 5 5600，Ubuntu 22.04，单线程 | 0.9447 us | 1,072,718.7 inference/s |
| PYNQ-Z2 Cortex-A9，650 MHz，单核 | 28.1850 us | 37,126.5 inference/s |

Ryzen 的 Linux `perf power/energy-pkg/` 包级能量计数做了三组 20 s 空闲/负载配对实验：负载 package 功率为 `48.737 ± 0.179 W`，总 package 能量为 `44.189 ± 0.696 uJ/inference`；扣除配对空闲后为 `16.610 ± 1.385 uJ/inference`。前者包含 CPU 基线和 uncore，后者对后台活动更敏感；二者都不是整机插座功耗。

FPGA 真正 batch-one 的教程 Python+AXI DMA `predict()` 中位数为 `940.094 us`，分类端到端为 `1,315.307 us`。该值受每次 DMA 建立和同步支配，不能代替 HLS 核的 `0.95 us` 间隔。FPGA 内核理论间隔约比 Cortex-A9 C batch-one 快 `29.67×`，而教程流式 FPGA 实测吞吐约为 Cortex-A9 的 `18.96×`。

PYNQ ARM 的 XPE 输入已经依据实际板端时钟和 PS7 配置整理完成。现有 Ubuntu 主机不需要再安装 Linux；但 AMD 官方只支持在 Windows + Microsoft Excel 宏环境运行 7 Series/Zynq-7000 XPE 2019.1.2，因此没有把 LibreOffice 输出或 Vivado 默认 PS7 `1.256 W` 冒充为专用 ARM 负载结果。详细结果和限制见 `cpu_baseline/RESULTS.md` 与 `cpu_baseline/XPE_INPUTS.md`。

## 交付物

- Tutorial 1 已执行副本：`part1_getting_started_vivado2019_2.ipynb`
- Part 4 必要训练前置：`part4_prerequisite_for_part7a_vivado2019_2.ipynb`
- Tutorial 7a 已执行副本：`part7a_bitstream_vivado2019_2.ipynb`
- Part 4 验证依赖副本：`part4_validation_for_part7c_vivado2019_2.ipynb`
- PYNQ-Z2 上执行并取回的 Part 7b：`part7b_deployment_executed_pynqz2.ipynb`
- 主机端 Part 7c 验证：`part7c_validation_pynqz2.ipynb`
- PYNQ-Z2 推理结果：`model_3/y_hw.npy`
- PYNQ-Z2 部署包：`model_3/hls4ml_prj_pynq/package.tar.gz`
- 展开的上板文件：`model_3/hls4ml_prj_pynq/package/`
- PYNQ-Z2 HLS 报告：`model_3/hls4ml_prj_pynq/myproject_prj/solution1/syn/report/myproject_axi_csynth.rpt`
- 实现资源报告：`model_3/hls4ml_prj_pynq/myproject_vivado_accelerator/project_1.runs/impl_1/design_1_wrapper_utilization_placed.rpt`
- Routed 时序报告：`model_3/hls4ml_prj_pynq/myproject_vivado_accelerator/project_1.runs/impl_1/design_1_wrapper_timing_summary_routed.rpt`
- Routed 功耗报告：`model_3/hls4ml_prj_pynq/myproject_vivado_accelerator/project_1.runs/impl_1/design_1_wrapper_power_routed.rpt`
- 测试环境依赖：`requirements-vivado2019_2.txt`
- Notebook 准备和执行工具：`tutorial_tools/prepare_notebooks.py`、`tutorial_tools/execute_notebook.py`
- 可移植 C 实现、复现说明与结果：`cpu_baseline/`
- CPU/FPGA 比较报告：`cpu_baseline/RESULTS.md`
- PYNQ Cortex-A9 XPE 输入：`cpu_baseline/XPE_INPUTS.md`
- 机器可读汇总：`cpu_baseline/results/summary.json`

部署包包含教程要求的六个文件：`hls4ml_nn.bit`、`hls4ml_nn.hwh`、`axi_stream_driver.py`、`X_test.npy`、`y_test.npy` 和 `part7b_deployment.ipynb`。

### SHA-256

```text
26de9998a06671ed09cb597afe461ce418be3a7190a711d48aeb97bbbed521d1  package.tar.gz
91b1baba2ce810a978002843a5a41f55f919c5df683c1c24e04c40351ff25fe9  hls4ml_nn.bit
66d0d2128555a77df4633f55114ba3d75c0be633158da68a6cfabdd2d22f3a00  hls4ml_nn.hwh
56ac6c194c519c863759339a6674096873d2a4901091f8cc9f309ddd2546652b  y_hw.npy
```

## 板端状态

教程的 Part 7a、7b 和 7c 已全部完成。板端教程文件和 `y_hw.npy` 保留在 `/home/<pynq-user>/jupyter_notebooks/`，FPGA 当前处于 `operating` 状态；主机已保存完整的执行记录和验证结果。
