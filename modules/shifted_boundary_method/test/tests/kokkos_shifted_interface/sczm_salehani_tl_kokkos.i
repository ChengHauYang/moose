use_kokkos = true
dirichlet_type = KokkosDirichletBC
file_base = sczm_salehani_tl_kokkos_out

!include sczm_tl_fbar_common.i

[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  stabilization = weighted_traction
[]

[Outputs]
  exodus = true
[]

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

[NEML2/interface]
  input_backend = kokkos
  output_backend = kokkos
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
    type = KokkosComputeStVenantKirchhoffStress
    youngs_modulus = 100
    poissons_ratio = 0.3
    pk2_stress = bulk_pk2
    dpk2_dF = bulk_pk2_jacobian
  []
  [bulk_pk1]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'ux uy uz'
    custom_pk2_stress = bulk_pk2
    custom_pk2_jacobian = bulk_pk2_jacobian
    stabilize_strain = ${fbar}
  []
  # These peaks give initial normal/tangential stiffnesses of 10 and 8
  [local_traction]
    type = KokkosSalehaniIraniTractionSeparation
    boundary = 'Block0_Block1'
    normal_gap_at_maximum_normal_traction = 1e8
    tangential_gap_at_maximum_shear_traction = 1e8
    maximum_normal_traction = 367879441.171442
    maximum_shear_traction = 485224527.770107
  []
[]
