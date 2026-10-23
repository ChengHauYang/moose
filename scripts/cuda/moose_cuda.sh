#!/usr/bin/env bash
set -Eeuo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=moose_cuda_common.sh
source "$SCRIPT_DIR/moose_cuda_common.sh"

help() {
cat <<'USAGE'
MOOSE Kokkos CUDA CLI

Usage: ./scripts/cuda/moose_cuda.sh <command> [args] [options]

  doctor                              Check compilers, CUDA, MPI, Python and paths
  status                              Report dependency/install/configure/build state
  init [--no-neml2] [-j N]            Initialize missing dependencies and configure MOOSE
  compile [target] [-j N]             Incremental MOOSE build, default solid_mechanics
  compile neml2                       Rebuild NEML2 in its existing CUDA venv
  run <target> <input.i> [args...]     Run with --compute-device=cuda unless overridden
  test <scope> [regex] [-j N] [args]   Run existing TestHarness, default 1 job
  verify <scope> [regex] [-j N]        Compile required executable and test
  benchmark                           Run profiler / benchmark explicitly
  all [-j N] [--no-neml2]             init -> compile -> test (no benchmark)

Targets: framework, test, solid_mechanics, shifted_boundary_method, other installed modules
Scope aliases: framework -> test/moose_test-opt

Global flags for init/compile/test/verify/all:
  -j N, -jN             Parallel build jobs; for test, TestHarness jobs
  --no-neml2            Only on init/all; configure without NEML2
  --force               Only on init; force dependency rebuild (CAUTION: slow)
  --compute-device=cpu  For run/test/verify; override CUDA default
  --                    Forward all following arguments unmodified

Environment overrides: MOOSE_JOBS, TEST_JOBS, NEML2_SUPPORT, CUDA_DIR,
  MINIFORGE_BIN, OpenBLAS_HOME, CUDA_ARCH, TORCH_CUDA_ARCH_LIST,
  OMPI_CC, OMPI_CXX, OMPI_FC, MOOSE_CUDA_PREFIX, METHOD=opt.

This CLI does not upload code, change Git branches, or reset dirty submodules.
USAGE
}

# Parse CLI job and driver controls, leaving positional args in REST.
parse_options() {
  local command="$1"
  shift
  REST=() BUILD_OVERRIDE='' FORCE_REBUILD=0
  while (($#)); do
    case "$1" in
      -j) (($#>=2)) || cuda_die '-j requires a number'; BUILD_OVERRIDE="$2"; shift 2 ;;
      -j*) BUILD_OVERRIDE="${1#-j}"; shift ;;
      --no-neml2)
        [[ "$command" == init || "$command" == all ]] || cuda_die '--no-neml2 is only supported by init/all; set NEML2_SUPPORT=0 for subsequent commands'
        NEML2_SUPPORT=0; shift ;;
      --force)
        [[ "$command" == init ]] || cuda_die '--force is only supported by init'
        FORCE_REBUILD=1; shift ;;
      --) shift; REST+=("$@"); break ;;
      *) REST+=("$1"); shift ;;
    esac
  done
  [[ -z "$BUILD_OVERRIDE" ]] || cuda_require_jobs "$BUILD_OVERRIDE"
  export FORCE_REBUILD
}

require_stack() {
  [[ -d "$STACK_DIR" ]] || cuda_die "Missing $STACK_DIR"
  [[ -f "$LEGACY/env.sh" ]] || cuda_die "Missing CUDA stack scripts in $LEGACY"
}

status() {
  require_stack
  cuda_runtime_env
  cuda_note "Repository : $MOOSE_DIR"
  cuda_note "Stack      : $STACK_DIR"
  cuda_note "Prefix     : $PREFIX"
  cuda_note "NEML2      : $NEML2_SUPPORT"
  cuda_note "CUDA       : $CUDA_DIR"
  cuda_note "Compiler   : $OMPI_CC, $OMPI_CXX, $OMPI_FC"
  local step
  for step in openmpi petsc libmesh wasp neml2; do
    [[ "$step" != neml2 || "$NEML2_SUPPORT" == 1 ]] || continue
    if cuda_dep_ok "$step"; then
      if cuda_state check-dependency "$step"; then cuda_status_line "$step" 1 'recorded identity matches'
      else cuda_status_line "$step" 0 'present but unrecorded/stale; run init'; fi
    else cuda_status_line "$step" 0 'not verified'; fi
  done
  if cuda_configured; then cuda_status_line 'MOOSE configure' 1 'CUDA flags match'
  else cuda_status_line 'MOOSE configure' 0 'run init'; fi
  for step in test solid_mechanics shifted_boundary_method; do
    local exe
    exe="$(cuda_executable "$step")"
    if [[ -x "$exe" ]]; then cuda_status_line "$step binary" 1 "$exe"
    else cuda_status_line "$step binary" 0 'not built'; fi
  done
  cuda_note 'Next: init (missing deps), compile solid_mechanics (source updates), verify solid_mechanics.'
}

doctor() {
  require_stack
  cuda_runtime_env
  local bad=0 x cmd
  cuda_note 'MOOSE CUDA environment checks (read-only)'
  for cmd in bash git make python3 cmake nvcc nvidia-smi; do
    if [[ "$cmd" == nvcc ]]; then x="$CUDA_DIR/bin/nvcc"
    elif [[ "$cmd" == nvidia-smi ]]; then x="$(command -v nvidia-smi || true)"
    else x="$(command -v "$cmd" || true)"; fi
    if [[ -n "$x" && -x "$x" ]]; then cuda_status_line "$cmd" 1 "$x"
    else cuda_status_line "$cmd" 0 'not found'; bad=1; fi
  done
  for x in "$OMPI_CC" "$OMPI_CXX" "$OMPI_FC" "${MINIFORGE_PYTHON:-$MINIFORGE_BIN/python3}"; do
    if [[ -x "$x" ]]; then cuda_status_line "$(basename "$x")" 1 "$x"
    else cuda_status_line "$(basename "$x")" 0 "$x"; bad=1; fi
  done
  for x in "$PREFIX/bin/mpicc" "$PREFIX/bin/mpicxx" "$PREFIX/bin/mpif90"; do
    if [[ -x "$x" ]]; then cuda_status_line "MPI $(basename "$x")" 1 "$x"
    else cuda_status_line "MPI $(basename "$x")" 0 'run init'; fi
  done
  if [[ -x "$NEML2_VENV/bin/python3" ]]; then
    "$NEML2_VENV/bin/python3" --version || true
  fi
  if [[ -x "$PREFIX/bin/ompi_info" ]]; then
    "$PREFIX/bin/ompi_info" 2>/dev/null | grep -Ei 'opal_built_with_cuda_support|MPI extensions' | head -4 || true
  fi
  if [[ "$bad" == 0 ]]; then cuda_note 'Basic checks passed (not a GPU execution test).'
  else cuda_note 'Missing prerequisites: fix above before init.'; return 1; fi
}

init_submodules() {
  # Never overwrite a locally modified NEML2 checkout. MOOSE's gitmodules
  # sets update=none for NEML2 and PyTorch.
  local sub extra=(petsc libmesh framework/contrib/wasp)
  if [[ "$NEML2_SUPPORT" == 1 ]]; then extra+=(framework/contrib/neml2 framework/contrib/pytorch); fi
  for sub in "${extra[@]}"; do
    if [[ -d "$MOOSE_DIR/$sub" ]] && [[ -n "$(git -C "$MOOSE_DIR" submodule status -- "$sub" 2>/dev/null | grep -v '^-' || true)" ]]; then
      cuda_note "Submodule present: $sub (leaving checkout untouched)"
    else
      cuda_logged "initialize $sub" git -C "$MOOSE_DIR" -c "submodule.$sub.update=checkout" submodule update --init --recursive -- "$sub"
    fi
  done
}

init() {
  require_stack
  cuda_require_jobs "$MOOSE_JOBS"
  cuda_build_env
  cuda_state check-toolchain
  cuda_mklog init
  init_submodules
  local step script
  for step in openmpi petsc libmesh wasp neml2; do
    [[ "$step" != neml2 || "$NEML2_SUPPORT" == 1 ]] || continue
    script="$LEGACY/build_${step}.sh"
    [[ -f "$script" ]] || cuda_die "Missing dependency installer: $script"
    if [[ "$FORCE_REBUILD" != 1 ]] && cuda_dep_ok "$step" &&
       { [[ ! -f "$MOOSE_CUDA_STATE/dependency-$step.json" ]] || cuda_state check-dependency "$step"; }; then
      if [[ ! -f "$MOOSE_CUDA_STATE/dependency-$step.json" ]]; then
        cuda_note "Adopting existing $step installation as baseline; original source provenance is unverified."
        cuda_state record-dependency "$step" adopted
      else cuda_note "reusing installed $step with matching dependency identity"; fi
    else
      # A rebuilt upstream installation invalidates every later installer.
      export FORCE_REBUILD=1
      cuda_logged "build $step" bash "$script"
      cuda_dep_ok "$step" || cuda_die "$step installer finished but dependency validation failed"
      cuda_state record-dependency "$step"
    fi
  done
  cuda_configure
  cuda_note 'Initialization complete. Run compile solid_mechanics.'
}

compile_neml2() {
  [[ "$NEML2_SUPPORT" == 1 ]] || cuda_die 'NEML2 disabled; omit --no-neml2 and rerun init'
  cuda_dep_ok openmpi || cuda_die 'CUDA MPI not installed; run init'
  cuda_build_env
  local dependency
  for dependency in openmpi petsc libmesh wasp; do
    cuda_dep_ok "$dependency" && cuda_state check-dependency "$dependency" || cuda_die "Run init before rebuilding NEML2: $dependency changed"
  done
  cuda_mklog neml2
  # Legacy NEML2 installer reuses matching torch, but import-only skip is wrong
  # for source edits: force NEML2 reinstall. Existing build_neml2.sh also
  # checks the pinned torch ABI/CUDA headers before its NEML2 step.
  export FORCE_REBUILD=1
  cuda_logged 'rebuild NEML2 from current checkout' bash "$LEGACY/build_neml2.sh"
  cuda_dep_ok neml2 || cuda_die 'NEML2 import check failed'
  cuda_state record-dependency neml2
  cuda_configure
  cuda_note 'NEML2 installed; compile solid_mechanics to relink MOOSE.'
}

compile() {
  local target="${1:-solid_mechanics}" jobs="${2:-$MOOSE_JOBS}" dir
  cuda_require_jobs "$jobs"
  export MOOSE_JOBS="$jobs"
  [[ "$target" != all ]] || {
    compile test "$jobs"
    compile solid_mechanics "$jobs"
    return
  }
  [[ "$target" != neml2 ]] || { compile_neml2; return; }
  cuda_build_env
  cuda_require_dependencies
  dir="$(cuda_target_dir "$target")" || cuda_die "Unknown compile target: $target"
  [[ -d "$dir" ]] || cuda_die "Missing build directory: $dir"
  cuda_configured || cuda_die 'MOOSE is not configured for selected CUDA/NEML2 mode; run init first.'
  cuda_dep_ok libmesh || cuda_die 'libMesh missing; run init'
  cuda_mklog "compile-$target"
  if [[ "$target" != framework ]]; then
    cuda_make_target framework "$MOOSE_DIR/framework" "$jobs"
    cuda_state record-target framework
  fi
  cuda_make_target "$target" "$dir" "$jobs"
  local exe
  exe="$(cuda_executable "$target")"
  if [[ "$target" == framework ]]; then
    cuda_note 'Framework library built; compile test or a module to validate executable capabilities.'
  elif [[ -x "$exe" ]]; then
    cuda_note "Executable: $exe"
    cuda_caps "$exe" || cuda_die 'Build succeeded but CUDA/NEML2 capabilities do not match.'
  elif [[ "$target" != framework ]]; then
    cuda_die "Build finished but expected executable is missing: $exe"
  fi
  cuda_state record-target "$target"
}

run_target() {
  (($# >= 2)) || cuda_die 'Usage: run <target> <input.i> [MOOSE arguments]'
  local target="$1" input="$2" exe="$3"; shift 3
  [[ -f "$input" ]] || cuda_die "Input does not exist: $input"
  [[ -x "$exe" ]] || cuda_die "Executable missing: $exe (compile $target first)"
  cuda_runtime_env
  cuda_require_dependencies
  cuda_configured && cuda_state check-target framework && cuda_state check-target "${target/framework/test}" || cuda_die "Build identity changed; compile $target first"
  cuda_caps "$exe" || cuda_die 'Executable CUDA/NEML2 capabilities do not match.'
  local has_device=0 arg
  for arg in "$@"; do [[ "$arg" == --compute-device || "$arg" == --compute-device=* ]] && has_device=1; done
  ((has_device)) || set -- --compute-device=cuda "$@"
  cuda_note "Running $target with input $input"
  exec "$exe" -i "$input" "$@"
}

test_scope() {
  local scope="$1" regex="$2" jobs="$3"; shift 3
  cuda_require_jobs "$jobs"
  local dir exe arg has_device=0
  if [[ "$scope" == framework ]]; then scope=test; fi
  dir="$(cuda_target_dir "$scope")" || cuda_die "Unknown test scope: $scope"
  exe="$(cuda_executable "$scope")"
  [[ -x "$exe" ]] || cuda_die "Missing $exe. First run: compile $scope"
  [[ -f "$dir/run_tests" ]] || cuda_die "Missing $dir/run_tests"
  cuda_runtime_env
  cuda_require_dependencies
  cuda_configured && cuda_state check-target framework && cuda_state check-target "$scope" || cuda_die "Build identity changed; compile $scope first"
  cuda_caps "$exe" || cuda_die 'Executable CUDA/NEML2 capabilities do not match.'
  cuda_mklog "test-$scope"
  # TestHarness counts slots across MPI ranks and threads, even for a single
  # test. Raise the conservative default to fit explicitly requested resources.
  local ranks=1 threads=1 next='' value
  for arg in "$@"; do
    value=''
    if [[ -n "$next" ]]; then value="$arg"
    else
      case "$arg" in
        -p|--parallel) next=ranks; continue ;;
        -p*) next=ranks; value="${arg#-p}" ;;
        --parallel=*) next=ranks; value="${arg#*=}" ;;
        --n-threads) next=threads; continue ;;
        --n-threads=*) next=threads; value="${arg#*=}" ;;
        *) continue ;;
      esac
    fi
    cuda_require_jobs "$value"
    printf -v "$next" '%s' "$value"
    next=''
  done
  [[ -z "$next" ]] || cuda_die 'Missing MPI rank or thread count'
  if ((jobs < ranks * threads)); then
    jobs=$((ranks * threads))
    cuda_note "Using $jobs TestHarness slots for $ranks MPI ranks x $threads threads"
  fi
  local args=(-j "$jobs")
  [[ -n "$regex" ]] && args+=(--re "$regex")
  for arg in "$@"; do
    [[ "$arg" == --compute-device || "$arg" == --compute-device=* ]] && has_device=1
  done
  ((has_device)) || args+=(--compute-device=cuda)
  args+=("$@")
  cuda_logged "TestHarness $scope ${regex:-all}" bash -c 'cd "$1"; shift; exec ./run_tests "$@"' _ "$dir" "${args[@]}"
}

# Commands accept -j anywhere. Positional test regex is optional; after the
# first two tokens remaining flags go straight to TestHarness.
main() {
  local cmd="${1:-help}"; (($#==0)) || shift
  [[ "$NEML2_SUPPORT" == 0 || "$NEML2_SUPPORT" == 1 ]] || cuda_die 'NEML2_SUPPORT must be 0 or 1'
  case "$METHOD" in opt|oprof|devel|dbg) ;; *) cuda_die "Unknown build method: $METHOD" ;; esac
  case "$cmd" in
    init|compile|build|run|test|verify|benchmark|all)
      [[ "$(uname -s)" == Linux ]] || cuda_die 'CUDA builds and runs require Linux; doctor/status/help can be used on this host.' ;;
  esac
  case "$cmd" in
    -h|--help|help) help ;;
    doctor|status)
      (($#==0)) || cuda_die "$cmd does not accept arguments"
      "$cmd" ;;
    init)
      parse_options "$cmd" "$@"
      ((${#REST[@]}==0)) || cuda_die "Unknown init arguments: ${REST[*]}"
      [[ -z "$BUILD_OVERRIDE" ]] || MOOSE_JOBS="$BUILD_OVERRIDE"
      init ;;
    compile|build)
      parse_options "$cmd" "$@"
      ((${#REST[@]}<=1)) || cuda_die 'compile accepts only one target'
      local target="${REST[0]:-solid_mechanics}"
      compile "$target" "${BUILD_OVERRIDE:-$MOOSE_JOBS}" ;;
    run)
      (($#>=2)) || cuda_die 'Usage: run <target> <input.i> [MOOSE options]'
      local target="$1" input="$2" exe
      shift 2
      exe="$(cuda_executable "$target")" || cuda_die "Unknown run target: $target"
      run_target "$target" "$input" "$exe" "$@" ;;
    test|verify)
      parse_options "$cmd" "$@"
      local scope=solid_mechanics regex='' test_jobs="${BUILD_OVERRIDE:-$TEST_JOBS}"
      local extra=()
      if ((${#REST[@]})); then
        if [[ "${REST[0]}" != -* ]]; then scope="${REST[0]}"; REST=("${REST[@]:1}"); fi
      fi
      if ((${#REST[@]})); then
        if [[ "${REST[0]}" != -* ]]; then regex="${REST[0]}"; extra=("${REST[@]:1}")
        else extra=("${REST[@]}"); fi
      fi
      if [[ "$cmd" == verify ]]; then
        local target="$scope"
        [[ "$scope" != framework ]] || target=test
        compile "$target" "${BUILD_OVERRIDE:-$MOOSE_JOBS}"
        # Verify defaults to conservative 1 TestHarness worker even if compile -j 8.
        test_jobs="${TEST_JOBS}"
      fi
      test_scope "$scope" "$regex" "$test_jobs" ${extra[@]+"${extra[@]}"} ;;
    benchmark)
      (($#==0)) || cuda_die 'benchmark has no arguments (use legacy run_benchmark.sh to customize)'
      cuda_runtime_env
      cuda_require_dependencies
      cuda_configured && cuda_state check-target framework && cuda_state check-target solid_mechanics || cuda_die 'Build identity changed; compile solid_mechanics first'
      cuda_mklog benchmark
      cuda_logged 'CUDA smoke profiling and benchmark' bash "$LEGACY/run_benchmark.sh" ;;
    all)
      parse_options "$cmd" "$@"
      ((${#REST[@]}==0)) || cuda_die 'all does not accept positional arguments'
      [[ -z "$BUILD_OVERRIDE" ]] || MOOSE_JOBS="$BUILD_OVERRIDE"
      init
      compile solid_mechanics "$MOOSE_JOBS"
      test_scope solid_mechanics '' "$TEST_JOBS" ;;
    *) cuda_die "Unknown command: $cmd (try --help)" ;;
  esac
}
main "$@"
