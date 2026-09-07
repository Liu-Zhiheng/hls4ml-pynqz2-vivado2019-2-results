#!/usr/bin/env python3
"""Execute one notebook in place and save partial outputs if execution fails."""

import argparse
from pathlib import Path

import nbformat
from nbclient import NotebookClient


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("notebook", type=Path)
    args = parser.parse_args()

    notebook_path = args.notebook.resolve()
    notebook = nbformat.read(notebook_path, as_version=4)

    def on_cell_start(cell, cell_index):
        first_line = cell.source.strip().splitlines()[0] if cell.source.strip() else "<empty>"
        print(f"START cell {cell_index}: {first_line[:100]}", flush=True)

    def on_cell_complete(cell, cell_index):
        print(f"DONE  cell {cell_index}", flush=True)

    client = NotebookClient(
        notebook,
        timeout=None,
        kernel_name="python3",
        resources={"metadata": {"path": str(notebook_path.parent)}},
        allow_errors=False,
        record_timing=True,
        on_cell_start=on_cell_start,
        on_cell_complete=on_cell_complete,
    )

    try:
        client.execute()
    finally:
        nbformat.write(notebook, notebook_path)


if __name__ == "__main__":
    main()
