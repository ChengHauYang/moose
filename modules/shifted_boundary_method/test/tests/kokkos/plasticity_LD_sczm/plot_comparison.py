#!/usr/bin/env python3
"""Plot benchmark results from an existing comparison.csv produced by analyze_results.py.

Usage:
    python3 plot_comparison.py                       # reads results/comparison.csv
    python3 plot_comparison.py path/to/comparison.csv
    python3 plot_comparison.py path/to/comparison.csv --output my_plot.png
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def plot_results(frame: pd.DataFrame, output: Path) -> None:
    """Reproduce the same 3x2 layout as analyze_results.py from a saved CSV."""
    short = [
        "1. Host CPU", "2. GPU NEML2", "3. GPU asm",
        "4. GPU NEML2+asm", "5. AIJKOKKOS", "6. MATHYPRE",
    ]
    # Pad to 6 rows if the CSV was produced from fewer steps.
    n = len(frame)
    x = short[:n]

    fig, axes = plt.subplots(3, 2, figsize=(14, 12), constrained_layout=True)
    fig.suptitle(
        "plasticity_LD_sczm benchmark\n(hypre boomeramg, same PC across all steps)",
        fontsize=11,
    )

    # --- row 0: wall time + NEML2 time ---
    ax = axes[0, 0]
    ax.bar(x, frame["total_wall_time_s"])
    ax.set_title("Total wall time")
    ax.set_ylabel("Seconds")
    ax.set_yscale("log")

    ax = axes[0, 1]
    ax.bar(x, frame["neml2_solve_s"])
    ax.set_title("NEML2::solve (bulk + interface)")
    ax.set_ylabel("Seconds")

    # --- row 1: GPU activity + iteration counts ---
    ax = axes[1, 0]
    k_time = frame["cuda_kernel_time_s"].fillna(0.0)
    h2d = frame["h2d_time_s"].fillna(0.0)
    d2h = frame["d2h_time_s"].fillna(0.0)
    ax.bar(x, k_time, label="CUDA kernels")
    ax.bar(x, h2d, bottom=k_time, label="H2D")
    ax.bar(x, d2h, bottom=k_time + h2d, label="D2H")
    ax.set_title("GPU activity (one profile run)")
    ax.set_ylabel("Seconds")
    ax.legend()

    ax = axes[1, 1]
    ax.plot(x, frame["newton_iterations"], "o-", label="Newton")
    ax.plot(x, frame["ksp_iterations"], "s-", label="KSP (total)")
    ax.set_title("Iteration totals")
    ax.set_ylabel("Iterations")
    ax.set_yscale("log")
    ax.legend()

    # --- row 2: MatConvert calls + speedup ---
    ax = axes[2, 0]
    mc_col = "mat_convert_calls" if "mat_convert_calls" in frame.columns else None
    if mc_col is not None:
        mc = frame[mc_col].fillna(0.0)
        bars = ax.bar(x, mc)
        for bar, val, raw in zip(bars, mc, frame[mc_col]):
            if not (isinstance(raw, float) and math.isnan(raw)):
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + 0.1,
                    str(int(val)),
                    ha="center", va="bottom", fontsize=8,
                )
    else:
        ax.text(0.5, 0.5, "mat_convert_calls not in CSV",
                ha="center", va="center", transform=ax.transAxes)
    ax.set_title(
        "MatConvert calls per run (AIJ→HYPRE)\n"
        "step5 pays one per PC setup; step6 = 0"
    )
    ax.set_ylabel("Calls")

    ax = axes[2, 1]
    ref = frame["total_wall_time_s"].iloc[0]
    speedup = ref / frame["total_wall_time_s"]
    ax.bar(x, speedup)
    ax.axhline(1.0, color="grey", linestyle="--", linewidth=0.8)
    ax.set_title("Speedup vs step1")
    ax.set_ylabel("×")

    for axis in axes.flat:
        axis.tick_params(axis="x", rotation=18, labelsize=8)
        axis.grid(axis="y", alpha=0.25)

    fig.savefig(output, dpi=180)
    plt.close(fig)
    print(f"Wrote {output}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Plot plasticity_LD_sczm benchmark results from a saved comparison.csv."
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
        help="Output PNG path (default: same directory as csv_path, comparison.png)",
    )
    args = parser.parse_args()

    frame = pd.read_csv(args.csv_path)
    output = args.output if args.output is not None else args.csv_path.with_name("comparison.png")
    plot_results(frame, output)


if __name__ == "__main__":
    main()
