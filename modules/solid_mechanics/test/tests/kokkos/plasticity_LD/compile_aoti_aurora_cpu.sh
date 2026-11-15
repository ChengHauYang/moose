#!/usr/bin/env bash
set -e

STACK=/lus/flare/projects/moose_gpu/packages/new-neml2
GCC_ROOT=/opt/aurora/26.26.0/spack/unified/1.1.1/install/linux-x86_64/gcc-14.3.0-siitp7a

# Load Aurora/NEML2 environment.
# Keep nounset disabled here because Aurora Lmod may reference unset variables.
source "${STACK}/activate"

export PATH="${STACK}/moose-env/bin:${GCC_ROOT}/bin:${PATH}"
export CC="${GCC_ROOT}/bin/gcc"
export CXX="${GCC_ROOT}/bin/g++"

NEML2_COMPILE="${STACK}/moose-env/bin/neml2-compile"

INPUT=exact_kinematics_neml2_aoti_dense.i
OUTPUT=aoti_dense

echo "CC  = ${CC}"
echo "CXX = ${CXX}"
echo "AOTI output = ${OUTPUT}"

rm -rf "${OUTPUT}"

"${NEML2_COMPILE}" \
  "${INPUT}" \
  --model model \
  --device cpu \
  --dtype float64 \
  --output-dir "${OUTPUT}" \
  -d neml2_stress:deformation_gradient

test -f "${OUTPUT}/model_aoti.i"

echo
echo "AOTI compile succeeded:"
echo "  ${OUTPUT}/model_aoti.i"
