#!/usr/bin/env bash
# Lightweight, reusable Nsight wrapper for the SCZM GPU qualification.
# Captures a profiling run (NOT a correctness run -- instrumentation can perturb
# execution, so correctness is proven separately by the TestHarness).
#
# Usage: profile.sh <label> <module_binary> <input.i> [extra moose args...]
#
# Writes <label>.nsys-rep and <label>.cuda-kernels.txt next to this script.
set -euo pipefail

REPO=/home/chenghau.yang/packages/moose-kokkos
OUT="$REPO/cuda-qualification"
source "$REPO/kokkos-cuda-stack/scripts/activate.sh" >/dev/null 2>&1

label="$1"; exe="$2"; input="$3"; shift 3

nsys profile --force-overwrite=true --trace=cuda,nvtx,mpi \
  -o "$OUT/$label" \
  "$exe" -i "$input" --compute-device=cuda Outputs/exodus=false "$@" \
  > "$OUT/$label.run.log" 2>&1

nsys stats --report cuda_gpu_kern_sum "$OUT/$label.nsys-rep" \
  > "$OUT/$label.cuda-kernels.txt"

echo "wrote $OUT/$label.nsys-rep and $OUT/$label.cuda-kernels.txt"
