#!/bin/bash
# Staged performance comparison for the plasticity_LD_sczm benchmark (crystal-plasticity bulk + shifted
# cohesive-zone interface). Same shape as solid_mechanics/.../kokkos/plasticity_LD/run_benchmarks.sh:
# pick a GPU once, run each staged input REPEATS times with GNU time, and take one Nsight Systems
# profile per step.
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
# step5 vs step6 is the AIJKOKKOS-vs-MATHYPRE matrix-backend comparison under one BoomerAMG PC. The two
# cannot share a PETSc vector type on this HYPRE_MEMORY_DEVICE build: AIJKOKKOS needs -vec_type kokkos,
# MATHYPRE needs -vec_type cuda (see step6 input header for the exact PETSc errors otherwise).
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MOOSE_DIR=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)

# shellcheck disable=SC1091
source "$MOOSE_DIR/kokkos-cuda-stack/scripts/activate.sh"

# Reuse the solid_mechanics GPU picker (same helper the sibling LD benchmark uses).
if [ -z "${CUDA_VISIBLE_DEVICES:-}" ]; then
  CUDA_VISIBLE_DEVICES=$(python3 "$MOOSE_DIR/modules/solid_mechanics/test/tests/kokkos/plasticity/select_cuda_device.py" \
    --reason "plasticity_LD_sczm benchmark")
  export CUDA_VISIBLE_DEVICES
else
  echo "[cuda] honoring existing CUDA_VISIBLE_DEVICES=$CUDA_VISIBLE_DEVICES" >&2
fi

EXE=${EXE:-$MOOSE_DIR/modules/shifted_boundary_method/shifted_boundary_method-opt}
RESULTS_DIR=${RESULTS_DIR:-$SCRIPT_DIR/results}
MESH_N=${MESH_N:-16}
REPEATS=${REPEATS:-3}
PROFILE=${PROFILE:-1}
# Extra Executioner overrides let callers change the loading without forking the script.
NUM_STEPS=${NUM_STEPS:-5}
DT=${DT:-5e-3}

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
CPU_PETSC_ARGS=(-vec_type standard -nl0_mat_type aij "${COMMON_DIAG[@]}")
# step5: full GPU, AIJKOKKOS matrix + kokkos vectors.
AIJKOKKOS_PETSC_ARGS=(-vec_type kokkos -nl0_mat_type aijkokkos "${COMMON_DIAG[@]}")
# step6: full GPU, MATHYPRE matrix + cuda vectors (HYPRE_MEMORY_DEVICE requires cuda, not kokkos).
MATHYPRE_PETSC_ARGS=(-vec_type cuda -nl0_mat_type hypre "${COMMON_DIAG[@]}")

steps=(
  step1_ld_sczm_host_cpu
  step2_ld_sczm_host_cuda_neml2
  step3_ld_sczm_kokkos_cpu_neml2
  step4_ld_sczm_gpu_neml2_kokkos_cpu_petsc
  step5_ld_sczm_full_gpu
  step6_ld_sczm_full_gpu_mathypre
)

run_step()
{
  local step=$1
  local input="$SCRIPT_DIR/$step.i"
  local petsc_args=("${CPU_PETSC_ARGS[@]}")
  local device_args=()

  # NEML2 runs on the GPU from step2 on (step2 is host assembly + GPU NEML2); Kokkos needs
  # --compute-device=cuda from step3 on. step2 sets its NEML2 device in the input, so the harness flag
  # only has to select the device for the Kokkos steps (3-6).
  case "$step" in
    step3_* | step4_* | step5_* | step6_*) device_args=(--compute-device=cuda) ;;
  esac
  case "$step" in
    step5_*) petsc_args=("${AIJKOKKOS_PETSC_ARGS[@]}") ;;
    step6_*) petsc_args=("${MATHYPRE_PETSC_ARGS[@]}") ;;
  esac

  for rep in $(seq 1 "$REPEATS"); do
    local prefix="$RESULTS_DIR/${step}_rep${rep}"
    echo "Running $step repetition $rep/$REPEATS"
    /usr/bin/time -f '%e' -o "$prefix.time" \
      "$EXE" -i "$input" "${device_args[@]}" \
        "N=$MESH_N" \
        "DT=$DT" \
        "NUM_STEPS=$NUM_STEPS" \
        "Outputs/perf_graph_json_file=$prefix.perf.json" \
        "${petsc_args[@]}" \
        --timing \
        > "$prefix.log" 2>&1
  done

  if [ "$PROFILE" = 1 ]; then
    if ! command -v nsys >/dev/null 2>&1; then
      echo "WARNING: nsys not found; skipping the $step GPU/MPI profile." >&2
      return
    fi

    local profile_prefix="$RESULTS_DIR/${step}_profile"
    echo "Profiling $step once with Nsight Systems"
    nsys profile --force-overwrite=true --trace=cuda,nvtx,mpi \
      -o "$profile_prefix" \
      "$EXE" -i "$input" "${device_args[@]}" \
        "N=$MESH_N" \
        "DT=$DT" \
        "NUM_STEPS=$NUM_STEPS" \
        "Outputs/perf_graph_json_file=$profile_prefix.perf.json" \
        "${petsc_args[@]}" \
        --timing \
        > "$profile_prefix.log" 2>&1
  fi
}

for step in "${steps[@]}"; do
  run_step "$step"
done

echo "Done. Per-run wall times in $RESULTS_DIR/*.time, logs (with -ksp_view matrix/vector types and"
echo "KSP iteration counts) in $RESULTS_DIR/*.log, perf graphs in $RESULTS_DIR/*.perf.json."
