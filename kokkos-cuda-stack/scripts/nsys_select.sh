#!/bin/bash
# Nsight Systems CLI selection shared by activate.sh, run_benchmark.sh, and
# scripts/cuda/moose_cuda.sh. Source this (bash or zsh) after STACK_DIR is
# set; do NOT execute.
#
# The nsys shipped with CUDA 12.4 ($CUDA_DIR/bin/nsys -> 2023.4.4)
# intermittently aborts the report import of MOOSE Kokkos runs with "Wrong
# event order"; 2024.2.3 imports the same runs. Standalone installs are
# unpacked without root under $NSYS_ROOT/<version>/ (see scripts/cuda/README.md).
#
#   NSYS_BIN          explicit absolute CLI path; if unusable, nothing else is selected
#   NSYS_ROOT         machine-local standalone installs, <root>/<version>/bin/nsys
#   NSYS_MIN_VERSION  oldest version preferred for profiling
#
# Without NSYS_BIN the newest standalone install, then $CUDA_DIR/bin/nsys, then
# nsys on PATH is tried; the first one at least NSYS_MIN_VERSION wins, else the
# first working older one. moose_nsys_select sets NSYS_SELECTED, NSYS_VERSION,
# and NSYS_SOURCE (NSYS_BIN, standalone, CUDA, or PATH).

: "${NSYS_ROOT:=$STACK_DIR/nsight-systems}"
# 2024.2.3 imported every step 4-6 plasticity_LD_sczm recording that 2023.4.4
# failed on intermittently; no release in between was tested.
: "${NSYS_MIN_VERSION:=2024.2}"

# Print the dotted version reported by `$1 --version`, or nothing.
moose_nsys_version() {
  { "$1" --version 2>/dev/null || true; } |
    sed -n 's/.*version \([0-9][0-9.]*[0-9]\).*/\1/p' | head -n 1
}

# Succeed if version $1 is at least version $2.
moose_nsys_at_least() {
  [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n 1)" = "$2" ]
}

# Print "source|path" lines in preference order.
moose_nsys_candidates() {
  if [ -d "$NSYS_ROOT" ]; then
    find "$NSYS_ROOT" -mindepth 3 -maxdepth 3 -path '*/bin/nsys' 2>/dev/null |
      sort -rV | sed 's/^/standalone|/'
  fi
  printf 'CUDA|%s\n' "${CUDA_DIR:-/usr/local/cuda}/bin/nsys"
  printf 'PATH|%s\n' "$(command -v nsys 2>/dev/null || true)"
}

moose_nsys_select() {
  NSYS_SELECTED='' NSYS_VERSION='' NSYS_SOURCE=''
  if [ -n "${NSYS_BIN:-}" ]; then
    NSYS_SOURCE=NSYS_BIN
    case "$NSYS_BIN" in /*) ;; *) return 1 ;; esac
    NSYS_VERSION=$(moose_nsys_version "$NSYS_BIN")
    [ -n "$NSYS_VERSION" ] || return 1
    NSYS_SELECTED=$NSYS_BIN
    return 0
  fi
  local _src _bin _ver _old='' _old_ver='' _old_src=''
  while IFS='|' read -r _src _bin; do
    [ -n "$_bin" ] && [ -x "$_bin" ] || continue
    _ver=$(moose_nsys_version "$_bin")
    [ -n "$_ver" ] || continue
    if moose_nsys_at_least "$_ver" "$NSYS_MIN_VERSION"; then
      NSYS_SELECTED=$_bin NSYS_VERSION=$_ver NSYS_SOURCE=$_src
      return 0
    fi
    [ -n "$_old" ] || { _old=$_bin; _old_ver=$_ver; _old_src=$_src; }
  done <<EOF
$(moose_nsys_candidates)
EOF
  [ -n "$_old" ] || return 1
  NSYS_SELECTED=$_old NSYS_VERSION=$_old_ver NSYS_SOURCE=$_old_src
}

# After moose_nsys_select, print why the selection is unusable or not
# preferred; print nothing when it is fine.
moose_nsys_problem() {
  if [ -z "$NSYS_SELECTED" ]; then
    if [ -n "${NSYS_BIN:-}" ]; then
      echo "NSYS_BIN=$NSYS_BIN is not a usable Nsight Systems CLI (absolute path required); refusing to fall back to another install"
    else
      echo "no Nsight Systems CLI found (NSYS_BIN, $NSYS_ROOT/<version>/bin/nsys, ${CUDA_DIR:-/usr/local/cuda}/bin/nsys, PATH)"
    fi
  elif ! moose_nsys_at_least "$NSYS_VERSION" "$NSYS_MIN_VERSION"; then
    echo "Nsight Systems $NSYS_VERSION ($NSYS_SELECTED) is older than $NSYS_MIN_VERSION; 2023.4.x intermittently fails report import with 'Wrong event order'. Install a newer standalone CLI under $NSYS_ROOT or set NSYS_BIN"
  fi
}
