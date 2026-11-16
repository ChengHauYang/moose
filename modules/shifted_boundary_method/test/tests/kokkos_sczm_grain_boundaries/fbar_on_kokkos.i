# F-bar ON overlay for grain_boundaries_kokkos.i.
# Kokkos counterpart of fbar_on_host.i: enables total-mode F-bar volumetric locking correction
# in the bulk (TorchDeformationGradient, KokkosComputeFbarAverage, KokkosComputeLagrangianStressCustomPK2,
# KokkosTotalLagrangianStressDivergence) and on the seven total-Lagrangian shifted cohesive-zone
# grain boundaries (KokkosComputeDeformationGradient, KokkosSCZMInterfaceKernelTotalLagrangian).

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
    input = 'exact_kinematics_pk1_fbar_neml2.i'
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
