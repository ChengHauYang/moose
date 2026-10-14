#!/usr/bin/env bash
#* This file is part of the MOOSE framework
#* https://www.mooseframework.org
#*
#* All rights reserved, see COPYRIGHT for full restrictions
#* https://github.com/idaholab/moose/blob/master/COPYRIGHT
#*
#* Licensed under LGPL 2.1, please see LICENSE for details
#* https://www.gnu.org/licenses/lgpl-2.1.html

# Run after compile_script_mac.sh has configured MOOSE with Kokkos and NEML2.
set -eo pipefail

# Conda activation hooks may reference unset variables, so enable nounset afterward.
source "$HOME/miniforge/etc/profile.d/conda.sh"
conda activate moose
set -u

export MOOSE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
export PATH="$CONDA_PREFIX/bin:$PATH"
export LIBMESH_DIR="$CONDA_PREFIX"
export WASP_DIR="$CONDA_PREFIX"
export METHOD=opt

echo "Testing MOOSE checkout: $MOOSE_DIR"

echo "Building framework tests"
cd "$MOOSE_DIR/test"
make -j8 METHOD=opt

echo "Running framework Kokkos tests on one MPI process"
./run_tests -j8 --compute-device=cpu \
  --re 'kokkos/(interfacekernels|kernels/(scalar_constraint|auto_scaling|kernel_precompute))'

echo "Running framework Kokkos tests on two MPI processes with a distributed mesh"
./run_tests -j8 -p2 --distributed-mesh --compute-device=cpu \
  --re 'kokkos/(interfacekernels|kernels/(scalar_constraint|auto_scaling|kernel_precompute))'

echo "Building shifted boundary method tests"
cd "$MOOSE_DIR/modules/shifted_boundary_method"
make -j8 METHOD=opt

echo "Running host and Kokkos SCZM tests on one MPI process"
./run_tests -j8 --compute-device=cpu \
  --re '(kokkos_shifted_interface|shifted_cohesive_zone)'

echo "Running Kokkos SCZM tests on two MPI processes with a distributed mesh"
./run_tests -j8 -p2 --distributed-mesh --compute-device=cpu \
  --re 'kokkos_shifted_interface'

echo "Building solid mechanics tests"
cd "$MOOSE_DIR/modules/solid_mechanics"
make -j8 METHOD=opt

echo "Running NEML2 AOTI compilation and host/Kokkos runtime tests"
# MPI wrappers add linker inputs that conflict with Clang's PCH output. Scope
# the ordinary C++ compiler to the AOTI test process, preserving build settings.
CXX=/usr/bin/clang++ ./run_tests -j8 --compute-device=cpu \
  --re 'kokkos/neml2_aoti_batch'

echo "Requested test runs completed. Check TestHarness output for skipped tests."
