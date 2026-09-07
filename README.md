# hls4ml PYNQ-Z2 Results with Vivado 2019.2

This repository records a completed reproduction of the
[hls4ml tutorial](https://github.com/fastmachinelearning/hls4ml-tutorial)
through PYNQ-Z2 deployment and validation. The host platform was Ubuntu
22.04, and the FPGA toolchain was Xilinx Vivado and Vivado HLS 2019.2.

The tutorial model was kept fixed throughout the cross-platform study:

- 16 inputs, hidden layers of 64, 32, and 32 units, and 5 outputs
- 4,389 parameters
- 6-bit QKeras quantisation
- approximately 75% pruning
- the original 166,000-sample test set

No additional algorithm or architecture-specific CPU optimisation was
introduced.

## Main results

| Platform or execution path | Latency or interval | Throughput | Energy status |
|---|---:|---:|---|
| Ryzen 5 5600 portable C, one thread | 0.9447 us median | 1,072,718.7 inf/s | 44.189 uJ/inf total package; 16.610 uJ/inf idle-subtracted |
| PYNQ-Z2 Cortex-A9 portable C, one core at 650 MHz | 28.1850 us median | 37,126.5 inf/s | Workload-specific XPE result pending |
| PYNQ-Z2 FPGA HLS kernel | 0.95 us initiation interval | 1,052,632 inf/s theoretical | 1.417 uJ/inf from routed-power estimate |
| PYNQ-Z2 FPGA tutorial streaming | 1.4207 us/inf average | 703,899.9 inf/s measured | 2.120 uJ/inf from measured throughput and estimated power |
| PYNQ-Z2 FPGA Python and DMA, batch one | 940.094 us median | Not reported | Fixed software and DMA overhead dominates |

The portable C implementation produced the same classifications as QKeras
on all 166,000 test samples. The x86 and ARM score files were byte-identical.
The PYNQ-Z2 FPGA implementation met timing and completed bitstream generation.

## Repository contents

- [`report/Cross_Platform_Inference_Comparison_Report.docx`](report/Cross_Platform_Inference_Comparison_Report.docx): short English report suitable for supervisor review
- [`results/RESULTS_VIVADO2019_2.md`](results/RESULTS_VIVADO2019_2.md): detailed tutorial completion record
- [`results/CPU_FPGA_RESULTS.md`](results/CPU_FPGA_RESULTS.md): CPU and FPGA comparison methodology, raw-result links, and limitations
- [`results/results_summary.json`](results/results_summary.json): machine-readable summary
- [`results/raw/`](results/raw/): Ryzen, Cortex-A9, and FPGA batch-one measurements
- [`notebooks/`](notebooks/): executed tutorial notebooks prepared for Vivado 2019.2 and PYNQ-Z2
- [`cpu_baseline/`](cpu_baseline/): portable C implementation, model export utility, benchmark programs, and generated inputs
- [`deployment/`](deployment/): PYNQ-Z2 bitstream, hardware handoff, and AXI-stream driver
- [`vivado_reports/`](vivado_reports/): HLS, utilisation, timing, and routed-power reports
- [`models/KERAS_check_best_model.h5`](models/KERAS_check_best_model.h5): fixed quantised and pruned model used for the comparison
- [`SHA256SUMS.txt`](SHA256SUMS.txt): integrity hashes for every uploaded artifact

## Validation highlights

| Validation check | Result |
|---|---:|
| QKeras test accuracy | 0.760566 |
| Portable C test accuracy | 0.760566 |
| Portable C agreement with QKeras classes | 100.000% |
| x86 and ARM portable C outputs | Byte-identical |
| PYNQ-Z2 agreement with hls4ml CPU classes | 99.4096% |
| Routed setup timing | WNS +0.213 ns; TNS 0.000 ns |
| Routed hold timing | WHS +0.024 ns; THS 0.000 ns |

## Reproduction environment

- Ubuntu 22.04.5 LTS
- Python 3.10.16
- hls4ml 1.2.0
- Xilinx Vivado and Vivado HLS 2019.2
- PYNQ-Z2 with PYNQ 3.1.1
- AMD Ryzen 5 5600 workstation

Python dependencies are listed in
[`requirements-vivado2019_2.txt`](requirements-vivado2019_2.txt). The original
tutorial notebooks remain available from the upstream repository. Local paths,
board addresses, and board credentials have been removed from the uploaded
notebook copies.

## Measurement limitations

Ryzen energy values cover the CPU package rather than whole-system wall power.
FPGA energy values combine measured throughput with Vivado routed-power
estimates and therefore are not physical whole-board measurements. A final ARM
and FPGA energy ranking requires matched idle and sustained-load measurements
at the PYNQ-Z2 board input, preferably with an external power meter.
