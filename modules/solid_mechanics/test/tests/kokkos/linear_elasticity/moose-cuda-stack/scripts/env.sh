#!/bin/bash
# Clean toolchain environment for the from-scratch MOOSE-CUDA build.
# Source this: `. scripts/env.sh` — do NOT execute.
#
# Design: aggressively erase every env var that (a) conda's activation
# stamps in (build_alias, CMAKE_ARGS, CXX_FOR_BUILD, CPP, GCC_RANLIB, all
# CONDA_*, SSL/CURL/REQUESTS_CA_BUNDLE, GSETTINGS_*, XML_CATALOG_FILES),
# (b) autotools/petsc/libmesh read to override compilers, or (c) leak
# conda-provided library paths into linker/preprocessor search. Then
# re-export only the small set of variables we actually want to be set.
# Anything not on the keep-list is unset.
#
# The `configure_libmesh.sh` step showed why this matters: with even one
# conda leftover (`build_alias=x86_64-conda-linux-gnu`), autoconf detected
# the build triplet as conda-linux-gnu and then went looking for
# `x86_64-conda-linux-gnu-mpicc`, which resolved back into the conda env.

KEEP_VARS="HOME USER LOGNAME PWD OLDPWD SHELL TERM DISPLAY XAUTHORITY MAIL LANG LC_ALL TMPDIR"
KEEP_VARS="$KEEP_VARS SSH_AUTH_SOCK SSH_CONNECTION SSH_CLIENT SSH_TTY XDG_RUNTIME_DIR"
KEEP_VARS="$KEEP_VARS MOOSE_CUDA_STACK_KEEP"  # escape hatch

# Extra vars the user may have set intentionally before sourcing env.sh
for extra in ${MOOSE_CUDA_STACK_KEEP:-}; do
  KEEP_VARS="$KEEP_VARS $extra"
done

# Build a Python one-liner that unsets everything not in KEEP_VARS.
# `env -i` inside a running shell is not an option — we need to keep the
# current shell alive for the caller — so we `unset` by name instead.
KEEP_SET=" $(echo $KEEP_VARS | tr -s ' ' | sed 's/ / /g') "
for var in $(compgen -e); do
  case " $KEEP_SET " in
    *" $var "*) : keep ;;
    *) unset "$var" ;;
  esac
done

# Deterministic PATH: cuda first, then system, then sbin for ldconfig.
export PATH=/usr/local/cuda/bin:/usr/bin:/bin:/sbin:/usr/sbin

# Anchor points. Derive MOOSE_DIR from this script's location so the tree
# is portable — env.sh lives at
#   $MOOSE_DIR/modules/solid_mechanics/test/tests/kokkos/linear_elasticity/moose-cuda-stack/scripts/env.sh
# so MOOSE_DIR is 7 levels up.
_ENV_SH_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
export MOOSE_DIR=$(cd "$_ENV_SH_DIR/../../../../../../../.." && pwd)

# STACK_DIR is the OUT-OF-TREE prefix for from-scratch builds (PETSc,
# libmesh, WASP install here). Default: ~/moose-cuda-stack. Override by
# exporting STACK_DIR before sourcing env.sh (needs to be on the keep
# list — either add STACK_DIR to $MOOSE_CUDA_STACK_KEEP or set both).
: ${STACK_DIR:=$HOME/moose-cuda-stack}
export STACK_DIR
export PREFIX=${STACK_DIR}/prefix
export LOGS=${STACK_DIR}/logs
mkdir -p "$PREFIX" "$LOGS"

# PETSc / libmesh / MOOSE will use these values
export PETSC_SRC_DIR=${MOOSE_DIR}/petsc
export PETSC_ARCH=arch-scratch-cuda
export LIBMESH_SRC_DIR=${MOOSE_DIR}/libmesh

# CUDA
export CUDA_DIR=/usr/local/cuda

# WASP (built into $PREFIX by scripts/build_wasp.sh)
export WASP_DIR="$PREFIX"

# OpenMPI needs OMPI_FC because system /usr/bin/gfortran symlink is absent.
export OMPI_CC=/usr/bin/gcc-11
export OMPI_CXX=/usr/bin/g++-11
export OMPI_FC=/usr/bin/gfortran-9

# Parallel build. Machine has 32 cores; use 8 to leave headroom.
export MOOSE_JOBS=8

# Announce, if run interactively.
if [ -n "${PS1:-}" ]; then
  echo "[env.sh] purged non-safelist env; PATH=$PATH"
  echo "[env.sh] MOOSE_DIR=$MOOSE_DIR"
  echo "[env.sh] STACK_DIR=$STACK_DIR (override: export STACK_DIR=... before sourcing)"
fi
