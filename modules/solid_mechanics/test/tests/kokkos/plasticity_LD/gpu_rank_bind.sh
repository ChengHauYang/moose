#!/bin/bash
# Pin this MPI rank to exactly one GPU, then exec the MOOSE executable.
#
# A multi-GPU run must give every CUDA consumer in a process the SAME device.
# MOOSE binds Kokkos to (node-local rank % num_devices) in KokkosMooseInit, but
# PETSc's non-Kokkos CUDA path (HYPRE BoomerAMG, cuSPARSE) binds independently
# and lands on device 0 on every rank. With both GPUs visible, rank 1's Kokkos
# matrix lives on GPU 1 while HYPRE reads it through GPU-0 pointers, which aborts
# the BoomerAMG setup with cudaErrorIllegalAddress. Exposing a single GPU per
# rank (as cuda:0) removes the mismatch: Kokkos, PETSc-Kokkos, HYPRE, and
# libtorch/NEML2 all share that one device.
set -euo pipefail

# Node-local rank across the shared-memory node. OpenMPI sets this.
local_rank=${OMPI_COMM_WORLD_LOCAL_RANK:-0}

# Candidate devices: honor a preset CUDA_VISIBLE_DEVICES so the caller can choose
# which physical GPUs to use; otherwise use every GPU nvidia-smi reports.
if [ -n "${CUDA_VISIBLE_DEVICES:-}" ]; then
  IFS=',' read -ra devices <<< "$CUDA_VISIBLE_DEVICES"
else
  mapfile -t devices < <(nvidia-smi --query-gpu=index --format=csv,noheader)
fi

ndev=${#devices[@]}
[ "$ndev" -gt 0 ] || { echo "gpu_rank_bind: no GPUs found" >&2; exit 1; }

export CUDA_VISIBLE_DEVICES="${devices[$(( local_rank % ndev ))]}"
exec "$@"
