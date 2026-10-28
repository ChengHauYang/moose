#!/usr/bin/env python3
"""Summarize and plot the six plasticity_LD_sczm benchmark steps.

All six steps share hypre boomeramg + gmres so the comparison isolates the
compute/solve backend, not the solver:

  step1  host MOOSE   / CPU NEML2 / CPU PETSc  (aij)
  step2  host MOOSE   / GPU NEML2 / CPU PETSc  (aij)
  step3  Kokkos   / CPU NEML2 / CPU PETSc  (aij)
  step4  Kokkos   / GPU NEML2 / CPU PETSc  (aij)
  step5  Kokkos   / GPU NEML2 / GPU PETSc  (AIJKOKKOS / kokkos vec)
  step6  Kokkos   / GPU NEML2 / GPU PETSc  (MATHYPRE  / cuda vec)

step5 vs step6 is the AIJKOKKOS-vs-MATHYPRE matrix-backend comparison. The
MatConvert call count (visible in -log_view output) quantifies the per-PC-setup
AIJ->HYPRE conversion: step5 pays it every PC setup; step6 eliminates it by
assembling directly in HYPRE's format via COO assembly.

Robust to GNU time files that contain messages such as:
    Command exited with non-zero status 1
    95.68
Failed repetitions are counted but excluded from timing averages.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import math
import re
import shutil
import subprocess
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


STEPS = [
    ("step1_ld_sczm_host_cpu",
     "1. Host CPU\n(CPU NEML2, CPU assembly, CPU PETSc)"),
    ("step2_ld_sczm_host_cuda_neml2",
     "2. GPU NEML2\n(host assembly, CPU PETSc)"),
    ("step3_ld_sczm_kokkos_cpu_neml2",
     "3. GPU kokkos\n(CPU NEML2, CPU PETSc)"),
    ("step4_ld_sczm_gpu_neml2_kokkos_cpu_petsc",
     "4. GPU NEML2+kokkos\n(CPU PETSc)"),
    ("step5_ld_sczm_full_gpu",
     "5. Full GPU\n(AIJKOKKOS)"),
    ("step6_ld_sczm_full_gpu_mathypre",
     "6. Full GPU\n(MATHYPRE)"),
]

# Regex for a standalone floating-point number on its own line (GNU time output).
FLOAT_LINE = re.compile(
    r"^\s*[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?\s*$"
)

# Regex to extract the MatConvert call count from a -log_view log.  PETSc prints
# something like "MatConvert   5 1.0  n/a  ..." or just the count after the name.
MAT_CONVERT_RE = re.compile(r"^\s*MatConvert\s+(\d+)\s+", re.MULTILINE)


def read_wall_time(path: Path) -> tuple[float, bool]:
    """Read wall time from a GNU time output file.

    Returns (wall_time_seconds, failed).  failed is True when GNU time reported
    that the command exited nonzero, or when the file is unreadable.
    """
    if not path.exists():
        return math.nan, True

    text = path.read_text(errors="replace")
    failed = "Command exited with non-zero status" in text

    values = [
        float(line.strip())
        for line in text.splitlines()
        if FLOAT_LINE.match(line)
    ]

    if not values:
        return math.nan, True

    return values[-1], failed


def perf_section_time(path: Path, wanted: str) -> float:
    """Sum self time for all nodes whose PerfGraph section name contains wanted."""
    if not path.exists():
        return math.nan

    try:
        data = json.loads(path.read_text())
    except (json.JSONDecodeError, OSError):
        return math.nan

    try:
        graph = data["time_steps"][-1]["perf_graph_json"]["graph"]
    except (KeyError, IndexError, TypeError):
        return math.nan

    def visit(value: object) -> float:
        if not isinstance(value, dict):
            return 0.0
        total = 0.0
        for name, node in value.items():
            if not isinstance(node, dict):
                continue
            if wanted in name:
                try:
                    total += float(node.get("time", 0.0))
                except (TypeError, ValueError):
                    pass
            total += visit(node.get("children", {}))
        return total

    return visit(graph)


def iteration_total(path: Path, kind: str) -> float:
    """Sum nonlinear or linear iteration counts from a MOOSE log."""
    if not path.exists():
        return math.nan

    pattern = re.compile(
        rf"{kind} solve converged.*?iterations?\s+(\d+)",
        re.IGNORECASE,
    )
    values = [int(m) for m in pattern.findall(path.read_text(errors="replace"))]
    return float(sum(values)) if values else math.nan


def mat_convert_calls(path: Path) -> float:
    """Extract the MatConvert call count from a -log_view log.

    Returns the total count across all matches (PETSc may repeat the event table
    for multiple MPI ranks; for single-rank runs there is exactly one match).
    Returns NaN if the log has no -log_view section (e.g. the run predates it).
    """
    if not path.exists():
        return math.nan

    text = path.read_text(errors="replace")
    matches = MAT_CONVERT_RE.findall(text)
    if not matches:
        return math.nan

    # Sum over any repeated tables; for single-rank runs this is one entry.
    return float(sum(int(m) for m in matches))


def nsys_csv(report_path: Path, report_name: str) -> list[dict[str, str]]:
    """Read one Nsight Systems CSV report from an .nsys-rep file."""
    if not report_path.exists() or shutil.which("nsys") is None:
        return []

    result = subprocess.run(
        ["nsys", "stats", "--report", report_name, "--format", "csv",
         str(report_path)],
        check=False,
        capture_output=True,
        text=True,
    )

    if result.returncode:
        return []

    lines = result.stdout.splitlines()
    header = next(
        (idx for idx, line in enumerate(lines) if "Total Time" in line),
        None,
    )
    if header is None:
        return []

    return list(csv.DictReader(io.StringIO("\n".join(lines[header:]))))


def seconds(value: str, header: str) -> float:
    """Convert an Nsight Systems time column to seconds."""
    units = {"ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1.0}
    match = re.search(r"\((ns|us|ms|s)\)", header)
    scale = units[match.group(1)] if match else 1e-9
    return float(value.replace(",", "")) * scale


def report_time(rows: list[dict[str, str]], name_pattern: str | None = None) -> float:
    """Sum one Nsight Systems report time column, optionally filtered by name."""
    total = 0.0
    found = False
    for row in rows:
        time_key = next((k for k in row if "Total Time" in k), None)
        if time_key is None:
            continue
        if name_pattern is not None:
            name = " ".join(str(v) for v in row.values())
            if re.search(name_pattern, name, re.IGNORECASE) is None:
                continue
        try:
            total += seconds(row[time_key], time_key)
            found = True
        except (TypeError, ValueError):
            pass
    return total if found else math.nan


def mean(values: list[float]) -> float:
    """Mean of all non-NaN values."""
    valid = [v for v in values if not math.isnan(v)]
    return sum(valid) / len(valid) if valid else math.nan


def collect(results_dir: Path) -> pd.DataFrame:
    """Collect benchmark data from one results directory."""
    records = []

    for step, label in STEPS:
        wall_times: list[float] = []
        neml2_times: list[float] = []
        newton_iters: list[float] = []
        ksp_iters: list[float] = []
        mat_convert: list[float] = []

        total_reps = failed_reps = used_reps = 0

        for time_path in sorted(results_dir.glob(f"{step}_rep*.time")):
            total_reps += 1
            stem = time_path.with_suffix("")
            wall_time, failed = read_wall_time(time_path)

            if math.isnan(wall_time):
                failed_reps += 1
                continue
            if failed:
                failed_reps += 1

            used_reps += 1
            wall_times.append(wall_time)

            neml2_times.append(
                perf_section_time(Path(f"{stem}.perf.json"), "NEML2::solve")
            )

            log_path = Path(f"{stem}.log")
            newton_iters.append(iteration_total(log_path, "Nonlinear"))
            ksp_iters.append(iteration_total(log_path, "Linear"))
            mat_convert.append(mat_convert_calls(log_path))

        profile = results_dir / f"{step}_profile.nsys-rep"
        kernels = nsys_csv(profile, "cuda_gpu_kern_sum")
        copies = nsys_csv(profile, "cuda_gpu_mem_time_sum")
        mpi = nsys_csv(profile, "mpi_event_sum")

        records.append({
            "step": step,
            "label": label,
            "total_reps": total_reps,
            "used_reps": used_reps,
            "failed_reps": failed_reps,
            "total_wall_time_s": mean(wall_times),
            "neml2_solve_s": mean(neml2_times),
            "cuda_kernel_time_s": report_time(kernels),
            "mpi_time_s": report_time(mpi),
            "h2d_time_s": report_time(copies, r"HtoD|Host to Device"),
            "d2h_time_s": report_time(copies, r"DtoH|Device to Host"),
            "newton_iterations": mean(newton_iters),
            "ksp_iterations": mean(ksp_iters),
            "mat_convert_calls": mean(mat_convert),
        })

    return pd.DataFrame.from_records(records)


def plot_results(frame: pd.DataFrame, output: Path) -> None:
    """Generate a 3x2 summary plot."""
    # Short display labels to avoid multi-line tick clutter.
    short = [
        "1. Host CPU", "2. GPU NEML2", "3. GPU kokkos",
        "4. GPU NEML2+kokkos", "5. AIJKOKKOS", "6. MATHYPRE",
    ]

    fig, axes = plt.subplots(3, 2, figsize=(14, 12), constrained_layout=True)
    fig.suptitle(
        "plasticity_LD_sczm benchmark\n(hypre boomeramg, same PC across all steps)",
        fontsize=11,
    )

    # --- row 0: wall time + NEML2 time ---
    ax = axes[0, 0]
    ax.bar(short, frame["total_wall_time_s"])
    ax.set_title("Total wall time")
    ax.set_ylabel("Seconds")
    ax.set_yscale("log")

    ax = axes[0, 1]
    ax.bar(short, frame["neml2_solve_s"])
    ax.set_title("NEML2::solve (both bulk + interface models)")
    ax.set_ylabel("Seconds")

    # --- row 1: GPU activity (kernels + H2D + D2H) + iteration counts ---
    ax = axes[1, 0]
    k_time = frame["cuda_kernel_time_s"].fillna(0.0)
    h2d_time = frame["h2d_time_s"].fillna(0.0)
    d2h_time = frame["d2h_time_s"].fillna(0.0)
    ax.bar(short, k_time, label="CUDA kernels")
    ax.bar(short, h2d_time, bottom=k_time, label="H2D")
    ax.bar(short, d2h_time, bottom=k_time + h2d_time, label="D2H")
    ax.set_title("GPU activity (one profile run)")
    ax.set_ylabel("Seconds")
    ax.legend()

    ax = axes[1, 1]
    ax.plot(short, frame["newton_iterations"], "o-", label="Newton")
    ax.plot(short, frame["ksp_iterations"], "s-", label="KSP (total)")
    ax.set_title("Iteration totals (sum over all time steps)")
    ax.set_ylabel("Iterations")
    ax.set_yscale("log")
    ax.legend()

    # --- row 2: MatConvert calls (step5 vs step6 isolation) + speedup ---
    ax = axes[2, 0]
    mc = frame["mat_convert_calls"].fillna(0.0)
    bars = ax.bar(short, mc)
    # Annotate step5 and step6 bars to make the elimination explicit.
    for idx, (bar, val) in enumerate(zip(bars, mc)):
        if not math.isnan(frame["mat_convert_calls"].iloc[idx]):
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                bar.get_height() + 0.1,
                str(int(val)),
                ha="center", va="bottom", fontsize=8,
            )
    ax.set_title(
        "MatConvert calls per run (AIJ→HYPRE, -log_view)\n"
        "step5 pays one per PC setup; step6 = 0 (COO assembly)"
    )
    ax.set_ylabel("Calls")

    ax = axes[2, 1]
    ref = frame["total_wall_time_s"].iloc[0]  # step1 = baseline
    speedup = ref / frame["total_wall_time_s"]
    ax.bar(short, speedup)
    ax.axhline(1.0, color="grey", linestyle="--", linewidth=0.8)
    ax.set_title("Speedup vs step1 (total wall time)")
    ax.set_ylabel("×")

    for axis in axes.flat:
        axis.tick_params(axis="x", rotation=18, labelsize=8)
        axis.grid(axis="y", alpha=0.25)

    fig.savefig(output, dpi=180)
    plt.close(fig)


def print_warnings(frame: pd.DataFrame) -> None:
    """Print warnings for failed or missing benchmark data."""
    failed = frame[frame["failed_reps"] > 0]
    if not failed.empty:
        print("\nWarning: some repetitions failed and were excluded from averages.")
        for _, row in failed.iterrows():
            print(
                f"  {row['step']}: "
                f"{int(row['failed_reps'])} failed, "
                f"{int(row['used_reps'])} used, "
                f"{int(row['total_reps'])} total"
            )

    missing = frame[frame["used_reps"] == 0]
    if not missing.empty:
        print("\nWarning: some steps have no successful repetitions.")
        for _, row in missing.iterrows():
            print(f"  {row['step']}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Collect and plot plasticity_LD_sczm benchmark results."
    )
    parser.add_argument(
        "results_dir",
        type=Path,
        nargs="?",
        default=Path("results"),
        help="Directory containing benchmark result files (default: results/).",
    )
    args = parser.parse_args()

    args.results_dir.mkdir(parents=True, exist_ok=True)

    frame = collect(args.results_dir)

    csv_path = args.results_dir / "comparison.csv"
    plot_path = args.results_dir / "comparison.png"

    frame.to_csv(csv_path, index=False)
    plot_results(frame, plot_path)

    print(frame.to_string(index=False))
    print_warnings(frame)

    print(f"\nWrote {csv_path}")
    print(f"Wrote {plot_path}")
    print(
        "Note: MPI time is not meaningful for these single-rank runs.\n"
        "Note: mat_convert_calls requires -log_view in the run (run_benchmarks.sh adds it)."
    )


if __name__ == "__main__":
    main()
