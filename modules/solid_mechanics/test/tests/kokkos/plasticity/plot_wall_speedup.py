#!/usr/bin/env python3
"""Plot (a) wall time in log scale and (b) speedup relative to step1 from comparison.csv."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def plot_results(frame: pd.DataFrame, output: Path) -> None:
    # Drop rows with no wall time so the log-scale bar and the speedup ratio stay well-defined.
    frame = frame.dropna(subset=["total_wall_time_s"]).reset_index(drop=True)
    if frame.empty:
        raise ValueError("No rows with total_wall_time_s to plot.")

    labels = frame["label"].tolist()
    wall = frame["total_wall_time_s"]
    baseline = wall.iloc[0]  # step1 is the first row in analyze_results.py's output
    speedup = baseline / wall

    # Short x-tick labels ("1"..."N"); full names go in a shared legend below.
    ticks = [str(i + 1) for i in range(len(labels))]
    colors = plt.get_cmap("tab10").colors[: len(labels)]

    figure, axes = plt.subplots(1, 2, figsize=(11, 5))

    bars = axes[0].bar(ticks, wall, color=colors)
    axes[0].set_title("Total wall time")
    axes[0].set_ylabel("Seconds (log)")
    axes[0].set_yscale("log")

    axes[1].bar(ticks, speedup, color=colors)
    axes[1].axhline(1.0, color="black", linewidth=0.8, linestyle="--")
    axes[1].set_title(f"Speedup vs {ticks[0]}. {labels[0].split('. ', 1)[-1]}")
    axes[1].set_ylabel(f"{baseline:.1f}s / step wall time")

    for axis in axes:
        axis.set_xlabel("Step")
        axis.grid(axis="y", alpha=0.25)

    ncol = min(len(labels), 3)
    figure.legend(bars, labels, loc="lower center", ncol=ncol, frameon=False)
    # Reserve enough bottom margin for the legend (grows with row count).
    nrow = (len(labels) + ncol - 1) // ncol
    figure.subplots_adjust(left=0.07, right=0.98, top=0.92, bottom=0.10 + 0.05 * nrow)
    figure.savefig(output, dpi=180)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Plot wall time (log y) and speedup vs step1 from comparison.csv."
    )
    parser.add_argument(
        "csv_path",
        type=Path,
        nargs="?",
        default=Path("results/comparison.csv"),
        help="Path to comparison.csv (default: results/comparison.csv)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output PNG path (default: <csv_path parent>/wall_speedup.png)",
    )
    args = parser.parse_args()

    frame = pd.read_csv(args.csv_path)
    output = args.output if args.output is not None else args.csv_path.with_name("wall_speedup.png")
    plot_results(frame, output)
    print(f"Wrote {output}")


if __name__ == "__main__":
    main()
