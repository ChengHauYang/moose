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

EXPONENTS = (1, 2, 3, 4, 5)
DEFAULT_CPU_MPI_RANKS = 1
DEFAULT_CPU_THREADS = 16
DEFAULT_GPU_MPI_RANKS = 1
SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_EXECUTABLE = SCRIPT_DIR.parents[3] / "solid_mechanics-opt"
DEFAULT_KOKKOS_INPUT = SCRIPT_DIR / "kokkos_material_linear_elasticity.i"


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compare MOOSE Kokkos CPU and GPU wall time as the mesh grows."
    )
    parser.add_argument(
        "--executable", type=Path, default=DEFAULT_EXECUTABLE, help="MOOSE executable to benchmark"
    )
    parser.add_argument(
        "--kokkos-input", type=Path, default=DEFAULT_KOKKOS_INPUT, help="Kokkos input file"
    )
    parser.add_argument(
        "--base-mesh-size",
        type=int,
        default=32,
        help="Base N for mesh sizes N * 2^n, n=1,...,5 (default: 32)",
    )
    parser.add_argument(
        "--cpu-mpi-ranks",
        type=int,
        default=DEFAULT_CPU_MPI_RANKS,
        help=f"MPI ranks for CPU runs (default: {DEFAULT_CPU_MPI_RANKS})",
    )
    parser.add_argument(
        "--cpu-threads",
        type=int,
        default=DEFAULT_CPU_THREADS,
        help=f"Threads per MPI rank for CPU runs (default: {DEFAULT_CPU_THREADS})",
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
        help="Skip GPU runs (useful when PETSc/Kokkos lacks CUDA support)",
    )
    parser.add_argument(
        "--gpu-ksp-env",
        default="",
        help="PETSC_OPTIONS value injected into GPU-run env only. Use to enable "
        "GPU linear solves, e.g. '-use_gpu_aware_mpi 0 -vec_type kokkos -mat_type aijkokkos'. "
        "CPU runs never see this env var.",
    )
    parser.add_argument("--csv", type=Path, default=SCRIPT_DIR / "mesh_scaling.csv")
    parser.add_argument("--plot", type=Path, default=SCRIPT_DIR / "mesh_scaling.png")
    parser.add_argument("--title", default="MOOSE Kokkos: CPU vs. GPU Mesh Scaling")
    parser.add_argument(
        "--plot-only", action="store_true", help="Regenerate the plot from an existing CSV"
    )
    args = parser.parse_args()

    if not args.plot_only:
        if args.base_mesh_size < 1:
            parser.error("--base-mesh-size must be positive")
        if args.repetitions < 2:
            parser.error("--repetitions must be at least 2")
        if args.warmups < 0:
            parser.error("--warmups cannot be negative")
        if args.cpu_mpi_ranks < 1:
            parser.error("--cpu-mpi-ranks must be positive")
        if args.cpu_threads < 1:
            parser.error("--cpu-threads must be positive")
        if args.gpu_mpi_ranks < 1:
            parser.error("--gpu-mpi-ranks must be positive")
    return args


def command(args, device, mesh_size):
    ranks = args.cpu_mpi_ranks if device == "CPU" else args.gpu_mpi_ranks
    threads = args.cpu_threads if device == "CPU" else 1
    forward_env = ["-x", "PETSC_OPTIONS"] if device == "GPU" and args.gpu_ksp_env else []
    cmd = [
        args.mpiexec,
        *shlex.split(args.mpiexec_extra),
        *forward_env,
        "-np",
        str(ranks),
        str(args.executable),
        "-i",
        str(args.kokkos_input),
        f"--n-threads={threads}",
        f"Mesh/generated/nx={mesh_size}",
        f"Mesh/generated/ny={mesh_size}",
        f"--compute-device={'cpu' if device == 'CPU' else 'cuda'}",
    ]
    return [*cmd, *shlex.split(args.extra_args)]


def run_once(args, device, mesh_size):
    cmd = command(args, device, mesh_size)
    env = os.environ.copy()
    env.setdefault("OMP_PROC_BIND", "spread")
    env.setdefault("OMP_PLACES", "cores")
    if device == "GPU" and args.gpu_ksp_env:
        env["PETSC_OPTIONS"] = args.gpu_ksp_env

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

    mesh_sizes = [args.base_mesh_size * 2**exponent for exponent in EXPONENTS]
    devices = ["CPU"] if args.skip_gpu else ["CPU", "GPU"]
    cases = [(device, mesh_size) for mesh_size in mesh_sizes for device in devices]

    print(f"Mesh sizes: {', '.join(str(size) for size in mesh_sizes)} elements per direction")
    print(f"CPU: {args.cpu_mpi_ranks} MPI rank(s) x {args.cpu_threads} thread(s)")
    if args.skip_gpu:
        print("GPU: skipped (--skip-gpu)")
    else:
        print(f"GPU: {args.gpu_mpi_ranks} MPI rank(s) x 1 thread")
    print(f"OMP_PROC_BIND={os.environ.get('OMP_PROC_BIND', 'spread (script default)')}")
    print(f"OMP_PLACES={os.environ.get('OMP_PLACES', 'cores (script default)')}")

    for warmup in range(args.warmups):
        print(f"Warm-up round {warmup + 1}/{args.warmups}")
        for device, mesh_size in cases:
            run_once(args, device, mesh_size)

    rows = []
    rng = random.Random(20260826)
    for repetition in range(1, args.repetitions + 1):
        shuffled = cases.copy()
        rng.shuffle(shuffled)
        for device, mesh_size in shuffled:
            elapsed = run_once(args, device, mesh_size)
            ranks = args.cpu_mpi_ranks if device == "CPU" else args.gpu_mpi_ranks
            threads = args.cpu_threads if device == "CPU" else 1
            rows.append(
                {
                    "device": device,
                    "mesh_size": mesh_size,
                    "elements": mesh_size**2,
                    "mpi_ranks": ranks,
                    "threads_per_rank": threads,
                    "repetition": repetition,
                    "wall_time_seconds": f"{elapsed:.9f}",
                }
            )
            print(
                f"[{repetition}/{args.repetitions}] {device:3s} "
                f"mesh={mesh_size:5d} x {mesh_size:<5d}: {elapsed:.4f} s"
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
            mesh_size = int(row["mesh_size"])
            wall_time = float(row["wall_time_seconds"])
            grouped.setdefault((device, mesh_size), []).append(wall_time)
    return grouped


def plot_results(args):
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise RuntimeError("Plotting requires Matplotlib: python -m pip install matplotlib") from error

    grouped = read_results(args.csv)
    mesh_sizes = sorted(mesh_size for device, mesh_size in grouped if device == "CPU")
    if not mesh_sizes:
        raise ValueError("CSV has no CPU results")
    missing_gpu = [size for size in mesh_sizes if ("GPU", size) not in grouped]
    have_gpu = not missing_gpu
    if missing_gpu and len(missing_gpu) != len(mesh_sizes):
        raise ValueError(f"CSV is missing GPU mesh sizes: {missing_gpu}")

    cpu_means = [statistics.mean(grouped["CPU", size]) for size in mesh_sizes]
    cpu_stdev = [statistics.stdev(grouped["CPU", size]) for size in mesh_sizes]

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
    ax_time.errorbar(
        mesh_sizes,
        cpu_means,
        yerr=cpu_stdev,
        color="#243B53",
        marker="o",
        linewidth=1.8,
        elinewidth=1.6,
        capsize=4,
        label="CPU",
    )

    if have_gpu:
        gpu_means = [statistics.mean(grouped["GPU", size]) for size in mesh_sizes]
        gpu_stdev = [statistics.stdev(grouped["GPU", size]) for size in mesh_sizes]
        ax_time.errorbar(
            mesh_sizes,
            gpu_means,
            yerr=gpu_stdev,
            color="#D1495B",
            marker="s",
            linewidth=1.8,
            elinewidth=1.6,
            capsize=4,
            label="GPU",
        )

        ratios = [cpu / gpu for cpu, gpu in zip(cpu_means, gpu_means)]
        ax_ratio.axhline(1.0, color="#718096", linewidth=1.2, linestyle="--")
        ax_ratio.plot(mesh_sizes, ratios, color="#2A9D8F", marker="D", linewidth=2.4)
        for x, ratio in zip(mesh_sizes, ratios):
            ax_ratio.annotate(
                f"{ratio:.2f}x",
                (x, ratio),
                xytext=(0, 8 if ratio >= 1 else -13),
                textcoords="offset points",
                ha="center",
                fontsize=8.5,
                color="#206F66",
            )
        ax_ratio.set_ylabel("GPU speedup\n(CPU/GPU)")
    else:
        ax_ratio.text(
            0.5,
            0.5,
            "GPU data unavailable",
            transform=ax_ratio.transAxes,
            ha="center",
            va="center",
            color="#7B8794",
        )
        ax_ratio.set_yticks([])

    ax_time.set_title(args.title, loc="left", fontsize=15, pad=14)
    ax_time.text(
        0,
        1.015,
        "Points show mean wall time; error bars show +/-1 standard deviation",
        transform=ax_time.transAxes,
        color="#52606D",
        fontsize=9.5,
    )
    ax_time.set_ylabel("Wall time (s), lower is better")
    ax_time.set_xscale("log", base=2)
    ax_time.set_yscale("log")
    ax_time.legend(loc="upper left")
    ax_ratio.set_xlabel("Elements per mesh direction")
    ax_ratio.set_xscale("log", base=2)
    ax_ratio.set_xticks(mesh_sizes, labels=[str(size) for size in mesh_sizes])

    fig.tight_layout()
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
