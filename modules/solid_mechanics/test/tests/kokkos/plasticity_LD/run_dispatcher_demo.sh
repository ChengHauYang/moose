#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MOOSE_DIR=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)
MODEL="$MOOSE_DIR/modules/solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i"
EXE=${EXE:-$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-opt}
AOTI_CXX=${AOTI_CXX:-clang++}

# The output dir must match [NEML2]/input in dispatcher_demo.i, and --device must
# match [NEML2]/all/device there.
CXX="$AOTI_CXX" neml2-compile "$MODEL" --model model --device cpu \
  --output-dir "$SCRIPT_DIR/aoti" -d neml2_stress:deformation_gradient

cd "$SCRIPT_DIR" &&
  "$EXE" -i dispatcher_demo.i
