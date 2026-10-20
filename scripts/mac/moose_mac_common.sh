#!/usr/bin/env bash

# Common helpers for MOOSE + NEML2 + Kokkos on Apple Silicon macOS.
# This file is meant to be sourced by other scripts.

moose_mac_section()
{
    echo
    echo "============================================"
    echo " $*"
    echo "============================================"
}

moose_mac_die()
{
    echo
    echo "ERROR: $*" >&2
    exit 1
}

moose_mac_activate()
{
    # Do not require nounset during Conda activation.
    # Conda compiler hooks may reference unset backup variables.
    set +u

    source "$HOME/miniforge/etc/profile.d/conda.sh"
    conda activate "${MOOSE_CONDA_ENV:-moose}"

    set -u
}

moose_mac_locate_repo()
{
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"

    MOOSE_DIR="$(git -C "$script_dir" rev-parse --show-toplevel 2>/dev/null || true)"

    if [[ -z "${MOOSE_DIR:-}" ]]; then
        MOOSE_DIR="$(git -C "$PWD" rev-parse --show-toplevel 2>/dev/null || true)"
    fi

    if [[ -z "${MOOSE_DIR:-}" || ! -f "$MOOSE_DIR/configure" || ! -d "$MOOSE_DIR/framework" ]]; then
        moose_mac_die "Could not locate MOOSE repository root. Run from inside a MOOSE checkout or place scripts under <repo>/scripts."
    fi

    export MOOSE_DIR
}

moose_mac_neml2_mode()
{
    case "${MOOSE_NEML2_SRC:-auto}" in
        conda)
            echo "conda"
            ;;
        committed|commit)
            echo "committed"
            ;;
        local)
            echo "local"
            ;;
        auto|submodule|*)
            echo "auto"
            ;;
    esac
}

moose_mac_local_neml2_site()
{
    echo "$MOOSE_DIR/framework/contrib/neml2/build/installed"
}

moose_mac_local_neml2_dir()
{
    echo "$(moose_mac_local_neml2_site)/neml2"
}

moose_mac_neml2_submodule_dirty()
{
    [[ -n "$(git -C "$MOOSE_DIR/framework/contrib/neml2" status --porcelain -uno -- neml2 CMakeLists.txt cmake pyproject.toml 2>/dev/null)" ]]
}

moose_mac_neml2_worktree_state()
{
    local sub_hash dirty_sig
    sub_hash="$(moose_mac_neml2_submodule_hash)"
    [[ -n "$sub_hash" ]] || { echo ""; return; }
    if moose_mac_neml2_submodule_dirty; then
        dirty_sig="$(git -C "$MOOSE_DIR/framework/contrib/neml2" diff HEAD -- neml2 CMakeLists.txt cmake pyproject.toml 2>/dev/null | shasum -a 256 | awk '{print substr($1,1,10)}')"
        echo "${sub_hash}-dirty-${dirty_sig}"
    else
        echo "$sub_hash"
    fi
}

moose_mac_local_neml2_state()
{
    local local_dir
    local_dir="$(moose_mac_local_neml2_dir)"
    if [[ -f "$local_dir/.moose_sync_state" ]]; then
        tr -d '[:space:]' < "$local_dir/.moose_sync_state"
    elif [[ -f "$local_dir/hash" ]]; then
        tr -d '[:space:]' < "$local_dir/hash"
    else
        echo ""
    fi
}

moose_mac_effective_neml2_backend()
{
    local mode
    mode="$(moose_mac_neml2_mode)"
    if [[ "$mode" == "conda" ]]; then
        echo "conda"
        return 0
    fi
    if [[ "$mode" == "local" ]]; then
        echo "local"
        return 0
    fi

    local target_hash conda_hash local_state target_state conf_dir local_dir
    if [[ "$mode" == "committed" ]]; then
        target_hash="$(moose_mac_neml2_committed_hash)"
        target_state="$target_hash"
    else
        target_hash="$(moose_mac_neml2_submodule_hash)"
        target_state="$(moose_mac_neml2_worktree_state)"
    fi
    conda_hash="$(moose_mac_conda_neml2_hash)"
    local_state="$(moose_mac_local_neml2_state)"
    conf_dir="$(moose_mac_configured_neml2_dir)"
    local_dir="$(moose_mac_local_neml2_dir)"

    if ! moose_mac_neml2_submodule_dirty && [[ -n "$target_hash" ]]; then
        if [[ "$conf_dir" == "$local_dir" && "$local_state" == "$target_state" ]] && moose_mac_local_neml2_ok; then
            echo "local"
            return 0
        fi
        if [[ -n "$conda_hash" && "$target_hash" == "$conda_hash" ]] && moose_mac_conda_neml2_ok; then
            echo "conda"
            return 0
        fi
    fi

    echo "local"
}

moose_mac_default_build_jobs()
{
    # Pick a compile parallelism suited to Apple Silicon rather than a fixed
    # number. Bound by performance-core count (efficiency cores are slow for
    # heavy C++ template work and only add stragglers) and by RAM, because
    # MOOSE unity translation units that pull in libtorch/NEML2/Kokkos headers
    # can peak around 2-3 GB each. The RAM cap divides by 3 GB/job.
    local pcores mem_gb ram_cap jobs
    pcores="$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || true)"
    [[ -n "$pcores" && "$pcores" -gt 0 ]] || pcores="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    mem_gb=$(( $(sysctl -n hw.memsize 2>/dev/null || echo 8589934592) / 1073741824 ))
    ram_cap=$(( mem_gb / 3 ))
    (( ram_cap < 1 )) && ram_cap=1

    jobs="$pcores"
    (( jobs > ram_cap )) && jobs="$ram_cap"
    (( jobs < 2 )) && jobs=2
    echo "$jobs"
}

# Echo a usable ccache binary, or nothing when ccache is unavailable or not
# opted into. This is called from command substitutions, so it must not rely on
# exporting anything; cache sizing is set by moose_mac_ccache_prepare in the
# shell that runs the build.
#
# ccache is OPT-IN (MOOSE_MAC_CCACHE=on), not on-by-default, because it provides
# no benefit on this MOOSE/libMesh/WASP stack and measurably slows builds:
#   1. Direct mode is disabled on every compile ("Include file Snippet.bison
#      does not exist, disabling direct mode"): WASP's bison-generated headers
#      reference intermediate files that are absent from the installed tree, so
#      ccache cannot resolve the include set and falls back to preprocessor mode.
#      Direct mode is the only mode in which the time_macros sloppiness below
#      takes effect.
#   2. In preprocessor-mode fallback ccache hashes the fully preprocessed text,
#      which still contains libMesh's __TIME__ expansion (the compile clock time
#      baked into libmesh_common.h's report_error macro at ~150 sites per TU).
#      That changes every second, so even two identical back-to-back compiles
#      both miss (measured: 0 hits, 2 misses).
# Net effect with ccache on: ~1 s/TU of extra preprocessing (~10%) for a
# guaranteed miss. The plumbing is kept so the cache works immediately if the
# upstream direct-mode blocker is ever resolved, but it stays off unless asked.
moose_mac_ccache_bin()
{
    case "${MOOSE_MAC_CCACHE:-off}" in
        1|on|true|yes|enabled) ;;
        *) return 0 ;;
    esac

    local bin=""
    if [[ -n "${CCACHE_BIN:-}" && -x "${CCACHE_BIN:-}" ]]; then
        bin="$CCACHE_BIN"
    else
        bin="$(command -v ccache 2>/dev/null || true)"
    fi
    [[ -n "$bin" ]] || return 0
    echo "$bin"
}

# Prepare ccache environment in the current shell before a build. Must be called
# directly (not in $(...)), since it exports variables the build needs.
#
# CCACHE_SLOPPINESS must include time_macros: libMesh's libmesh_common.h bakes
# __DATE__/__TIME__ into its error-report macro, which is pulled into virtually
# every MOOSE translation unit. Without time_macros every object hashes
# differently on every build and ccache never hits. The only effect of ignoring
# them is that a cached object reports the date/time of its first compile in
# libMesh error messages, which does not affect behavior.
#
# CCACHE_MAXSIZE defaults generously: MOOSE unity objects are large and ccache's
# 5G default evicts them between branch switches, defeating the point.
moose_mac_ccache_prepare()
{
    [[ -n "$(moose_mac_ccache_bin)" ]] || return 0

    export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-25G}"

    if [[ -z "${CCACHE_SLOPPINESS:-}" ]]; then
        export CCACHE_SLOPPINESS="time_macros"
    elif [[ ",${CCACHE_SLOPPINESS}," != *",time_macros,"* ]]; then
        export CCACHE_SLOPPINESS="${CCACHE_SLOPPINESS},time_macros"
    fi
}

# Populate MOOSE_MAC_MAKE_CCACHE_ARGS with make command-line overrides that make
# the libmesh/MOOSE compile rules route through ccache while keeping the MPI
# wrapper. build.mk defines libmesh_CXX with ?= and libmesh_UNDERLYING_CXX with
# :=; command-line assignments win over both, so no Makefile edits are needed.
moose_mac_set_ccache_make_args()
{
    MOOSE_MAC_MAKE_CCACHE_ARGS=()

    local ccache
    ccache="$(moose_mac_ccache_bin)"
    [[ -n "$ccache" ]] || return 0

    moose_mac_ccache_prepare

    local cfg base_cxx base_cc underlying
    cfg="$LIBMESH_DIR/bin/libmesh-config"
    [[ -x "$cfg" ]] || cfg="$LIBMESH_DIR/contrib/bin/libmesh-config"
    [[ -x "$cfg" ]] || return 0

    base_cxx="$(METHOD="${METHOD:-opt}" "$cfg" --cxx 2>/dev/null || true)"
    base_cc="$(METHOD="${METHOD:-opt}" "$cfg" --cc 2>/dev/null || true)"
    [[ -n "$base_cxx" ]] || return 0

    # Resolve the real underlying compiler so pycapabilities (which uses
    # libmesh_UNDERLYING_CXX directly, not through ccache) keeps compiling.
    underlying="$( ("$base_cxx" -show 2>/dev/null || echo "$base_cxx") | awk '{print $1}')"

    MOOSE_MAC_MAKE_CCACHE_ARGS=(
        "libmesh_CXX=$ccache $base_cxx"
        "libmesh_CC=$ccache $base_cc"
    )
    [[ -n "$underlying" ]] && MOOSE_MAC_MAKE_CCACHE_ARGS+=("libmesh_UNDERLYING_CXX=$underlying")
}

# Sum of ccache hits (direct + preprocessed). Echoes an integer, 0 if no ccache.
moose_mac_ccache_hits()
{
    local bin; bin="$(moose_mac_ccache_bin)"; [[ -n "$bin" ]] || { echo 0; return 0; }
    "$bin" --print-stats 2>/dev/null | awk '
        $1=="direct_cache_hit" || $1=="preprocessed_cache_hit" { s += $2 }
        END { print s+0 }'
}

# ccache misses. Echoes an integer, 0 if no ccache.
moose_mac_ccache_misses()
{
    local bin; bin="$(moose_mac_ccache_bin)"; [[ -n "$bin" ]] || { echo 0; return 0; }
    "$bin" --print-stats 2>/dev/null | awk '$1=="cache_miss" { print $2+0; found=1 } END { if (!found) print 0 }'
}

moose_mac_env_exports()
{
    export PATH="$CONDA_PREFIX/bin:$PATH"
    export LIBMESH_DIR="$CONDA_PREFIX"
    export WASP_DIR="$CONDA_PREFIX"
    export METHOD="${METHOD:-opt}"
    export JOBS="${JOBS:-8}"
    export BUILD_JOBS="${BUILD_JOBS:-$(moose_mac_default_build_jobs)}"

    local local_site
    local_site="$(moose_mac_local_neml2_site)"

    if [[ "$(moose_mac_effective_neml2_backend)" == "conda" ]]; then
        if [[ -n "${PYTHONPATH:-}" ]]; then
            PYTHONPATH=":${PYTHONPATH}:"
            PYTHONPATH="${PYTHONPATH//:${local_site}:/:}"
            PYTHONPATH="${PYTHONPATH#:}"
            PYTHONPATH="${PYTHONPATH%:}"
            export PYTHONPATH
        fi
        PATH=":${PATH}:"
        PATH="${PATH//:${local_site}\/bin:/:}"
        PATH="${PATH#:}"
        PATH="${PATH%:}"
        export PATH
    else
        case ":${PYTHONPATH:-}:" in
            *":${local_site}:"*) ;;
            *) export PYTHONPATH="${local_site}${PYTHONPATH:+:$PYTHONPATH}" ;;
        esac
        case ":${PATH}:" in
            *":${local_site}/bin:"*) ;;
            *) export PATH="${local_site}/bin:$PATH" ;;
        esac
    fi
}

moose_mac_check_arch()
{
    local arch
    arch="$(uname -m)"

    if [[ "$arch" != "arm64" ]]; then
        moose_mac_die "This script expects Apple Silicon arm64. Detected: $arch"
    fi
}

moose_mac_check_sdk()
{
    if [[ -z "${SDKROOT:-}" || ! -d "${SDKROOT:-}" ]]; then
        moose_mac_die "MOOSE macOS SDK is not configured. Run: \"\$CONDA_PREFIX/share/moose-compilers/scripts/get_mac_sdk.sh\""
    fi
}

moose_mac_mpi_show_cmd()
{
    if "$1" --showme:command >/dev/null 2>&1; then
        "$1" --showme:command
    else
        "$1" -show
    fi
}

moose_mac_print_env()
{
    moose_mac_section "Environment"

    echo "MOOSE_DIR=${MOOSE_DIR:-}"
    echo "ARCH=$(uname -m)"
    echo "CONDA_PREFIX=${CONDA_PREFIX:-}"
    echo "SDKROOT=${SDKROOT:-}"
    echo "METHOD=${METHOD:-opt}"
    echo "BUILD_JOBS=${BUILD_JOBS:-5}"
    echo "JOBS=${JOBS:-8}"
    echo "NEML2_MODE=$(moose_mac_neml2_mode)"
    echo
    echo "CC=${CC:-}"
    echo "CXX=${CXX:-}"
    echo
    echo "python=$(command -v python || true)"
    echo "mpicc=$(command -v mpicc || true)"
    echo "mpicxx=$(command -v mpicxx || true)"
}

moose_mac_torch_dir()
{
    python - <<'PY'
import importlib.util
spec = importlib.util.find_spec("torch")
if spec and spec.submodule_search_locations:
    print(next(iter(spec.submodule_search_locations)))
PY
}

moose_mac_torch_ok()
{
    python -c 'import torch' >/dev/null 2>&1
}

moose_mac_ensure_torch()
{
    if ! python -c 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("torch") else 1)'; then
        moose_mac_section "Installing PyTorch"
        python -m pip install torch==2.13.0
    fi
}

moose_mac_dedupe_torch_libomp()
{
    local torch_dir torch_lib

    torch_dir="$(moose_mac_torch_dir || true)"
    torch_lib="$torch_dir/lib"

    if [[ -n "$torch_dir" && -f "$torch_lib/libomp.dylib" && ! -L "$torch_lib/libomp.dylib" ]]; then
        if [[ -f "$CONDA_PREFIX/lib/libomp.dylib" ]]; then
            moose_mac_section "De-duplicating PyTorch libomp.dylib"
            echo "torch libomp -> $CONDA_PREFIX/lib/libomp.dylib"
            rm -f "$torch_lib/libomp.dylib"
            ln -s "$CONDA_PREFIX/lib/libomp.dylib" "$torch_lib/libomp.dylib"
        fi
    fi
}

moose_mac_verify_torch()
{
    python - <<'PY'
import torch
print("Torch version :", torch.__version__)
print("Torch path    :", torch.__file__)
PY
}

moose_mac_local_neml2_ok()
{
    local local_dir local_site
    local_site="$(moose_mac_local_neml2_site)"
    local_dir="$(moose_mac_local_neml2_dir)"
    [[ -d "$local_dir" && -f "$local_dir/share/pkgconfig/neml2.pc" ]] || return 1
    PYTHONPATH="$local_site" python - "$local_dir" <<'PY' >/dev/null 2>&1
import neml2, os, sys
actual = os.path.realpath(os.path.dirname(neml2.__file__))
expected = os.path.realpath(sys.argv[1])
sys.exit(0 if actual == expected else 1)
PY
}

moose_mac_conda_neml2_ok()
{
    PYTHONPATH="" python -c 'import neml2' >/dev/null 2>&1
}

moose_mac_conda_neml2_dir()
{
    PYTHONPATH="" python - <<'PY' 2>/dev/null || true
try:
    import neml2, os
    print(os.path.dirname(neml2.__file__))
except Exception:
    print("")
PY
}

moose_mac_neml2_ok()
{
    if [[ "$(moose_mac_effective_neml2_backend)" == "conda" ]]; then
        moose_mac_conda_neml2_ok
    else
        moose_mac_local_neml2_ok
    fi
}

moose_mac_neml2_committed_hash()
{
    git -C "$MOOSE_DIR" ls-tree HEAD framework/contrib/neml2 2>/dev/null | awk '{print $3}'
}

moose_mac_neml2_submodule_hash()
{
    git -C "$MOOSE_DIR/framework/contrib/neml2" rev-parse HEAD 2>/dev/null || echo ""
}

moose_mac_neml2_submodule_branch()
{
    local br dirty=""
    br="$(git -C "$MOOSE_DIR/framework/contrib/neml2" symbolic-ref --short -q HEAD 2>/dev/null || echo "detached")"
    if moose_mac_neml2_submodule_dirty; then
        dirty=", dirty"
    fi
    echo "${br}${dirty}"
}

moose_mac_local_neml2_hash()
{
    local local_dir
    local_dir="$(moose_mac_local_neml2_dir)"
    if [[ -f "$local_dir/hash" ]]; then
        tr -d '[:space:]' < "$local_dir/hash"
    else
        echo ""
    fi
}

moose_mac_conda_neml2_hash()
{
    PYTHONPATH="" python - <<'PY' 2>/dev/null || true
try:
    import neml2, os
    hash_path = getattr(neml2, "hash_file", os.path.join(os.path.dirname(neml2.__file__), "hash"))
    if os.path.exists(hash_path):
        with open(hash_path) as f:
            print(f.read().strip())
    elif hasattr(neml2, "__git_hash__"):
        print(neml2.__git_hash__.strip())
    else:
        print("")
except Exception:
    print("")
PY
}

moose_mac_installed_neml2_hash()
{
    if [[ "$(moose_mac_effective_neml2_backend)" == "conda" ]]; then
        moose_mac_conda_neml2_hash
    else
        moose_mac_local_neml2_hash
    fi
}

moose_mac_active_neml2_hash()
{
    moose_mac_installed_neml2_hash
}

moose_mac_active_neml2_dir()
{
    if [[ "$(moose_mac_effective_neml2_backend)" == "conda" ]]; then
        moose_mac_conda_neml2_dir
    else
        moose_mac_local_neml2_dir
    fi
}

moose_mac_configured_neml2_dir()
{
    if [[ -f "$MOOSE_DIR/conf_vars.mk" ]]; then
        awk -F':=' '/^NEML2_DIR[[:space:]]*:=/ {gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' "$MOOSE_DIR/conf_vars.mk"
    fi
}

moose_mac_neml2_synced()
{
    local mode backend
    mode="$(moose_mac_neml2_mode)"
    backend="$(moose_mac_effective_neml2_backend)"

    if [[ "$backend" == "conda" ]]; then
        moose_mac_conda_neml2_ok
        return $?
    fi

    if ! moose_mac_local_neml2_ok; then
        return 1
    fi

    local target_state local_state
    if [[ "$mode" == "committed" ]]; then
        target_state="$(moose_mac_neml2_committed_hash)"
    else
        target_state="$(moose_mac_neml2_worktree_state)"
    fi
    local_state="$(moose_mac_local_neml2_state)"

    [[ -n "$target_state" && -n "$local_state" && "$target_state" == "$local_state" ]]
}

moose_mac_print_neml2_notices()
{
    local mode backend committed_hash sub_hash sub_branch local_hash conda_hash
    local committed_disp sub_disp local_disp conda_disp
    mode="$(moose_mac_neml2_mode)"
    backend="$(moose_mac_effective_neml2_backend)"
    committed_hash="$(moose_mac_neml2_committed_hash)"
    sub_hash="$(moose_mac_neml2_submodule_hash)"
    sub_branch="$(moose_mac_neml2_submodule_branch)"
    local_hash="$(moose_mac_local_neml2_hash)"
    conda_hash="$(moose_mac_conda_neml2_hash)"

    committed_disp="${committed_hash:0:10}"
    [[ -z "$committed_disp" ]] && committed_disp="unknown"
    if [[ -n "$sub_hash" ]]; then
        sub_disp="${sub_hash:0:10} ($sub_branch)"
    else
        sub_disp="uninitialized"
    fi
    local_disp="${local_hash:0:10}"
    [[ -z "$local_disp" ]] && local_disp="not built"
    conda_disp="${conda_hash:0:10}"
    [[ -z "$conda_disp" ]] && conda_disp="none"

    echo "NEML2 mode             : $mode -> active: $backend ($(moose_mac_active_neml2_dir))"
    echo "  MOOSE committed SHA  : $committed_disp"
    echo "  Submodule checkout   : $sub_disp"
    echo "  Repo-local installed : $local_disp"
    echo "  Conda env installed  : $conda_disp"

    if [[ -n "$sub_hash" && -n "$committed_hash" && "$sub_hash" != "$committed_hash" ]]; then
        echo "  [NOTICE] Repo checkout of framework/contrib/neml2 (${sub_hash:0:10}, $sub_branch) != MOOSE committed NEML2 (${committed_hash:0:10})."
        if [[ "$mode" != "committed" ]]; then
            echo "           -> Pass --use-committed-neml2 to reset & build MOOSE's committed NEML2."
        fi
    fi

    if [[ -n "$conda_hash" && -n "$sub_hash" && "$sub_hash" != "$conda_hash" ]]; then
        echo "  [NOTICE] Repo checkout of framework/contrib/neml2 (${sub_hash:0:10}) != Conda NEML2 (${conda_hash:0:10})."
        if [[ "$backend" == "conda" ]]; then
            echo "           -> Using Conda NEML2 (${conda_hash:0:10}); repo submodule checkout is ignored."
        else
            echo "           -> Using repo-isolated NEML2 (Conda is untouched). Pass --use-conda-neml2 to use Conda's version."
        fi
    elif [[ "$backend" == "conda" && -n "$conda_hash" ]]; then
        echo "  [INFO] Submodule matches Conda NEML2 (${conda_hash:0:10}); using Conda installation directly."
    elif [[ -n "$conda_hash" ]]; then
        echo "  [INFO] Using repo-isolated NEML2 (Conda's neml2 at ${conda_hash:0:10} is untouched)."
    fi
}

moose_mac_ensure_neml2_submodule()
{
    cd "$MOOSE_DIR"

    local mode committed_hash sub_hash
    mode="$(moose_mac_neml2_mode)"
    committed_hash="$(moose_mac_neml2_committed_hash)"
    sub_hash="$(moose_mac_neml2_submodule_hash)"

    if [[ "$mode" == "committed" ]]; then
        if [[ ! -f framework/contrib/neml2/CMakeLists.txt || "$sub_hash" != "$committed_hash" ]] || moose_mac_neml2_submodule_dirty; then
            moose_mac_section "Checking out MOOSE-committed framework/contrib/neml2 (${committed_hash:0:10})"
            git submodule sync --recursive -- framework/contrib/neml2
            git submodule update --init --checkout --force --recursive framework/contrib/neml2
        fi
    elif [[ ! -f framework/contrib/neml2/CMakeLists.txt ]]; then
        moose_mac_section "Initializing framework/contrib/neml2"
        git submodule sync --recursive -- framework/contrib/neml2
        git submodule update \
            --init \
            --checkout \
            --force \
            --recursive \
            framework/contrib/neml2
    else
        # Preserve the user's current branch/commit in framework/contrib/neml2,
        # only ensuring nested submodules inside neml2 are initialized.
        git -C "$MOOSE_DIR/framework/contrib/neml2" submodule update --init --recursive
    fi

    [[ -f framework/contrib/neml2/CMakeLists.txt ]] || \
        moose_mac_die "NEML2 submodule is not initialized."
}

moose_mac_touch_neml2_consumers()
{
    find "$MOOSE_DIR/framework/src/neml2" \
         "$MOOSE_DIR/modules/solid_mechanics/src/neml2" \
         "$MOOSE_DIR/modules/shifted_boundary_method" \
         "$MOOSE_DIR/test/src/neml2" \
         -name "*.C" -exec touch {} + 2>/dev/null || true
    rm -rf "$MOOSE_DIR/.git/moose_mac_receipts" "$MOOSE_DIR/.moose_mac_receipts" 2>/dev/null || true
    rm -f "$MOOSE_DIR/framework/libmoose-${METHOD:-opt}.la" \
          "$MOOSE_DIR/framework/libmoose-${METHOD:-opt}.dylib" \
          "$MOOSE_DIR/framework/libmoose_kokkos-${METHOD:-opt}.la" \
          "$MOOSE_DIR/modules/solid_mechanics/lib/libsolid_mechanics-${METHOD:-opt}.la" \
          "$MOOSE_DIR/modules/solid_mechanics/lib/libsolid_mechanics-${METHOD:-opt}.dylib" \
          "$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-${METHOD:-opt}" \
          "$MOOSE_DIR/modules/shifted_boundary_method/lib/libshifted_boundary_method-${METHOD:-opt}.la" \
          "$MOOSE_DIR/modules/shifted_boundary_method/lib/libshifted_boundary_method-${METHOD:-opt}.dylib" \
          "$MOOSE_DIR/modules/shifted_boundary_method/lib/libshifted_boundary_method_kokkos-${METHOD:-opt}.la" \
          "$MOOSE_DIR/modules/shifted_boundary_method/shifted_boundary_method-${METHOD:-opt}" \
          "$MOOSE_DIR/test/lib/libmoose_test-${METHOD:-opt}.la" \
          "$MOOSE_DIR/test/moose_test-${METHOD:-opt}" \
          "$MOOSE_DIR/unit/moose-unit-${METHOD:-opt}" 2>/dev/null || true
}

moose_mac_record_local_neml2_state()
{
    local local_dir sub_hash worktree_state
    local_dir="$(moose_mac_local_neml2_dir)"
    sub_hash="$(moose_mac_neml2_submodule_hash)"
    worktree_state="$(moose_mac_neml2_worktree_state)"
    if [[ -d "$local_dir" ]]; then
        [[ -n "$sub_hash" ]] && echo "$sub_hash" > "$local_dir/hash"
        [[ -n "$worktree_state" ]] && echo "$worktree_state" > "$local_dir/.moose_sync_state"
    fi
}

moose_mac_classify_neml2_changes()
{
    local base_hash="$1"
    local target_hash="$2"
    local neml2_repo="$MOOSE_DIR/framework/contrib/neml2"
    local build_dir="$neml2_repo/build/moose-apple"
    local local_state
    local_state="$(moose_mac_local_neml2_state)"

    if ! moose_mac_local_neml2_ok || [[ -z "$base_hash" ]]; then
        echo "full"
        return 0
    fi

    if ! git -C "$neml2_repo" cat-file -e "${base_hash}^{commit}" 2>/dev/null; then
        echo "full"
        return 0
    fi

    local changed_files
    changed_files="$(
        {
            git -C "$neml2_repo" diff --name-only "$base_hash" "$target_hash" 2>/dev/null
            git -C "$neml2_repo" diff --name-only HEAD 2>/dev/null
        } | sort -u
    )"

    local ninja_needs_cxx=0
    if [[ -f "$build_dir/build.ninja" ]] && command -v ninja >/dev/null 2>&1; then
        if ! ninja -C "$build_dir" -n aoti eager pyneml2 2>/dev/null | grep -q "no work to do"; then
            ninja_needs_cxx=1
        fi
    fi

    if [[ -z "$changed_files" && "$ninja_needs_cxx" -eq 0 ]]; then
        if [[ "$local_state" == *-dirty-* ]]; then
            echo "python_only"
        else
            echo "none"
        fi
        return 0
    fi

    local has_build_cfg=0
    local has_cxx_hdr=0
    local has_cxx_src="$ninja_needs_cxx"
    local has_python=0
    local file

    while IFS= read -r file; do
        [[ -z "$file" ]] && continue
        case "$file" in
            pyproject.toml|CMakeLists.txt|cmake/*|*/CMakeLists.txt)
                has_build_cfg=1
                ;;
            neml2/csrc/*.h|neml2/csrc/*.hpp|neml2/csrc/*/*.h|neml2/csrc/*/*.hpp|neml2/csrc/*/*/*.h|neml2/csrc/*/*/*.hpp)
                has_cxx_hdr=1
                ;;
            neml2/csrc/*)
                has_cxx_src=1
                ;;
            neml2/*.py|neml2/*/*.py|neml2/*/*/*.py|neml2/*/*/*/*.py)
                has_python=1
                ;;
        esac
    done <<< "$changed_files"

    if [[ "$has_build_cfg" -eq 1 ]]; then
        echo "full"
    elif [[ "$has_cxx_hdr" -eq 1 ]]; then
        if [[ -f "$build_dir/build.ninja" || -f "$build_dir/Makefile" ]]; then
            echo "cxx_headers"
        else
            echo "full"
        fi
    elif [[ "$has_cxx_src" -eq 1 ]]; then
        if [[ -f "$build_dir/build.ninja" || -f "$build_dir/Makefile" ]]; then
            echo "cxx_impl"
        else
            echo "full"
        fi
    elif [[ "$has_python" -eq 1 || "$local_state" == *-dirty-* ]]; then
        echo "python_only"
    else
        echo "none"
    fi
}

moose_mac_sync_neml2_python_only()
{
    local local_dir
    local_dir="$(moose_mac_local_neml2_dir)"
    rsync -a --checksum \
        --include='*/' \
        --include='*.py' \
        --exclude='*' \
        "$MOOSE_DIR/framework/contrib/neml2/neml2/" "$local_dir/"
    moose_mac_record_local_neml2_state
}

moose_mac_build_neml2_fast_cxx()
{
    local touch_moose="${1:-0}"
    local build_dir="$MOOSE_DIR/framework/contrib/neml2/build/moose-apple"
    local local_dir
    local_dir="$(moose_mac_local_neml2_dir)"

    cmake --build "$build_dir" --parallel "${NEML2_JOBS:-$JOBS}" --target aoti eager pyneml2
    cmake --install "$build_dir" --prefix "$local_dir"
    moose_mac_sync_neml2_python_only

    if [[ "$touch_moose" -eq 1 ]] && moose_mac_configured; then
        moose_mac_touch_neml2_consumers
    fi
}

moose_mac_build_neml2()
{
    cd "$MOOSE_DIR"

    local local_site torch_dir
    local_site="$(moose_mac_local_neml2_site)"
    torch_dir="$(moose_mac_torch_dir)"
    [[ -n "$torch_dir" && -d "$torch_dir" ]] || \
        moose_mac_die "Could not locate PyTorch directory in active environment."

    moose_mac_section "Building NEML2 v3 (Repo-Isolated: $local_site)"

    mkdir -p "$local_site"
    rm -rf "$local_site/neml2" "$local_site"/neml2-*.dist-info "$local_site/bin"
    ln -sfn "$torch_dir" "$local_site/torch"

    moose_mac_env_exports

    # Route NEML2's clang compiles through ccache when available. The launcher is
    # baked into the CMake cache at configure time, so later incremental fast-cxx
    # rebuilds (cmake --build) benefit too.
    local ccache_launcher=()
    local ccache_bin
    ccache_bin="$(moose_mac_ccache_bin)"
    if [[ -n "$ccache_bin" ]]; then
        moose_mac_ccache_prepare
        ccache_launcher=(
            "-DCMAKE_C_COMPILER_LAUNCHER=$ccache_bin"
            "-DCMAKE_CXX_COMPILER_LAUNCHER=$ccache_bin"
        )
    fi

    NEML2_JOBS="${NEML2_JOBS:-${BUILD_JOBS:-$JOBS}}" \
    ./scripts/update_and_rebuild_neml2.sh \
        --skip-submodule-update \
        --target "$local_site" \
        -DCMAKE_C_COMPILER=/usr/bin/clang \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
        -DMPI_C_COMPILER="$CONDA_PREFIX/bin/mpicc" \
        -DMPI_CXX_COMPILER="$CONDA_PREFIX/bin/mpicxx" \
        ${ccache_launcher[@]+"${ccache_launcher[@]}"} \
        --config-settings=build-dir=build/moose-apple

    moose_mac_record_local_neml2_state

    if moose_mac_configured; then
        moose_mac_touch_neml2_consumers
    fi
}

moose_mac_sync_neml2_config_dir()
{
    if ! moose_mac_configured; then
        return 0
    fi

    local conf_dir active_dir
    conf_dir="$(moose_mac_configured_neml2_dir)"
    active_dir="$(moose_mac_active_neml2_dir)"

    if [[ -n "$active_dir" && "$conf_dir" != "$active_dir" ]]; then
        moose_mac_section "Updating MOOSE NEML2_DIR Configuration"
        echo "Previous NEML2_DIR : ${conf_dir:-unset}"
        echo "Active NEML2_DIR   : $active_dir"

        python - "$MOOSE_DIR/conf_vars.mk" "$MOOSE_DIR/framework/include/base/MooseConfig.h" "$active_dir" <<'PY'
import os, re, sys
conf_mk, moose_cfg, active_dir = sys.argv[1], sys.argv[2], sys.argv[3]

if os.path.exists(conf_mk):
    with open(conf_mk, "r") as f:
        text = f.read()
    text = re.sub(r"(?m)^NEML2_DIR\s*:=.*$", f"NEML2_DIR         := {active_dir}", text)
    with open(conf_mk, "w") as f:
        f.write(text)

if os.path.exists(moose_cfg):
    st = os.stat(moose_cfg)
    with open(moose_cfg, "r") as f:
        text = f.read()
    text = re.sub(r"(?m)^(#define\s+MOOSE_NEML2_DIR\s+).*$", rf"\1{active_dir}", text)
    with open(moose_cfg, "w") as f:
        f.write(text)
    os.utime(moose_cfg, (st.st_atime, st.st_mtime))
PY
        moose_mac_touch_neml2_consumers
    fi
}

moose_mac_sync_neml2()
{
    if [[ "${MOOSE_MAC_SKIP_NEML2_SYNC:-0}" == "1" ]]; then
        moose_mac_section "NEML2 Status (--skip-neml2-sync)"
        echo "Skipping NEML2 sync check as requested."
        return 0
    fi

    local mode backend
    mode="$(moose_mac_neml2_mode)"

    if [[ "$mode" != "conda" ]]; then
        moose_mac_ensure_neml2_submodule
    fi

    backend="$(moose_mac_effective_neml2_backend)"
    moose_mac_env_exports

    if [[ "$backend" == "conda" ]]; then
        if ! moose_mac_conda_neml2_ok; then
            moose_mac_die "NEML2 is not installed in Conda environment '${MOOSE_CONDA_ENV:-moose}'. Run without --use-conda-neml2 to build repo-isolated NEML2."
        fi
        moose_mac_section "NEML2 Status"
        moose_mac_print_neml2_notices
        moose_mac_sync_neml2_config_dir
        return 0
    fi

    local sub_hash local_hash worktree_state local_state tier
    sub_hash="$(moose_mac_neml2_submodule_hash)"
    local_hash="$(moose_mac_local_neml2_hash)"
    worktree_state="$(moose_mac_neml2_worktree_state)"
    local_state="$(moose_mac_local_neml2_state)"

    if ! moose_mac_local_neml2_ok || [[ "$worktree_state" != "$local_state" ]]; then
        tier="$(moose_mac_classify_neml2_changes "$local_hash" "$sub_hash")"
        moose_mac_section "NEML2 Smart Sync (tier: $tier)"
        echo "Submodule state    : ${worktree_state:-unknown} ($(moose_mac_neml2_submodule_branch))"
        echo "Repo-local state   : ${local_state:-not built}"

        case "$tier" in
            none)
                echo "[SKIP] Only non-runtime files (tests/docs/benchmarks) changed in NEML2. Skipping rebuild."
                moose_mac_record_local_neml2_state
                ;;
            python_only)
                echo "[FAST-PY] Only Python files (.py) changed in NEML2. Syncing via rsync (skipping CMake/C++ & MOOSE relink)..."
                moose_mac_sync_neml2_python_only
                ;;
            cxx_impl)
                echo "[FAST-CXX] Only C++ implementation (.cpp) changed in NEML2. Incremental CMake build+install (skipping pip wheel & MOOSE recompile)..."
                moose_mac_build_neml2_fast_cxx 0
                ;;
            cxx_headers)
                echo "[FAST-CXX-HDR] C++ headers (.h) changed in NEML2. Incremental CMake build+install (skipping pip wheel) + MOOSE NEML2 relink..."
                moose_mac_build_neml2_fast_cxx 1
                ;;
            full|*)
                echo "[FULL-BUILD] Building repo-isolated NEML2 (Conda environment will NOT be modified)..."
                moose_mac_build_neml2
                ;;
        esac
    fi

    moose_mac_section "NEML2 Status"
    moose_mac_print_neml2_notices
    moose_mac_sync_neml2_config_dir
}

moose_mac_export_torch_neml2()
{
    export LIBTORCH_DIR="$(
        python -c 'import torch; print(torch.__path__[0])'
    )"

    export NEML2_DIR="$(moose_mac_active_neml2_dir)"
}

moose_mac_configured()
{
    # configure generates MooseConfig.h from MooseConfig.h.in; there is no framework_config.h.
    [[ -f "$MOOSE_DIR/framework/include/base/MooseConfig.h" && -f "$MOOSE_DIR/conf_vars.mk" ]]
}

moose_mac_kokkos_enabled()
{
    [[ -f "$MOOSE_DIR/conf_vars.mk" ]] && grep -q 'ENABLE_KOKKOS.*true' "$MOOSE_DIR/conf_vars.mk"
}

moose_mac_neml2_enabled()
{
    [[ -f "$MOOSE_DIR/conf_vars.mk" ]] && grep -q 'ENABLE_NEML2.*true' "$MOOSE_DIR/conf_vars.mk"
}

moose_mac_libtorch_enabled()
{
    [[ -f "$MOOSE_DIR/conf_vars.mk" ]] && grep -q 'ENABLE_LIBTORCH.*true' "$MOOSE_DIR/conf_vars.mk"
}

moose_mac_module_dir()
{
    case "$1" in
        framework-core|framework_core)
            # Builds only the framework library (libmoose) via framework/Makefile,
            # which includes build.mk + moose.mk but not app.mk, so the moose_test
            # application and its test library are skipped.
            echo "$MOOSE_DIR/framework"
            ;;
        framework|test)
            echo "$MOOSE_DIR/test"
            ;;
        unit|framework_unit)
            echo "$MOOSE_DIR/unit"
            ;;
        solid_mechanics)
            echo "$MOOSE_DIR/modules/solid_mechanics"
            ;;
        solid_mechanics_unit)
            echo "$MOOSE_DIR/modules/solid_mechanics/unit"
            ;;
        shifted_boundary_method|sbm)
            echo "$MOOSE_DIR/modules/shifted_boundary_method"
            ;;
        shifted_boundary_method_unit|sbm_unit)
            echo "$MOOSE_DIR/modules/shifted_boundary_method/unit"
            ;;
        *_unit)
            local mod="${1%_unit}"
            if [[ -d "$MOOSE_DIR/modules/$mod/unit" ]]; then
                echo "$MOOSE_DIR/modules/$mod/unit"
            else
                moose_mac_die "Unknown unit target '$1'. Unit directory not found: $MOOSE_DIR/modules/$mod/unit"
            fi
            ;;
        *)
            moose_mac_die "Unknown build target '$1'. Use: framework, solid_mechanics, shifted_boundary_method, unit, all"
            ;;
    esac
}

moose_mac_module_exe()
{
    local method="${METHOD:-opt}"

    case "$1" in
        framework-core|framework_core)
            # No executable: this target produces only the framework library.
            echo ""
            ;;
        framework|test)
            echo "$MOOSE_DIR/test/moose_test-$method"
            ;;
        unit|framework_unit)
            echo "$MOOSE_DIR/unit/moose-unit-$method"
            ;;
        solid_mechanics)
            echo "$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-$method"
            ;;
        solid_mechanics_unit)
            echo "$MOOSE_DIR/modules/solid_mechanics/unit/solid_mechanics-unit-$method"
            ;;
        shifted_boundary_method|sbm)
            echo "$MOOSE_DIR/modules/shifted_boundary_method/shifted_boundary_method-$method"
            ;;
        shifted_boundary_method_unit|sbm_unit)
            echo "$MOOSE_DIR/modules/shifted_boundary_method/unit/shifted_boundary_method-unit-$method"
            ;;
        *_unit)
            local mod="${1%_unit}"
            echo "$MOOSE_DIR/modules/$mod/unit/${mod}-unit-$method"
            ;;
        *)
            echo ""
            ;;
    esac
}

moose_mac_target_receipt_file()
{
    local exe="$1"
    local base_name="$(basename "$exe")"
    local receipt_dir="$MOOSE_DIR/.git/moose_mac_receipts"
    if [[ ! -d "$MOOSE_DIR/.git" ]]; then
        receipt_dir="$MOOSE_DIR/.moose_mac_receipts"
    fi
    mkdir -p "$receipt_dir"
    echo "$receipt_dir/${base_name}.receipt"
}

moose_mac_write_target_receipt()
{
    local target="$1"
    local exe="$2"
    [[ -n "$exe" && -f "$exe" ]] || return 0

    local receipt
    receipt="$(moose_mac_target_receipt_file "$exe")"
    local active_backend active_dir active_hash neml2_mtime
    active_backend="$(moose_mac_effective_neml2_backend)"
    active_dir="$(moose_mac_active_neml2_dir)"
    active_hash="$(moose_mac_active_neml2_hash)"

    local neml2_lib="$active_dir/lib/libneml2.dylib"
    if [[ -f "$neml2_lib" ]]; then
        neml2_mtime="$(stat -f "%m" "$neml2_lib" 2>/dev/null || date +%s)"
    else
        neml2_mtime="0"
    fi

    cat > "$receipt" <<EOF
TARGET=$target
EXE=$exe
NEML2_BACKEND=$active_backend
NEML2_DIR=$active_dir
NEML2_HASH=$active_hash
NEML2_LIB_MTIME=$neml2_mtime
BUILT_AT=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
EOF
}

moose_mac_clean_target_binaries()
{
    local target="$1"
    local method="${METHOD:-opt}"
    local exe
    exe="$(moose_mac_module_exe "$target")"
    if [[ -n "$exe" ]]; then
        rm -f "$exe"
        rm -f "$(moose_mac_target_receipt_file "$exe")"
    fi

    case "$target" in
        solid_mechanics)
            rm -f "$MOOSE_DIR/modules/solid_mechanics/lib/libsolid_mechanics-${method}.la" \
                  "$MOOSE_DIR/modules/solid_mechanics/lib/libsolid_mechanics-${method}.dylib" \
                  "$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-${method}"
            ;;
        shifted_boundary_method|sbm)
            rm -f "$MOOSE_DIR/modules/shifted_boundary_method/lib/libshifted_boundary_method-${method}.la" \
                  "$MOOSE_DIR/modules/shifted_boundary_method/lib/libshifted_boundary_method-${method}.dylib" \
                  "$MOOSE_DIR/modules/shifted_boundary_method/lib/libshifted_boundary_method_kokkos-${method}.la" \
                  "$MOOSE_DIR/modules/shifted_boundary_method/shifted_boundary_method-${method}"
            ;;
        framework|test)
            rm -f "$MOOSE_DIR/test/moose_test-${method}" \
                  "$MOOSE_DIR/test/lib/libmoose_test-${method}.la" \
                  "$MOOSE_DIR/framework/libmoose-${method}.la" \
                  "$MOOSE_DIR/framework/libmoose-${method}.dylib" \
                  "$MOOSE_DIR/framework/libmoose_kokkos-${method}.la"
            ;;
        unit|framework_unit)
            rm -f "$MOOSE_DIR/unit/moose-unit-${method}"
            ;;
    esac
}

# Echo the source/header trees whose contents are compiled into a target's
# executable. Used by moose_mac_target_status to detect edits made since the
# last build. These are the target's own trees only; an edit to the framework
# shows up on the framework/test entry, not on a module entry that merely links
# libmoose.
moose_mac_target_source_roots()
{
    case "$1" in
        framework|test)
            echo "$MOOSE_DIR/framework/src $MOOSE_DIR/framework/include" \
                 "$MOOSE_DIR/test/src $MOOSE_DIR/test/include"
            ;;
        solid_mechanics)
            echo "$MOOSE_DIR/modules/solid_mechanics/src" \
                 "$MOOSE_DIR/modules/solid_mechanics/include"
            ;;
        shifted_boundary_method|sbm)
            echo "$MOOSE_DIR/modules/shifted_boundary_method/src" \
                 "$MOOSE_DIR/modules/shifted_boundary_method/include"
            ;;
        unit|framework_unit)
            echo "$MOOSE_DIR/unit/src $MOOSE_DIR/unit/include"
            ;;
        *_unit)
            local mod="${1%_unit}"
            echo "$MOOSE_DIR/modules/$mod/unit/src" \
                 "$MOOSE_DIR/modules/$mod/unit/include"
            ;;
        *)
            echo ""
            ;;
    esac
}

moose_mac_target_status()
{
    local target="$1"
    local exe
    exe="$(moose_mac_module_exe "$target")"

    TARGET_STATUS="MISSING"
    TARGET_STALE_REASON=""
    TARGET_BUILT_NEML2_HASH=""
    TARGET_ACTIVE_NEML2_HASH=""
    TARGET_NEML2_LINKAGE="None"
    TARGET_KOKKOS_LINKAGE="None"

    if [[ -z "$exe" || ! -f "$exe" || ! -x "$exe" ]]; then
        TARGET_STATUS="MISSING"
        return 0
    fi

    # Check Kokkos linkage
    if otool -L "$exe" 2>/dev/null | grep -q libkokkos; then
        TARGET_KOKKOS_LINKAGE="WITH KOKKOS ($(moose_mac_detect_kokkos_backends))"
    else
        TARGET_KOKKOS_LINKAGE="WITHOUT KOKKOS"
    fi

    local links_neml2=0
    if otool -L "$exe" 2>/dev/null | grep -q libneml2; then
        links_neml2=1
    fi

    local neml2_rpath="" linked_backend="none"
    if [[ "$links_neml2" -eq 1 ]]; then
        neml2_rpath="$(otool -l "$exe" 2>/dev/null | grep -A2 LC_RPATH | grep neml2 | head -n 1 | awk '{print $2}' || true)"
        if [[ "$neml2_rpath" == *"framework/contrib/neml2/build/installed"* ]]; then
            linked_backend="local"
            TARGET_NEML2_LINKAGE="WITH NEML2 -> Repo-Local (build/installed/neml2)"
        elif [[ "$neml2_rpath" == *"site-packages/neml2"* ]]; then
            linked_backend="conda"
            TARGET_NEML2_LINKAGE="WITH NEML2 -> Conda (site-packages/neml2)"
        elif [[ -n "$neml2_rpath" ]]; then
            linked_backend="custom"
            TARGET_NEML2_LINKAGE="WITH NEML2 -> Custom ($neml2_rpath)"
        else
            linked_backend="dynamic"
            TARGET_NEML2_LINKAGE="WITH NEML2 (dynamic link)"
        fi
    else
        TARGET_NEML2_LINKAGE="WITHOUT NEML2"
    fi

    local active_backend active_hash active_dir neml2_lib
    active_backend="$(moose_mac_effective_neml2_backend)"
    active_hash="$(moose_mac_active_neml2_hash)"
    active_dir="$(moose_mac_active_neml2_dir)"
    neml2_lib="$active_dir/lib/libneml2.dylib"
    TARGET_ACTIVE_NEML2_HASH="$active_hash"

    local is_neml2_consumer=0
    case "$target" in
        solid_mechanics|shifted_boundary_method|sbm)
            is_neml2_consumer=1
            ;;
    esac

    # Source freshness: has any source or header in this target's own trees been
    # modified since the executable was last linked? This is an mtime heuristic,
    # not make's full dependency graph, so it reliably catches edits to this
    # target's code but does not chase transitive framework edits onto module
    # entries. find -quit stops at the first newer file, so it is cheap.
    local src_roots existing_roots=() root newer
    src_roots="$(moose_mac_target_source_roots "$target")"
    for root in $src_roots; do
        [[ -d "$root" ]] && existing_roots+=("$root")
    done
    if [[ ${#existing_roots[@]} -gt 0 ]]; then
        newer="$(find "${existing_roots[@]}" \
            \( -name '*.C' -o -name '*.h' -o -name '*.K' \) \
            -newer "$exe" -print -quit 2>/dev/null)"
        if [[ -n "$newer" ]]; then
            TARGET_STATUS="STALE"
            TARGET_STALE_REASON="Source files changed since last build (e.g. ${newer#"$MOOSE_DIR"/}); recompile needed"
            return 0
        fi
    fi

    # Check NEML2 configuration vs target
    if ! moose_mac_neml2_enabled; then
        # MOOSE was configured without NEML2
        if [[ "$links_neml2" -eq 1 ]]; then
            TARGET_STATUS="STALE"
            TARGET_STALE_REASON="Built WITH NEML2, but current MOOSE configuration has NEML2 disabled (--without-neml2)"
            return 0
        fi
        TARGET_STATUS="READY"
        return 0
    fi

    # NEML2 is enabled in MOOSE configuration
    if [[ "$is_neml2_consumer" -eq 1 && "$links_neml2" -eq 0 ]]; then
        TARGET_STATUS="STALE"
        TARGET_STALE_REASON="Built WITHOUT NEML2, but current configuration has NEML2 enabled"
        return 0
    fi

    if [[ "$links_neml2" -eq 1 ]]; then
        # 1. Backend mismatch
        if [[ "$linked_backend" != "$active_backend" && "$linked_backend" != "custom" ]]; then
            TARGET_STATUS="STALE"
            TARGET_STALE_REASON="Backend mismatch: executable links $linked_backend NEML2, but active mode is $active_backend"
            return 0
        fi

        # 2. Receipt check
        local receipt
        receipt="$(moose_mac_target_receipt_file "$exe")"
        if [[ -f "$receipt" ]]; then
            local built_hash built_backend
            built_hash="$(grep '^NEML2_HASH=' "$receipt" 2>/dev/null | cut -d= -f2- || true)"
            built_backend="$(grep '^NEML2_BACKEND=' "$receipt" 2>/dev/null | cut -d= -f2- || true)"
            TARGET_BUILT_NEML2_HASH="$built_hash"

            if [[ -n "$built_hash" && -n "$active_hash" && "$built_hash" != "$active_hash" ]]; then
                TARGET_STATUS="STALE"
                TARGET_STALE_REASON="NEML2 switch detected: built with ${built_hash:0:10}, but active NEML2 is ${active_hash:0:10}"
                return 0
            fi
            if [[ -n "$built_backend" && "$built_backend" != "$active_backend" ]]; then
                TARGET_STATUS="STALE"
                TARGET_STALE_REASON="NEML2 backend changed: built with $built_backend, active mode is $active_backend"
                return 0
            fi
        fi

        # 3. Timestamp comparison: is libneml2.dylib newer than executable?
        if [[ -f "$neml2_lib" && "$exe" -ot "$neml2_lib" ]]; then
            TARGET_STATUS="STALE"
            local exe_mtime_str neml2_mtime_str
            exe_mtime_str="$(date -r "$(stat -f "%m" "$exe")" +"%Y-%m-%d %H:%M:%S" 2>/dev/null || echo "earlier")"
            neml2_mtime_str="$(date -r "$(stat -f "%m" "$neml2_lib")" +"%Y-%m-%d %H:%M:%S" 2>/dev/null || echo "now")"
            TARGET_STALE_REASON="libneml2.dylib was updated ($neml2_mtime_str) after executable was compiled ($exe_mtime_str)"
            return 0
        fi

        # 4. If using repo-local NEML2, is submodule checkout different from repo-local installed?
        if [[ "$active_backend" == "local" ]]; then
            local sub_hash local_hash
            sub_hash="$(moose_mac_neml2_submodule_hash)"
            local_hash="$(moose_mac_local_neml2_hash)"
            if [[ -n "$sub_hash" && -n "$local_hash" && "$sub_hash" != "$local_hash" ]]; then
                TARGET_STATUS="STALE"
                TARGET_STALE_REASON="framework/contrib/neml2 checkout (${sub_hash:0:10}) != installed NEML2 (${local_hash:0:10}); run ./scripts/mac/moose.sh compile neml2"
                return 0
            fi
        fi
    fi

    TARGET_STATUS="READY"
    return 0
}

moose_mac_unit_dir()
{
    case "${1:-framework}" in
        framework|unit|"")
            echo "$MOOSE_DIR/unit"
            ;;
        solid_mechanics|sbm|shifted_boundary_method|*)
            local mod="${1:-framework}"
            [[ "$mod" == "sbm" ]] && mod="shifted_boundary_method"
            mod="${mod%_unit}"
            if [[ -d "$MOOSE_DIR/modules/$mod/unit" ]]; then
                echo "$MOOSE_DIR/modules/$mod/unit"
            else
                moose_mac_die "Unit test directory not found for '$mod': $MOOSE_DIR/modules/$mod/unit"
            fi
            ;;
    esac
}

moose_mac_unit_exe()
{
    local method="${METHOD:-opt}"
    case "${1:-framework}" in
        framework|unit|"")
            echo "$MOOSE_DIR/unit/moose-unit-$method"
            ;;
        solid_mechanics|sbm|shifted_boundary_method|*)
            local mod="${1:-framework}"
            [[ "$mod" == "sbm" ]] && mod="shifted_boundary_method"
            mod="${mod%_unit}"
            echo "$MOOSE_DIR/modules/$mod/unit/${mod}-unit-$method"
            ;;
    esac
}

moose_mac_diagnose_dyld_error()
{
    local log_file="$1"
    [[ -f "$log_file" ]] || return 1

    if grep -q "symbol not found in flat namespace" "$log_file" 2>/dev/null; then
        local raw_line mangled_symbol demangled=""
        raw_line="$(grep "symbol not found in flat namespace" "$log_file" | head -n 1)"
        mangled_symbol="$(printf '%s\n' "$raw_line" | sed -E "s/.*symbol not found in flat namespace '([^']+)'.*/\1/")"
        if command -v c++filt >/dev/null 2>&1 && [[ -n "$mangled_symbol" ]]; then
            demangled="$(c++filt "$mangled_symbol" 2>/dev/null || true)"
        fi

        echo
        echo "============================================"
        echo " Dynamic Linker (dyld) ABI Error Detected"
        echo "============================================"
        echo "Missing symbol : ${demangled:-$mangled_symbol}"
        if [[ -n "$demangled" && "$demangled" != "$mangled_symbol" ]]; then
            echo "Mangled symbol : $mangled_symbol"
        fi
        echo
        echo "Diagnosis:"
        echo "  The executable failed to launch because dynamic libraries have mismatched ABIs."
        echo "  This typically happens when 'framework' was updated with new symbols,"
        echo "  but the application module or unit test was not recompiled (or vice versa)."
        echo
        echo "Recommended action:"
        echo "  1. Recompile framework : ./scripts/mac/moose.sh compile framework"
        echo "  2. Recompile module    : ./scripts/mac/moose.sh compile <module>"
        echo "  Or recompile all       : ./scripts/mac/moose.sh compile all"
        echo "============================================"
        echo
        return 0
    elif grep -q "Library not loaded" "$log_file" 2>/dev/null; then
        local lib
        lib="$(grep "Library not loaded" "$log_file" | head -n 1 | sed -E "s/.*Library not loaded: ([^ ]+).*/\1/")"

        echo
        echo "============================================"
        echo " Dynamic Linker (dyld) Missing Library"
        echo "============================================"
        echo "Missing library : $lib"
        echo
        echo "Diagnosis:"
        echo "  The executable requires a shared library that is no longer found at the expected path."
        echo "  This commonly happens after a Conda package update."
        echo
        echo "Recommended action:"
        echo "  Recompile the target to refresh shared library bindings:"
        echo "  ./scripts/mac/moose.sh compile <target>"
        echo "============================================"
        echo
        return 0
    fi
    return 1
}

moose_mac_check_preflight()
{
    local exe="$1"
    [[ -x "$exe" ]] || return 0

    local tmp_log
    tmp_log="$(mktemp /tmp/moose-preflight-XXXXXX.log)"
    if ! "$exe" --show-capabilities >"$tmp_log" 2>&1; then
        if moose_mac_diagnose_dyld_error "$tmp_log"; then
            rm -f "$tmp_log"
            return 1
        fi
    fi
    rm -f "$tmp_log"
    return 0
}

moose_mac_shorten_paths()
{
    sed \
        -e "s|${MOOSE_DIR}|<MOOSE>|g" \
        -e "s|${CONDA_PREFIX}|<CONDA>|g"
}

moose_mac_detect_kokkos_backends()
{
    local candidate
    local kokkos_header=""

    for candidate in \
        "${CONDA_PREFIX}/include/KokkosCore_config.h" \
        "${CONDA_PREFIX}/include/kokkos/KokkosCore_config.h"; do
        if [[ -f "${candidate}" ]]; then
            kokkos_header="${candidate}"
            break
        fi
    done

    if [[ -n "${kokkos_header}" ]]; then
        grep -E '^#define KOKKOS_ENABLE_[A-Za-z0-9_]+' "${kokkos_header}" 2>/dev/null |
            awk '
            {
                name=$2
                sub(/^KOKKOS_ENABLE_/, "", name)
                if (name == "CUDA" || name == "HIP" || name == "SYCL" || name == "OPENMP" || name == "THREADS" || name == "SERIAL" || name == "HPX")
                    enabled[name]=1
            }
            END {
                first=1
                order[1]="CUDA"; order[2]="HIP"; order[3]="SYCL"; order[4]="OPENMP"; order[5]="THREADS"; order[6]="SERIAL"; order[7]="HPX"
                for (i=1; i<=7; i++) {
                    n=order[i]
                    if (enabled[n]) {
                        if (!first) printf ","
                        printf "%s", n
                        first=0
                    }
                }
                if (first) printf "unknown"
            }'
    else
        echo "unknown"
    fi
}

moose_mac_make_target()
{
    local target="$1"
    local module_dir exe
    module_dir="$(moose_mac_module_dir "$target")"
    exe="$(moose_mac_module_exe "$target")"

    moose_mac_target_status "$target"
    if [[ "$TARGET_STATUS" == "STALE" ]]; then
        echo "[INFO] Stale binary detected for target '$target': $TARGET_STALE_REASON"
        echo "       Cleaning $target libraries and executable to force clean relink against active NEML2..."
        moose_mac_clean_target_binaries "$target"
    fi

    local ccache_bin
    ccache_bin="$(moose_mac_ccache_bin)"
    moose_mac_set_ccache_make_args

    local prof_hits_before=0 prof_misses_before=0
    if [[ "${MOOSE_MAC_PROFILE:-0}" == "1" && -n "$ccache_bin" ]]; then
        prof_hits_before="$(moose_mac_ccache_hits)"
        prof_misses_before="$(moose_mac_ccache_misses)"
    fi

    if [[ "${MOOSE_MAC_RAW:-0}" == "1" ]]; then
        moose_mac_section "Building $target (raw mode)"
        echo "Directory: $module_dir"
        cd "$module_dir"
        make -j"$BUILD_JOBS" METHOD="$METHOD" \
            ${MOOSE_MAC_MAKE_CCACHE_ARGS[@]+"${MOOSE_MAC_MAKE_CCACHE_ARGS[@]}"}
        if [[ -n "$exe" && -x "$exe" ]]; then
            moose_mac_write_target_receipt "$target" "$exe"
        fi
        return 0
    fi

    local sha branch tree_state kokkos_config log_file
    sha="$(git -C "$MOOSE_DIR" rev-parse --short=10 HEAD 2>/dev/null || echo "unknown")"
    branch="$(git -C "$MOOSE_DIR" branch --show-current 2>/dev/null || echo "detached-HEAD")"
    [[ -z "$branch" ]] && branch="detached-HEAD"

    if [[ -n "$(git -C "$MOOSE_DIR" status --porcelain 2>/dev/null)" ]]; then
        tree_state="DIRTY"
    else
        tree_state="clean"
    fi

    kokkos_config="$(moose_mac_detect_kokkos_backends)"
    log_file="/tmp/moose-build-${target}-${sha}-$$.log"

    moose_mac_section "Building $target"
    echo "Directory : $module_dir"
    echo "Command   : make -j$BUILD_JOBS METHOD=$METHOD$([[ -n "$ccache_bin" ]] && echo ' (ccache)')"
    echo "Log file  : $log_file"

    local start_time end_time elapsed status
    start_time="$(date +%s)"

    cd "$module_dir"

    set +e
    make -j"$BUILD_JOBS" METHOD="$METHOD" \
        ${MOOSE_MAC_MAKE_CCACHE_ARGS[@]+"${MOOSE_MAC_MAKE_CCACHE_ARGS[@]}"} \
        >"$log_file" 2>&1
    status=$?
    set -e

    end_time="$(date +%s)"
    elapsed=$((end_time - start_time))

    if [[ $status -eq 0 ]]; then
        echo
        echo "========== BUILD SUMMARY =========="
        echo "RESULT    : BUILD SUCCESSFUL"
        echo "TARGET    : $target"
        echo "COMMIT    : $sha ($branch, $tree_state)"
        echo "KOKKOS    : $kokkos_config"
        echo "TIME      : ${elapsed}s"
        if [[ -n "$exe" && -x "$exe" ]]; then
            echo "EXECUTABLE: $exe"
            moose_mac_write_target_receipt "$target" "$exe"
        fi
        echo "LOG       : $log_file"
        echo "==================================="

        if [[ "${MOOSE_MAC_PROFILE:-0}" == "1" ]]; then
            local cxx_count link_count
            cxx_count="$(grep -cE 'Compiling (C\+\+|Kokkos C\+\+)' "$log_file" 2>/dev/null || echo 0)"
            link_count="$(grep -cE 'Linking (Library|Executable|Kokkos)' "$log_file" 2>/dev/null || echo 0)"
            echo
            echo "---------- BUILD PROFILE ----------"
            echo "WALL TIME         : ${elapsed}s"
            echo "C++ TUs COMPILED  : $cxx_count"
            echo "LINK STEPS        : $link_count"
            echo "PARALLEL JOBS     : $BUILD_JOBS"
            if [[ -n "$ccache_bin" ]]; then
                local hits_after misses_after dh dm total pct
                hits_after="$(moose_mac_ccache_hits)"
                misses_after="$(moose_mac_ccache_misses)"
                dh=$(( hits_after - prof_hits_before ))
                dm=$(( misses_after - prof_misses_before ))
                total=$(( dh + dm ))
                if (( total > 0 )); then
                    pct=$(( dh * 100 / total ))
                    echo "CCACHE            : $dh hit / $dm miss (${pct}% hit) [$ccache_bin]"
                else
                    echo "CCACHE            : enabled, nothing recompiled this run [$ccache_bin]"
                fi
            else
                echo "CCACHE            : off (opt in with MOOSE_MAC_CCACHE=on; see note below)"
            fi
            echo "-----------------------------------"
        fi
        return 0
    fi

    local error_regex='fatal error:|(^|[[:space:]])error:|undefined reference|Undefined symbols for architecture|linker command failed|ld: .*error|No rule to make target|No such file or directory|out of memory|Out of memory|Killed:|Segmentation fault|make(\[[0-9]+\])?: \*\*\*'
    local first_error_record first_error_line="" reason failed_near key_errors context

    first_error_record="$(grep -nEi "$error_regex" "$log_file" 2>/dev/null | head -n 1 || true)"
    if [[ -n "$first_error_record" ]]; then
        first_error_line="${first_error_record%%:*}"
    fi

    if [[ -n "$first_error_line" ]]; then
        reason="$(sed -n "${first_error_line}p" "$log_file" | moose_mac_shorten_paths | sed 's/^[[:space:]]*//' | cut -c1-500)"
        failed_near="$(sed -n "1,${first_error_line}p" "$log_file" | grep -E 'Compiling (C\+\+|Kokkos C\+\+)|Linking (Library|Executable|Kokkos)' | tail -n 1 | moose_mac_shorten_paths || true)"
    else
        reason="make exited with code $status; no standard compiler/linker error pattern was detected."
        failed_near=""
    fi

    key_errors="$(grep -Ei "$error_regex" "$log_file" 2>/dev/null | moose_mac_shorten_paths | awk '
        length($0) <= 600 {
            if (!seen[$0]++) {
                print
                count++
            }
        }
        count >= 10 { exit }
    ' || true)"

    if [[ -n "$first_error_line" ]]; then
        local context_start=$((first_error_line - 4))
        local context_end=$((first_error_line + 10))
        (( context_start < 1 )) && context_start=1

        context="$(sed -n "${context_start},${context_end}p" "$log_file" | moose_mac_shorten_paths | awk '
            length($0) > 700 {
                if (!long_line) print "[long compiler command omitted]"
                long_line=1
                next
            }
            { long_line=0; print }
        ' || true)"
    else
        context="$(tail -n 15 "$log_file" | moose_mac_shorten_paths | awk '
            length($0) > 700 { print "[long compiler command omitted]"; next }
            { print }
        ' || true)"
    fi

    echo
    echo "========== BUILD SUMMARY =========="
    echo "RESULT    : BUILD FAILED"
    echo "TARGET    : $target"
    echo "COMMIT    : $sha ($branch, $tree_state)"
    echo "KOKKOS    : $kokkos_config"
    echo "EXIT CODE : $status"
    echo "TIME      : ${elapsed}s"
    if [[ -n "$failed_near" ]]; then
        echo
        echo "FAILED NEAR:"
        echo "$failed_near"
    fi
    echo
    echo "REASON:"
    echo "$reason"
    if [[ -n "$key_errors" ]]; then
        echo
        echo "KEY ERRORS:"
        echo "$key_errors"
    fi
    echo
    echo "ERROR CONTEXT:"
    echo "-----------------------------------"
    echo "$context"
    echo "-----------------------------------"
    echo "FULL LOG  : $log_file"
    echo "==================================="

    return "$status"
}

moose_mac_recommend_next_steps_after_build()
{
    local targets=("$@")
    moose_mac_section "Next Steps"
    echo "Compilation finished for: ${targets[*]}"
    echo
    echo "Recommended commands:"

    for t in "${targets[@]}"; do
        case "$t" in
            neml2)
                echo "  • Recompile downstream consumers using NEML2:"
                echo "    ./scripts/mac/moose.sh compile solid_mechanics"
                echo "    ./scripts/mac/moose.sh compile shifted_boundary_method"
                echo "  • Run NEML2 regression tests:"
                echo "    ./scripts/mac/moose.sh test neml2"
                ;;
            solid_mechanics)
                echo "  • Run solid_mechanics regression tests:"
                echo "    ./scripts/mac/moose.sh test solid_mechanics"
                echo "    ./scripts/mac/moose.sh test neml2"
                echo "  • Run a solid_mechanics simulation input:"
                echo "    ./scripts/mac/moose.sh run solid_mechanics path/to/input.i"
                echo "  • Run C++ GoogleTest unit tests:"
                echo "    ./scripts/mac/moose.sh unit solid_mechanics"
                echo "    ./scripts/mac/moose.sh unit RankTwoTensor*"
                ;;
            shifted_boundary_method|sbm)
                echo "  • Run shifted boundary method (SCZM) regression tests:"
                echo "    ./scripts/mac/moose.sh test sczm"
                echo "  • Run an SBM simulation input:"
                echo "    ./scripts/mac/moose.sh run shifted_boundary_method path/to/input.i"
                ;;
            framework)
                echo "  • Run framework Kokkos regression tests:"
                echo "    ./scripts/mac/moose.sh test framework-kokkos"
                echo "  • Run framework C++ unit tests:"
                echo "    ./scripts/mac/moose.sh unit"
                ;;
            unit|*_unit)
                echo "  • Run C++ unit tests:"
                echo "    ./scripts/mac/moose.sh unit ${t%_unit}"
                ;;
            all)
                echo "  • Run all regression tests:"
                echo "    ./scripts/mac/moose.sh test all"
                echo "  • Run C++ unit tests:"
                echo "    ./scripts/mac/moose.sh unit"
                ;;
        esac
    done

    echo "  • Check executable linkages and overall pipeline status:"
    echo "    ./scripts/mac/moose.sh dummy"
}
