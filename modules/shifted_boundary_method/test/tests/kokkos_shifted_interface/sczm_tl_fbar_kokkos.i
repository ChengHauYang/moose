use_kokkos = true
dirichlet_type = KokkosDirichletBC
file_base = sczm_tl_fbar_kokkos_out

!include sczm_tl_fbar_common.i

[Kernels]
  [stress_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = ux
    component = 0
    displacements = 'ux uy uz'
    stabilize_strain = ${fbar}
  []
  [stress_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = uy
    component = 1
    displacements = 'ux uy uz'
    stabilize_strain = ${fbar}
  []
  [stress_z]
    type = KokkosTotalLagrangianStressDivergence
    variable = uz
    component = 2
    displacements = 'ux uy uz'
    stabilize_strain = ${fbar}
  []
[]

[NEML2]
  [bulk]
    model = bulk_model
    executor_name = neml2_bulk
    input_kernels = deformation_gradient
    auto_output = false
  []
  [interface]
    input_backend = kokkos
    output_backend = kokkos
  []
[]

[UserObjects]
  [assembly]
    type = TorchAssembly
  []
  [fe]
    type = TorchFEInterpolation
    assembly = assembly
  []
  [deformation_gradient]
    type = TorchDeformationGradient
    assembly = assembly
    fe = fe
    to_neml2 = deformation_gradient
    displacements = 'ux uy uz'
    stabilize_strain = ${fbar}
  []
[]

[Materials]
  # Interface kinematics: U, and with F-bar also the element-volume average M and B
  [interface_deformation_gradient]
    type = KokkosComputeDeformationGradient
    displacements = 'ux uy uz'
    stabilize_strain = ${fbar}
  []
  [f_bar_average]
    type = KokkosComputeFbarAverage
    displacements = 'ux uy uz'
  []
  [bulk_pk2]
    type = NEML2ToKokkosFullRankTwoMaterialProperty
    neml2_executor = neml2_bulk
    from_neml2 = neml2_stress
    to_moose = bulk_pk2
  []
  [bulk_pk2_jacobian]
    type = NEML2ToKokkosFullRankFourMaterialProperty
    neml2_executor = neml2_bulk
    from_neml2 = neml2_stress
    neml2_input_derivative = deformation_gradient
    to_moose = bulk_pk2_jacobian
  []
  [bulk_pk1]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'ux uy uz'
    custom_pk2_stress = bulk_pk2
    custom_pk2_jacobian = bulk_pk2_jacobian
    stabilize_strain = ${fbar}
  []
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
[]
