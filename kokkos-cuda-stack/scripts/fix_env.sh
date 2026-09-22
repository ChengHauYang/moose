#!/bin/bash
# One-shot cleanup for the MOOSE build tree.
#
# When a previous MOOSE ./configure ran with a bad PATH (e.g. `python3-config` resolving to
# system /usr/bin -> Python 3.10) it baked "-lpython3.10 -lcrypt -ldl" into every .la file's
# dependency_libs. libtool re-uses .la dependency_libs on every subsequent link, so a plain
# `make -j` after fixing PATH keeps producing binaries linked against libpython3.10 (which
# then SIGSEGVs at runtime when NEML2 loads its own libpython3.12).
#
# The fix is to wipe every stale .la / .so / .libs / build directory / executable so the next
# build's configure regenerates .la files from the (now-correct) PATH. env.sh has been fixed
# to put miniforge/bin before /usr/bin so `python3-config` returns -lpython3.12; run this
# script once, then rerun build_moose.sh.
#
# This does NOT touch the installed stack under $PREFIX (openmpi, petsc, libmesh, wasp),
# framework/contrib/neml2, or the neml2-venv. Only MOOSE's own build artifacts.

set -e
set -o pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
STACK_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
MOOSE_DIR=$(cd "$STACK_DIR/.." && pwd)

echo "[fix_env] MOOSE_DIR = $MOOSE_DIR"
echo "[fix_env] wiping stale framework + solid_mechanics + test build artifacts"

# Per-subdir cleanup: libtool intermediates (.la/.lai/.lo/.o), Kokkos .K objects (.opt.o /
# .opt.o.d), the .libs/ directory libtool stages links in, and MOOSE's per-module build/
# scratch dir. Errors are non-fatal so a partial tree still gets cleaned.
for d in framework framework/contrib/hit framework/contrib/pcre test modules/solid_mechanics; do
  find "$MOOSE_DIR/$d" \
       \( -name '*.la' -o -name '*.lai' -o -name '*.lo' -o -name '*.o' \
          -o -name '*.opt.o' -o -name '*.opt.o.d' \
          -o -name '*.opt.lo' -o -name '*.opt.lo.d' \) \
       -type f -delete 2>/dev/null || true
  rm -rf "$MOOSE_DIR/$d/.libs"  2>/dev/null || true
  rm -rf "$MOOSE_DIR/$d/build"  2>/dev/null || true
done

# The versioned .so files (libmoose-opt.so.0.0.0 etc.) live alongside their .la in framework/
# and modules/*/lib -- glob them off, since the find above only matches by suffix.
rm -f "$MOOSE_DIR/framework/libmoose"*.la          "$MOOSE_DIR/framework/libmoose"*.so*
rm -f "$MOOSE_DIR/framework/contrib/hit/libhit"*.la  "$MOOSE_DIR/framework/contrib/hit/libhit"*.so*
rm -f "$MOOSE_DIR/framework/contrib/pcre/libpcre"*.la "$MOOSE_DIR/framework/contrib/pcre/libpcre"*.so*
rm -f "$MOOSE_DIR/test/moose_test-opt"
rm -f "$MOOSE_DIR/test/lib/libmoose_test"*.la      "$MOOSE_DIR/test/lib/libmoose_test"*.so*
rm -f "$MOOSE_DIR/test/lib/dlink.o"
rm -f "$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-opt"
rm -f "$MOOSE_DIR/modules/solid_mechanics/lib/libsolid_mechanics"*.la
rm -f "$MOOSE_DIR/modules/solid_mechanics/lib/libsolid_mechanics"*.so*

# Sanity check: verify env.sh's PATH resolves python3-config to Python 3.12 BEFORE the next
# build starts. Sourcing env.sh here purges the current shell, so run it in a subshell.
py_libs=$(bash -c ". $SCRIPT_DIR/env.sh >/dev/null 2>&1; python3-config --embed --libs 2>&1" || true)
case "$py_libs" in
  *-lpython3.12*)
    echo "[fix_env] env.sh sanity check: python3-config --embed --libs OK ($py_libs)" ;;
  *)
    echo "[fix_env] WARNING: env.sh's python3-config does NOT return -lpython3.12" >&2
    echo "[fix_env]          got: $py_libs" >&2
    echo "[fix_env]          the next build will fail its preflight with the same message." >&2
    echo "[fix_env]          check that /home/chenghau.yang/miniforge/bin/python3-config exists" >&2
    echo "[fix_env]          and that env.sh's PATH lists it before /usr/bin." >&2
    ;;
esac

echo "[fix_env] done. Next: bash $SCRIPT_DIR/build_moose.sh"
