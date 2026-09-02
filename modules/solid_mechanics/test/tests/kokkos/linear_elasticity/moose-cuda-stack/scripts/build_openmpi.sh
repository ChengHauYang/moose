#!/bin/bash
# Step 0 (Option C addendum): build OpenMPI 4.1.6 from source with CUDA
# awareness so `-mat_type aijkokkos -vec_type kokkos` no longer needs the
# `-use_gpu_aware_mpi 0` fallback. This mpi is installed into $PREFIX and
# then env.sh points $PATH at $PREFIX/bin so downstream PETSc + libmesh +
# MOOSE builds pick up our mpicc/mpicxx/mpif90 instead of /usr/bin/mpicc.
#
# Source: https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-4.1.6.tar.bz2
# Runtime: ~30-45 min on 8 jobs. Reruns are cheap if src already extracted.

set -e
set -o pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
. "$SCRIPT_DIR/env.sh"

OMPI_VER=${OMPI_VER:-4.1.6}
OMPI_SRC_DIR="$STACK_DIR/src/openmpi-$OMPI_VER"
OMPI_TARBALL="$STACK_DIR/src/openmpi-$OMPI_VER.tar.bz2"
OMPI_URL="https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-$OMPI_VER.tar.bz2"

LOG="$LOGS/openmpi-$(date +%Y%m%d-%H%M%S).log"
echo "[build_openmpi] logging to $LOG"

mkdir -p "$STACK_DIR/src"
if [ ! -f "$OMPI_TARBALL" ]; then
  echo "[build_openmpi] downloading $OMPI_URL"
  cd "$STACK_DIR/src"
  wget -q "$OMPI_URL"
fi
if [ ! -d "$OMPI_SRC_DIR" ]; then
  echo "[build_openmpi] extracting"
  cd "$STACK_DIR/src"
  tar xf "openmpi-$OMPI_VER.tar.bz2"
fi

# Fresh build dir every run so failed partial builds do not leak.
BUILD_DIR="$STACK_DIR/src/openmpi-$OMPI_VER-build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# --with-cuda        : enables CUDA-aware datatypes (opal_built_with_cuda_support = true)
# --enable-mpi-fortran=usempi : match what libmesh/PETSc expect (mpif.h + use mpi)
# Compilers: system gcc-11 / g++-11 / gfortran-9 (same as build_petsc.sh)
"$OMPI_SRC_DIR/configure" \
  --prefix="$PREFIX" \
  --with-cuda="$CUDA_DIR" \
  --enable-mpi-fortran=usempi \
  --disable-mpi-cxx \
  --disable-oshmem \
  --without-verbs \
  --without-cma \
  CC=/usr/bin/gcc-11 CXX=/usr/bin/g++-11 FC=/usr/bin/gfortran-9 \
  2>&1 | tee "$LOG"

make -j"$MOOSE_JOBS" 2>&1 | tee -a "$LOG"
make install 2>&1 | tee -a "$LOG"

echo
echo "[build_openmpi] Verifying CUDA support:"
"$PREFIX/bin/ompi_info" --parsable --all 2>&1 | grep 'built_with_cuda_support:value' | head -2
echo
echo "[build_openmpi] mpicc / mpicxx / mpif90 wrappers:"
ls -1 "$PREFIX/bin/mpicc" "$PREFIX/bin/mpicxx" "$PREFIX/bin/mpif90" 2>&1
echo
echo "[build_openmpi] Done. Downstream scripts should now source env.sh which"
echo "[build_openmpi] prepends \$PREFIX/bin to PATH."
