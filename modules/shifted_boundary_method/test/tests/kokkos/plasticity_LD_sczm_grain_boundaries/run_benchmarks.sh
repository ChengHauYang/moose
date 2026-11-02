#!/bin/bash
# Staged performance comparison for the plasticity_LD_sczm_grain_boundaries benchmark (2D plane
# strain polycrystal: crystal-plasticity grains + total-Lagrangian shifted cohesive-zone grain
# boundaries, F-bar off). Same shape as ../plasticity_LD_sczm/run_benchmarks.sh: pick a GPU once,
# run each staged input REPEATS times with GNU time, and take one Nsight Systems profile per step.
#
# All six steps use the SAME preconditioner (hypre boomeramg + gmres, line_search=none) so the series
# isolates the compute/solve backend, not the solver:
#   step1  host MOOSE   / CPU NEML2 / CPU PETSc (aij / standard vec)
#   step2  host MOOSE   / GPU NEML2 / CPU PETSc (aij / standard vec)
#   step3  Kokkos   / CPU NEML2 / CPU PETSc (aij / standard vec)
#   step4  Kokkos   / GPU NEML2 / CPU PETSc (aij / standard vec)
#   step5  Kokkos   / GPU NEML2 / GPU PETSc (AIJKOKKOS / kokkos vec)
#   step6  Kokkos   / GPU NEML2 / GPU PETSc (MATHYPRE  / cuda   vec)
#
# Differences from the sibling script: the mesh resolution is the inputs' ny (nx = 3 ny / 2), the
# loading defaults are the inputs' own DT = 0.25 and NUM_STEPS = 20, and every run writes its
# exodus/CSV output under RESULTS_DIR (Outputs/file_base) so the step*_out.* files next to the inputs
# are never overwritten.
#
# VARIANT=fbar runs the total-mode F-bar ON version of the same problem: each step input is merged
# with fbar_on_host.i (steps 1-2) or fbar_on_kokkos.i (steps 3-6). The default runs F-bar OFF.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MOOSE_DIR=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)

# shellcheck disable=SC1091
source "$MOOSE_DIR/kokkos-cuda-stack/scripts/activate.sh"

# Reuse the solid_mechanics GPU picker (same helper the sibling LD benchmark uses).
if [ -z "${CUDA_VISIBLE_DEVICES:-}" ]; then
  CUDA_VISIBLE_DEVICES=$(python3 "$MOOSE_DIR/modules/solid_mechanics/test/tests/kokkos/plasticity/select_cuda_device.py" \
    --reason "plasticity_LD_sczm_grain_boundaries benchmark")
  export CUDA_VISIBLE_DEVICES
else
  echo "[cuda] honoring existing CUDA_VISIBLE_DEVICES=$CUDA_VISIBLE_DEVICES" >&2
fi

EXE=${EXE:-$MOOSE_DIR/modules/shifted_boundary_method/shifted_boundary_method-opt}
# Raw run output (logs, .time, exodus/CSV, profiles) goes under raw/, kept out of git; results/ holds
# only the figures and reports. Use RESULTS_DIR=raw/<dataset name> to keep each study separate.
RESULTS_DIR=${RESULTS_DIR:-$SCRIPT_DIR/raw}
MESH_N=${MESH_N:-16}
REPEATS=${REPEATS:-3}
PROFILE=${PROFILE:-1}
# Extra Executioner overrides let callers change the loading without forking the script.
NUM_STEPS=${NUM_STEPS:-20}
DT=${DT:-0.25}
VARIANT=${VARIANT:-}
# Nsight Systems trace list; a reduced list (e.g. cuda,nvtx) can re-profile a step whose full
# trace fails to import.
NSYS_TRACE=${NSYS_TRACE:-cuda,nvtx,mpi}

case "$VARIANT" in
  "" | fbar) ;;
  *)
    echo "ERROR: unknown VARIANT='$VARIANT' (expected empty or 'fbar')" >&2
    exit 1
    ;;
esac

# AMG selects the BoomerAMG parameters added to every step's PETSc options. "simple" keeps the plain
# boomeramg of the step inputs (the earlier simple_amg baseline); "tuned" adds the reference parameters
# below to the CPU and GPU PETSc steps alike. Options left unset (coarsening, relaxation, CF ordering,
# aggressive-coarsening interpolation, SpGEMM) still take PCHYPRE's separate CPU and GPU defaults, so
# the two preconditioners are not identical; -ksp_view reports the effective values.
AMG=${AMG:-tuned}
case "$AMG" in
  simple) AMG_ARGS=() ;;
  tuned)
    AMG_ARGS=(
      # One V-cycle per preconditioner application with no internal AMG convergence test.
      -pc_hypre_boomeramg_max_iter 1
      -pc_hypre_boomeramg_tol 0.0
      # Extended+i interpolation, which hypre recommends with aggressive coarsening.
      -pc_hypre_boomeramg_interp_type ext+i
      # Aggressive coarsening on the finest level only.
      -pc_hypre_boomeramg_agg_nl 1
      # Strength-of-connection threshold of the reference configuration (hypre default 0.25).
      -pc_hypre_boomeramg_strong_threshold 0.6
    )
    ;;
  *)
    echo "ERROR: unknown AMG='$AMG' (expected 'simple' or 'tuned')" >&2
    exit 1
    ;;
esac
# EXTRA_PETSC_ARGS appends further whitespace-separated PETSc options to every step, after the AMG
# parameters, e.g. EXTRA_PETSC_ARGS="-pc_hypre_boomeramg_coarsen_type HMIS" for a preconditioner study.
read -r -a EXTRA_ARGS <<< "${EXTRA_PETSC_ARGS:-}"

if [ ! -x "$EXE" ]; then
  echo "ERROR: shifted_boundary_method executable not found at $EXE" >&2
  echo "Set EXE=/path/to/shifted_boundary_method-opt and rerun." >&2
  exit 1
fi

mkdir -p "$RESULTS_DIR"

# Common diagnostics on every run (matrix/vector type, KSP convergence, PETSc event log).
# -log_view is included so analyze_results.py can extract the MatConvert call count per run,
# which quantifies the per-PC-setup AIJ->HYPRE conversion overhead (step5 vs step6).
COMMON_DIAG=(
  -use_gpu_aware_mpi 0
  -snes_converged_reason
  -ksp_converged_reason
  -ksp_view
  -log_view
)
# steps 1-4: CPU PETSc, aij matrix + standard vectors.
CPU_PETSC_ARGS=(-vec_type standard -nl0_mat_type aij "${AMG_ARGS[@]}" "${EXTRA_ARGS[@]}" "${COMMON_DIAG[@]}")
# step5: full GPU, AIJKOKKOS matrix + kokkos vectors.
AIJKOKKOS_PETSC_ARGS=(-vec_type kokkos -nl0_mat_type aijkokkos "${AMG_ARGS[@]}" "${EXTRA_ARGS[@]}" "${COMMON_DIAG[@]}")
# step6: full GPU, MATHYPRE matrix + cuda vectors (HYPRE_MEMORY_DEVICE requires cuda, not kokkos).
MATHYPRE_PETSC_ARGS=(-vec_type cuda -nl0_mat_type hypre "${AMG_ARGS[@]}" "${EXTRA_ARGS[@]}" "${COMMON_DIAG[@]}")

# STEPS selects a subset (e.g. STEPS="step2 step4"), so a step that fails can be run on its own
# without set -e stopping the steps after it.
read -r -a steps <<< "${STEPS:-step1 step2 step3 step4 step5 step6}"

run_step()
{
  local step=$1
  local inputs=("$SCRIPT_DIR/$step.i")
  local petsc_args=("${CPU_PETSC_ARGS[@]}")
  local device_args=()

  # NEML2 runs on the GPU from step2 on (step2 is host assembly + GPU NEML2); Kokkos needs
  # --compute-device=cuda from step3 on. step2 sets its NEML2 device in the input, so the harness flag
  # only has to select the device for the Kokkos steps (3-6).
  case "$step" in
    step3 | step4 | step5 | step6) device_args=(--compute-device=cuda) ;;
  esac
  case "$step" in
    step5) petsc_args=("${AIJKOKKOS_PETSC_ARGS[@]}") ;;
    step6) petsc_args=("${MATHYPRE_PETSC_ARGS[@]}") ;;
  esac
  if [ "$VARIANT" = fbar ]; then
    case "$step" in
      step1 | step2) inputs+=("$SCRIPT_DIR/fbar_on_host.i") ;;
      *) inputs+=("$SCRIPT_DIR/fbar_on_kokkos.i") ;;
    esac
  fi

  for rep in $(seq 1 "$REPEATS"); do
    local prefix="$RESULTS_DIR/${step}_rep${rep}"
    # Timestamps let externally sampled GPU memory be attributed to each run.
    echo "$(date '+%F %T') Running $step repetition $rep/$REPEATS"
    /usr/bin/time -f '%e' -o "$prefix.time" \
      "$EXE" -i "${inputs[@]}" "${device_args[@]}" \
        "ny=$MESH_N" \
        "DT=$DT" \
        "NUM_STEPS=$NUM_STEPS" \
        "Outputs/file_base=$prefix" \
        "Outputs/perf_graph_json_file=$prefix.perf.json" \
        "${petsc_args[@]}" \
        --timing \
        > "$prefix.log" 2>&1
    echo "$(date '+%F %T') Finished $step repetition $rep/$REPEATS"
  done

  if [ "$PROFILE" = 1 ]; then
    if ! command -v nsys >/dev/null 2>&1; then
      echo "WARNING: nsys not found; skipping the $step GPU/MPI profile." >&2
      return
    fi

    local profile_prefix="$RESULTS_DIR/${step}_profile"
    echo "$(date '+%F %T') Profiling $step once with Nsight Systems (trace $NSYS_TRACE)"
    nsys profile --force-overwrite=true --trace="$NSYS_TRACE" \
      -o "$profile_prefix" \
      "$EXE" -i "${inputs[@]}" "${device_args[@]}" \
        "ny=$MESH_N" \
        "DT=$DT" \
        "NUM_STEPS=$NUM_STEPS" \
        "Outputs/file_base=$profile_prefix" \
        "Outputs/perf_graph_json_file=$profile_prefix.perf.json" \
        "${petsc_args[@]}" \
        --timing \
        > "$profile_prefix.log" 2>&1
    echo "$(date '+%F %T') Finished $step profile"
  fi
}

for step in "${steps[@]}"; do
  run_step "$step"
done

echo "Done. Per-run wall times in $RESULTS_DIR/*.time, logs (with -ksp_view matrix/vector types and"
echo "KSP iteration counts) in $RESULTS_DIR/*.log, perf graphs in $RESULTS_DIR/*.perf.json."
