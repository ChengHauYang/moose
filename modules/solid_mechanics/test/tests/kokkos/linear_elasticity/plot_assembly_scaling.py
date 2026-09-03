#!/usr/bin/env python3
"""Standalone plotter for assembly_scaling.csv (produced by
benchmark_assembly_scaling.py).

Independent of the benchmark script -- reads the CSV, computes mean +/- s.d.
per (implementation, section, mesh_size), and writes a 2x2 PNG:
  top row    : CPU and GPU average PerfGraph time per call (log-log, error bars)
  bottom row : CPU/GPU speedup ratio, annotated per point
  left col   : computeResidualInternal
  right col  : computeJacobianInternal

Also prints the table to stdout so you can eyeball the numbers without
opening the PNG.

Usage:
  ./plot_assembly_scaling.py                         # reads ./assembly_scaling.csv
  ./plot_assembly_scaling.py --csv path/to.csv       # explicit CSV
  ./plot_assembly_scaling.py --plot path/to.png      # explicit PNG output
  ./plot_assembly_scaling.py --title 'my title'
"""
import argparse
import csv
import statistics
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
SECTIONS = ("computeResidualInternal", "computeJacobianInternal")
SECTION_TITLES = {
    "computeResidualInternal": "Residual assembly",
    "computeJacobianInternal": "Jacobian assembly",
}


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--csv",
        type=Path,
        default=SCRIPT_DIR / "assembly_scaling.csv",
        help="Input CSV (default: ./assembly_scaling.csv)",
    )
    parser.add_argument(
        "--plot",
        type=Path,
        default=None,
        help="Output PNG (default: same directory + basename as CSV, .png extension)",
    )
    parser.add_argument(
        "--title",
        default="MOOSE Assembly Scaling: CPU vs. GPU",
    )
    parser.add_argument(
        "--no-print", action="store_true", help="Skip printing the summary table to stdout"
    )
    args = parser.parse_args()
    if args.plot is None:
        args.plot = args.csv.with_suffix(".png")
    return args


def read_grouped(csv_path):
    """Return {(implementation, section, mesh_size): [avg_seconds, ...], ...}."""
    grouped = {}
    with csv_path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None:
            raise ValueError(f"CSV {csv_path} is empty or has no header")
        for row in reader:
            key = (row["implementation"], row["section"], int(row["mesh_size"]))
            grouped.setdefault(key, []).append(float(row["average_seconds"]))
    if not grouped:
        raise ValueError(f"CSV {csv_path} has no data rows")
    return grouped


def summarise(grouped):
    """Yield (implementation, section, mesh_size, n, mean, stdev) rows in sorted order."""
    for (implementation, section, mesh_size), samples in sorted(grouped.items()):
        n = len(samples)
        mean = statistics.mean(samples)
        stdev = statistics.stdev(samples) if n > 1 else 0.0
        yield implementation, section, mesh_size, n, mean, stdev


def print_table(grouped):
    print(
        f"{'impl':<4} {'section':<24} {'mesh':>6} {'n':>3} "
        f"{'mean (s/call)':>14} {'stdev (s/call)':>15}"
    )
    print("-" * 70)
    for implementation, section, mesh_size, n, mean, stdev in summarise(grouped):
        print(
            f"{implementation:<4} {section:<24} {mesh_size:>6} {n:>3} "
            f"{mean:>14.6f} {stdev:>15.6f}"
        )


def plot(args, grouped):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit(
            "Matplotlib is not installed. Install it with:\n"
            "    python3 -m pip install --user matplotlib\n"
            f"(underlying error: {exc})"
        )

    mesh_sizes = sorted(
        {size for (impl, section, size) in grouped if impl == "CPU" and section == SECTIONS[0]}
    )
    if not mesh_sizes:
        raise ValueError("CSV has no CPU residual rows")
    have_gpu = all(
        ("GPU", section, size) in grouped for section in SECTIONS for size in mesh_sizes
    )

    def series(implementation, section):
        samples = [grouped[implementation, section, size] for size in mesh_sizes]
        means = [statistics.mean(values) for values in samples]
        stdev = [statistics.stdev(values) if len(values) > 1 else 0.0 for values in samples]
        return means, stdev

    plt.rcParams.update(
        {
            "font.family": "sans-serif",
            "font.size": 10.5,
            "axes.titleweight": "bold",
            "axes.spines.top": False,
            "axes.spines.right": False,
            "axes.grid": True,
            "grid.alpha": 0.22,
            "grid.linewidth": 0.8,
            "legend.frameon": False,
        }
    )
    fig, axes = plt.subplots(
        2, 2, figsize=(11.0, 7.6), sharex=True, gridspec_kw={"height_ratios": (2.1, 1)}
    )

    for column, section in enumerate(SECTIONS):
        ax_time = axes[0, column]
        ax_ratio = axes[1, column]

        cpu_means, cpu_stdev = series("CPU", section)
        ax_time.errorbar(
            mesh_sizes, cpu_means, yerr=cpu_stdev, color="#243B53",
            marker="o", linewidth=1.8, elinewidth=1.6, capsize=4, label="CPU",
        )

        if have_gpu:
            gpu_means, gpu_stdev = series("GPU", section)
            ax_time.errorbar(
                mesh_sizes, gpu_means, yerr=gpu_stdev, color="#D1495B",
                marker="s", linewidth=1.8, elinewidth=1.6, capsize=4, label="GPU",
            )
            ratios = [cpu / gpu for cpu, gpu in zip(cpu_means, gpu_means)]
            ax_ratio.axhline(1.0, color="#718096", linewidth=1.2, linestyle="--")
            ax_ratio.plot(mesh_sizes, ratios, color="#2A9D8F", marker="D", linewidth=2.4)
            for x, ratio in zip(mesh_sizes, ratios):
                ax_ratio.annotate(
                    f"{ratio:.2f}x", (x, ratio),
                    xytext=(0, 8 if ratio >= 1 else -13),
                    textcoords="offset points", ha="center", fontsize=8.5, color="#206F66",
                )
            if column == 0:
                ax_ratio.set_ylabel("GPU speedup\n(CPU/GPU)")
        else:
            ax_ratio.text(
                0.5, 0.5, "GPU data unavailable",
                transform=ax_ratio.transAxes, ha="center", va="center", color="#7B8794",
            )
            ax_ratio.set_yticks([])

        ax_time.set_title(SECTION_TITLES[section], loc="left", fontsize=12, pad=8)
        ax_time.set_xscale("log", base=2)
        ax_time.set_yscale("log")
        ax_time.legend(loc="upper left")
        if column == 0:
            ax_time.set_ylabel("PerfGraph average\n(s/call), lower is better")
        ax_ratio.set_xlabel("Elements per mesh direction (nx=ny)")
        ax_ratio.set_xscale("log", base=2)
        ax_ratio.set_xticks(mesh_sizes, labels=[str(size) for size in mesh_sizes])

    fig.suptitle(args.title, x=0.06, ha="left", fontsize=15, fontweight="bold")
    fig.text(
        0.06, 0.935,
        "Points show mean time per call; error bars show +/-1 standard deviation",
        color="#52606D", fontsize=9.5,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.92))
    args.plot.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.plot, dpi=300, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"Plot: {args.plot}")


def main():
    args = parse_args()
    if not args.csv.exists():
        print(f"error: CSV not found: {args.csv}", file=sys.stderr)
        return 1
    grouped = read_grouped(args.csv)
    if not args.no_print:
        print_table(grouped)
        print()
    plot(args, grouped)
    return 0


if __name__ == "__main__":
    sys.exit(main())
