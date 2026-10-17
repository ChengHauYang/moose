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
moose_mac_print_env

moose_mac_section "Torch setup"
moose_mac_ensure_torch
moose_mac_dedupe_torch_libomp
moose_mac_verify_torch

moose_mac_section "NEML2 submodule"
moose_mac_ensure_neml2_submodule

moose_mac_build_neml2

moose_mac_export_torch_neml2

moose_mac_section "Bootstrap result"
echo "LIBTORCH_DIR=$LIBTORCH_DIR"
echo "NEML2_DIR=$NEML2_DIR"

python - <<'PY'
import torch
import neml2
print("torch:", torch.__version__, torch.__file__)
print("neml2:", neml2.__file__)
PY

echo
echo "BOOTSTRAP_COMPLETED=1"
