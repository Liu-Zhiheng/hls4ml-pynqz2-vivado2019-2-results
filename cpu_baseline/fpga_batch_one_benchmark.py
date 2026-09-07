#!/usr/bin/env python3
"""Measure true batch-one PYNQ-Z2 latency through the tutorial DMA driver."""

import argparse
import gc
import json
import os
from pathlib import Path
from time import perf_counter_ns

import numpy as np

from axi_stream_driver import NeuralNetworkOverlay


def percentile(values, fraction):
    try:
        return float(np.percentile(values, fraction, method="linear"))
    except TypeError:
        return float(np.percentile(values, fraction, interpolation="linear"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", type=int, default=5000)
    parser.add_argument("--warmup", type=int, default=200)
    parser.add_argument("--output", type=Path, default=Path("fpga_batch_one_results.json"))
    args = parser.parse_args()

    os.sched_setaffinity(0, {0})
    x_test = np.load("X_test.npy")
    y_test = np.load("y_test.npy")
    streaming_scores = np.load("y_hw.npy") if Path("y_hw.npy").exists() else None
    sample_count = min(args.samples, len(x_test))

    overlay = NeuralNetworkOverlay("hls4ml_nn.bit", (1, x_test.shape[1]), (1, y_test.shape[1]))

    for index in range(args.warmup):
        overlay.predict(x_test[index % len(x_test) : index % len(x_test) + 1])

    transfer_latencies_ns = np.empty(sample_count, dtype=np.int64)
    classification_latencies_ns = np.empty(sample_count, dtype=np.int64)
    predictions = np.empty(sample_count, dtype=np.uint8)
    scores = np.empty((sample_count, y_test.shape[1]), dtype=np.float32)

    gc.disable()
    try:
        for index in range(sample_count):
            start = perf_counter_ns()
            output = overlay.predict(x_test[index : index + 1])
            transfer_complete = perf_counter_ns()
            scores[index] = output[0]
            predictions[index] = np.argmax(output[0])
            classification_complete = perf_counter_ns()
            transfer_latencies_ns[index] = transfer_complete - start
            classification_latencies_ns[index] = classification_complete - start
    finally:
        gc.enable()

    truth = np.argmax(y_test[:sample_count], axis=1)
    result = {
        "platform": "PYNQ-Z2 programmable logic",
        "measurement": "batch-one tutorial Python driver plus AXI DMA",
        "cpu_affinity": [0],
        "warmup_inferences": args.warmup,
        "measured_inferences": sample_count,
        "accuracy_subset": float(np.mean(predictions == truth)),
        "transfer_only_us": {
            "mean": float(np.mean(transfer_latencies_ns) / 1000.0),
            "median": float(np.median(transfer_latencies_ns) / 1000.0),
            "p95": percentile(transfer_latencies_ns / 1000.0, 95),
            "p99": percentile(transfer_latencies_ns / 1000.0, 99),
            "minimum": float(np.min(transfer_latencies_ns) / 1000.0),
            "maximum": float(np.max(transfer_latencies_ns) / 1000.0),
        },
        "classification_end_to_end_us": {
            "mean": float(np.mean(classification_latencies_ns) / 1000.0),
            "median": float(np.median(classification_latencies_ns) / 1000.0),
            "p95": percentile(classification_latencies_ns / 1000.0, 95),
            "p99": percentile(classification_latencies_ns / 1000.0, 99),
        },
    }
    if streaming_scores is not None:
        result["class_agreement_with_streaming_run"] = float(
            np.mean(predictions == np.argmax(streaming_scores[:sample_count], axis=1))
        )

    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    np.save("fpga_batch_one_scores.npy", scores)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
