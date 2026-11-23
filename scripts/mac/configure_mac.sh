#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$SCRIPT_DIR/moose_mac_common.sh"

WITHOUT_NEML2=0

for arg in "$@"; do
    case "$arg" in
        --without-neml2|--no-neml2)
            WITHOUT_NEML2=1
            ;;
        *)
            moose_mac_die "Unknown configure option '$arg'."
            ;;
    esac
done

moose_mac_activate
moose_mac_locate_repo
moose_mac_env_exports

set -u

moose_mac_check_arch
moose_mac_check_sdk

if ! moose_mac_torch_ok; then
    moose_mac_die "Torch is not importable. Run ./scripts/mac/bootstrap_mac.sh first."
fi

neml2_configure_flag="--with-neml2=no"
if [[ "$WITHOUT_NEML2" -eq 0 ]]; then
    if ! moose_mac_neml2_ok; then
        moose_mac_die "NEML2 is not importable. Run ./scripts/mac/moose.sh init first, or pass --without-neml2."
    fi
    moose_mac_export_torch_neml2
    neml2_configure_flag="--with-neml2=$NEML2_DIR"
else
    export LIBTORCH_DIR="$(
        python -c 'import torch; print(torch.__path__[0])'
    )"
fi

moose_mac_dedupe_torch_libomp

cd "$MOOSE_DIR"

moose_mac_section "Configuring MOOSE"
echo "MOOSE_DIR=$MOOSE_DIR"
echo "LIBTORCH_DIR=$LIBTORCH_DIR"
if [[ "$WITHOUT_NEML2" -eq 0 ]]; then
    echo "NEML2_DIR=$NEML2_DIR"
else
    echo "NEML2: DISABLED (--without-neml2)"
fi

./configure \
    --with-libtorch="$LIBTORCH_DIR" \
    "$neml2_configure_flag" \
    --with-kokkos=yes

moose_mac_section "Configuration summary"

grep -E \
    'ENABLE_LIBTORCH|ENABLE_NEML2|ENABLE_KOKKOS|NEML2_DIR|LIBTORCH_DIR' \
    conf_vars.mk \
    || true

echo
echo "CONFIGURE_COMPLETED=1"
