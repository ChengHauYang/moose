#!/usr/bin/env bash
# Shared functions for scripts/cuda/moose_cuda.sh.

CUDA_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
MOOSE_DIR="$(cd "$CUDA_SCRIPT_DIR/../.." && pwd -P)"
STACK_DIR="$MOOSE_DIR/kokkos-cuda-stack"
PREFIX="${MOOSE_CUDA_PREFIX:-$STACK_DIR/prefix}"
LOGS="$STACK_DIR/logs"
CUDA_DIR="${CUDA_DIR:-/usr/local/cuda}"
MINIFORGE_BIN="${MINIFORGE_BIN:-$HOME/miniforge/bin}"
OpenBLAS_HOME="${OpenBLAS_HOME:-$HOME/miniforge/envs/moose}"
NEML2_VENV="$STACK_DIR/neml2-venv"
LEGACY="$STACK_DIR/scripts"
METHOD="${METHOD:-opt}"
MOOSE_JOBS="${MOOSE_JOBS:-8}"
TEST_JOBS="${TEST_JOBS:-1}"
NEML2_SUPPORT="${NEML2_SUPPORT:-1}"
TORCH_CUDA_ARCH_LIST="${TORCH_CUDA_ARCH_LIST:-8.6}"
CUDA_ARCH="${CUDA_ARCH:-86}"
OMPI_CC="${OMPI_CC:-/usr/bin/gcc-11}"
OMPI_CXX="${OMPI_CXX:-/usr/bin/g++-11}"
OMPI_FC="${OMPI_FC:-/usr/bin/gfortran-9}"
MOOSE_CUDA_STATE="$STACK_DIR/.cuda-cli"

cuda_die() { printf '[cuda] ERROR: %s\n' "$*" >&2; exit 1; }
cuda_note() { printf '[cuda] %s\n' "$*"; }
cuda_good_number() { [[ "$1" =~ ^[1-9][0-9]*$ ]]; }
cuda_require_jobs() { cuda_good_number "$1" || cuda_die "Expected positive job count, got: $1"; }
cuda_state() { python3 "$CUDA_SCRIPT_DIR/moose_cuda_state.py" "$@"; }

cuda_export_settings() {
  export MOOSE_DIR STACK_DIR PREFIX LOGS CUDA_DIR MINIFORGE_BIN
  export OpenBLAS_HOME MOOSE_JOBS NEML2_SUPPORT METHOD
  export TORCH_CUDA_ARCH_LIST CUDA_ARCH OMPI_CC OMPI_CXX OMPI_FC
  export PETSC_DIR="$PREFIX" PETSC_ARCH="" LIBMESH_DIR="$PREFIX" WASP_DIR="$PREFIX"
  export OPAL_PREFIX="$PREFIX" CUDA_HOME="$CUDA_DIR"
}

cuda_runtime_env() {
  cuda_export_settings
  local p="$PREFIX/bin"
  [[ -x "$NEML2_VENV/bin/python3" ]] && { p="$p:$NEML2_VENV/bin"; export VIRTUAL_ENV="$NEML2_VENV"; }
  [[ -d "$MINIFORGE_BIN" ]] && p="$p:$MINIFORGE_BIN"
  export PATH="$p:$CUDA_DIR/bin:/usr/bin:/bin:${PATH:-}"
  export LD_LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib64:${LD_LIBRARY_PATH:-}"
}

cuda_build_env() {
  cuda_export_settings
  # The CLI is a child process: purge Conda build hints here as well as in
  # dependency installers, without changing the caller's interactive shell.
  source "$LEGACY/env.sh"
  cuda_runtime_env
  export LD_LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib64"
}

cuda_mklog() {
  mkdir -p "$LOGS" "$MOOSE_CUDA_STATE"
  LOGFILE="$LOGS/cli-${1}-$(date +%Y%m%d-%H%M%S)-$$.log"
}

cuda_logged() {
  local description="$1" rc
  local codes=()
  shift
  cuda_note "$description"
  cuda_note "log: $LOGFILE"
  printf '[command] ' >> "$LOGFILE"
  printf '%q ' "$@" >> "$LOGFILE"
  printf '\n' >> "$LOGFILE"
  if "$@" 2>&1 | tee -a "$LOGFILE"; then
    cuda_note "OK: $description"
  else
    codes=("${PIPESTATUS[@]}")
    rc="${codes[0]}"
    [[ "$rc" != 0 ]] || rc="${codes[1]}"
    printf '[cuda] FAILED (%s): %s\n' "$rc" "$description" >&2
    printf '[cuda] log: %s\n' "$LOGFILE" >&2
    printf '[cuda] retry: ' >&2
    printf '%q ' "$@" >&2
    printf '\n' >&2
    return "$rc"
  fi
}

cuda_target_dir() {
  case "$1" in
    framework) printf '%s\n' "$MOOSE_DIR/framework" ;;
    test) printf '%s\n' "$MOOSE_DIR/test" ;;
    solid_mechanics|shifted_boundary_method)
      printf '%s\n' "$MOOSE_DIR/modules/$1" ;;
    *)
      [[ "$1" =~ ^[a-zA-Z][a-zA-Z0-9_]*$ ]] || return 1
      [[ -d "$MOOSE_DIR/modules/$1" ]] || return 1
      printf '%s\n' "$MOOSE_DIR/modules/$1" ;;
  esac
}

cuda_executable() {
  local scope="$1" dir
  case "$scope" in
    framework|test) printf '%s\n' "$MOOSE_DIR/test/moose_test-${METHOD}" ;;
    *) dir="$(cuda_target_dir "$scope")" || return 1
       printf '%s\n' "$dir/${scope}-${METHOD}" ;;
  esac
}

cuda_status_line() {
  local label="$1" present="$2" hint="$3"
  if [[ "$present" == 1 ]]; then printf '  [OK]   %-22s %s\n' "$label" "$hint"
  else printf '  [MISS] %-22s %s\n' "$label" "$hint"; fi
}

cuda_caps() {
  local exe="$1" require_neml2="${2:-$NEML2_SUPPORT}" outfile
  [[ -x "$exe" ]] || { printf '  Missing executable: %s\n' "$exe" >&2; return 1; }
  # Some MOOSE builds print a heading/footer around the JSON capabilities.
  # Extract the outer JSON object instead of relying on fixed line offsets.
  outfile="$(mktemp)" || return 1
  if ! "$exe" --show-capabilities >"$outfile" 2>&1; then
    cat "$outfile" >&2; rm -f "$outfile"; return 1
  fi
  if ! python3 - "$outfile" "$require_neml2" <<'PY'
import json, sys
s=open(sys.argv[1],encoding='utf-8',errors='replace').read()
start=s.find('{')
try: d,_=json.JSONDecoder().raw_decode(s[start:])
except (ValueError, TypeError):
    print('  Cannot parse --show-capabilities JSON',file=sys.stderr);sys.exit(1)
def val(k):
    v=d.get(k,{})
    return v.get('value') if isinstance(v,dict) else v
def ok(k):
    v=val(k)
    return v is not None and v is not False and str(v).strip().lower() not in ('','false','none','0','off')
for k in ('kokkos','cuda','neml2'): print('  %-8s = %s' % (k,val(k)))
if not (ok('kokkos') and ok('cuda') and (sys.argv[2]=='0' or ok('neml2'))):
    print('  Capability validation failed',file=sys.stderr);sys.exit(1)
PY
  then rm -f "$outfile"; return 1; fi
  rm -f "$outfile"
}

cuda_configure_flags() {
  if [[ -x "$MOOSE_DIR/config.status" ]]; then
    "$MOOSE_DIR/config.status" --config 2>/dev/null || true
  elif [[ -f "$MOOSE_DIR/config.log" ]]; then
    grep -m1 ' \$ ./configure ' "$MOOSE_DIR/config.log" || true
  fi
}

cuda_configuration_matches() {
  [[ -f "$MOOSE_DIR/conf_vars.mk" && -f "$MOOSE_DIR/config.status" ]] || return 1
  local opts
  opts="$(cuda_configure_flags)"
  [[ "$opts" == *'--with-kokkos=cuda'* ]] || return 1
  grep -Eq '^ENABLE_KOKKOS[[:space:]]*:=[[:space:]]*true$' "$MOOSE_DIR/conf_vars.mk" || return 1
  grep -Eq '^ENABLE_KOKKOS_GPU[[:space:]]*:=[[:space:]]*true$' "$MOOSE_DIR/conf_vars.mk" || return 1
  if [[ "$NEML2_SUPPORT" == 1 ]]; then
    [[ "$opts" == *'--with-neml2'* && "$opts" == *'--with-libtorch'* ]] || return 1
    grep -Eq '^ENABLE_NEML2[[:space:]]*:=[[:space:]]*true$' "$MOOSE_DIR/conf_vars.mk" || return 1
    grep -Eq '^ENABLE_LIBTORCH[[:space:]]*:=[[:space:]]*true$' "$MOOSE_DIR/conf_vars.mk" || return 1
    cuda_state check-paths || return 1
  else
    [[ "$opts" != *'--with-neml2'* ]] || return 1
  fi
}

cuda_configured() {
  cuda_configuration_matches && cuda_state check-config
}

cuda_configure() {
  local old="" arguments option
  local args=()
  old="$(cuda_configure_flags)"
  if cuda_configured; then
    cuda_note 'MOOSE configuration and dependency identities match; skipping configure.'
    return 0
  fi
  if ! cuda_configuration_matches; then
    arguments="$(cuda_state configure-args)" || return 1
    while IFS= read -r option; do args+=("$option"); done <<< "$arguments"
    cuda_note "Previous configuration: ${old:-'(none)'}"
    cuda_logged 'configure MOOSE with installed library paths' bash -c 'cd "$1"; shift; exec ./configure "$@"' _ "$MOOSE_DIR" "${args[@]}" || return 1
    cuda_configuration_matches || cuda_die 'Generated configuration does not match the CUDA stack'
  fi
  cuda_state record-config
  cuda_note 'Recorded configuration identity; affected targets will be cleaned on their next compile.'
}

cuda_require_dependencies() {
  local dependency
  for dependency in openmpi petsc libmesh wasp neml2; do
    [[ "$dependency" != neml2 || "$NEML2_SUPPORT" == 1 ]] || continue
    cuda_dep_ok "$dependency" && cuda_state check-dependency "$dependency" ||
      cuda_die "$dependency installation or dependency identity changed; run init first"
  done
}

cuda_make_target() {
  local target="$1" directory="$2" jobs="$3"
  if ! cuda_state check-target "$target"; then
    cuda_logged "clean $target for current dependency identity" make -C "$directory" clean
  fi
  rm -f "$MOOSE_CUDA_STATE/target-$target-$METHOD.json"
  cuda_logged "incremental make $target" make -C "$directory" -j "$jobs"
}

cuda_dep_ok() {
  case "$1" in
    openmpi) [[ -x "$PREFIX/bin/mpicc" && -x "$PREFIX/bin/mpicxx" && -x "$PREFIX/bin/mpif90" && -x "$PREFIX/bin/ompi_info" ]] && "$PREFIX/bin/ompi_info" 2>/dev/null | grep -E 'opal_built_with_cuda_support:true|MPI extensions:.*cuda' >/dev/null ;;
    petsc) [[ -f "$PREFIX/lib/libpetsc.so" && -f "$PREFIX/include/petscconf.h" ]] && grep -Eq '^[[:space:]]*#define PETSC_HAVE_CUDA[[:space:]]+1' "$PREFIX/include/petscconf.h" && grep -Eq '^[[:space:]]*#define PETSC_HAVE_KOKKOS[[:space:]]+1' "$PREFIX/include/petscconf.h" ;;
    libmesh) [[ -x "$PREFIX/bin/libmesh-config" && -f "$PREFIX/lib/libmesh_${METHOD}.so" ]] ;;
    wasp) [[ -f "$PREFIX/lib/libwaspcore.so" || -L "$PREFIX/lib/libwaspcore.so" ]] ;;
    neml2) [[ -x "$NEML2_VENV/bin/python3" ]] && "$NEML2_VENV/bin/python3" -c 'import neml2, torch; assert torch.version.cuda' >/dev/null 2>&1 ;;
    *) return 1 ;;
  esac
}
