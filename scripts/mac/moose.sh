#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=scripts/mac/moose_mac_common.sh
source "$SCRIPT_DIR/moose_mac_common.sh"

if [[ "${1:-}" == "-C" ]]; then
    [[ $# -ge 3 ]] || moose_mac_die "Usage: moose.sh -C <checkout> <command> [...]"
    MOOSE_HARNESS_ROOT="$(git -C "$2" rev-parse --show-toplevel)" || \
        moose_mac_die "Invalid target checkout: $2"
    export MOOSE_HARNESS_ROOT
    shift 2
fi

show_help()
{
    cat <<'EOF'
MOOSE-SCZM macOS CLI

Usage:
  ./scripts/mac/moose.sh [-C checkout] <command> [target|scope] [options...]

Commands:
  dummy                          Show complete dashboard: built opts, NEML2/Kokkos linkage,
                                 submodule guidance (with/without NEML2), pipeline progress,
                                 and next step suggestions
  status                         Alias for dummy
  doctor                         Inspect the environment, NEML2 hashes, and build state
  init [neml2-flag]              Install dependencies and configure when needed;
                                 builds repo-isolated NEML2 without modifying Conda
  compile [target] [-j N] [--raw] [--profile] [neml2-flag]
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
  compare <file1> <file2> [...]  Compare Exodus or CSV output
  logstat <log...> [--verbose]   Summarize solver convergence
  format [base] [--check]       Format changed C++ files with Conda clang-format v21
  baseline <ref> <scope> [...]  Build and test a detached worktree using this harness
  exodiff <file1> <file2> [...] Compare Exodus with the TestHarness default tolerances
  all [-j N] [--raw]             Init, compile all, and test all
  precheck                       Install this clone's pre-commit hook
  help [--agent]                 Show this message or a short agent reference

NEML2 Source Flags (supported on init, compile, verify, all, doctor, test, unit, run):
  --use-local-neml2      Force repo-isolated build of framework/contrib/neml2
                         in framework/contrib/neml2/build/installed (Conda untouched)
  --use-committed-neml2  Reset framework/contrib/neml2 to the MOOSE-committed SHA
                         and sync/use it
  --use-conda-neml2      Skip repo-isolated NEML2 and use Conda's installed neml2
  --skip-neml2-sync      Skip all NEML2 preflight checks and sync actions

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
  BUILD_JOBS        Parallel compile jobs (default: 3; scripts/mac/harness.conf)
  JOBS              Parallel test jobs (default: 3; scripts/mac/harness.conf)
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
                [[ "$2" =~ ^[1-9][0-9]*$ ]] || moose_mac_die "-j requires a positive integer."
                printf -v "$job_variable" '%s' "$2"
                export "$job_variable"
                shift 2
                ;;
            -j*)
                [[ "${1#-j}" =~ ^[1-9][0-9]*$ ]] || moose_mac_die "-j requires a positive integer."
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

    "$SCRIPT_DIR/build_mac.sh" "$target" \
        ${raw_flag[@]+"${raw_flag[@]}"} ${profile_flag[@]+"${profile_flag[@]}"}
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
    python3 "$SCRIPT_DIR/../harness/control.py" test --unit "${target}_unit" \
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

    if [[ $# -gt 0 && "$1" != -* ]]; then
        scope="$1"
        shift
    fi

    parse_jobs JOBS "$@"
    export BUILD_JOBS="$JOBS"

    local filtered_args=()
    for arg in "${REMAINING_ARGS[@]}"; do
        case "$arg" in
            --raw)
                raw_flag=(--raw)
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
        --failed)
            FILTERED_CLI_ARGS+=("--failed-tests")
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
    init|compile|build|test|unit|verify|all|run|format|baseline)
        moose_mac_guard "$COMMAND" "$COMMAND" "$@"
        ;;
esac

case "$COMMAND" in
    help|--help|-h)
        if [[ "${1:-}" == "--agent" ]]; then
            cat <<'EOF'
moose.sh [-C checkout] COMMAND: this harness also works on refs without scripts/mac.
doctor: inspect Conda, SDK, NEML2 and build state before building.
compile TARGET [-j N]: build only; jobs default to 3 in harness.conf.
test SCOPE [pattern] [--failed]: no build; use --failed to rerun failures.
Scopes: framework-kokkos, sczm, neml2, neml2-aoti, all, or a module.
unit [target] [filter]: run GoogleTest; zero selected tests is an error.
verify SCOPE: compile then test; a checkout lock excludes concurrent builds/runs.
RESULT reports counts, status and exit; ERROR also covers missing/empty results.
format [base] [--check]: Conda clang-format v21, repository style, changed files only.
baseline REF SCOPE; exodiff A B; compare A B --cmp SPEC; logstat LOG --verbose.
EOF
        else
            show_help
        fi
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
    format)
        moose_mac_activate
        moose_mac_locate_repo
        exec python3 "$SCRIPT_DIR/../harness/control.py" format "$MOOSE_DIR" "$@"
        ;;
    baseline)
        [[ $# -ge 2 ]] || moose_mac_die "Usage: moose.sh baseline <ref> <scope> [verify options...]"
        moose_mac_activate
        moose_mac_locate_repo
        moose_mac_env_exports
        if [[ "$(moose_mac_effective_neml2_backend)" == "local" ]]; then
            export MOOSE_NEML2_SITE="$(moose_mac_local_neml2_site)"
            [[ -d "$MOOSE_NEML2_SITE/neml2" ]] || moose_mac_die "Build/install NEML2 in the main checkout first."
            export MOOSE_NEML2_SRC=local
        else
            export MOOSE_NEML2_SRC=conda
        fi
        exec python3 "$SCRIPT_DIR/../harness/baseline.py" "$MOOSE_DIR" "$SCRIPT_DIR/moose.sh" "$@"
        ;;
    exodiff)
        [[ $# -ge 2 ]] || moose_mac_die "Usage: moose.sh exodiff <file1> <file2> [exodiff options...]"
        moose_mac_activate
        moose_mac_locate_repo
        moose_mac_env_exports
        exec python3 "$SCRIPT_DIR/../harness/cli.py" exodiff "$@"
        ;;
    compare|logstat)
        moose_mac_activate
        exec python3 "$SCRIPT_DIR/../harness/cli.py" "$COMMAND" "$@"
        ;;
    *)
        moose_mac_die "Unknown command '$COMMAND'. Run './scripts/mac/moose.sh --help'."
        ;;
esac
