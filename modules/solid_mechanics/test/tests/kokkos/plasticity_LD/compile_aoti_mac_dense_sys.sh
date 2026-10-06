# DENSE
CC="$CONDA_PREFIX/bin/clang" \
CXX="$CONDA_PREFIX/bin/clang++" \
neml2-compile \
  exact_kinematics_neml2_aoti_dense.i \
  --model model \
  --device cpu \
  --output-dir aoti_dense \
  -d neml2_stress:deformation_gradient
