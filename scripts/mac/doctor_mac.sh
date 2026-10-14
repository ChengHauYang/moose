#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$SCRIPT_DIR/moose_mac_common.sh"

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
MOOSE_CONFIGURED=0
KOKKOS_ENABLED=0
NEML2_ENABLED=0
LIBTORCH_ENABLED=0

[[ "$(uname -m)" == "arm64" ]] && ARCH_OK=1
[[ -n "${SDKROOT:-}" && -d "${SDKROOT:-}" ]] && SDK_OK=1
moose_mac_torch_ok && TORCH_OK=1
moose_mac_neml2_ok && NEML2_PY_OK=1
moose_mac_configured && MOOSE_CONFIGURED=1
moose_mac_kokkos_enabled && KOKKOS_ENABLED=1
moose_mac_neml2_enabled && NEML2_ENABLED=1
moose_mac_libtorch_enabled && LIBTORCH_ENABLED=1

echo "ARCH_OK=$ARCH_OK"
echo "SDK_OK=$SDK_OK"
echo "TORCH_OK=$TORCH_OK"
echo "NEML2_PY_OK=$NEML2_PY_OK"
echo "MOOSE_CONFIGURED=$MOOSE_CONFIGURED"
echo "KOKKOS_ENABLED=$KOKKOS_ENABLED"
echo "NEML2_ENABLED=$NEML2_ENABLED"
echo "LIBTORCH_ENABLED=$LIBTORCH_ENABLED"

echo
echo "MPI_WRAPPERS:"
echo "mpicc:"
moose_mac_mpi_show_cmd mpicc || true
echo
echo "mpicxx:"
moose_mac_mpi_show_cmd mpicxx || true

echo
echo "TARGETS:"
for target in framework solid_mechanics shifted_boundary_method; do
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
elif [[ "$TORCH_OK" -eq 0 || "$NEML2_PY_OK" -eq 0 ]]; then
    echo "RECOMMENDED_ACTION=bootstrap"
    echo "RECOMMENDED_COMMAND=./scripts/mac/bootstrap_mac.sh"
elif [[ "$MOOSE_CONFIGURED" -eq 0 || "$KOKKOS_ENABLED" -eq 0 || "$NEML2_ENABLED" -eq 0 || "$LIBTORCH_ENABLED" -eq 0 ]]; then
    echo "RECOMMENDED_ACTION=configure"
    echo "RECOMMENDED_COMMAND=./scripts/mac/configure_mac.sh"
else
    echo "RECOMMENDED_ACTION=build_or_test"
    echo "RECOMMENDED_COMMAND=./scripts/mac/build_mac.sh <target>"
fi
