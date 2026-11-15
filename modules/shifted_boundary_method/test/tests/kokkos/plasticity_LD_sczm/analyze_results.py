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
# Speedup references: step1 (all host) and step4, which differs from steps 5/6 only in the PETSc backend.
STEP1 = STEPS[0][0]
STEP4 = STEPS[3][0]

# Regex for a standalone floating-point number on its own line (GNU time output).
FLOAT_LINE = re.compile(
    r"^\s*[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?\s*$"
)

# Regex to extract the MatConvert call count from a -log_view log.  PETSc prints
# something like "MatConvert   5 1.0  n/a  ..." or just the count after the name.
MAT_CONVERT_RE = re.compile(r"^\s*MatConvert\s+(\d+)\s+", re.MULTILINE)

# MOOSE prints the system size in its header as "Num DOFs:   968".
NUM_DOFS_RE = re.compile(r"Num DOFs:\s+(\d+)")
NUM_ELEMS_RE = re.compile(r"^\s*Elems:\s+(\d+)", re.MULTILINE)
# Mesh-size command-line override passed by run_benchmarks.sh ("N=16").
MESH_N_RE = re.compile(r"^\s*N=(\d+)\s*$", re.MULTILINE)

# MOOSE colors residual norms with ANSI escapes; strip them before parsing "N Linear |R| = x".
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
LINEAR_RESIDUAL_RE = re.compile(r"^\s*(\d+) Linear \|R\| = ([0-9.eE+-]+)", re.MULTILINE)

# Effective BoomerAMG settings and matrix type from the first -ksp_view block of a log. PCHYPRE picks
# different defaults for host and device matrices, so these are read back rather than assumed.
KSP_VIEW_FIELDS = {
    "mat_type": re.compile(r"Mat Object: \(nl0_\).*\n\s+type: (\S+)"),
    "amg_coarsen": re.compile(r"Coarsen type\s+(\S+)"),
    "amg_relax": re.compile(r"Relax down\s+(\S+)"),
    "amg_cf_relax": re.compile(r"(Using|Not using) CF-relaxation"),
    "amg_interp": re.compile(r"Interpolation type\s+(\S+)"),
    "amg_strong_threshold": re.compile(r"Threshold for strong coupling\s+(\S+)"),
    "amg_agg_levels": re.compile(r"Number of levels of aggressive coarsening\s+(\d+)"),
    "amg_spgemm": re.compile(r"SpGEMM type\s+(\S+)"),
}


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

    # The word boundary keeps "Linear" from also matching "Nonlinear solve converged".
    pattern = re.compile(
        rf"\b{kind} solve converged.*?iterations?\s+(\d+)",
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


def petsc_event_time(path: Path, event: str) -> float:
    """Max time (s) of one -log_view event from a single-rank log.

    PETSc prints "n/a" instead of a time for events nested inside GPU-timed events, so
    those are returned as NaN (not available) rather than zero.
    """
    if not path.exists():
        return math.nan

    match = re.search(rf"^{event}\s+\d+\s+\S+\s+(\S+)", path.read_text(errors="replace"), re.MULTILINE)
    if match is None:
        return math.nan
    try:
        return float(match.group(1))
    except ValueError:
        return math.nan


def header_count(path: Path, pattern: re.Pattern) -> float:
    """One integer (e.g. DOFs or elements) from the MOOSE header of a log."""
    if not path.exists():
        return math.nan
    match = pattern.search(path.read_text(errors="replace"))
    return float(match.group(1)) if match else math.nan


def run_signature(path: Path) -> tuple:
    """(N, tuned AMG, elements, DOFs) of one run, used to refuse mixed directories."""
    text = path.read_text(errors="replace")
    mesh_n = MESH_N_RE.search(text)
    elems = NUM_ELEMS_RE.search(text)
    dofs = NUM_DOFS_RE.search(text)
    return (
        mesh_n.group(1) if mesh_n else None,
        # run_benchmarks.sh AMG=tuned is the only source of this option.
        "-pc_hypre_boomeramg_strong_threshold" in text,
        elems.group(1) if elems else None,
        dofs.group(1) if dofs else None,
    )


def check_consistent_runs(results_dir: Path) -> None:
    """Stop if the directory holds runs of different mesh sizes or AMG configurations."""
    signatures: dict[tuple, list[str]] = {}
    for log in sorted(results_dir.glob("step*_rep*.log")) + sorted(results_dir.glob("step*_profile.log")):
        signatures.setdefault(run_signature(log), []).append(log.name)
    if len(signatures) > 1:
        lines = [
            f"  (N, tuned_amg, elems, dofs) = {sig}: {', '.join(logs)}" for sig, logs in signatures.items()
        ]
        raise SystemExit(f"ERROR: {results_dir} mixes run configurations:\n" + "\n".join(lines))


def ksp_view_settings(path: Path) -> dict[str, str]:
    """Effective matrix type and BoomerAMG settings from the first -ksp_view block, N/A if absent."""
    text = path.read_text(errors="replace") if path.exists() else ""
    settings = {}
    for key, pattern in KSP_VIEW_FIELDS.items():
        match = pattern.search(text)
        settings[key] = match.group(1) if match else "N/A"
    return settings


def ksp_solve_stats(path: Path) -> tuple[float, float, float]:
    """(median, max) iterations per linear solve and the median over solves of the mean residual
    contraction per iteration, (|r_end| / |r_0|)^(1 / its), from the linear residual history."""
    if not path.exists():
        return math.nan, math.nan, math.nan
    solves: list[list[float]] = []
    for its, norm in LINEAR_RESIDUAL_RE.findall(ANSI_RE.sub("", path.read_text(errors="replace"))):
        # Each linear solve restarts the count at 0.
        if int(its) == 0:
            solves.append([])
        if solves:
            solves[-1].append(float(norm))
    iterations = pd.Series([len(s) - 1 for s in solves], dtype=float)
    rates = pd.Series(
        [(s[-1] / s[0]) ** (1.0 / (len(s) - 1)) for s in solves if len(s) > 1 and s[0] > 0], dtype=float
    )
    return iterations.median(), iterations.max(), rates.median()


def time_history(path: Path) -> tuple[float, float]:
    """(final simulation time, number of completed time steps) from a postprocessor CSV."""
    if not path.exists():
        return math.nan, math.nan
    times = pd.read_csv(path)["time"]
    # The first row is the initial condition at t = 0.
    return float(times.iloc[-1]), float(len(times) - 1)


def max_rel_diff(path: Path, ref_path: Path) -> float:
    """Max relative difference of all postprocessor columns over all times against a reference CSV."""
    if not path.exists() or not ref_path.exists():
        return math.nan
    frame = pd.read_csv(path)
    ref = pd.read_csv(ref_path)
    # Same time history required; 1e-12 only absorbs CSV print rounding of the dt multiples.
    if len(frame) != len(ref) or not (frame["time"] - ref["time"]).abs().max() <= 1e-12:
        return math.nan
    worst = 0.0
    for col in ref.columns.drop("time"):
        scale = ref[col].abs()
        mask = scale > 0
        if mask.any():
            worst = max(worst, ((frame[col] - ref[col]).abs()[mask] / scale[mask]).max())
    return worst


def nsys_csv(report_path: Path, report_name: str) -> list[dict[str, str]] | None:
    """Read one Nsight Systems CSV report from an .nsys-rep file.

    Returns the report rows, an empty list when the profile holds no such activity (a measured
    zero), or None when there is no measurement (no profile, no nsys, or the report failed).
    """
    if not report_path.exists() or shutil.which("nsys") is None:
        return None

    result = subprocess.run(
        ["nsys", "stats", "--report", report_name, "--format", "csv",
         str(report_path)],
        check=False,
        capture_output=True,
        text=True,
    )

    # nsys stats exits 0 even for failures such as an unknown report, so the output decides.
    output = result.stdout + result.stderr
    lines = result.stdout.splitlines()
    header = next(
        (idx for idx, line in enumerate(lines) if "Total Time" in line),
        None,
    )
    if header is None:
        # Nsight Systems 2023.4 prints "SKIPPED: <file> does not contain <kind> data." for a profile
        # without that activity.
        if re.search(r"SKIPPED: .* does not contain", output):
            return []
        return None

    return list(csv.DictReader(io.StringIO("\n".join(lines[header:]))))


def seconds(value: str, header: str) -> float:
    """Convert an Nsight Systems time column to seconds."""
    units = {"ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1.0}
    match = re.search(r"\((ns|us|ms|s)\)", header)
    scale = units[match.group(1)] if match else 1e-9
    return float(value.replace(",", "")) * scale


def report_time(rows: list[dict[str, str]] | None, name_pattern: str | None = None) -> float:
    """Sum one Nsight Systems report time column, optionally filtered by name.

    NaN when there is no measurement (rows is None); zero when the report has no matching row.
    """
    if rows is None:
        return math.nan
    total = 0.0
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
        except (TypeError, ValueError):
            # An unreadable time makes the sum unknown, not smaller.
            return math.nan
    return total


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
        snes_solve: list[float] = []
        ksp_solve: list[float] = []
        pc_setup: list[float] = []
        pc_apply: list[float] = []
        failed_timesteps: list[float] = []

        total_reps = failed_reps = used_reps = 0

        for time_path in sorted(results_dir.glob(f"{step}_rep*.time")):
            total_reps += 1
            stem = time_path.with_suffix("")
            wall_time, failed = read_wall_time(time_path)

            log_path = Path(f"{stem}.log")
            if log_path.exists() and "*** ERROR ***" in log_path.read_text(errors="replace"):
                failed = True

            # Failed repetitions are counted but kept out of every statistic.
            if math.isnan(wall_time) or failed:
                failed_reps += 1
                continue

            used_reps += 1
            wall_times.append(wall_time)

            neml2_times.append(
                perf_section_time(Path(f"{stem}.perf.json"), "NEML2::solve")
            )

            newton_iters.append(iteration_total(log_path, "Nonlinear"))
            ksp_iters.append(iteration_total(log_path, "Linear"))
            mat_convert.append(mat_convert_calls(log_path))
            snes_solve.append(petsc_event_time(log_path, "SNESSolve"))
            ksp_solve.append(petsc_event_time(log_path, "KSPSolve"))
            pc_setup.append(petsc_event_time(log_path, "PCSetUp"))
            pc_apply.append(petsc_event_time(log_path, "PCApply"))
            # Each failed time step (cut back and retried) prints this once.
            failed_timesteps.append(float(log_path.read_text(errors="replace").count("Solve Did NOT Converge")))

        rep1 = results_dir / f"{step}_rep1"
        final_time, num_timesteps = time_history(Path(f"{rep1}.csv"))
        its_median, its_max, contraction = ksp_solve_stats(Path(f"{rep1}.log"))
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
            "wall_time_std_s": pd.Series(wall_times, dtype=float).std(),
            "wall_times_s": ";".join(f"{t:g}" for t in wall_times),
            "neml2_solve_s": mean(neml2_times),
            "cuda_kernel_time_s": report_time(kernels),
            "mpi_time_s": report_time(mpi),
            # Nsight Systems 2023.4 names these "[CUDA memcpy Host-to-Device]" / "Device-to-Host".
            "h2d_time_s": report_time(copies, r"HtoD|Host[- ]to[- ]Device"),
            "d2h_time_s": report_time(copies, r"DtoH|Device[- ]to[- ]Host"),
            "newton_iterations": mean(newton_iters),
            "ksp_iterations": mean(ksp_iters),
            "mat_convert_calls": mean(mat_convert),
            "snes_solve_s": mean(snes_solve),
            "ksp_solve_s": mean(ksp_solve),
            "pc_setup_s": mean(pc_setup),
            "pc_apply_s": mean(pc_apply),
            # Excess of the first timed run over the mean of the later ones; the first run is kept in
            # every statistic, this only flags a possible warm-up (e.g. CUDA/JIT/file-cache) cost.
            "first_run_excess": (
                wall_times[0] / mean(wall_times[1:]) - 1.0 if len(wall_times) > 1 else math.nan
            ),
            "num_elems": header_count(Path(f"{rep1}.log"), NUM_ELEMS_RE),
            "num_dofs": header_count(Path(f"{rep1}.log"), NUM_DOFS_RE),
            "has_nsys_profile": profile.exists(),
            "max_rel_diff_vs_step1": max_rel_diff(
                Path(f"{rep1}.csv"), results_dir / f"{STEP1}_rep1.csv"
            ),
            "final_time": final_time,
            "num_timesteps": num_timesteps,
            "failed_timesteps": mean(failed_timesteps),
            "ksp_its_per_solve_median": its_median,
            "ksp_its_per_solve_max": its_max,
            "ksp_contraction_median": contraction,
            **ksp_view_settings(Path(f"{rep1}.log")),
        })

    frame = pd.DataFrame.from_records(records)
    for name, ref in (("step1", STEP1), ("step4", STEP4)):
        frame[f"speedup_vs_{name}"] = (
            frame.loc[frame["step"] == ref, "total_wall_time_s"].iloc[0] / frame["total_wall_time_s"]
        )
    return frame


def plot_results(frame: pd.DataFrame, output: Path, title: str = "") -> None:
    """Generate a 3x2 summary plot; title, when given, is added as a line to the figure title."""
    # Short display labels to avoid multi-line tick clutter.
    short = [
        "1. Host CPU", "2. GPU NEML2", "3. GPU kokkos",
        "4. GPU NEML2+kokkos", "5. AIJKOKKOS", "6. MATHYPRE",
    ]

    fig, axes = plt.subplots(3, 2, figsize=(14, 12), constrained_layout=True)
    fig.suptitle(
        "plasticity_LD_sczm benchmark"
        + (f" ({title})" if title else "")
        + "\n(hypre boomeramg + gmres, same PETSc options in all steps; effective CPU/GPU AMG"
        " defaults differ, see the CSV amg_* columns)",
        fontsize=11,
    )

    # --- row 0: wall time + NEML2 time ---
    ax = axes[0, 0]
    ax.bar(short, frame["total_wall_time_s"], yerr=frame["wall_time_std_s"].fillna(0.0), capsize=3)
    ax.set_title("Total wall time (mean ± std over timed runs)")
    ax.set_ylabel("Seconds")
    ax.set_yscale("log")

    ax = axes[0, 1]
    ax.bar(short, frame["neml2_solve_s"])
    ax.set_title("NEML2::solve (both bulk + interface models)")
    ax.set_ylabel("Seconds")

    # --- row 1: GPU activity (kernels + H2D + D2H) + iteration counts ---
    ax = axes[1, 0]
    # collect() leaves a quantity NaN only when it was not measured (no usable profile); a measured
    # zero stays 0. An unmeasured step gets no bar and an N/A label instead of a zero-height bar.
    gpu_cols = ["cuda_kernel_time_s", "h2d_time_s", "d2h_time_s"]
    k_time, h2d_time, d2h_time = (frame[col] for col in gpu_cols)
    ax.bar(short, k_time, label="CUDA kernels")
    ax.bar(short, h2d_time, bottom=k_time, label="H2D")
    ax.bar(short, d2h_time, bottom=k_time + h2d_time, label="D2H")
    for idx, missing in enumerate(frame[gpu_cols].isna().any(axis=1)):
        if missing:
            ax.text(idx, 0, "N/A", ha="center", va="bottom", fontsize=9, color="grey")
    # Autoscaling ignores all-NaN bars, which would drop unmeasured steps off the axis; keep every
    # step on the axis, aligned with the wall-time panel.
    ax.set_xlim(axes[0, 0].get_xlim())
    ax.set_title("GPU activity (one Nsight profile run; N/A = not measured)")
    ax.set_ylabel("Seconds")
    # Step 1 never has tall GPU bars, so the upper left stays clear of the data.
    ax.legend(loc="upper left")

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
        "steps 1-5 convert once per PC setup; step6 = 0 (COO assembly)"
    )
    ax.set_ylabel("Calls")

    ax = axes[2, 1]
    positions = range(len(frame))
    width = 0.4
    ax.bar([p - width / 2 for p in positions], frame["speedup_vs_step1"], width, label="vs step1")
    ax.bar([p + width / 2 for p in positions], frame["speedup_vs_step4"], width, label="vs step4")
    ax.set_xticks(list(positions), short)
    ax.axhline(1.0, color="grey", linestyle="--", linewidth=0.8)
    ax.set_title("Speedup (mean total wall time)")
    ax.set_ylabel("×")
    ax.legend()

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

    cut_back = frame[frame["failed_timesteps"] > 0]
    if not cut_back.empty:
        print("\nWarning: some steps cut back failed time steps (different work per run).")
        for _, row in cut_back.iterrows():
            print(f"  {row['step']}: {row['failed_timesteps']:g} failed time steps, final time {row['final_time']:g}")

    # A 5% coefficient of variation is well above the run-to-run noise of these sequential runs
    # (plasticity_LD_sczm_grain_boundaries n=16 step1: 0.4%), so larger spreads point at contention
    # or a slow first run.
    noisy = frame[frame["wall_time_std_s"] > 0.05 * frame["total_wall_time_s"]]
    if not noisy.empty:
        print("\nWarning: wall-time spread above 5% of the mean.")
        for _, row in noisy.iterrows():
            print(f"  {row['step']}: {row['wall_times_s']} s")

    # Same 5% bound as the spread check above.
    warm_up = frame[frame["first_run_excess"] > 0.05]
    if not warm_up.empty:
        print("\nNote: first timed run more than 5% slower than the later ones (kept in the mean).")
        for _, row in warm_up.iterrows():
            print(f"  {row['step']}: {row['wall_times_s']} s ({100 * row['first_run_excess']:+.1f}%)")

    history = frame.dropna(subset=["final_time"])
    if history["final_time"].nunique() > 1 or history["num_timesteps"].nunique() > 1:
        print("\nWarning: steps end at different times or step counts.")
        for _, row in history.iterrows():
            print(f"  {row['step']}: final time {row['final_time']:g}, {row['num_timesteps']:g} steps")


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
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Plot path (default: RESULTS_DIR/comparison.png); the CSV is written next to it "
        "with the same stem.",
    )
    parser.add_argument(
        "--title",
        default="",
        help="Extra figure-title text, e.g. the variant and mesh size.",
    )
    args = parser.parse_args()

    args.results_dir.mkdir(parents=True, exist_ok=True)

    check_consistent_runs(args.results_dir)
    frame = collect(args.results_dir)

    plot_path = args.output or args.results_dir / "comparison.png"
    csv_path = plot_path.with_suffix(".csv")

    frame.to_csv(csv_path, index=False, na_rep="N/A")
    plot_results(frame, plot_path, args.title)

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
