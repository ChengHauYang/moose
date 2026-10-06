#!/bin/bash -l

#PBS -l filesystems=flare
#PBS -A moose_gpu
#PBS -l walltime=00:20:00
#PBS -q debug
#PBS -N compile_aoti
#PBS -l place=scatter
#PBS -k doe
#PBS -l select=1

set -e

STACK=/lus/flare/projects/moose_gpu/packages/new-neml2
GCC_ROOT=/opt/aurora/26.26.0/spack/unified/1.1.1/install/linux-x86_64/gcc-14.3.0-siitp7a

# Run from the directory where qsub was issued
cd "${PBS_O_WORKDIR}" || exit 1

# Aurora / NEML2 environment
source "${STACK}/activate"

export ONEAPI_DEVICE_SELECTOR=level_zero:gpu

export PATH="${STACK}/moose-env/bin:${GCC_ROOT}/bin:${PATH}"
export CC="${GCC_ROOT}/bin/gcc"
export CXX="${GCC_ROOT}/bin/g++"

PYTHON="${STACK}/moose-env/bin/python"
NEML2_COMPILE="${STACK}/moose-env/bin/neml2-compile"

INPUT=exact_kinematics_neml2_aoti_dense.i
OUTPUT=aoti_dense

echo "Host        = $(hostname)"
echo "CC          = ${CC}"
echo "CXX         = ${CXX}"
echo "Input       = ${INPUT}"
echo "AOTI output = ${OUTPUT}"

# Check XPU availability
XPU_COUNT="$("${PYTHON}" -c 'import torch; print(torch.xpu.device_count())')"

echo "XPU devices = ${XPU_COUNT}"

if [[ "${XPU_COUNT}" -eq 0 ]]; then
  echo "ERROR: No XPU device available."
  exit 1
fi

# Remove old artifact only after XPU is confirmed
rm -rf "${OUTPUT}"

# Compile XPU AOTI artifact
"${NEML2_COMPILE}" \
  "${INPUT}" \
  --model model \
  --device xpu \
  --dtype float64 \
  --output-dir "${OUTPUT}" \
  -d neml2_stress:deformation_gradient

# Verify output
test -f "${OUTPUT}/model_aoti.i"
test -d "${OUTPUT}/model/xpu/float64"

echo
echo "AOTI XPU compile succeeded:"
echo "  ${OUTPUT}/model_aoti.i"
echo "  ${OUTPUT}/model/xpu/float64"
