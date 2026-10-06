#!/bin/bash

CC="$CONDA_PREFIX/bin/clang" \
CXX="$CONDA_PREFIX/bin/clang++" \
neml2-compile \
  exact_kinematics_neml2_aoti.i \
  --model model \
  --device cpu \
  -d neml2_stress:deformation_gradient
