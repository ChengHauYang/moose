#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=scripts/mac/moose_mac_common.sh
source "$SCRIPT_DIR/moose_mac_common.sh"

show_help()
{
    cat <<'EOF'
MOOSE-SCZM macOS CLI

Usage:
  ./scripts/mac/moose.sh <command> [target|scope] [options...]

Commands:
  dummy                          Show complete dashboard: built opts, NEML2/Kokkos linkage,
                                 submodule guidance (with/without NEML2), pipeline progress,
                                 and next step suggestions
  status                         Alias for dummy
  doctor                         Inspect the environment, NEML2 hashes, and build state
  init [neml2-flag]              Install dependencies and configure when needed;
                                 builds repo-isolated NEML2 without modifying Conda
  compile [target] [-j N] [--raw] [--profile] [neml2-flag]
  clobber [target|all] [--yes]    Full clobber (generated/unity/dep files) for a
                                 target or all; expensive, use only when the
                                 harness reports build-system corruption
                                 Auto-sync repo-isolated NEML2 if branch changed, then
                                 compile framework-core (library only), framework,
                                 solid_mechanics (default), shifted_boundary_method,
                                 unit, neml2, or all.
                                 --profile prints a build profile (TUs, link steps,
                                 wall time, ccache hit/miss).
  build [target] [-j N] [--raw]  Alias for compile
  run <target> <input.i> [...]   Run an input file with the selected executable
  test [scope|mod] [pattern] [...]
                                 Run regression tests (without compiling)
  unit [target] [pattern] [...]  Run C++ GoogleTest unit tests (default: framework)
  verify [scope] [-j N] [--raw]  Compile the required target, then test it
  all [-j N] [--raw]             Init, compile all, and test all
  precheck                       Install this clone's pre-commit hook
  help                           Show this message

NEML2 Source Flags (supported on init, compile, verify, all, doctor, test, unit, run):
  --use-local-neml2      Force repo-isolated build of framework/contrib/neml2
                         in framework/contrib/neml2/build/installed (Conda untouched)
  --use-committed-neml2  Reset framework/contrib/neml2 to the MOOSE-committed SHA
                         and sync/use it
  --use-conda-neml2      Skip repo-isolated NEML2 and use Conda's installed neml2
  --skip-neml2-sync      Skip NEML2 synchronization/rebuild; retain installation sanity checks

Test scopes:
  framework-kokkos, sczm, neml2-aoti, neml2, all (default),
  or any module name (e.g. solid_mechanics, shifted_boundary_method)

Run targets:
  framework, solid_mechanics, shifted_boundary_method

Examples:
  ./scripts/mac/moose.sh compile solid_mechanics
  ./scripts/mac/moose.sh compile neml2
  ./scripts/mac/moose.sh compile solid_mechanics --skip-neml2-sync
  ./scripts/mac/moose.sh compile solid_mechanics --use-committed-neml2
  ./scripts/mac/moose.sh compile solid_mechanics --use-conda-neml2
  ./scripts/mac/moose.sh test sczm -j 8
  ./scripts/mac/moose.sh test solid_mechanics crystal_plasticity
  ./scripts/mac/moose.sh unit RankTwoTensor*
  ./scripts/mac/moose.sh verify neml2

Environment overrides:
  MOOSE_CONDA_ENV   Conda environment (default: moose)
  MOOSE_NEML2_SRC   NEML2 source mode: auto (default), local, committed, or conda
  METHOD            Build method (default: opt)
  BUILD_JOBS        Parallel compile jobs (default: half of P-cores minus one,
                    capped by half of RAM capacity)
  JOBS              Parallel test jobs (default: 8)
  MOOSE_MAC_CCACHE  Set to 'on' to opt into ccache (off by default: it yields no
                    hits on this libMesh/WASP stack and adds ~10% per TU)
  CCACHE_BIN        Explicit ccache binary to use when ccache is opted in
EOF
}

initialize()
{
    moose_mac_activate
    moose_mac_locate_repo
    moose_mac_env_exports

    if ! moose_mac_torch_ok; then
        "$SCRIPT_DIR/bootstrap_mac.sh"
    fi

    moose_mac_sync_neml2

    if ! moose_mac_configured ||
       ! moose_mac_kokkos_enabled ||
       ! moose_mac_neml2_enabled ||
       ! moose_mac_libtorch_enabled; then
        "$SCRIPT_DIR/configure_mac.sh"
    else
        moose_mac_section "Initialization"
        echo "Dependencies and MOOSE configuration are ready."
    fi

    echo
    echo "INIT_COMPLETED=1"
}

parse_jobs()
{
    local job_variable="$1"
    shift
    REMAINING_ARGS=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            -j)
                [[ $# -ge 2 ]] || moose_mac_die "-j requires a job count."
                printf -v "$job_variable" '%s' "$2"
                export "$job_variable"
                shift 2
                ;;
            -j*)
                printf -v "$job_variable" '%s' "${1#-j}"
                export "$job_variable"
                shift
                ;;
            *)
                REMAINING_ARGS+=("$1")
                shift
                ;;
        esac
    done
}

compile_target()
{
    moose_mac_activate
    moose_mac_locate_repo
    moose_mac_env_exports

    local target="solid_mechanics"
    local raw_flag=()
    local profile_flag=()

    parse_jobs BUILD_JOBS "$@"

    for arg in "${REMAINING_ARGS[@]}"; do
        case "$arg" in
            --raw)
                raw_flag=(--raw)
                ;;
            --profile)
                profile_flag=(--profile)
                ;;
            framework-core|framework_core|framework|solid_mechanics|shifted_boundary_method|sbm|all|unit|framework_unit|*_unit|neml2)
                target="$arg"
                ;;
            *)
                moose_mac_die "Unknown compile option '$arg'. Use: framework-core, framework, solid_mechanics, shifted_boundary_method, unit, neml2, all, --raw, or --profile"
                ;;
        esac
    done

    if "$SCRIPT_DIR/build_mac.sh" "$target" \
        ${raw_flag[@]+"${raw_flag[@]}"} ${profile_flag[@]+"${profile_flag[@]}"}; then
        rm -f "$(moose_mac_build_failed_marker "$target" "${METHOD:-opt}")"
        if ! ls "$MOOSE_DIR"/.moose_mac_build_failed.*.* >/dev/null 2>&1; then
            rm -f "$MOOSE_DIR/.moose_mac_build_failed"
        fi
    else
        touch "$MOOSE_DIR/.moose_mac_build_failed"
        touch "$(moose_mac_build_failed_marker "$target" "${METHOD:-opt}")"
        local reason
        reason="$(moose_mac_clobberall_reason)"
        if [[ -n "$reason" ]]; then
            echo
            echo "[CLOBBERALL RECOMMENDED] $reason"
            echo "  This looks like build-system corruption rather than ordinary staleness."
            echo "  Try a defensive full clobber, then rebuild:"
            echo "    ./scripts/mac/moose.sh clobber $target"
            echo "    ./scripts/mac/moose.sh compile $target"
        fi
        return 1
    fi
}

clobber_target()
{
    moose_mac_activate
    moose_mac_locate_repo
    moose_mac_env_exports

    local target="all"
    local confirm=0
    for arg in "$@"; do
        case "$arg" in
            --yes|-y) confirm=1 ;;
            all|framework|solid_mechanics|shifted_boundary_method|sbm|unit|framework_unit|*_unit)
                target="$arg" ;;
            *) moose_mac_die "Unknown clobber option '$arg'." ;;
        esac
    done

    if [[ "$confirm" -ne 1 ]]; then
        echo "This will run 'make clobber' (full clean including generated/unity/dependency files) for: $target"
        echo "It can take a long time to rebuild afterwards."
        read -r -p "Continue? [y/N] " reply
        [[ "$reply" =~ ^[Yy]$ ]] || { echo "Aborted."; exit 0; }
    fi

    local dirs=()
    if [[ "$target" == "all" ]]; then
        dirs+=("$MOOSE_DIR/framework" "$MOOSE_DIR/test" "$MOOSE_DIR/unit")
        local mod_dir
        for mod_dir in "$MOOSE_DIR"/modules/*/; do
            [[ -d "$mod_dir" ]] || continue
            if [[ -f "$mod_dir/Makefile" ]]; then
                dirs+=("${mod_dir%/}")
                if [[ -d "${mod_dir}unit" && -f "${mod_dir}unit/Makefile" ]]; then
                    dirs+=("${mod_dir%/}/unit")
                fi
            fi
        done
    else
        dirs+=("$(moose_mac_module_dir "$target")")
    fi

    local dir
    for dir in "${dirs[@]}"; do
        echo "Clobbering in $dir ..."
        make -C "$dir" clobber METHOD="${METHOD:-opt}"
    done

    rm -f "$MOOSE_DIR"/.moose_mac_build_failed* 2>/dev/null || true
    echo "Clobber complete. Rebuild with: ./scripts/mac/moose.sh compile $target"
}

run_input()
{
    [[ $# -ge 2 ]] || \
        moose_mac_die "Usage: ./scripts/mac/moose.sh run <target> <input.i> [MOOSE options...]"

    local target="$1"
    local input="$2"
    shift 2

    moose_mac_activate
    moose_mac_locate_repo
    moose_mac_env_exports

    case "$target" in
        framework|solid_mechanics|shifted_boundary_method|sbm)
            ;;
        *)
            moose_mac_die "Unknown run target '$target'. Use: framework, solid_mechanics, shifted_boundary_method"
            ;;
    esac

    local executable
    executable="$(moose_mac_module_exe "$target")"
    [[ -f "$input" ]] || moose_mac_die "Input file not found: $input"
    [[ -x "$executable" ]] || \
        moose_mac_die "Executable not found: $executable. Run './scripts/mac/moose.sh compile $target' first."

    moose_mac_section "Running $input"
    exec "$executable" -i "$input" "$@"
}

run_tests()
{
    local scope="all"

    if [[ $# -gt 0 && "$1" != -* ]]; then
        scope="$1"
        shift
    fi

    parse_jobs JOBS "$@"
    MOOSE_MAC_SKIP_BUILD=1 "$SCRIPT_DIR/test_mac.sh" \
        "$scope" ${REMAINING_ARGS[@]+"${REMAINING_ARGS[@]}"}
}

run_unit()
{
    local target="framework"
    local filter=""
    local extra_args=()

    for arg in "$@"; do
        case "$arg" in
            framework|unit)
                target="framework"
                ;;
            solid_mechanics|sbm|shifted_boundary_method|*_unit)
                target="${arg%_unit}"
                [[ "$target" == "sbm" ]] && target="shifted_boundary_method"
                ;;
            --gtest_filter=*)
                filter="$arg"
                ;;
            -*)
                extra_args+=("$arg")
                ;;
            *)
                if [[ -z "$filter" ]]; then
                    filter="--gtest_filter=$arg"
                else
                    extra_args+=("$arg")
                fi
                ;;
        esac
    done

    moose_mac_activate
    moose_mac_locate_repo
    moose_mac_env_exports

    set -u
    moose_mac_check_arch
    moose_mac_check_sdk

    local unit_dir unit_exe
    unit_dir="$(moose_mac_unit_dir "$target")"
    unit_exe="$(moose_mac_unit_exe "$target")"

    if [[ ! -f "$unit_exe" ]]; then
        local compile_hint="./scripts/mac/moose.sh compile unit"
        [[ "$target" != "framework" ]] && compile_hint="./scripts/mac/moose.sh compile ${target}_unit"
        moose_mac_die "Unit test executable not found: $unit_exe\nRun '$compile_hint' first."
    fi

    local run_args=()
    [[ -n "$filter" ]] && run_args+=("$filter")
    [[ ${#extra_args[@]} -gt 0 ]] && run_args+=("${extra_args[@]}")

    moose_mac_section "Running unit tests: $target"
    echo "Directory  : $unit_dir"
    echo "Executable : $unit_exe"
    [[ -n "$filter" ]] && echo "Filter     : ${filter#--gtest_filter=}"
    echo "Command    : $unit_exe ${run_args[*]:-}"
    echo

    cd "$unit_dir"

    local tmp_out
    tmp_out="$(mktemp /tmp/moose-unit-run-XXXXXX.log)"

    set +e
    "$unit_exe" ${run_args[@]+"${run_args[@]}"} 2>&1 | tee "$tmp_out"
    local status="${PIPESTATUS[0]}"
    set -e

    if [[ $status -ne 0 ]]; then
        moose_mac_diagnose_dyld_error "$tmp_out" || true
    fi
    rm -f "$tmp_out"

    return "$status"
}

build_target_for_scope()
{
    case "$1" in
        framework|framework-kokkos|kokkos)
            echo framework
            ;;
        sczm|shifted_boundary_method|sbm)
            echo shifted_boundary_method
            ;;
        neml2|neml2-aoti|aoti)
            echo solid_mechanics
            ;;
        unit|framework_unit)
            echo unit
            ;;
        *_unit)
            echo "$1"
            ;;
        all)
            echo all
            ;;
        *)
            echo "$1"
            ;;
    esac
}

verify_scope()
{
    local scope="all"
    local raw_flag=()

    parse_jobs JOBS "$@"
    export BUILD_JOBS="$JOBS"

    local filtered_args=()
    for arg in "${REMAINING_ARGS[@]}"; do
        case "$arg" in
            --raw)
                raw_flag=(--raw)
                ;;
            framework|framework-kokkos|kokkos|sczm|shifted_boundary_method|sbm|neml2|neml2-aoti|aoti|all|unit|*_unit)
                scope="$arg"
                ;;
            *)
                filtered_args+=("$arg")
                ;;
        esac
    done

    local target
    target="$(build_target_for_scope "$scope")"

    initialize
    MOOSE_MAC_NEML2_ALREADY_SYNCED=1 "$SCRIPT_DIR/build_mac.sh" "$target" ${raw_flag[@]+"${raw_flag[@]}"}

    case "$scope" in
        unit|*_unit)
            run_unit "$scope" ${filtered_args[@]+"${filtered_args[@]}"}
            ;;
        *)
            MOOSE_MAC_SKIP_BUILD=1 "$SCRIPT_DIR/test_mac.sh" \
                "$scope" ${filtered_args[@]+"${filtered_args[@]}"}
            ;;
    esac

    echo
    echo "VERIFY_COMPLETED=1"
}

FILTERED_CLI_ARGS=()
for arg in "$@"; do
    case "$arg" in
        --use-conda-neml2)
            export MOOSE_NEML2_SRC=conda
            ;;
        --use-committed-neml2)
            export MOOSE_NEML2_SRC=committed
            ;;
        --use-local-neml2)
            export MOOSE_NEML2_SRC=local
            ;;
        --skip-neml2-sync)
            export MOOSE_MAC_SKIP_NEML2_SYNC=1
            ;;
        *)
            FILTERED_CLI_ARGS+=("$arg")
            ;;
    esac
done
set -- ${FILTERED_CLI_ARGS[@]+"${FILTERED_CLI_ARGS[@]}"}

if [[ $# -eq 0 ]]; then
    show_help
    exit 0
fi

COMMAND="$1"
shift

case "$COMMAND" in
    help|--help|-h)
        show_help
        ;;
    doctor)
        exec "$SCRIPT_DIR/doctor_mac.sh" "$@"
        ;;
    dummy|status)
        exec "$SCRIPT_DIR/dummy_mac.sh" "$@"
        ;;
    init)
        if [[ $# -gt 0 ]]; then
            moose_mac_die "Unknown init option '$1'. Use: --use-local-neml2, --use-committed-neml2, or --use-conda-neml2"
        fi
        initialize
        ;;
    compile|build)
        compile_target "$@"
        ;;
    run)
        run_input "$@"
        ;;
    test)
        run_tests "$@"
        ;;
    unit)
        run_unit "$@"
        ;;
    clobber)
        clobber_target "$@"
        ;;
    verify)
        verify_scope "$@"
        ;;
    all)
        parse_jobs JOBS "$@"
        all_raw=()
        for arg in "${REMAINING_ARGS[@]}"; do
            case "$arg" in
                --raw) all_raw=(--raw) ;;
                *) moose_mac_die "Unknown all option '$arg'." ;;
            esac
        done
        export BUILD_JOBS="$JOBS"
        initialize
        MOOSE_MAC_NEML2_ALREADY_SYNCED=1 "$SCRIPT_DIR/build_mac.sh" all ${all_raw[@]+"${all_raw[@]}"}
        MOOSE_MAC_SKIP_BUILD=1 "$SCRIPT_DIR/test_mac.sh" all
        echo
        echo "ALL_COMPLETED=1"
        ;;
    precheck)
        exec "$SCRIPT_DIR/init_precheck.sh" "$@"
        ;;
    *)
        moose_mac_die "Unknown command '$COMMAND'. Run './scripts/mac/moose.sh --help'."
        ;;
esac
