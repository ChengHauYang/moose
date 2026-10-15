#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$SCRIPT_DIR/moose_mac_common.sh"

SCOPE="${1:-all}"

moose_mac_activate
moose_mac_locate_repo
moose_mac_env_exports

set -u

moose_mac_check_arch
moose_mac_check_sdk

run_framework_kokkos()
{
    "$SCRIPT_DIR/build_mac.sh" framework

    cd "$MOOSE_DIR/test"

    moose_mac_section "Framework Kokkos tests: 1 MPI rank"

    ./run_tests -j"$JOBS" --compute-device=cpu \
        --re 'kokkos/(interfacekernels|kernels/(scalar_constraint|auto_scaling|kernel_precompute))'

    moose_mac_section "Framework Kokkos tests: 2 MPI ranks distributed mesh"

    ./run_tests -j"$JOBS" -p2 --distributed-mesh --compute-device=cpu \
        --re 'kokkos/(interfacekernels|kernels/(scalar_constraint|auto_scaling|kernel_precompute))'
}

run_sczm()
{
    "$SCRIPT_DIR/build_mac.sh" shifted_boundary_method

    cd "$MOOSE_DIR/modules/shifted_boundary_method"

    moose_mac_section "SCZM tests: 1 MPI rank"

    ./run_tests -j"$JOBS" --compute-device=cpu \
        --re '(kokkos_shifted_interface|shifted_cohesive_zone)'

    moose_mac_section "SCZM Kokkos tests: 2 MPI ranks distributed mesh"

    ./run_tests -j"$JOBS" -p2 --distributed-mesh --compute-device=cpu \
        --re 'kokkos_shifted_interface'
}

run_neml2_aoti()
{
    "$SCRIPT_DIR/build_mac.sh" solid_mechanics

    cd "$MOOSE_DIR/modules/solid_mechanics"

    moose_mac_section "NEML2 AOTI Kokkos tests"

    # MPI wrappers add linker inputs that conflict with Clang's PCH output.
    # Scope the ordinary C++ compiler to the AOTI test process only.
    CXX=/usr/bin/clang++ ./run_tests -j"$JOBS" --compute-device=cpu \
        --re 'kokkos/neml2_aoti_batch'
}

run_neml2()
{
    "$SCRIPT_DIR/build_mac.sh" solid_mechanics

    cd "$MOOSE_DIR/modules/solid_mechanics"

    moose_mac_section "NEML2 tests"

    ./run_tests -j"$JOBS" --re neml2
}

case "$SCOPE" in
    framework|framework-kokkos|kokkos)
        run_framework_kokkos
        ;;
    sczm|shifted_boundary_method|sbm)
        run_sczm
        ;;
    neml2-aoti|aoti)
        run_neml2_aoti
        ;;
    neml2)
        run_neml2
        ;;
    all)
        run_framework_kokkos
        run_sczm
        run_neml2_aoti
        ;;
    *)
        moose_mac_die "Unknown test scope '$SCOPE'. Use: framework-kokkos, sczm, neml2-aoti, neml2, all"
        ;;
esac

echo
echo "TEST_COMPLETED=1"
