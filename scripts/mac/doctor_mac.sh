#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$SCRIPT_DIR/moose_mac_common.sh"

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
        *)
            moose_mac_die "Unknown doctor option '$arg'."
            ;;
    esac
done

moose_mac_activate
moose_mac_locate_repo
moose_mac_env_exports
moose_mac_print_env

echo
echo "============================================"
echo " Doctor"
echo "============================================"

ARCH_OK=0
SDK_OK=0
TORCH_OK=0
NEML2_PY_OK=0
NEML2_SYNCED=0
NEML2_DIR_SYNCED=0
MOOSE_CONFIGURED=0
KOKKOS_ENABLED=0
NEML2_ENABLED=0
LIBTORCH_ENABLED=0

committed_hash="$(moose_mac_neml2_committed_hash)"
sub_hash="$(moose_mac_neml2_submodule_hash)"
sub_branch="$(moose_mac_neml2_submodule_branch)"
local_hash="$(moose_mac_local_neml2_hash)"
conda_hash="$(moose_mac_conda_neml2_hash)"
inst_hash="$(moose_mac_installed_neml2_hash)"
conf_neml2_dir="$(moose_mac_configured_neml2_dir)"
active_neml2_dir="$(moose_mac_active_neml2_dir)"

[[ "$(uname -m)" == "arm64" ]] && ARCH_OK=1
[[ -n "${SDKROOT:-}" && -d "${SDKROOT:-}" ]] && SDK_OK=1
moose_mac_torch_ok && TORCH_OK=1
moose_mac_neml2_ok && NEML2_PY_OK=1
moose_mac_neml2_synced && NEML2_SYNCED=1
[[ -n "$conf_neml2_dir" && "$conf_neml2_dir" == "$active_neml2_dir" ]] && NEML2_DIR_SYNCED=1
moose_mac_configured && MOOSE_CONFIGURED=1
moose_mac_kokkos_enabled && KOKKOS_ENABLED=1
moose_mac_neml2_enabled && NEML2_ENABLED=1
moose_mac_libtorch_enabled && LIBTORCH_ENABLED=1

echo "ARCH_OK=$ARCH_OK"
echo "SDK_OK=$SDK_OK"
echo "TORCH_OK=$TORCH_OK"
echo "NEML2_MODE=$(moose_mac_neml2_mode)"
echo "NEML2_PY_OK=$NEML2_PY_OK"
echo "NEML2_SYNCED=$NEML2_SYNCED"
echo "NEML2_DIR_SYNCED=$NEML2_DIR_SYNCED"
[[ -n "$committed_hash" ]] && echo "NEML2_COMMITTED_HASH=${committed_hash:0:10}"
[[ -n "$sub_hash" ]] && echo "NEML2_SUBMODULE_HASH=${sub_hash:0:10} ($sub_branch)"
[[ -n "$local_hash" ]] && echo "NEML2_LOCAL_HASH=${local_hash:0:10}" || echo "NEML2_LOCAL_HASH=none"
[[ -n "$conda_hash" ]] && echo "NEML2_CONDA_HASH=${conda_hash:0:10}" || echo "NEML2_CONDA_HASH=none"
[[ -n "$inst_hash" ]] && echo "NEML2_INSTALLED_HASH=${inst_hash:0:10}"
echo "MOOSE_CONFIGURED=$MOOSE_CONFIGURED"
echo "KOKKOS_ENABLED=$KOKKOS_ENABLED"
echo "KOKKOS_BACKENDS=$(moose_mac_detect_kokkos_backends)"
echo "NEML2_ENABLED=$NEML2_ENABLED"
echo "LIBTORCH_ENABLED=$LIBTORCH_ENABLED"

echo
moose_mac_print_neml2_notices

echo
echo "MPI_WRAPPERS:"
echo "mpicc:"
moose_mac_mpi_show_cmd mpicc || true
echo
echo "mpicxx:"
moose_mac_mpi_show_cmd mpicxx || true

echo
echo "TARGETS:"
for target in framework solid_mechanics shifted_boundary_method unit; do
    exe="$(moose_mac_module_exe "$target")"
    if [[ -n "$exe" && -x "$exe" ]]; then
        echo "${target}=BUILT"
    else
        echo "${target}=MISSING"
    fi
done

echo
if [[ "$SDK_OK" -eq 0 ]]; then
    echo "RECOMMENDED_ACTION=install_sdk"
    echo 'RECOMMENDED_COMMAND="$CONDA_PREFIX/share/moose-compilers/scripts/get_mac_sdk.sh"'
elif [[ "$TORCH_OK" -eq 0 || "$NEML2_PY_OK" -eq 0 || "$NEML2_SYNCED" -eq 0 || "$NEML2_DIR_SYNCED" -eq 0 ]]; then
    echo "RECOMMENDED_ACTION=init_or_compile"
    echo "RECOMMENDED_COMMAND=./scripts/mac/moose.sh compile solid_mechanics"
elif [[ "$MOOSE_CONFIGURED" -eq 0 || "$KOKKOS_ENABLED" -eq 0 || "$NEML2_ENABLED" -eq 0 || "$LIBTORCH_ENABLED" -eq 0 ]]; then
    echo "RECOMMENDED_ACTION=configure"
    echo "RECOMMENDED_COMMAND=./scripts/mac/configure_mac.sh"
else
    echo "RECOMMENDED_ACTION=build_or_test"
    echo "RECOMMENDED_COMMAND=./scripts/mac/moose.sh compile <target>"
fi
