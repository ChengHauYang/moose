#!/bin/bash
# Fast variant of run_and_plot.sh: run the six-step benchmark once per step on a smaller mesh, with no
# Nsight profile, and turn the raw output into results/<name>.png and results/<name>.csv.
#
#   ./run_and_plot_fast.sh                             # N=8, tuned AMG, GPU 0, 1 run per step
#   MESH_N=4 NUM_STEPS=1 NAME=smoke ./run_and_plot_fast.sh   # quick end-to-end check
#
# Raw output goes to raw/<name>/ (git-excluded), including run.log and gpu<id>_samples.csv. The GPU
# memory samples are not used by analyze_results.py; match them to runs with the run.log timestamps
# to get each step's peak GPU memory.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$SCRIPT_DIR"

# N = 8 (16 x 8 x 8 mesh) is the step-input default; one timed run and no profile per step.
export MESH_N=${MESH_N:-8}
export AMG=${AMG:-tuned}
export REPEATS=${REPEATS:-1}
export PROFILE=${PROFILE:-0}
export NUM_STEPS=${NUM_STEPS:-5}
export DT=${DT:-5e-3}
GPU=${GPU:-0}
export CUDA_VISIBLE_DEVICES=$GPU

# Every step input enables total-mode F-bar; names follow ../plasticity_LDinSCZM/results/README.md.
# The _fast suffix keeps single-run data apart from the averaged run_and_plot.sh datasets.
NAME=${NAME:-fbar_${AMG}_amg_n${MESH_N}_fast}
TITLE=${TITLE:-"F-bar ON, $AMG AMG, N=$MESH_N, 1 run per step"}
RAW_DIR=$SCRIPT_DIR/raw/$NAME

if [ -e "$RAW_DIR" ] && [ -n "$(ls -A "$RAW_DIR")" ]; then
  echo "ERROR: $RAW_DIR already holds data; pick another NAME or move it aside." >&2
  exit 1
fi
mkdir -p "$RAW_DIR" "$SCRIPT_DIR/results"

# Sample GPU memory every 0.5 s for the whole run; stop the sampler however the script exits.
nvidia-smi -i "$GPU" --query-gpu=timestamp,memory.used,utilization.gpu --format=csv -lms 500 \
  > "$RAW_DIR/gpu${GPU}_samples.csv" &
SMI_PID=$!
trap 'kill $SMI_PID 2>/dev/null || true' EXIT

echo "$(date '+%F %T') Benchmark $NAME -> $RAW_DIR"
RESULTS_DIR=$RAW_DIR ./run_benchmarks.sh 2>&1 | tee "$RAW_DIR/run.log"

kill $SMI_PID 2>/dev/null || true

echo "$(date '+%F %T') Analyzing (exporting Nsight reports can take several minutes)"
python3 -B analyze_results.py "$RAW_DIR" \
  --output "results/$NAME.png" \
  --title "$TITLE" 2>&1 | tee "$RAW_DIR/analyze.log"

echo "$(date '+%F %T') Done: results/$NAME.png, results/$NAME.csv"
