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

moose_mac_env_exports()
{
    export PATH="$CONDA_PREFIX/bin:$PATH"
    export LIBMESH_DIR="$CONDA_PREFIX"
    export WASP_DIR="$CONDA_PREFIX"
    export METHOD="${METHOD:-opt}"
    export JOBS="${JOBS:-8}"
    export BUILD_JOBS="${BUILD_JOBS:-5}"
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

moose_mac_neml2_ok()
{
    python -c 'import neml2' >/dev/null 2>&1
}

moose_mac_ensure_neml2_submodule()
{
    cd "$MOOSE_DIR"

    git submodule sync --recursive
    git submodule update --init --recursive

    if [[ ! -f framework/contrib/neml2/CMakeLists.txt ]]; then
        moose_mac_section "Initializing framework/contrib/neml2"
        git submodule update \
            --init \
            --checkout \
            --force \
            --recursive \
            framework/contrib/neml2
    fi

    [[ -f framework/contrib/neml2/CMakeLists.txt ]] || \
        moose_mac_die "NEML2 submodule is not initialized."
}

moose_mac_build_neml2()
{
    cd "$MOOSE_DIR"

    moose_mac_section "Building NEML2 v3"

    NEML2_JOBS="${NEML2_JOBS:-$JOBS}" \
    ./scripts/update_and_rebuild_neml2.sh \
        --skip-submodule-update \
        -DCMAKE_C_COMPILER=/usr/bin/clang \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
        -DMPI_C_COMPILER="$CONDA_PREFIX/bin/mpicc" \
        -DMPI_CXX_COMPILER="$CONDA_PREFIX/bin/mpicxx" \
        --config-settings=build-dir=build/moose-apple
}

moose_mac_export_torch_neml2()
{
    export LIBTORCH_DIR="$(
        python -c 'import torch; print(torch.__path__[0])'
    )"

    export NEML2_DIR="$(
        python -c 'import neml2, os; print(os.path.dirname(neml2.__file__))'
    )"
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
        framework|test)
            echo "$MOOSE_DIR/test"
            ;;
        solid_mechanics)
            echo "$MOOSE_DIR/modules/solid_mechanics"
            ;;
        shifted_boundary_method|sbm)
            echo "$MOOSE_DIR/modules/shifted_boundary_method"
            ;;
        *)
            moose_mac_die "Unknown build target '$1'. Use: framework, solid_mechanics, shifted_boundary_method, all"
            ;;
    esac
}

moose_mac_module_exe()
{
    case "$1" in
        framework|test)
            echo "$MOOSE_DIR/test/moose_test-opt"
            ;;
        solid_mechanics)
            echo "$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-opt"
            ;;
        shifted_boundary_method|sbm)
            echo "$MOOSE_DIR/modules/shifted_boundary_method/shifted_boundary_method-opt"
            ;;
        *)
            echo ""
            ;;
    esac
}
