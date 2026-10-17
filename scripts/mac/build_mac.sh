#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$SCRIPT_DIR/moose_mac_common.sh"

TARGET="${1:-solid_mechanics}"

moose_mac_activate
moose_mac_locate_repo
moose_mac_env_exports

set -u

moose_mac_check_arch
moose_mac_check_sdk

if ! moose_mac_configured; then
    moose_mac_die "MOOSE is not configured. Run ./scripts/mac/configure_mac.sh first."
fi

if [[ "$TARGET" == "all" ]]; then
    targets=(framework shifted_boundary_method solid_mechanics)
else
    targets=("$TARGET")
fi

for target in "${targets[@]}"; do
    module_dir="$(moose_mac_module_dir "$target")"

    moose_mac_section "Building $target"
    echo "Directory: $module_dir"

    cd "$module_dir"
    make -j"$BUILD_JOBS" METHOD="$METHOD"
done

echo
echo "BUILD_COMPLETED=1"
