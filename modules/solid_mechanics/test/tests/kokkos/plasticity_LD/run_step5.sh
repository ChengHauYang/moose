#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MOOSE_DIR=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)
EXODUS=false

if [ "${1:-}" = "--exodus" ]; then
  EXODUS=true
  shift
fi

if [ "$#" -ne 0 ]; then
  echo "Usage: $0 [--exodus]" >&2
  exit 1
fi

# shellcheck disable=SC1091
source "$MOOSE_DIR/kokkos-cuda-stack/scripts/activate.sh"

NP=${NP:-1}

# Single-process runs pin to the least-loaded GPU here. Multi-rank MPI runs leave
# the device list alone and let gpu_rank_bind.sh give each rank its own GPU (see
# the launcher below); pinning a single id here would funnel every rank onto it.
if [ "$NP" -eq 1 ] && [ -z "${CUDA_VISIBLE_DEVICES:-}" ]; then
  CUDA_VISIBLE_DEVICES=$(python3 "$MOOSE_DIR/modules/solid_mechanics/test/tests/kokkos/plasticity/select_cuda_device.py" \
    --reason "plasticity_LD Step 5")
  export CUDA_VISIBLE_DEVICES
fi

EXE=${EXE:-$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-opt}
RESULTS_DIR=${RESULTS_DIR:-$SCRIPT_DIR/results_step5}
MESH_N=${MESH_N:-32}
NUM_STEPS=${NUM_STEPS:-5}
DT=${DT:-1e-3}

if [ ! -x "$EXE" ]; then
  echo "ERROR: solid_mechanics executable not found at $EXE" >&2
  exit 1
fi

mkdir -p "$RESULTS_DIR"

launcher=()
if [ "$NP" -gt 1 ]; then
  # gpu_rank_bind.sh pins each rank to one GPU so Kokkos/PETSc/HYPRE/libtorch
  # agree on the device (see that script for the cudaErrorIllegalAddress it avoids).
  launcher=(mpiexec -n "$NP" "$SCRIPT_DIR/gpu_rank_bind.sh")
fi

prefix="$RESULTS_DIR/step5_plasticity_full_gpu"
set +e
/usr/bin/time -f '%e' -o "$prefix.time" \
  "${launcher[@]}" "$EXE" -i "$SCRIPT_DIR/step5_plasticity_full_gpu.i" \
    --compute-device=cuda \
    "N=$MESH_N" \
    "Executioner/dt=$DT" \
    "Executioner/num_steps=$NUM_STEPS" \
    "Outputs/file_base=$prefix" \
    "Outputs/perf_graph_json_file=$prefix.perf.json" \
    "Outputs/exodus=$EXODUS" \
    -vec_type kokkos \
    -nl0_mat_type aijkokkos \
    -use_gpu_aware_mpi 0 \
    -snes_converged_reason \
    -ksp_converged_reason \
    -ksp_view \
    --timing \
    > "$prefix.log" 2>&1
status=$?
set -e

if [ "$status" -ne 0 ]; then
  echo "ERROR: Step 5 did not finish successfully (exit code $status)." >&2
  grep -Ei 'out of memory|OOM|DIVERGED|ERROR|exception|cudaMalloc.*failed' "$prefix.log" >&2 || true
  echo "Full log: $prefix.log" >&2
  exit "$status"
fi

echo "Step 5 completed successfully. Results are in $RESULTS_DIR"
