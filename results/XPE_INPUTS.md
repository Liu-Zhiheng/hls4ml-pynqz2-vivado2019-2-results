# PYNQ-Z2 Cortex-A9：XPE 输入与功耗边界

## 当前状态

Cortex-A9 的可移植 C 推理、全量数值验证、batch-one 延迟和持续吞吐已经完成。当前 Ubuntu 22.04 主机无法生成一份受 AMD 支持的 XPE 工作簿结果：AMD 官方要求 Windows 10 或更高版本，7 Series/Zynq-7000 使用 XPE 2019.1.2 `.xlsm`，并要求 Microsoft Excel 启用宏；非 Windows 平台以及 LibreOffice/OpenOffice 均不受支持。

- [AMD Xilinx Power Estimator 下载与兼容性](https://www.amd.com/en/products/adaptive-socs-and-fpgas/technologies/power-efficiency/power-estimator.html)
- [UG440：Xilinx Power Estimator User Guide](https://www.xilinx.com/support/documents/sw_manuals/xilinx2022_2/ug440-xilinx-power-estimator.pdf)

因此，本项目没有用 LibreOffice 强行运行宏，也没有伪造 XPE 输出。下面的值已经从实际 PYNQ-Z2 时钟树和 Vivado 2019.2 PS 配置中核对，可直接填写到 XPE。

## XPE 场景：ARM 单核执行 C 模型

| XPE 项 | 填写值 | 依据 |
|---|---:|---|
| Device | XC7Z020 | PYNQ-Z2 / 实现工程 |
| Package | CLG400 | 实现工程 |
| Speed grade | -1 | `xc7z020-clg400-1` |
| Temperature grade | Commercial | Vivado routed power report |
| Process | Typical | 与现有 routed report 保持一致 |
| Ambient temperature | 25 °C | 与现有 routed report 保持一致 |
| A9 cores used | 1 | C 进程固定在 CPU 0；CPU 1 不执行推理 |
| A9 clock | 650 MHz | 板端 `/sys/kernel/debug/clk/clk_summary` |
| Processor load | 80% | UG440 对“每周期执行的循环程序”的建议值；Linux 进程利用率实测约 99.99% |
| ARM PLL | 1300 MHz | 板端时钟树与 PS7 IP 配置一致 |
| DDR PLL | 1050 MHz | 板端时钟树与 PS7 IP 配置一致 |
| I/O PLL | 1000 MHz | 板端时钟树与 PS7 IP 配置一致 |
| DDR type | DDR3 | PS7 IP 配置 |
| DDR part | MT41J256M16 RE-125 | PS7 IP 配置 |
| DDR width | 16 bit | PS7 IP 配置 |
| DDR clock | 525 MHz | PS7 IP 配置；DDR3-1050 data rate |
| DDR ECC | Disabled | PS7 IP 配置 |
| Ethernet 0 | Enabled, 1 Gb/s link | 板端通过以太网 SSH；推理计时区间无网络传输 |
| SD 0 | Enabled, idle | 系统从 SD 卡运行；模型计时区间不读文件 |
| USB 0 | Enabled, idle | PS preset；模型计时区间不使用 |

模型权重的稀疏表示约为数 KB，可驻留缓存。每次推理顺序读取 16 个 `float32` 输入，即 64 byte；以实测 `37,126.5 inference/s` 计算，模型输入流量约为 `2.38 MB/s`。输出和中间激活均在栈/缓存中。因此在 XPE 的 DDR 使用率中应采用很低的读活动，不应填成持续满带宽。

UG440 对 Processor Load 的定义是：0% 表示 WFI，100% 表示 Dhrystone；对于占满执行周期的普通循环，建议填 80%。如需给导师一个敏感性范围，可额外保存 80% 和 100% 两个工作簿结果；主结果使用 80%。

## 不能当作 ARM 负载功耗的现有数字

现有实现后 Vivado 报告给出 PS7 `1.256 W`、全芯片 `1.492 W`，但报告置信度为 Medium，未导入真实活动文件，PS7 数字也没有针对上述 C 负载设置 processor load。把 `1.256 W` 与 ARM 吞吐率机械相除会得到 `33.83 uJ/inference`，这个值只能作为临时参考，不能标为 XPE 结果或板卡实测值。

要取得可发表的 PYNQ ARM 能量值，有两条合规路径：在 Windows + Microsoft Excel 中用以上输入运行 XPE 2019.1.2，或使用导师提供的外部电源测量设备测量板卡输入功率，并做空闲差分。当前项目报告明确保留了这一区别。
