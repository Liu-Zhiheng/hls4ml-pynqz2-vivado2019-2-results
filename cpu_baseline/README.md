# Portable CPU baseline

This directory implements the already-trained tutorial model as portable C for
the PYNQ-Z2 Cortex-A9 and the Ubuntu workstation. It does not retrain or change
the network. The C implementation uses the exact values produced by the model's
QKeras 6-bit weight/bias quantizers, emulates each 6-bit QKeras ReLU, exploits
the existing pruned zero weights with a sparse representation, and includes the
final softmax.

The timed region operates on data already resident in memory. File loading,
model loading, and accuracy validation are outside the latency and throughput
measurements. The primary build uses the same portable compiler options on both
CPU architectures and deliberately avoids `-ffast-math` and architecture-specific
flags.

The implementation and measurements are complete. See `RESULTS.md` for the
three-run Ryzen 5 5600, PYNQ-Z2 Cortex-A9, FPGA batch-one, and energy results;
see `XPE_INPUTS.md` for the audited PYNQ-Z2 power-estimator inputs and the XPE
platform limitation.

## Generate and run

```bash
../.venv/bin/python cpu_baseline/export_model.py
make -C cpu_baseline
taskset -c 0 cpu_baseline/nn_benchmark \
  cpu_baseline/generated/X_test_f32.bin \
  cpu_baseline/generated/y_test_labels_u8.bin \
  cpu_baseline/generated/qkeras_reference_scores_f32.bin \
  3 10000 cpu_baseline/c_scores_f32.bin
```

The benchmark reports classification accuracy, agreement with QKeras,
batch-one latency distribution, sustained single-thread throughput, and
process CPU utilisation. Power-tool sampling should run around a longer
throughput loop; microsecond-scale individual calls are too short for a power
profiler's sampling interval.

For the long steady-state energy workload:

```bash
taskset -c 0 cpu_baseline/energy_benchmark \
  cpu_baseline/generated/X_test_f32.bin 20
```

On the Ryzen host, Linux `perf stat -a -e power/energy-pkg/` was placed around
this command and around a matched idle interval. Raw readings are retained in
`results/ryzen5_5600/energy_runs.csv`.
