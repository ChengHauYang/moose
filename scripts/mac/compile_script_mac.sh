#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"

"$SCRIPT_DIR/bootstrap_mac.sh"
"$SCRIPT_DIR/configure_mac.sh"
"$SCRIPT_DIR/build_mac.sh" solid_mechanics
"$SCRIPT_DIR/test_mac.sh" neml2

echo
echo "COMPILE_SCRIPT_MAC_COMPLETED=1"
