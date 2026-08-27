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

DEFAULT_MESH_SIZES = (64, 128, 256, 512, 1024)
MPI_RANKS = 1
THREADS = 1
SECTIONS = ("computeResidualInternal", "computeJacobianInternal")
SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_EXECUTABLE = SCRIPT_DIR.parents[3] / "solid_mechanics-opt"
DEFAULT_BASELINE_INPUT = SCRIPT_DIR / "baseline_linear_elasticity.i"
DEFAULT_KOKKOS_INPUT = SCRIPT_DIR / "kokkos_material_linear_elasticity.i"


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Compare normal MOOSE CPU and Kokkos-MOOSE GPU residual/Jacobian PerfGraph "
            "times as nx=ny grows."
        )
    )
    parser.add_argument(
        "--executable", type=Path, default=DEFAULT_EXECUTABLE, help="MOOSE executable to benchmark"
    )
    parser.add_argument(
        "--baseline-input",
        type=Path,
        default=DEFAULT_BASELINE_INPUT,
        help="Normal MOOSE CPU input file",
    )
    parser.add_argument(
        "--kokkos-input", type=Path, default=DEFAULT_KOKKOS_INPUT, help="Kokkos-MOOSE GPU input file"
    )
    parser.add_argument(
        "--mesh-sizes",
        type=int,
        nargs="+",
        default=DEFAULT_MESH_SIZES,
        metavar="N",
        help="Mesh sizes applied as nx=ny=N (default: 64 128 256 512 1024)",
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
    parser.add_argument(
        "--skip-gpu",
        action="store_true",
        help="Collect only CPU data when a CUDA-enabled build is unavailable",
    )
    parser.add_argument("--csv", type=Path, default=SCRIPT_DIR / "assembly_scaling.csv")
    parser.add_argument("--plot", type=Path, default=SCRIPT_DIR / "assembly_scaling.png")
    parser.add_argument("--title", default="MOOSE Assembly Scaling: CPU vs. GPU")
    parser.add_argument(
        "--plot-only", action="store_true", help="Regenerate the plot from an existing CSV"
    )
    args = parser.parse_args()

    if not args.plot_only:
        if any(size < 1 for size in args.mesh_sizes):
            parser.error("--mesh-sizes values must be positive")
        if len(set(args.mesh_sizes)) != len(args.mesh_sizes):
            parser.error("--mesh-sizes values must be unique")
        if args.repetitions < 2:
            parser.error("--repetitions must be at least 2")
        if args.warmups < 0:
            parser.error("--warmups cannot be negative")
    return args


def command(args, implementation, mesh_size):
    input_file = args.baseline_input if implementation == "CPU" else args.kokkos_input
    cmd = [
        args.mpiexec,
        *shlex.split(args.mpiexec_extra),
        "-np",
        str(MPI_RANKS),
        str(args.executable),
        "-i",
        str(input_file),
        f"--n-threads={THREADS}",
        f"Mesh/generated/nx={mesh_size}",
        f"Mesh/generated/ny={mesh_size}",
        "-t",
    ]
    if implementation == "GPU":
        cmd.append("--compute-device=cuda")
    return [*cmd, *shlex.split(args.extra_args)]


def parse_perf_graph(output):
    metrics = {}
    for line in output.splitlines():
        columns = [column.strip() for column in line.split("|")]
        if len(columns) != 12:
            continue
        label = columns[1]
        for section in SECTIONS:
            if label == f"FEProblem::{section}":
                if section in metrics:
                    raise RuntimeError(f"PerfGraph contains multiple rows for {section}")
                calls = int(columns[2])
                total_seconds = float(columns[7])
                metrics[section] = {
                    "calls": calls,
                    "total_seconds": total_seconds,
                    "average_seconds": total_seconds / calls,
                }

    missing = [section for section in SECTIONS if section not in metrics]
    if missing:
        raise RuntimeError(
            f"PerfGraph output is missing {', '.join(missing)}; ensure performance logging is enabled"
        )
    return metrics


def run_once(args, implementation, mesh_size):
    cmd = command(args, implementation, mesh_size)
    env = os.environ.copy()
    env.setdefault("OMP_PROC_BIND", "spread")
    env.setdefault("OMP_PLACES", "cores")
    result = subprocess.run(cmd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(
            f"Command failed ({result.returncode}):\n{shlex.join(cmd)}\n\n{result.stdout}"
        )
    return parse_perf_graph(result.stdout)


def collect(args):
    for path_name in ("executable", "baseline_input", "kokkos_input"):
        path = getattr(args, path_name).expanduser().resolve()
        if not path.exists():
            raise FileNotFoundError(f"{path_name.replace('_', ' ')} not found: {path}")
        setattr(args, path_name, path)

    implementations = ["CPU"] if args.skip_gpu else ["CPU", "GPU"]
    cases = [(implementation, size) for size in args.mesh_sizes for implementation in implementations]
    print(f"Mesh sizes: {', '.join(str(size) for size in args.mesh_sizes)} (nx=ny)")
    print(f"CPU: normal MOOSE, {MPI_RANKS} MPI rank x {THREADS} thread")
    print(
        "GPU: skipped (--skip-gpu)"
        if args.skip_gpu
        else f"GPU: Kokkos-MOOSE, {MPI_RANKS} MPI rank x {THREADS} host thread x 1 CUDA GPU"
    )

    for warmup in range(args.warmups):
        print(f"Warm-up round {warmup + 1}/{args.warmups}")
        for implementation, mesh_size in cases:
            run_once(args, implementation, mesh_size)

    rows = []
    rng = random.Random(20260827)
    for repetition in range(1, args.repetitions + 1):
        shuffled = cases.copy()
        rng.shuffle(shuffled)
        for implementation, mesh_size in shuffled:
            metrics = run_once(args, implementation, mesh_size)
            for section, values in metrics.items():
                rows.append(
                    {
                        "implementation": implementation,
                        "mesh_size": mesh_size,
                        "elements": mesh_size**2,
                        "mpi_ranks": MPI_RANKS,
                        "threads_per_rank": THREADS,
                        "repetition": repetition,
                        "section": section,
                        "calls": values["calls"],
                        "total_seconds": f"{values['total_seconds']:.9f}",
                        "average_seconds": f"{values['average_seconds']:.9f}",
                    }
                )
            residual = metrics["computeResidualInternal"]
            jacobian = metrics["computeJacobianInternal"]
            print(
                f"[{repetition}/{args.repetitions}] {implementation:3s} "
                f"mesh={mesh_size:4d} x {mesh_size:<4d}: "
                f"residual={residual['average_seconds']:.6f} s/call "
                f"({residual['calls']} calls), jacobian={jacobian['average_seconds']:.6f} s/call "
                f"({jacobian['calls']} calls)"
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
            key = row["implementation"], row["section"], int(row["mesh_size"])
            grouped.setdefault(key, []).append(float(row["average_seconds"]))
    return grouped


def plot_results(args):
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise RuntimeError("Plotting requires Matplotlib: python -m pip install matplotlib") from error

    grouped = read_results(args.csv)
    mesh_sizes = sorted(
        size for implementation, section, size in grouped if implementation == "CPU" and section == SECTIONS[0]
    )
    if not mesh_sizes:
        raise ValueError("CSV has no CPU residual results")
    implementations = ["CPU"]
    if any(implementation == "GPU" for implementation, _, _ in grouped):
        implementations.append("GPU")

    expected = {
        (implementation, section, size)
        for implementation in implementations
        for section in SECTIONS
        for size in mesh_sizes
    }
    missing = sorted(expected - grouped.keys())
    if missing:
        raise ValueError(f"CSV is missing configurations: {missing}")

    colors = {"CPU": "#243B53", "GPU": "#D1495B"}
    markers = {"CPU": "o", "GPU": "s"}
    titles = {
        "computeResidualInternal": "Residual assembly",
        "computeJacobianInternal": "Jacobian assembly",
    }
    plt.rcParams.update(
        {
            "font.family": "sans-serif",
            "font.size": 10.5,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "axes.grid": True,
            "grid.alpha": 0.22,
            "legend.frameon": False,
        }
    )
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.8), sharex=True)

    for axis, section in zip(axes, SECTIONS):
        for implementation in implementations:
            samples = [grouped[implementation, section, size] for size in mesh_sizes]
            means = [statistics.mean(values) for values in samples]
            deviations = [statistics.stdev(values) if len(values) > 1 else 0 for values in samples]
            axis.errorbar(
                mesh_sizes,
                means,
                yerr=deviations,
                color=colors[implementation],
                marker=markers[implementation],
                linewidth=1.8,
                capsize=4,
                label=implementation,
            )
        axis.set_title(titles[section], loc="left", fontweight="bold")
        axis.set_xscale("log", base=2)
        axis.set_yscale("log")
        axis.set_xticks(mesh_sizes, labels=[str(size) for size in mesh_sizes])
        axis.set_xlabel("Elements per direction (nx=ny)")
        axis.set_ylabel("PerfGraph average time (s/call)")
        axis.legend()

    fig.suptitle(args.title, x=0.06, ha="left", fontsize=15, fontweight="bold")
    fig.text(
        0.06,
        0.925,
        "Points show mean time per call; error bars show +/-1 standard deviation",
        color="#52606D",
        fontsize=9.5,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.9))
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
