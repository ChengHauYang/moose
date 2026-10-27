#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$SCRIPT_DIR/moose_mac_common.sh"

TARGET="solid_mechanics"
RAW_FLAG=0

for arg in "$@"; do
    case "$arg" in
        --raw)
            RAW_FLAG=1
            ;;
        --profile)
            export MOOSE_MAC_PROFILE=1
            ;;
        --use-conda-neml2)
            export MOOSE_NEML2_SRC=conda
            ;;
        --use-committed-neml2)
            export MOOSE_NEML2_SRC=committed
            ;;
        --use-local-neml2)
            export MOOSE_NEML2_SRC=local
            ;;
        --skip-neml2-sync)
            export MOOSE_MAC_SKIP_NEML2_SYNC=1
            ;;
        -*)
            moose_mac_die "Unknown option '$arg' for build_mac.sh."
            ;;
        *)
            TARGET="$arg"
            ;;
    esac
done

if [[ "$RAW_FLAG" -eq 1 ]]; then
    export MOOSE_MAC_RAW=1
fi

moose_mac_activate
moose_mac_locate_repo
moose_mac_env_exports

set -u

moose_mac_check_arch
moose_mac_check_sdk

if [[ "$TARGET" == "neml2" ]]; then
    moose_mac_ensure_neml2_submodule
    if ! moose_mac_build_neml2; then
        touch "$MOOSE_DIR/.moose_mac_build_failed"
        touch "$(moose_mac_build_failed_marker "neml2" "${METHOD:-opt}")"
        exit 1
    fi
    rm -f "$(moose_mac_build_failed_marker "neml2" "${METHOD:-opt}")"
    if ! ls "$MOOSE_DIR"/.moose_mac_build_failed.*.* >/dev/null 2>&1; then
        rm -f "$MOOSE_DIR/.moose_mac_build_failed"
    fi
    moose_mac_sync_neml2_config_dir
    moose_mac_recommend_next_steps_after_build "neml2"
    echo
    echo "BUILD_COMPLETED=1"
    exit 0
fi

if ! moose_mac_configured; then
    moose_mac_die "MOOSE is not configured. Run ./scripts/mac/configure_mac.sh first."
fi

if moose_mac_neml2_enabled; then
    if ! moose_mac_neml2_install_complete; then
        moose_mac_die "Installed NEML2 is incomplete (missing neml2.pc, libneml2.dylib, libneml2_eager.dylib, or a required header such as neml2/csrc/eager/Model.h). Rebuild NEML2 first: ./scripts/mac/moose.sh compile neml2"
    fi
fi

if moose_mac_neml2_enabled && [[ "${MOOSE_MAC_NEML2_ALREADY_SYNCED:-0}" != "1" ]]; then
    moose_mac_sync_neml2
fi

if [[ "$TARGET" == "all" ]]; then
    targets=(framework shifted_boundary_method solid_mechanics)
else
    targets=("$TARGET")
fi

for target in "${targets[@]}"; do
    if ! moose_mac_make_target "$target"; then
        touch "$MOOSE_DIR/.moose_mac_build_failed"
        touch "$(moose_mac_build_failed_marker "$target" "${METHOD:-opt}")"
        exit 1
    fi
    rm -f "$(moose_mac_build_failed_marker "$target" "${METHOD:-opt}")"
    if ! ls "$MOOSE_DIR"/.moose_mac_build_failed.*.* >/dev/null 2>&1; then
        rm -f "$MOOSE_DIR/.moose_mac_build_failed"
    fi
done

moose_mac_recommend_next_steps_after_build "${targets[@]}"

echo
echo "BUILD_COMPLETED=1"
