#!/usr/bin/env python3

import argparse
import csv
import os
from pathlib import Path
import random
import shlex
import statistics
import subprocess
import sys
import time

THREAD_COUNTS = (1, 2, 4, 8, 16)
DEFAULT_CPU_MPI_RANKS = 2
DEFAULT_GPU_MPI_RANKS = 1
SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_EXECUTABLE = SCRIPT_DIR.parents[3] / "solid_mechanics-opt"
DEFAULT_KOKKOS_INPUT = SCRIPT_DIR / "kokkos_material_linear_elasticity.i"


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Compare MOOSE Kokkos wall time on CPU (varying threads per MPI rank) "
            "against a single GPU configuration."
        )
    )
    parser.add_argument(
        "--executable", type=Path, default=DEFAULT_EXECUTABLE, help="MOOSE executable to benchmark"
    )
    parser.add_argument(
        "--kokkos-input", type=Path, default=DEFAULT_KOKKOS_INPUT, help="Kokkos input file"
    )
    parser.add_argument(
        "--mesh-size",
        type=int,
        default=256,
        help="Elements in each mesh direction (default: 256)",
    )
    parser.add_argument(
        "--cpu-mpi-ranks",
        type=int,
        default=DEFAULT_CPU_MPI_RANKS,
        help=f"MPI ranks for CPU runs (default: {DEFAULT_CPU_MPI_RANKS})",
    )
    parser.add_argument(
        "--gpu-mpi-ranks",
        type=int,
        default=DEFAULT_GPU_MPI_RANKS,
        help=f"MPI ranks for GPU runs (default: {DEFAULT_GPU_MPI_RANKS})",
    )
    parser.add_argument("--repetitions", type=int, default=5, help="Timed runs per configuration")
    parser.add_argument("--warmups", type=int, default=1, help="Untimed runs per configuration")
    parser.add_argument("--mpiexec", default="mpirun", help="MPI launcher (default: mpirun)")
    parser.add_argument(
        "--mpiexec-extra",
        default="",
        help="Extra MPI launcher arguments as one quoted string, e.g. '--bind-to core'",
    )
    parser.add_argument(
        "--extra-args",
        default="Outputs/exodus=false",
        help="Arguments appended to all MOOSE commands as one quoted string",
    )
    parser.add_argument(
        "--skip-gpu",
        action="store_true",
        help="Skip GPU runs (useful when PETSc/Kokkos lacks CUDA/HIP/SYCL support)",
    )
    parser.add_argument("--csv", type=Path, default=SCRIPT_DIR / "cpu_and_gpu.csv")
    parser.add_argument("--plot", type=Path, default=SCRIPT_DIR / "cpu_and_gpu.png")
    parser.add_argument("--title", default="MOOSE Kokkos: CPU Threading vs. GPU")
    parser.add_argument(
        "--plot-only", action="store_true", help="Regenerate the plot from an existing CSV"
    )
    args = parser.parse_args()

    if not args.plot_only:
        if args.mesh_size < 1:
            parser.error("--mesh-size must be positive")
        if args.repetitions < 2:
            parser.error("--repetitions must be at least 2")
        if args.warmups < 0:
            parser.error("--warmups cannot be negative")
        if args.cpu_mpi_ranks < 1:
            parser.error("--cpu-mpi-ranks must be positive")
        if args.gpu_mpi_ranks < 1:
            parser.error("--gpu-mpi-ranks must be positive")
    return args


def command(args, device, threads):
    ranks = args.cpu_mpi_ranks if device == "CPU" else args.gpu_mpi_ranks
    cmd = [
        args.mpiexec,
        *shlex.split(args.mpiexec_extra),
        "-np",
        str(ranks),
        str(args.executable),
        "-i",
        str(args.kokkos_input),
        f"--n-threads={threads}",
        f"Mesh/generated/nx={args.mesh_size}",
        f"Mesh/generated/ny={args.mesh_size}",
        f"--compute-device={'cpu' if device == 'CPU' else 'cuda'}",
    ]
    return [*cmd, *shlex.split(args.extra_args)]


def run_once(args, device, threads):
    cmd = command(args, device, threads)
    env = os.environ.copy()
    env.setdefault("OMP_PROC_BIND", "spread")
    env.setdefault("OMP_PLACES", "cores")

    start = time.perf_counter()
    result = subprocess.run(cmd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    elapsed = time.perf_counter() - start
    if result.returncode:
        quoted = shlex.join(cmd)
        raise RuntimeError(f"Command failed ({result.returncode}):\n{quoted}\n\n{result.stdout}")
    return elapsed


def collect(args):
    for path_name in ("executable", "kokkos_input"):
        path = getattr(args, path_name).expanduser().resolve()
        if not path.exists():
            raise FileNotFoundError(f"{path_name.replace('_', ' ')} not found: {path}")
        setattr(args, path_name, path)

    cases = [("CPU", threads) for threads in THREAD_COUNTS]
    if not args.skip_gpu:
        cases.append(("GPU", 1))

    print(f"CPU: {args.cpu_mpi_ranks} MPI rank(s); threads/rank in {THREAD_COUNTS}")
    if not args.skip_gpu:
        print(f"GPU: {args.gpu_mpi_ranks} MPI rank(s); one aggregated data point")
    else:
        print("GPU: skipped (--skip-gpu)")
    print(f"Mesh: {args.mesh_size} x {args.mesh_size} elements")
    print(f"OMP_PROC_BIND={os.environ.get('OMP_PROC_BIND', 'spread (script default)')}")
    print(f"OMP_PLACES={os.environ.get('OMP_PLACES', 'cores (script default)')}")

    for warmup in range(args.warmups):
        print(f"Warm-up round {warmup + 1}/{args.warmups}")
        for device, threads in cases:
            run_once(args, device, threads)

    rows = []
    rng = random.Random(20260825)
    for repetition in range(1, args.repetitions + 1):
        shuffled = cases.copy()
        rng.shuffle(shuffled)
        for device, threads in shuffled:
            elapsed = run_once(args, device, threads)
            ranks = args.cpu_mpi_ranks if device == "CPU" else args.gpu_mpi_ranks
            row = {
                "device": device,
                "mpi_ranks": ranks,
                "threads_per_rank": threads,
                "total_cpu_threads": ranks * threads if device == "CPU" else 0,
                "repetition": repetition,
                "wall_time_seconds": f"{elapsed:.9f}",
            }
            rows.append(row)
            label = f"threads/rank={threads:2d}" if device == "CPU" else "GPU        "
            print(
                f"[{repetition}/{args.repetitions}] {device:3s} {label}: {elapsed:.4f} s"
            )

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    print(f"Raw timings: {args.csv}")


def read_results(csv_path):
    grouped = {}
    with csv_path.open(newline="") as stream:
        for row in csv.DictReader(stream):
            device = row["device"]
            threads = int(row["threads_per_rank"])
            wall_time = float(row["wall_time_seconds"])
            grouped.setdefault((device, threads), []).append(wall_time)
    return grouped


def plot_results(args):
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise RuntimeError("Plotting requires Matplotlib: python -m pip install matplotlib") from error

    grouped = read_results(args.csv)
    missing_cpu = sorted({n for n in THREAD_COUNTS if ("CPU", n) not in grouped})
    if missing_cpu:
        raise ValueError(f"CSV is missing CPU thread counts: {missing_cpu}")
    have_gpu = ("GPU", 1) in grouped

    cpu_color = "#243B53"
    gpu_color = "#D1495B"
    speedup_color = "#2A9D8F"

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
        2, 1, figsize=(8.2, 7.4), sharex=True, gridspec_kw={"height_ratios": (2.1, 1)}
    )

    cpu_means = [statistics.mean(grouped[("CPU", n)]) for n in THREAD_COUNTS]
    cpu_stdev = [statistics.stdev(grouped[("CPU", n)]) for n in THREAD_COUNTS]
    ax_time.errorbar(
        THREAD_COUNTS,
        cpu_means,
        yerr=cpu_stdev,
        color=cpu_color,
        marker="o",
        linestyle="-",
        linewidth=1.4,
        elinewidth=1.8,
        capsize=5,
        capthick=1.8,
        markersize=6.5,
        label=f"CPU (Kokkos, {args.cpu_mpi_ranks} MPI rank{'s' if args.cpu_mpi_ranks > 1 else ''})",
    )

    if have_gpu:
        gpu_times = grouped[("GPU", 1)]
        gpu_mean = statistics.mean(gpu_times)
        gpu_stdev = statistics.stdev(gpu_times)
        ax_time.axhspan(
            gpu_mean - gpu_stdev,
            gpu_mean + gpu_stdev,
            color=gpu_color,
            alpha=0.18,
            label=f"GPU (Kokkos, {args.gpu_mpi_ranks} MPI rank{'s' if args.gpu_mpi_ranks > 1 else ''}, +/-1 s.d.)",
        )
        ax_time.axhline(
            gpu_mean,
            color=gpu_color,
            linewidth=1.8,
            linestyle="--",
        )
        ax_time.annotate(
            f"GPU mean = {gpu_mean:.3f} s",
            xy=(THREAD_COUNTS[-1], gpu_mean),
            xytext=(4, 6),
            textcoords="offset points",
            color=gpu_color,
            fontsize=9,
            ha="right",
        )

        ax_ratio.axhline(1.0, color="#718096", linewidth=1.2, linestyle="--")
        ratios = [cpu / gpu_mean for cpu in cpu_means]
        ax_ratio.plot(
            THREAD_COUNTS,
            ratios,
            color=speedup_color,
            marker="D",
            linewidth=2.4,
            markersize=6,
        )
        ax_ratio.fill_between(
            THREAD_COUNTS,
            1.0,
            ratios,
            where=[ratio >= 1 for ratio in ratios],
            color=speedup_color,
            alpha=0.12,
            interpolate=True,
        )
        for x, ratio in zip(THREAD_COUNTS, ratios):
            ax_ratio.annotate(
                f"{ratio:.2f}x",
                (x, ratio),
                xytext=(0, 8 if ratio >= 1 else -13),
                textcoords="offset points",
                ha="center",
                fontsize=8.5,
                color="#206F66",
            )
        ax_ratio.set_ylabel("GPU speedup (CPU/GPU)")
    else:
        ax_ratio.text(
            0.5,
            0.5,
            "GPU data unavailable\n(rerun without --skip-gpu when PETSc has GPU Kokkos support)",
            transform=ax_ratio.transAxes,
            ha="center",
            va="center",
            color="#7B8794",
            fontsize=10,
        )
        ax_ratio.set_yticks([])

    ax_time.set_title(args.title, loc="left", fontsize=15, pad=14)
    ax_time.text(
        0,
        1.015,
        "CPU: mean +/-1 s.d. across thread counts. GPU: horizontal band = mean +/-1 s.d.",
        transform=ax_time.transAxes,
        color="#52606D",
        fontsize=9.5,
    )
    ax_time.set_ylabel("Wall time (s), lower is better")
    ax_time.legend(loc="upper right")
    ax_time.set_xscale("log", base=2)
    ax_ratio.set_xlabel(f"Threads per MPI rank ({args.cpu_mpi_ranks} MPI rank(s) fixed for CPU)")
    ax_ratio.set_xscale("log", base=2)
    ax_ratio.set_xticks(THREAD_COUNTS, labels=[str(n) for n in THREAD_COUNTS])

    fig.text(
        0.99,
        0.01,
        f"Total CPU threads: {', '.join(str(args.cpu_mpi_ranks * n) for n in THREAD_COUNTS)}",
        ha="right",
        color="#7B8794",
        fontsize=8.5,
    )
    fig.tight_layout(rect=(0, 0.025, 1, 1))
    args.plot.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.plot, dpi=300, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"Plot: {args.plot}")


def main():
    args = parse_args()
    try:
        if not args.plot_only:
            collect(args)
        if not args.csv.exists():
            raise FileNotFoundError(f"CSV not found: {args.csv}")
        plot_results(args)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
