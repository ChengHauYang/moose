#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$SCRIPT_DIR/moose_mac_common.sh"

SCOPE="${1:-all}"
if [[ $# -gt 0 ]]; then shift; fi
EXTRA_ARGS=("$@")

moose_mac_activate
moose_mac_locate_repo
moose_mac_env_exports

set -u

moose_mac_check_arch
moose_mac_check_sdk

has_re=0
for arg in ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}; do
    if [[ "$arg" == "--re" || "$arg" == --re=* ]]; then
        has_re=1
        break
    fi
done

PARSED_ARGS=()
RE_PATTERN=""
for arg in ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}; do
    if [[ $has_re -eq 0 && -z "$RE_PATTERN" && "$arg" != -* ]]; then
        RE_PATTERN="$arg"
        PARSED_ARGS+=("--re" "$RE_PATTERN")
    else
        PARSED_ARGS+=("$arg")
    fi
done

run_framework_kokkos()
{
    if [[ "${MOOSE_MAC_SKIP_BUILD:-0}" != "1" ]]; then
        "$SCRIPT_DIR/build_mac.sh" framework
    fi

    local exe="$MOOSE_DIR/test/moose_test-${METHOD:-opt}"
    moose_mac_check_preflight "$exe"

    cd "$MOOSE_DIR/test"

    local default_re='kokkos/(interfacekernels|kernels/(scalar_constraint|auto_scaling|kernel_precompute))'
    local run_args=()
    [[ -z "$RE_PATTERN" ]] && run_args+=("--re" "$default_re")
    [[ ${#PARSED_ARGS[@]} -gt 0 ]] && run_args+=("${PARSED_ARGS[@]}")

    moose_mac_section "Framework Kokkos tests: 1 MPI rank"
    echo "Directory : $MOOSE_DIR/test"
    echo "Command   : ./run_tests -j$JOBS --compute-device=cpu ${run_args[*]:-}"
    echo

    ./run_tests -j"$JOBS" --compute-device=cpu \
        ${run_args[@]+"${run_args[@]}"}

    moose_mac_section "Framework Kokkos tests: 2 MPI ranks distributed mesh"
    echo "Directory : $MOOSE_DIR/test"
    echo "Command   : ./run_tests -j$JOBS -p2 --distributed-mesh --compute-device=cpu ${run_args[*]:-}"
    echo

    ./run_tests -j"$JOBS" -p2 --distributed-mesh --compute-device=cpu \
        ${run_args[@]+"${run_args[@]}"}
}

run_sczm()
{
    if [[ "${MOOSE_MAC_SKIP_BUILD:-0}" != "1" ]]; then
        "$SCRIPT_DIR/build_mac.sh" shifted_boundary_method
    fi

    local exe="$MOOSE_DIR/modules/shifted_boundary_method/shifted_boundary_method-${METHOD:-opt}"
    moose_mac_check_preflight "$exe"

    cd "$MOOSE_DIR/modules/shifted_boundary_method"

    local default_re='(kokkos_shifted_interface|shifted_cohesive_zone)'
    local run_args=()
    [[ -z "$RE_PATTERN" ]] && run_args+=("--re" "$default_re")
    [[ ${#PARSED_ARGS[@]} -gt 0 ]] && run_args+=("${PARSED_ARGS[@]}")

    moose_mac_section "SCZM tests: 1 MPI rank"
    echo "Directory : $MOOSE_DIR/modules/shifted_boundary_method"
    echo "Command   : ./run_tests -j$JOBS --compute-device=cpu ${run_args[*]:-}"
    echo

    ./run_tests -j"$JOBS" --compute-device=cpu \
        ${run_args[@]+"${run_args[@]}"}

    local default_re_p2='kokkos_shifted_interface'
    local run_args_p2=()
    [[ -z "$RE_PATTERN" ]] && run_args_p2+=("--re" "$default_re_p2")
    [[ ${#PARSED_ARGS[@]} -gt 0 ]] && run_args_p2+=("${PARSED_ARGS[@]}")

    moose_mac_section "SCZM Kokkos tests: 2 MPI ranks distributed mesh"
    echo "Directory : $MOOSE_DIR/modules/shifted_boundary_method"
    echo "Command   : ./run_tests -j$JOBS -p2 --distributed-mesh --compute-device=cpu ${run_args_p2[*]:-}"
    echo

    ./run_tests -j"$JOBS" -p2 --distributed-mesh --compute-device=cpu \
        ${run_args_p2[@]+"${run_args_p2[@]}"}
}

run_neml2_aoti()
{
    if [[ "${MOOSE_MAC_SKIP_BUILD:-0}" != "1" ]]; then
        "$SCRIPT_DIR/build_mac.sh" solid_mechanics
    fi

    local exe="$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-${METHOD:-opt}"
    moose_mac_check_preflight "$exe"

    cd "$MOOSE_DIR/modules/solid_mechanics"

    local default_re='kokkos/neml2_aoti_batch'
    local run_args=()
    [[ -z "$RE_PATTERN" ]] && run_args+=("--re" "$default_re")
    [[ ${#PARSED_ARGS[@]} -gt 0 ]] && run_args+=("${PARSED_ARGS[@]}")

    moose_mac_section "NEML2 AOTI Kokkos tests"
    echo "Directory : $MOOSE_DIR/modules/solid_mechanics"
    echo "Command   : CXX=/usr/bin/clang++ ./run_tests -j$JOBS --compute-device=cpu ${run_args[*]:-}"
    echo

    CXX=/usr/bin/clang++ ./run_tests -j"$JOBS" --compute-device=cpu \
        ${run_args[@]+"${run_args[@]}"}
}

run_neml2()
{
    if [[ "${MOOSE_MAC_SKIP_BUILD:-0}" != "1" ]]; then
        "$SCRIPT_DIR/build_mac.sh" solid_mechanics
    fi

    local exe="$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-${METHOD:-opt}"
    moose_mac_check_preflight "$exe"

    cd "$MOOSE_DIR/modules/solid_mechanics"

    local default_re='neml2'
    local run_args=()
    [[ -z "$RE_PATTERN" ]] && run_args+=("--re" "$default_re")
    [[ ${#PARSED_ARGS[@]} -gt 0 ]] && run_args+=("${PARSED_ARGS[@]}")

    moose_mac_section "NEML2 tests"
    echo "Directory : $MOOSE_DIR/modules/solid_mechanics"
    echo "Command   : ./run_tests -j$JOBS ${run_args[*]:-}"
    echo

    ./run_tests -j"$JOBS" \
        ${run_args[@]+"${run_args[@]}"}
}

run_module_tests()
{
    local mod="$1"
    local mod_dir mod_exe
    mod_dir="$(moose_mac_module_dir "$mod")"
    mod_exe="$(moose_mac_module_exe "$mod")"

    if [[ "${MOOSE_MAC_SKIP_BUILD:-0}" != "1" ]]; then
        "$SCRIPT_DIR/build_mac.sh" "$mod"
    fi

    moose_mac_check_preflight "$mod_exe"

    cd "$mod_dir"

    moose_mac_section "Tests: $mod"
    echo "Directory : $mod_dir"
    echo "Command   : ./run_tests -j$JOBS --compute-device=cpu ${PARSED_ARGS[*]:-}"
    echo

    ./run_tests -j"$JOBS" --compute-device=cpu \
        ${PARSED_ARGS[@]+"${PARSED_ARGS[@]}"}
}

case "$SCOPE" in
    framework-kokkos|kokkos)
        run_framework_kokkos
        ;;
    sczm)
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
    framework)
        run_module_tests framework
        ;;
    solid_mechanics)
        run_module_tests solid_mechanics
        ;;
    shifted_boundary_method|sbm)
        run_module_tests shifted_boundary_method
        ;;
    *)
        if [[ -d "$MOOSE_DIR/modules/$SCOPE" ]]; then
            run_module_tests "$SCOPE"
        else
            moose_mac_die "Unknown test scope or module '$SCOPE'. Use: framework-kokkos, sczm, neml2-aoti, neml2, all, or a module name (e.g. solid_mechanics)"
        fi
        ;;
esac

echo
echo "TEST_COMPLETED=1"
