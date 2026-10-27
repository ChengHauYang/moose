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
            moose_mac_die "Unknown dummy option '$arg'."
            ;;
    esac
done

moose_mac_activate
moose_mac_locate_repo
moose_mac_env_exports

echo
echo "============================================"
echo " MOOSE-SCZM Dummy Dashboard & Status"
echo "============================================"

# Stage checks
ARCH_OK=0
SDK_OK=0
TORCH_OK=0
NEML2_SUBMODULE_OK=0
NEML2_OK=0
MOOSE_CONFIGURED=0
KOKKOS_ENABLED=0
NEML2_ENABLED=0

[[ "$(uname -m)" == "arm64" ]] && ARCH_OK=1
[[ -n "${SDKROOT:-}" && -d "${SDKROOT:-}" ]] && SDK_OK=1
moose_mac_torch_ok && TORCH_OK=1
[[ -f "$MOOSE_DIR/framework/contrib/neml2/CMakeLists.txt" ]] && NEML2_SUBMODULE_OK=1
moose_mac_neml2_ok && NEML2_OK=1
NEML2_INSTALL_COMPLETE=0
moose_mac_neml2_install_complete && NEML2_INSTALL_COMPLETE=1
moose_mac_configured && MOOSE_CONFIGURED=1
moose_mac_kokkos_enabled && KOKKOS_ENABLED=1
moose_mac_neml2_enabled && NEML2_ENABLED=1

# ----------------------------------------------------------------------------
# 1. Executables (-opt) Status & Linkages
# ----------------------------------------------------------------------------
echo
echo "--- [1] Target Executables (-opt) ---"
ready_count=0
stale_count=0
missing_count=0
total_count=4
targets=(framework solid_mechanics shifted_boundary_method unit)
ready_targets=()
stale_targets=()
missing_targets=()

for target in "${targets[@]}"; do
    exe="$(moose_mac_module_exe "$target")"
    name="$target"
    [[ "$target" == "unit" ]] && name="unit (framework)"

    moose_mac_target_status "$target"

    if [[ "$target" == "solid_mechanics" ]]; then
        solid_status="$TARGET_STATUS"
        solid_stale_reason="$TARGET_STALE_REASON"
        solid_stale_kind="$TARGET_STALE_KIND"
        solid_built_hash="$TARGET_BUILT_NEML2_HASH"
        solid_linkage="$TARGET_NEML2_LINKAGE"
    fi

    case "$TARGET_STATUS" in
        READY)
            ready_count=$((ready_count + 1))
            ready_targets+=("$target")
            echo "  • $name: [READY]"
            echo "    Path   : $exe"
            if [[ "$TARGET_NEML2_LINKAGE" != "WITHOUT NEML2" ]]; then
                echo "    NEML2  : $TARGET_NEML2_LINKAGE (active: ${TARGET_ACTIVE_NEML2_HASH:0:10})"
            else
                echo "    NEML2  : $TARGET_NEML2_LINKAGE"
            fi
            echo "    Kokkos : $TARGET_KOKKOS_LINKAGE"
            ;;
        STALE)
            stale_count=$((stale_count + 1))
            stale_targets+=("$target")
            echo "  • $name: [STALE - Recompile Needed]"
            echo "    Path   : $exe"
            echo "    NEML2  : $TARGET_NEML2_LINKAGE"
            echo "    Issue  : [OUTDATED] $TARGET_STALE_REASON"
            echo "    Kokkos : $TARGET_KOKKOS_LINKAGE"
            ;;
        MISSING|*)
            missing_count=$((missing_count + 1))
            missing_targets+=("$target")
            echo "  • $name: [MISSING] (Not built yet)"
            ;;
    esac
    echo
done

# ----------------------------------------------------------------------------
# 2. Kokkos Configuration
# ----------------------------------------------------------------------------
echo "--- [2] Kokkos Configuration ---"
if [[ "$KOKKOS_ENABLED" -eq 1 ]]; then
    echo "  Status   : ENABLED"
    echo "  Backends : $(moose_mac_detect_kokkos_backends)"
    echo "  GPU mode : $(grep -q 'ENABLE_KOKKOS_GPU.*true' "$MOOSE_DIR/conf_vars.mk" 2>/dev/null && echo 'true' || echo 'false')"
else
    echo "  Status   : DISABLED"
fi
echo

# ----------------------------------------------------------------------------
# 3. NEML2 Submodule & Integration Status
# ----------------------------------------------------------------------------
echo "--- [3] NEML2 Integration & Submodule Status ---"
if [[ "$NEML2_SUBMODULE_OK" -eq 0 ]]; then
    echo "  Submodule : [MISSING] (framework/contrib/neml2 is not initialized)"
    echo
    echo "============================================"
    echo " NEML2 Submodule Guidance (Submodule Missing)"
    echo "============================================"
    echo "The NEML2 submodule (framework/contrib/neml2) is not checked out."
    echo
    echo "Choose how you want to proceed:"
    echo
    echo "  Option A: Build and run WITH NEML2 (Material models & Torch/ML integration):"
    echo "    1. Initialize submodule & build repo-isolated NEML2:"
    echo "       ./scripts/mac/moose.sh init"
    echo "    2. Compile solid_mechanics:"
    echo "       ./scripts/mac/moose.sh compile solid_mechanics"
    echo "    3. Run NEML2 regression tests:"
    echo "       ./scripts/mac/moose.sh test neml2"
    echo
    echo "  Option B: Build and run WITHOUT NEML2 (Pure FEM, SBM, or faster minimal builds):"
    echo "    1. Reconfigure MOOSE to disable NEML2:"
    echo "       ./scripts/mac/configure_mac.sh --without-neml2"
    echo "    2. Compile targets that do not require NEML2:"
    echo "       ./scripts/mac/moose.sh compile framework"
    echo "       ./scripts/mac/moose.sh compile shifted_boundary_method"
    echo "    3. Run tests without NEML2:"
    echo "       ./scripts/mac/moose.sh test framework-kokkos"
    echo "       ./scripts/mac/moose.sh test sczm"
    echo "============================================"
else
    echo "  Submodule : [INITIALIZED] (framework/contrib/neml2 is ready)"
    moose_mac_print_neml2_notices

    committed_hash="$(moose_mac_neml2_committed_hash)"
    sub_hash="$(moose_mac_neml2_submodule_hash)"
    sub_branch="$(moose_mac_neml2_submodule_branch)"
    local_hash="$(moose_mac_local_neml2_hash)"
    conda_hash="$(moose_mac_conda_neml2_hash)"
    active_backend="$(moose_mac_effective_neml2_backend)"

    NEML2_SUBMODULE_OUT_OF_SYNC=0
    if [[ -n "$sub_hash" && -n "$committed_hash" && "$sub_hash" != "$committed_hash" ]]; then
        NEML2_SUBMODULE_OUT_OF_SYNC=1
        echo
        echo "  [WARNING] Submodule framework/contrib/neml2 is OUT OF SYNC with MOOSE commit!"
        echo "            MOOSE committed SHA : ${committed_hash:0:10}"
        echo "            Submodule checkout  : ${sub_hash:0:10} ($sub_branch)"
        echo "            -> Submodule was not updated after switching git branches or pulling MOOSE."
        echo "            -> Fix: Run './scripts/mac/moose.sh init' to update submodule."
    fi

    NEML2_NEEDS_COMPILE=0
    if [[ "$active_backend" == "local" && -n "$sub_hash" ]]; then
        if [[ -z "$local_hash" || "$sub_hash" != "$local_hash" ]]; then
            NEML2_NEEDS_COMPILE=1
            echo
            echo "  [NOTICE]  framework/contrib/neml2 source checkout != Repo-local installed NEML2!"
            echo "            Submodule checkout  : ${sub_hash:0:10} ($sub_branch)"
            echo "            Installed in repo   : ${local_hash:0:10:-not built}"
            echo "            -> NEML2 source changed, but repo-isolated NEML2 has not been compiled."
            echo "            -> Fix: Run './scripts/mac/moose.sh compile neml2' to build NEML2."
        fi
    fi

    SOLID_NEML2_MISMATCH=0
    if [[ "${solid_status:-}" == "STALE" && -z "${solid_stale_kind:-}" ]]; then
        SOLID_NEML2_MISMATCH=1
        echo
        echo "  [MISMATCH] framework/contrib/neml2 and solid_mechanics are using DIFFERENT versions of NEML2!"
        echo "             framework/contrib/neml2 : ${sub_hash:0:10} ($sub_branch)"
        echo "             solid_mechanics issue   : $solid_stale_reason"
        echo "             -> Fix: You must recompile NEML2 and solid_mechanics:"
        if [[ "$NEML2_NEEDS_COMPILE" -eq 1 ]]; then
            echo "                1. ./scripts/mac/moose.sh compile neml2"
            echo "                2. ./scripts/mac/moose.sh compile solid_mechanics"
        else
            echo "                • ./scripts/mac/moose.sh compile solid_mechanics"
        fi
    fi
fi
echo

# ----------------------------------------------------------------------------
# 4. Pipeline Progress Summary
# ----------------------------------------------------------------------------
echo "============================================"
echo " Pipeline Progress"
echo "============================================"

mark_ok="[✓]"
mark_warn="[!]"
mark_todo="[ ]"

[[ "$ARCH_OK" -eq 1 && "$SDK_OK" -eq 1 ]] && s1="$mark_ok" || s1="$mark_warn"
[[ "$TORCH_OK" -eq 1 ]] && s2="$mark_ok" || s2="$mark_warn"
if [[ "$NEML2_SUBMODULE_OK" -eq 1 && "$NEML2_OK" -eq 1 ]]; then
    if [[ "${NEML2_SUBMODULE_OUT_OF_SYNC:-0}" -eq 1 || "${NEML2_NEEDS_COMPILE:-0}" -eq 1 ]]; then
        s3="$mark_warn"
    else
        s3="$mark_ok"
    fi
elif [[ "$NEML2_SUBMODULE_OK" -eq 0 && "$NEML2_ENABLED" -eq 0 ]]; then
    s3="[-] (Disabled by user)"
else
    s3="$mark_warn"
fi
[[ "$MOOSE_CONFIGURED" -eq 1 ]] && s4="$mark_ok" || s4="$mark_warn"

if [[ "$ready_count" -eq "$total_count" ]]; then
    s5="$mark_ok"
    s5_desc="Stage 5: Target Executables ($ready_count/$total_count ready)"
elif [[ "$stale_count" -gt 0 ]]; then
    s5="$mark_warn"
    s5_desc="Stage 5: Target Executables ($ready_count/$total_count ready, $stale_count STALE, $missing_count missing)"
else
    s5="$mark_todo"
    s5_desc="Stage 5: Target Executables ($ready_count/$total_count ready, $missing_count missing)"
fi
s6="$mark_todo"

echo "  $s1 Stage 1: Apple Silicon arm64 & macOS SDK"
echo "  $s2 Stage 2: Conda Environment & PyTorch"
echo "  $s3 Stage 3: NEML2 Submodule & Library"
echo "  $s4 Stage 4: MOOSE Configuration (conf_vars.mk)"
echo "  $s5 $s5_desc"
echo "  $s6 Stage 6: Regression & Unit Tests"
echo

# ----------------------------------------------------------------------------
# 5. Next Step Recommendations
# ----------------------------------------------------------------------------
echo "============================================"
echo " Next Step Recommendations"
echo "============================================"

if [[ "$SDK_OK" -eq 0 ]]; then
    echo "Current bottleneck: macOS SDK is missing."
    echo "Run this command to install the SDK:"
    echo '  "$CONDA_PREFIX/share/moose-compilers/scripts/get_mac_sdk.sh"'
elif [[ "$TORCH_OK" -eq 0 ]]; then
    echo "Current bottleneck: PyTorch is missing in active environment."
    echo "Run this command to bootstrap Torch:"
    echo "  ./scripts/mac/moose.sh init"
elif [[ "$NEML2_SUBMODULE_OK" -eq 0 && "$NEML2_ENABLED" -eq 1 ]]; then
    echo "Current bottleneck: NEML2 submodule is not initialized."
    echo "  • To use NEML2 : ./scripts/mac/moose.sh init"
    echo "  • Without NEML2: ./scripts/mac/configure_mac.sh --without-neml2"
elif [[ "${NEML2_SUBMODULE_OUT_OF_SYNC:-0}" -eq 1 && "${MOOSE_NEML2_SRC:-}" != "local" ]]; then
    echo "Current bottleneck: Submodule framework/contrib/neml2 is out of sync with MOOSE!"
    echo "MOOSE expects commit ${committed_hash:0:10}, but checkout is at ${sub_hash:0:10}."
    echo
    echo "Run this command to update the submodule to match MOOSE:"
    echo "  ./scripts/mac/moose.sh init"
elif [[ "${NEML2_NEEDS_COMPILE:-0}" -eq 1 && "$stale_count" -eq 0 && "$ready_count" -eq 0 ]]; then
    echo "Current bottleneck: NEML2 source was updated, but the library has not been recompiled."
    echo
    echo "Run this command to compile NEML2:"
    echo "  ./scripts/mac/moose.sh compile neml2"
elif [[ "$MOOSE_CONFIGURED" -eq 0 ]]; then
    echo "Current bottleneck: MOOSE is not configured."
    echo "Run this command to configure MOOSE:"
    echo "  ./scripts/mac/configure_mac.sh"
elif [[ "$stale_count" -gt 0 ]]; then
    echo "Current bottleneck: Stale executables detected ($stale_count target(s))!"
    if [[ "${SOLID_NEML2_MISMATCH:-0}" -eq 1 ]]; then
        echo "framework/contrib/neml2 and solid_mechanics are using DIFFERENT versions of NEML2!"
        echo
        echo "You MUST recompile before running tests or simulations:"
        if [[ "${NEML2_NEEDS_COMPILE:-0}" -eq 1 ]]; then
            echo "  1. ./scripts/mac/moose.sh compile neml2"
            echo "  2. ./scripts/mac/moose.sh compile solid_mechanics"
        else
            echo "  ./scripts/mac/moose.sh compile solid_mechanics"
        fi
        for target in "${stale_targets[@]}"; do
            [[ "$target" == "solid_mechanics" ]] && continue
            echo "  ./scripts/mac/moose.sh compile $target"
        done
    else
        echo "NEML2 was switched or rebuilt, so existing binaries are linked against old code."
        echo
        echo "You MUST recompile stale targets before running tests or simulations:"
        for target in "${stale_targets[@]}"; do
            echo "  ./scripts/mac/moose.sh compile $target"
        done
    fi
    if [[ "$ready_count" -gt 0 ]]; then
        echo
        echo "Ready targets (can run tests):"
        for target in "${ready_targets[@]}"; do
            case "$target" in
                solid_mechanics) echo "  ./scripts/mac/moose.sh test solid_mechanics" ;;
                shifted_boundary_method) echo "  ./scripts/mac/moose.sh test sczm" ;;
                framework) echo "  ./scripts/mac/moose.sh test framework-kokkos" ;;
                unit) echo "  ./scripts/mac/moose.sh unit" ;;
            esac
        done
    fi
elif [[ "$ready_count" -eq 0 ]]; then
    echo "Current step: Executables have not been compiled yet."
    echo "Recommended commands:"
    echo "  • Compile solid_mechanics (default):"
    echo "    ./scripts/mac/moose.sh compile solid_mechanics"
    echo "  • Compile shifted_boundary_method (SCZM):"
    echo "    ./scripts/mac/moose.sh compile shifted_boundary_method"
    echo "  • Compile all targets:"
    echo "    ./scripts/mac/moose.sh compile all"
elif [[ "$ready_count" -lt "$total_count" ]]; then
    echo "Current step: Some targets are ready ($ready_count/$total_count), but some are missing ($missing_count missing)."
    echo "You can:"
    echo "  • Compile remaining targets:"
    for target in "${missing_targets[@]}"; do
        echo "    ./scripts/mac/moose.sh compile $target"
    done
    echo "  • Or run tests on ready targets:"
    for target in "${ready_targets[@]}"; do
        case "$target" in
            solid_mechanics) echo "    ./scripts/mac/moose.sh test solid_mechanics" ;;
            shifted_boundary_method) echo "    ./scripts/mac/moose.sh test sczm" ;;
            framework) echo "    ./scripts/mac/moose.sh test framework-kokkos" ;;
            unit) echo "    ./scripts/mac/moose.sh unit" ;;
        esac
    done
else
    echo "Current step: All core executables are built and up-to-date ($ready_count/$total_count)!"
    echo "Recommended commands:"
    echo "  • Run regression tests:"
    echo "    ./scripts/mac/moose.sh test solid_mechanics"
    echo "    ./scripts/mac/moose.sh test sczm"
    echo "    ./scripts/mac/moose.sh test neml2"
    echo "  • Run C++ GoogleTest unit tests:"
    echo "    ./scripts/mac/moose.sh unit"
    echo "    ./scripts/mac/moose.sh unit RankTwoTensor*"
    echo "  • Run a simulation input:"
    echo "    ./scripts/mac/moose.sh run solid_mechanics path/to/input.i"
    echo "    ./scripts/mac/moose.sh run shifted_boundary_method path/to/input.i"
fi
_clobber_reason="$(moose_mac_clobberall_reason)"
if [[ -n "$_clobber_reason" ]]; then
    echo
    echo "  [CLOBBERALL RECOMMENDED] $_clobber_reason"
    echo "  Full clobber is expensive; use only if the above signals apply:"
    echo "    ./scripts/mac/moose.sh clobber all"
fi

echo "============================================"
