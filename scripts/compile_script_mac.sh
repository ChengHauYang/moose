#!/bin/bash

# ============================================================
# Mac build script:
#
#   MOOSE + Kokkos + NEML2 v3 + PyTorch
#
# Supported:
#   Apple Silicon M3 / M4 (arm64)
#
# Important compiler policy:
#
#   MOOSE:
#       keep compiler settings provided by Conda/MOOSE
#       CC  = mpicc
#       CXX = mpicxx
#
#   NEML2:
#       ordinary C   -> /usr/bin/clang
#       ordinary C++ -> /usr/bin/clang++
#       MPI C        -> Conda mpicc
#       MPI C++      -> Conda mpicxx
#
# ============================================================

set -eo pipefail


# ============================================================
# 0. Load Conda and activate MOOSE
#
# Do NOT enable `set -u` before Conda activation.
# Some Conda compiler hooks reference unset backup variables.
# ============================================================

source "$HOME/miniforge/etc/profile.d/conda.sh"

conda activate moose

# Safe to enable undefined-variable checking after activation.
set -u


# ============================================================
# 1. Environment information
# ============================================================

echo
echo "============================================"
echo " Environment"
echo "============================================"

ARCH="$(uname -m)"

echo "ARCH=$ARCH"
echo "CONDA_PREFIX=$CONDA_PREFIX"
echo "SDKROOT=${SDKROOT:-}"
echo
echo "CC=${CC:-}"
echo "CXX=${CXX:-}"
echo

echo "python:"
which python

echo
echo "mpicc:"
which mpicc

echo
echo "mpicxx:"
which mpicxx


# ============================================================
# 2. Verify Apple Silicon
# ============================================================

if [[ "$ARCH" != "arm64" ]]; then
    echo
    echo "ERROR: This script expects Apple Silicon (arm64)."
    echo "Detected architecture: $ARCH"
    exit 1
fi


# ============================================================
# 3. Verify MOOSE macOS SDK
# ============================================================

if [[ -z "${SDKROOT:-}" || ! -d "${SDKROOT:-}" ]]; then
    echo
    echo "ERROR: MOOSE macOS SDK is not configured."
    echo
    echo "Run:"
    echo
    echo '  "$CONDA_PREFIX/share/moose-compilers/scripts/get_mac_sdk.sh"'
    echo
    exit 1
fi

echo
echo "macOS SDK:"
echo "$SDKROOT"


# ============================================================
# 4. Check PyTorch
# ============================================================

if ! python -c \
    'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("torch") else 1)'
then
    echo
    echo "PyTorch not found in the MOOSE environment."
    echo "Installing PyTorch 2.13.0..."
    echo

    python -m pip install torch==2.13.0
fi


# ============================================================
# 5. OpenMP runtime de-duplication
#
# Conda may provide:
#
#   $CONDA_PREFIX/lib/libomp.dylib
#
# while the pip PyTorch wheel may provide:
#
#   torch/lib/libomp.dylib
#
# Loading both can produce:
#
#   OMP Error #15
#   multiple copies of the OpenMP runtime
#
# Use importlib.find_spec here so that torch itself does not
# need to be imported before the duplicate runtime is fixed.
# ============================================================

TORCH_DIR="$(
python - <<'PY'
import importlib.util

spec = importlib.util.find_spec("torch")

if spec and spec.submodule_search_locations:
    print(next(iter(spec.submodule_search_locations)))
PY
)"

TORCH_LIB="$TORCH_DIR/lib"

if [[ -f "$TORCH_LIB/libomp.dylib" && ! -L "$TORCH_LIB/libomp.dylib" ]]; then

    if [[ -f "$CONDA_PREFIX/lib/libomp.dylib" ]]; then

        echo
        echo "De-duplicating PyTorch libomp.dylib..."
        echo "  torch -> $CONDA_PREFIX/lib/libomp.dylib"

        rm -f "$TORCH_LIB/libomp.dylib"

        ln -s \
            "$CONDA_PREFIX/lib/libomp.dylib" \
            "$TORCH_LIB/libomp.dylib"
    fi
fi


# ============================================================
# 6. Verify PyTorch
# ============================================================

echo
echo "============================================"
echo " PyTorch"
echo "============================================"

python - <<'PY'
import torch

print("Torch version :", torch.__version__)
print("Torch path    :", torch.__file__)
PY


# ============================================================
# 7. Verify MPI compiler wrappers
#
# OpenMPI:
#   mpicc --showme:command
#
# MPICH:
#   mpicc -show
# ============================================================

echo
echo "============================================"
echo " MPI compiler wrappers"
echo "============================================"

mpi_show_cmd()
{
    if "$1" --showme:command >/dev/null 2>&1; then

        # OpenMPI
        "$1" --showme:command

    else

        # MPICH
        "$1" -show

    fi
}

echo
echo "mpicc:"
mpi_show_cmd mpicc

echo
echo "mpicxx:"
mpi_show_cmd mpicxx


# ============================================================
# 8. Locate MOOSE repository automatically
#
# Do NOT hard-code:
#
#   username
#   Package / Packages
#   moose / moose-sczm / moose-kokkos / ...
#
# Normally this script is inside:
#
#   <MOOSE repo>/scripts/compile_script_mac.sh
#
# so git can determine the repository root.
# ============================================================

SCRIPT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)"

MOOSE_DIR="$(
    git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null || true
)"


# Fallback:
# If the script was copied outside the repository, try the
# directory from which the user launched it.
if [[ -z "$MOOSE_DIR" ]]; then

    MOOSE_DIR="$(
        git -C "$PWD" rev-parse --show-toplevel 2>/dev/null || true
    )"

fi


# Verify that what we found actually looks like MOOSE.
if [[ -z "$MOOSE_DIR" || ! -f "$MOOSE_DIR/configure" || ! -d "$MOOSE_DIR/framework" ]]; then

    echo
    echo "ERROR: Could not locate the MOOSE repository root."
    echo
    echo "Script directory:"
    echo "  $SCRIPT_DIR"
    echo
    echo "Current directory:"
    echo "  $PWD"
    echo
    echo "Run this script from inside a MOOSE repository,"
    echo "or place it under <MOOSE repo>/scripts/."
    exit 1

fi


cd "$MOOSE_DIR"

echo
echo "============================================"
echo " MOOSE repository"
echo "============================================"

echo "MOOSE_DIR=$MOOSE_DIR"
echo "BRANCH=$(git branch --show-current)"
echo "COMMIT=$(git rev-parse --short HEAD)"


# ============================================================
# 9. MOOSE dependency paths
# ============================================================

export LIBMESH_DIR="$CONDA_PREFIX"
export WASP_DIR="$CONDA_PREFIX"


# ============================================================
# 10. Initialize submodules
# ============================================================

echo
echo "============================================"
echo " Updating submodules"
echo "============================================"

git submodule sync --recursive
git submodule update --init --recursive


# ============================================================
# 11. Explicitly initialize NEML2
#
# framework/contrib/neml2 may use:
#
#   update = none
#
# which causes the generic recursive update above to skip it.
# ============================================================

if [[ ! -f framework/contrib/neml2/CMakeLists.txt ]]; then

    echo
    echo "Initializing framework/contrib/neml2..."
    echo

    git submodule update \
        --init \
        --checkout \
        --force \
        --recursive \
        framework/contrib/neml2

fi


if [[ ! -f framework/contrib/neml2/CMakeLists.txt ]]; then

    echo
    echo "ERROR: NEML2 submodule is still not initialized:"
    echo
    echo "  $MOOSE_DIR/framework/contrib/neml2"
    echo

    exit 1

fi


# ============================================================
# 12. Build NEML2 v3
#
# IMPORTANT:
#
# Do NOT globally:
#
#   export CC=/usr/bin/clang
#   export CXX=/usr/bin/clang++
#
# because Conda/MOOSE intentionally sets:
#
#   CC  = mpicc
#   CXX = mpicxx
#
# We override the ordinary compiler ONLY inside the NEML2
# CMake configuration.
#
#
# NEML2:
#
#   ordinary C
#       -> /usr/bin/clang
#
#   ordinary C++
#       -> /usr/bin/clang++
#
#   MPI
#       -> Conda mpicc/mpicxx
# ============================================================

echo
echo "============================================"
echo " Building NEML2 v3"
echo "============================================"

echo
echo "NEML2 C compiler:"
echo "  /usr/bin/clang"

echo
echo "NEML2 C++ compiler:"
echo "  /usr/bin/clang++"

echo
echo "NEML2 MPI C compiler:"
echo "  $CONDA_PREFIX/bin/mpicc"

echo
echo "NEML2 MPI C++ compiler:"
echo "  $CONDA_PREFIX/bin/mpicxx"

echo


NEML2_JOBS=8 \
./scripts/update_and_rebuild_neml2.sh \
    --skip-submodule-update \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DMPI_C_COMPILER="$CONDA_PREFIX/bin/mpicc" \
    -DMPI_CXX_COMPILER="$CONDA_PREFIX/bin/mpicxx" \
    --config-settings=build-dir=build/moose-apple


# ============================================================
# 13. Discover Torch / NEML2
# ============================================================

export LIBTORCH_DIR="$(
python -c 'import torch; print(torch.__path__[0])'
)"

export NEML2_DIR="$(
python -c '
import neml2
import os
print(os.path.dirname(neml2.__file__))
'
)"


echo
echo "============================================"
echo " NEML2 / Torch"
echo "============================================"

echo "LIBTORCH_DIR=$LIBTORCH_DIR"
echo "NEML2_DIR=$NEML2_DIR"


python - <<'PY'
import torch
import neml2

print()
print("Torch version :", torch.__version__)
print("Torch path    :", torch.__file__)
print("NEML2 path    :", neml2.__file__)
PY


# ============================================================
# 14. Verify MOOSE compiler environment was NOT overwritten
#
# We expect Conda activation to control these.
#
# Usually:
#
#   CC  = $CONDA_PREFIX/bin/mpicc
#   CXX = $CONDA_PREFIX/bin/mpicxx
# ============================================================

echo
echo "============================================"
echo " MOOSE compiler environment"
echo "============================================"

echo "CC=${CC:-}"
echo "CXX=${CXX:-}"

echo
echo "mpicc:"
which mpicc

echo
echo "mpicxx:"
which mpicxx


# ============================================================
# 15. Configure MOOSE
# ============================================================

echo
echo "============================================"
echo " Configuring MOOSE"
echo "============================================"

./configure \
    --with-libtorch="$LIBTORCH_DIR" \
    --with-neml2=yes \
    --with-kokkos=yes


echo
echo "MOOSE configuration:"
echo

grep -E \
    'ENABLE_LIBTORCH|ENABLE_NEML2|ENABLE_KOKKOS|NEML2_DIR|LIBTORCH_DIR' \
    conf_vars.mk \
    || true


# ============================================================
# 16. Build Solid Mechanics
# ============================================================

echo
echo "============================================"
echo " Building solid_mechanics"
echo "============================================"

cd "$MOOSE_DIR/modules/solid_mechanics"

make -j8 METHOD=opt


# ============================================================
# 17. Verify capabilities
# ============================================================

echo
echo "============================================"
echo " MOOSE capabilities"
echo "============================================"

./solid_mechanics-opt --show-capabilities


# ============================================================
# 18. Run NEML2 tests
# ============================================================

echo
echo "============================================"
echo " Running NEML2 tests"
echo "============================================"

./run_tests -j8 --re neml2


# ============================================================
# 19. Done
# ============================================================

echo
echo "============================================"
echo " BUILD COMPLETED SUCCESSFULLY"
echo "============================================"

echo
echo "Repository:"
echo "  $MOOSE_DIR"

echo
echo "Executable:"
echo "  $MOOSE_DIR/modules/solid_mechanics/solid_mechanics-opt"

echo
