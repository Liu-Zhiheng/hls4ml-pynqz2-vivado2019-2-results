#!/usr/bin/env python3
"""Prepare executable tutorial copies for the local Vivado 2019.2 setup."""

import argparse
from copy import deepcopy
from pathlib import Path

import nbformat


ROOT = Path(__file__).resolve().parents[1]


def clear_code_outputs(notebook):
    for cell in notebook.cells:
        # The source notebooks use an older v4 minor version that predates cell IDs.
        cell.pop("id", None)
        if cell.cell_type == "code":
            cell.execution_count = None
            cell.outputs = []


def set_local_kernel(notebook):
    notebook.metadata["kernelspec"] = {
        "display_name": "Python 3 (hls4ml tutorial)",
        "language": "python",
        "name": "python3",
    }


def write_notebook(notebook, filename):
    clear_code_outputs(notebook)
    set_local_kernel(notebook)
    nbformat.validate(notebook)
    output = ROOT / filename
    nbformat.write(notebook, output)
    print(output)


def prepare_part1():
    notebook = nbformat.read(ROOT / "part1_getting_started.ipynb", as_version=4)
    note = nbformat.v4.new_markdown_cell(
        "## Local compatibility note\n\n"
        "This execution copy uses the tutorial's `Vivado` backend with Vivado HLS "
        "2019.2, because this workstation has the Vivado 2019.2 toolchain requested "
        "for the project rather than a separate Vitis HLS installation. The dataset, "
        "network, training parameters, fixed-point configuration, and target part are "
        "unchanged from the tutorial."
    )
    notebook.cells.insert(1, note)
    for cell in notebook.cells:
        cell.source = cell.source.replace("XILINX_VITIS", "XILINX_VIVADO")
        cell.source = cell.source.replace("backend='Vitis'", "backend='Vivado'")
        cell.source = cell.source.replace("with Vitis HLS", "with Vivado HLS 2019.2")
        cell.source = cell.source.replace("Vitis HLS", "Vivado HLS 2019.2")
        cell.source = cell.source.replace("vitis_hls.log", "vivado_hls.log")
    write_notebook(notebook, "part1_getting_started_vivado2019_2.ipynb")


def prepare_part4_prerequisite():
    source = nbformat.read(ROOT / "part4_quantization.ipynb", as_version=4)
    notebook = deepcopy(source)
    notebook.cells = notebook.cells[:12]
    notebook.cells.insert(
        1,
        nbformat.v4.new_markdown_cell(
            "## Scope of this execution copy\n\n"
            "Part 7a explicitly requires the quantized/pruned `model_3` produced by "
            "Part 4. This copy runs only the original Part 4 cells needed to create "
            "that prerequisite. No model, quantization, pruning, or training setting "
            "has been changed."
        ),
    )
    for cell in notebook.cells:
        cell.source = cell.source.replace("XILINX_VITIS", "XILINX_VIVADO")
    notebook.cells.append(
        nbformat.v4.new_markdown_cell(
            "## Check\n\n"
            "Successful completion creates `model_3/KERAS_check_best_model.h5`, "
            "which is the documented input to Part 7a."
        )
    )
    write_notebook(notebook, "part4_prerequisite_for_part7a_vivado2019_2.ipynb")


def prepare_part4_validation():
    source = nbformat.read(ROOT / "part4_quantization.ipynb", as_version=4)
    notebook = deepcopy(source)
    notebook.cells = notebook.cells[:15]
    notebook.cells.insert(
        1,
        nbformat.v4.new_markdown_cell(
            "## Validation dependency for Part 7c\n\n"
            "Part 7c loads `model_3/y_qkeras.npy`, which is saved by the original "
            "Part 4 validation cell. This execution copy reloads the already-trained "
            "model (`train = False`) and runs the tutorial's validation cells without "
            "retraining or changing the algorithm."
        ),
    )
    for cell in notebook.cells:
        cell.source = cell.source.replace("XILINX_VITIS", "XILINX_VIVADO")
        cell.source = cell.source.replace("backend='Vitis'", "backend='Vivado'")
        cell.source = cell.source.replace("train = True", "train = False")
    write_notebook(notebook, "part4_validation_for_part7c_vivado2019_2.ipynb")


def prepare_part7a():
    notebook = nbformat.read(ROOT / "part7a_bitstream.ipynb", as_version=4)
    notebook.cells.insert(
        1,
        nbformat.v4.new_markdown_cell(
            "## Local execution boundary\n\n"
            "This copy uses the installed Vivado/Vivado HLS 2019.2 toolchain and "
            "runs through bitstream generation and deployment-package creation. "
            "It does not connect to, configure, copy files to, or execute code on "
            "the PYNQ-Z2 board."
        ),
    )
    write_notebook(notebook, "part7a_bitstream_vivado2019_2.ipynb")


def prepare_part7c():
    notebook = nbformat.read(ROOT / "part7c_validation.ipynb", as_version=4)
    notebook.cells.insert(
        1,
        nbformat.v4.new_markdown_cell(
            "## Board execution record\n\n"
            "This execution copy validates `model_3/y_hw.npy`, copied back from "
            "the PYNQ-Z2 after successfully running Part 7b. The validation code "
            "is unchanged from the tutorial."
        ),
    )
    write_notebook(notebook, "part7c_validation_pynqz2.ipynb")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "part",
        choices=("all", "part1", "part4", "part4-validation", "part7a", "part7c"),
        default="all",
        nargs="?",
    )
    args = parser.parse_args()

    preparers = {
        "part1": prepare_part1,
        "part4": prepare_part4_prerequisite,
        "part4-validation": prepare_part4_validation,
        "part7a": prepare_part7a,
        "part7c": prepare_part7c,
    }
    if args.part == "all":
        for prepare in preparers.values():
            prepare()
    else:
        preparers[args.part]()
