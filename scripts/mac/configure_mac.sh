#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$SCRIPT_DIR/moose_mac_common.sh"

moose_mac_activate
moose_mac_locate_repo
moose_mac_env_exports

set -u

moose_mac_check_arch
moose_mac_check_sdk

if ! moose_mac_torch_ok; then
    moose_mac_die "Torch is not importable. Run ./scripts/mac/bootstrap_mac.sh first."
fi

if ! moose_mac_neml2_ok; then
    moose_mac_die "NEML2 is not importable. Run ./scripts/mac/bootstrap_mac.sh first."
fi

moose_mac_dedupe_torch_libomp
moose_mac_export_torch_neml2

cd "$MOOSE_DIR"

moose_mac_section "Configuring MOOSE"
echo "MOOSE_DIR=$MOOSE_DIR"
echo "LIBTORCH_DIR=$LIBTORCH_DIR"
echo "NEML2_DIR=$NEML2_DIR"

./configure \
    --with-libtorch="$LIBTORCH_DIR" \
    --with-neml2=yes \
    --with-kokkos=yes

moose_mac_section "Configuration summary"

grep -E \
    'ENABLE_LIBTORCH|ENABLE_NEML2|ENABLE_KOKKOS|NEML2_DIR|LIBTORCH_DIR' \
    conf_vars.mk \
    || true

echo
echo "CONFIGURE_COMPLETED=1"
