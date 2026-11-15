# F-bar ON overlay for the Kokkos steps (step3-step6) of the plasticity_LD_sczm_grain_boundaries
# benchmark. Merged on top of the step input, which keeps every other setting:
#   shifted_boundary_method-opt -i step3.i fbar_on_kokkos.i
#
# Same total-mode F-bar as fbar_on_host.i: the bulk NEML2 input kernel, the stress divergence, and
# the PK1 material use B = gamma * U (U_zz = 1, gamma = cbrt(det(M) / det(U))) with M averaged by
# KokkosComputeFbarAverage, and the interface NEML2 model evaluates P = U * PK2(B) with dP/dU and
# dP/dB for the total-Lagrangian SCZM (strain = FINITE).

[UserObjects]
  [deformation_gradient]
    stabilize_strain = true
  []
[]

[Kernels]
  [stress_x]
    stabilize_strain = true
  []
  [stress_y]
    stabilize_strain = true
  []
[]

[Physics]
  [SolidMechanics]
    [ShiftedCohesiveZone]
      [sczm]
        stabilized_tangent = stabilized_pk1_jacobian
        strain = FINITE
        volumetric_locking_correction = true
      []
    []
  []
[]

[NEML2]
  [interface]
    input = '../plasticity_LDinSCZM/exact_kinematics_pk1_neml2.i'
    input_types = 'MATERIAL MATERIAL'
    inputs = 'physical_deformation_gradient deformation_gradient'
    derivatives = 'pk1_stress physical_deformation_gradient pk1_jacobian; pk1_stress deformation_gradient stabilized_pk1_jacobian'
  []
[]

[Materials]
  [interface_deformation_gradient]
    stabilize_strain = true
  []
  [f_bar_average]
    type = KokkosComputeFbarAverage
    displacements = 'disp_x disp_y'
  []
  [pk1]
    stabilize_strain = true
  []
[]
