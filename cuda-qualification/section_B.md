# Section B - 3D Mandel Action (Kokkos input + output backend, six-component Mandel)

Environment
- MOOSE HEAD: f0a0cd799b ([framework] Fix Kokkos-to-NEML2 tensor device labeling) on branch
  neml2-v3-kokkos-solid-mechanics-LD-mac
- NEML2 12f3d599, Kokkos 4.7.4, CUDA 12.4.0, build opt, RTX A5000 (Ampere CC 8.6)
- Harness: ./scripts/cuda/moose_cuda.sh (solid_mechanics rebuilt against the Section A fix)

## Scope
The 3D NEML2 Kokkos Action drives Cartesian linear elasticity with input_backend = kokkos and
output_backend = kokkos: KokkosComputeSmallStrain packs six-component Mandel strain
(order xx,yy,zz,yz,xz,xy, sqrt(2) on shears), the NEML2 model evaluates, and Kokkos stress and
tangent return as material properties. The displacement field excites all three normal and all
three shear Mandel components, so a dropped or transposed component fails the comparison.

Tests (modules/solid_mechanics/test/tests/kokkos/linear_elasticity/tests), both compute_devices='cpu cuda':
- neml2_action_material_3d           - Exodiff against neml2_action_kokkos_linear_elasticity_3d_out.e
- neml2_action_material_3d_jacobian  - PetscJacobianTester, all displacement couplings

## Results
CUDA (default device):
  test solid_mechanics 'neml2_action_material_3d' -j 4  => 2 passed, 0 skipped, 0 failed.
    neml2_action_material_3d ........... OK
    neml2_action_material_3d_jacobian .. OK
CPU (--compute-device=cpu, exercises the Section A gather path CUDA Kokkos -> CPU NEML2):
  test solid_mechanics 'neml2_action_material_3d' --compute-device=cpu -j 4 => 2 passed, 0 failed.
    neml2_action_material_3d ........... OK
    neml2_action_material_3d_jacobian .. OK

Exodiff confirms numerical equivalence and Mandel component ordering against gold; the
PetscJacobianTester confirms the coupled NEML2-stress-derivative Jacobian across all displacement
couplings on both devices.

## GPU execution confirmed (Nsight, post-fix)
profile.sh mandel_action_3d_sectionB (neml2_action_kokkos_linear_elasticity_3d.i, --compute-device=cuda)
-> cuda-qualification/mandel_action_3d_sectionB.{nsys-rep,cuda-kernels.txt}. Solve converged over 5
time steps. GPU kernel summary shows both:
- Kokkos CUDA ParallelFor kernels: stress-divergence (Di...), FE interpolation (FE...), strain gather,
  assembly - the Kokkos compute path runs on the GPU.
- NEML2 torch at::native CUDA elementwise/vectorized kernels - NEML2 evaluates on CUDA with the
  outputs kept on-device (zero-copy to the Kokkos retriever).
This confirms the whole Mandel Action path (Kokkos gather -> NEML2 eval -> Kokkos stress/tangent)
executes on the device, not merely that it compiled.

## Status
Section B: PASS on cpu and cuda (Exodiff + PetscJacobianTester), GPU execution confirmed.
No failures; proceeding criterion met. Next: Section C (3D LD), then Section D (3D SCZM direct input).
