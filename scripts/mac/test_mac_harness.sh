#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd -P)"
# shellcheck source=scripts/mac/moose_mac_common.sh
source "$SCRIPT_DIR/moose_mac_common.sh"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }

# 1. A failed-build marker alone must not recommend clobber.
repo="$tmp/repo1"
mkdir -p "$repo"
MOOSE_DIR="$repo"
touch "$repo/.moose_mac_build_failed"
reason="$(METHOD=opt moose_mac_clobberall_reason)"
[[ -z "$reason" ]] || fail "marker alone triggered clobber: $reason"

# 2. Corruption heuristic respects METHOD for object/library names.
repo2="$tmp/repo2"
mkdir -p "$repo2/framework/src"
touch "$repo2/framework/src/foo.dbg.lo"
# no libmoose-dbg -> should recommend
MOOSE_DIR="$repo2" METHOD=dbg reason="$(moose_mac_clobberall_reason)"
[[ "$reason" == *"libmoose is missing"* ]] || fail "dbg method not handled: $reason"

# 3. install completeness requires eager library.
site="$tmp/site"
mkdir -p "$site/lib" "$site/share/pkgconfig" \
  "$site/include/neml2/csrc/eager" \
  "$site/include/neml2/csrc/dispatchers"
touch "$site/share/pkgconfig/neml2.pc" "$site/lib/libneml2.dylib"
for h in \
  "neml2/csrc/eager/Model.h" \
  "neml2/csrc/eager/load_model.h" \
  "neml2/csrc/dispatchers/factory.h" \
  "neml2/csrc/dispatchers/DispatchedModel.h" \
  "neml2/csrc/dispatchers/MPISimpleScheduler.h"; do
  touch "$site/include/$h"
done
# missing eager -> incomplete
moose_mac_neml2_install_complete "$site" && fail "install complete without eager lib"
touch "$site/lib/libneml2_eager.dylib"
moose_mac_neml2_install_complete "$site" || fail "install incomplete with eager lib"

# 4. Help text updated.
grep -q "retain installation sanity checks" "$ROOT/scripts/mac/moose.sh" || fail "help text not updated"

echo "All macOS harness checks passed."
