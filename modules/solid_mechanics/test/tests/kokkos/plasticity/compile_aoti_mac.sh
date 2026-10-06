#!/bin/bash
set -e

CC="$CONDA_PREFIX/bin/clang" \
CXX="$CONDA_PREFIX/bin/clang++" \
neml2-compile \
  perfect_neml2_aoti.i \
  --model model \
  --device cpu \
  --output-dir aoti \
  -d neml2_stress:neml2_strain
