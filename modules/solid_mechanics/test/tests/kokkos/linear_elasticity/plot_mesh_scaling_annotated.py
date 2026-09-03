#!/usr/bin/env python3
"""Plot mesh_scaling_*.csv with a config annotation subtitle.

Same two-panel layout as plot_mesh_scaling.py (wall time and CPU/GPU ratio),
plus a subtitle showing how the run was configured: MPI ranks, threads per
rank, GPU count, Kokkos backend, and any free-form extra note (e.g. PETSc
Vec/Mat types, CUDA-aware MPI).

MPI ranks and threads-per-rank are read from the CSV (per device). Backend
labels, GPU count, and the extra note are passed via CLI flags because the
CSV does not record them.

Usage:
  ./plot_mesh_scaling_annotated.py --csv mesh_scaling_gpu_ksp.csv \
      --cpu-backend 'Kokkos::OpenMP' \
      --gpu-backend 'Kokkos::CUDA' --gpu-count 1 \
      --extra-note 'PETSc Vec/Mat on device (KSP on GPU)'
"""
import argparse
import csv
import statistics
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--csv", type=Path, default=SCRIPT_DIR / "mesh_scaling.csv")
    parser.add_argument(
        "--plot",
        type=Path,
        default=None,
        help="Output PNG (default: same directory + basename as CSV, .png)",
    )
    parser.add_argument(
        "--title",
        default="MOOSE Kokkos: mesh-size scaling, CPU vs GPU",
    )
    parser.add_argument(
        "--cpu-backend",
        default="Kokkos::OpenMP",
        help="Kokkos backend label for CPU rows",
    )
    parser.add_argument(
        "--gpu-backend",
        default="Kokkos::CUDA",
        help="Kokkos backend label for GPU rows",
    )
    parser.add_argument(
        "--gpu-count",
        type=int,
        default=1,
        help="Number of GPUs used per GPU run (informational, shown on plot)",
    )
    parser.add_argument(
        "--extra-note",
        default="",
        help="Extra free-form line shown under the config subtitle "
             "(e.g. 'PETSc Vec/Mat on device, CUDA-aware MPI')",
    )
    parser.add_argument("--no-print", action="store_true")
    args = parser.parse_args()
    if args.plot is None:
        args.plot = args.csv.with_suffix(".png")
    return args


def read_grouped(csv_path):
    """Return (grouped_times, run_config).

    grouped_times : {(device, mesh_size): [wall_time, ...]}
    run_config    : {device: (mpi_ranks, threads_per_rank)} inferred from CSV.
                    If a device has inconsistent counts across rows, raises.
    """
    grouped = {}
    config = {}
    with csv_path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None:
            raise ValueError(f"CSV {csv_path} is empty or has no header")
        for row in reader:
            device = row["device"]
            mesh_size = int(row["mesh_size"])
            wall_time = float(row["wall_time_seconds"])
            ranks = int(row["mpi_ranks"])
            threads = int(row["threads_per_rank"])
            grouped.setdefault((device, mesh_size), []).append(wall_time)
            existing = config.setdefault(device, (ranks, threads))
            if existing != (ranks, threads):
                raise ValueError(
                    f"CSV {csv_path} has inconsistent {device} config: "
                    f"first saw {existing}, now {(ranks, threads)}"
                )
    if not grouped:
        raise ValueError(f"CSV {csv_path} has no data rows")
    return grouped, config


def summarise(grouped):
    for (device, mesh_size), samples in sorted(grouped.items()):
        n = len(samples)
        mean = statistics.mean(samples)
        stdev = statistics.stdev(samples) if n > 1 else 0.0
        yield device, mesh_size, n, mean, stdev


def print_table(grouped):
    print(f"{'device':<6} {'mesh':>6} {'n':>3} {'mean (s)':>12} {'stdev (s)':>12}")
    print("-" * 44)
    for device, mesh_size, n, mean, stdev in summarise(grouped):
        print(f"{device:<6} {mesh_size:>6} {n:>3} {mean:>12.4f} {stdev:>12.4f}")


def config_subtitle(config, args):
    """Compose the one-or-two-line subtitle from CSV counts + CLI labels."""
    parts = []
    if "CPU" in config:
        cpu_ranks, cpu_threads = config["CPU"]
        parts.append(
            f"CPU: {cpu_ranks} MPI x {cpu_threads} thread"
            f"{'s' if cpu_threads != 1 else ''} ({args.cpu_backend})"
        )
    if "GPU" in config:
        gpu_ranks, gpu_threads = config["GPU"]
        gpu_str = (
            f"GPU: {gpu_ranks} MPI x {args.gpu_count} GPU"
            f"{'s' if args.gpu_count != 1 else ''} x {gpu_threads} thread"
            f"{'s' if gpu_threads != 1 else ''} ({args.gpu_backend})"
        )
        parts.append(gpu_str)
    return "   |   ".join(parts)


def plot(args, grouped, config):
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

    mesh_sizes = sorted({m for (d, m) in grouped if d == "CPU"})
    if not mesh_sizes:
        raise ValueError("CSV has no CPU rows")
    have_gpu = all(("GPU", m) in grouped for m in mesh_sizes)

    cpu_means = [statistics.mean(grouped["CPU", m]) for m in mesh_sizes]
    cpu_stdev = [
        statistics.stdev(grouped["CPU", m]) if len(grouped["CPU", m]) > 1 else 0.0
        for m in mesh_sizes
    ]

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
    fig, (ax_time, ax_ratio) = plt.subplots(
        2, 1, figsize=(8.6, 7.8), sharex=True, gridspec_kw={"height_ratios": (2.1, 1)}
    )

    ax_time.errorbar(
        mesh_sizes, cpu_means, yerr=cpu_stdev, color="#243B53",
        marker="o", linewidth=1.8, elinewidth=1.6, capsize=4, label="CPU",
    )

    if have_gpu:
        gpu_means = [statistics.mean(grouped["GPU", m]) for m in mesh_sizes]
        gpu_stdev = [
            statistics.stdev(grouped["GPU", m]) if len(grouped["GPU", m]) > 1 else 0.0
            for m in mesh_sizes
        ]
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
        ax_ratio.set_ylabel("GPU speedup\n(CPU/GPU)")
    else:
        ax_ratio.text(
            0.5, 0.5, "GPU data unavailable",
            transform=ax_ratio.transAxes, ha="center", va="center", color="#7B8794",
        )
        ax_ratio.set_yticks([])

    # Title + config annotation stack.
    # Extra vertical padding leaves room for two subtitle lines above the top panel.
    ax_time.set_title(args.title, loc="left", fontsize=15, pad=44)
    subtitle = config_subtitle(config, args)
    ax_time.text(
        0, 1.115,
        subtitle,
        transform=ax_time.transAxes, color="#243B53", fontsize=10, fontweight="bold",
    )
    caption = "Points show mean wall time; error bars show +/-1 standard deviation"
    if args.extra_note:
        caption = f"{args.extra_note}   |   {caption}"
    ax_time.text(
        0, 1.03,
        caption,
        transform=ax_time.transAxes, color="#52606D", fontsize=9.5,
    )
    ax_time.set_ylabel("Wall time (s), lower is better")
    ax_time.set_xscale("log", base=2)
    ax_time.set_yscale("log")
    ax_time.legend(loc="upper left")
    ax_ratio.set_xlabel("Elements per mesh direction")
    ax_ratio.set_xscale("log", base=2)
    ax_ratio.set_xticks(mesh_sizes, labels=[str(m) for m in mesh_sizes])

    fig.tight_layout()
    args.plot.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.plot, dpi=300, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"Plot: {args.plot}")


def main():
    args = parse_args()
    if not args.csv.exists():
        print(f"error: CSV not found: {args.csv}", file=sys.stderr)
        return 1
    grouped, config = read_grouped(args.csv)
    if not args.no_print:
        print_table(grouped)
        print()
        print(config_subtitle(config, args))
        if args.extra_note:
            print(args.extra_note)
        print()
    plot(args, grouped, config)
    return 0


if __name__ == "__main__":
    sys.exit(main())
