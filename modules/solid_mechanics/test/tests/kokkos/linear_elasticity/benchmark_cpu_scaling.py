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
MPI_RANKS = 2
SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_EXECUTABLE = SCRIPT_DIR.parents[3] / "solid_mechanics-opt"
DEFAULT_BASELINE_INPUT = SCRIPT_DIR / "baseline_linear_elasticity.i"
DEFAULT_KOKKOS_INPUT = SCRIPT_DIR / "kokkos_material_linear_elasticity.i"


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compare CPU scaling of equivalent MOOSE Kokkos and non-Kokkos inputs."
    )
    parser.add_argument(
        "--executable", type=Path, default=DEFAULT_EXECUTABLE, help="MOOSE executable to benchmark"
    )
    parser.add_argument(
        "--baseline-input",
        type=Path,
        default=DEFAULT_BASELINE_INPUT,
        help="Equivalent non-Kokkos input file",
    )
    parser.add_argument(
        "--kokkos-input", type=Path, default=DEFAULT_KOKKOS_INPUT, help="Kokkos input file"
    )
    parser.add_argument(
        "--mesh-size",
        type=int,
        default=256,
        help="Elements in each mesh direction for both inputs (default: 256)",
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
        help="Arguments appended to both MOOSE commands as one quoted string",
    )
    parser.add_argument("--csv", type=Path, default=SCRIPT_DIR / "cpu_scaling.csv")
    parser.add_argument("--plot", type=Path, default=SCRIPT_DIR / "cpu_scaling.png")
    parser.add_argument("--title", default="MOOSE CPU Scaling: Kokkos vs. Baseline")
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
    return args


def command(args, implementation, threads):
    input_file = args.kokkos_input if implementation == "Kokkos" else args.baseline_input
    cmd = [
        args.mpiexec,
        *shlex.split(args.mpiexec_extra),
        "-np",
        str(MPI_RANKS),
        str(args.executable),
        "-i",
        str(input_file),
        f"--n-threads={threads}",
        f"Mesh/generated/nx={args.mesh_size}",
        f"Mesh/generated/ny={args.mesh_size}",
    ]
    if implementation == "Kokkos":
        cmd.append("--compute-device=cpu")
    return [*cmd, *shlex.split(args.extra_args)]


def run_once(args, implementation, threads):
    cmd = command(args, implementation, threads)
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
    for path_name in ("executable", "baseline_input", "kokkos_input"):
        path = getattr(args, path_name).expanduser().resolve()
        if not path.exists():
            raise FileNotFoundError(f"{path_name.replace('_', ' ')} not found: {path}")
        setattr(args, path_name, path)

    cases = [(name, threads) for threads in THREAD_COUNTS for name in ("Baseline", "Kokkos")]
    print(f"Using {MPI_RANKS} MPI ranks; total CPU threads range from 2 to 32.")
    print(f"Mesh: {args.mesh_size} x {args.mesh_size} elements")
    print(f"OMP_PROC_BIND={os.environ.get('OMP_PROC_BIND', 'spread (script default)')}")
    print(f"OMP_PLACES={os.environ.get('OMP_PLACES', 'cores (script default)')}")

    for warmup in range(args.warmups):
        print(f"Warm-up round {warmup + 1}/{args.warmups}")
        for implementation, threads in cases:
            run_once(args, implementation, threads)

    rows = []
    rng = random.Random(20260825)
    for repetition in range(1, args.repetitions + 1):
        shuffled = cases.copy()
        rng.shuffle(shuffled)
        for implementation, threads in shuffled:
            elapsed = run_once(args, implementation, threads)
            row = {
                "implementation": implementation,
                "mpi_ranks": MPI_RANKS,
                "threads_per_rank": threads,
                "total_cpu_threads": MPI_RANKS * threads,
                "repetition": repetition,
                "wall_time_seconds": f"{elapsed:.9f}",
            }
            rows.append(row)
            print(
                f"[{repetition}/{args.repetitions}] {implementation:8s} "
                f"threads/rank={threads:2d}: {elapsed:.4f} s"
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
            implementation = row["implementation"]
            threads = int(row["threads_per_rank"])
            wall_time = float(row["wall_time_seconds"])
            grouped.setdefault((implementation, threads), []).append(wall_time)
    return grouped


def plot_results(args):
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise RuntimeError("Plotting requires Matplotlib: python -m pip install matplotlib") from error

    grouped = read_results(args.csv)
    expected = {(name, n) for name in ("Baseline", "Kokkos") for n in THREAD_COUNTS}
    missing = sorted(expected - grouped.keys())
    if missing:
        raise ValueError(f"CSV is missing configurations: {missing}")

    colors = {"Baseline": "#243B53", "Kokkos": "#D1495B"}
    markers = {"Baseline": "o", "Kokkos": "s"}
    position_factors = {"Baseline": 1 / 1.06, "Kokkos": 1.06}
    means = {}

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

    for name in ("Baseline", "Kokkos"):
        mean = [statistics.mean(grouped[name, n]) for n in THREAD_COUNTS]
        standard_deviation = [statistics.stdev(grouped[name, n]) for n in THREAD_COUNTS]
        means[name] = mean
        positions = [n * position_factors[name] for n in THREAD_COUNTS]
        ax_time.errorbar(
            positions,
            mean,
            yerr=standard_deviation,
            color=colors[name],
            marker=markers[name],
            linestyle="none",
            elinewidth=1.8,
            capsize=5,
            capthick=1.8,
            markersize=6.5,
            label=name,
        )

    ratios = [base / kokkos for base, kokkos in zip(means["Baseline"], means["Kokkos"])]
    ax_ratio.axhline(1.0, color="#718096", linewidth=1.2, linestyle="--")
    ax_ratio.plot(
        THREAD_COUNTS,
        ratios,
        color="#2A9D8F",
        marker="D",
        linewidth=2.4,
        markersize=6,
    )
    ax_ratio.fill_between(
        THREAD_COUNTS,
        1.0,
        ratios,
        where=[ratio >= 1 for ratio in ratios],
        color="#2A9D8F",
        alpha=0.12,
        interpolate=True,
    )

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
    ax_time.legend(ncol=2, loc="upper left")
    ax_time.set_xscale("log", base=2)
    ax_ratio.set_ylabel("Kokkos speedup")
    ax_ratio.set_xlabel(f"Threads per MPI rank ({MPI_RANKS} MPI ranks fixed)")
    ax_ratio.set_xscale("log", base=2)
    ax_ratio.set_xticks(THREAD_COUNTS, labels=[str(n) for n in THREAD_COUNTS])

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

    fig.text(
        0.99,
        0.01,
        f"Total CPU threads: {', '.join(str(MPI_RANKS * n) for n in THREAD_COUNTS)}",
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
