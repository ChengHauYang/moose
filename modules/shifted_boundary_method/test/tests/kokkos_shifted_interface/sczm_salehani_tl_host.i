use_kokkos = false
dirichlet_type = DirichletBC
file_base = sczm_salehani_tl_host_out

!include sczm_tl_fbar_common.i

[Outputs]
  exodus = true
[]

[Kernels]
  [stress_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = ux
    component = 0
    displacements = 'ux uy uz'
    pk1_stress = bulk_pk1
    dpk1_d_grad_u = bulk_pk1_jacobian
  []
  [stress_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = uy
    component = 1
    displacements = 'ux uy uz'
    pk1_stress = bulk_pk1
    dpk1_d_grad_u = bulk_pk1_jacobian
  []
  [stress_z]
    type = KokkosTotalLagrangianStressDivergence
    variable = uz
    component = 2
    displacements = 'ux uy uz'
    pk1_stress = bulk_pk1
    dpk1_d_grad_u = bulk_pk1_jacobian
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_automatic_differentiation = false
  stabilization = weighted_traction
[]

[Materials]
  # Interface kinematics: U, and with F-bar also the element-volume average M and B
  [interface_deformation_gradient]
    type = ComputeDeformationGradient
    displacements = 'ux uy uz'
    stabilize_strain = ${fbar}
  []
  [bulk_deformation_gradient]
    type = KokkosComputeDeformationGradient
    displacements = 'ux uy uz'
    deformation_gradient = bulk_deformation_gradient
    stabilize_strain = ${fbar}
  []
  [bulk_pk2]
    type = KokkosComputeStVenantKirchhoffStress
    youngs_modulus = 100
    poissons_ratio = 0.3
    deformation_gradient = bulk_deformation_gradient
    pk2_stress = bulk_pk2
    dpk2_dF = bulk_pk2_jacobian
  []
  [bulk_pk1]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'ux uy uz'
    custom_pk2_stress = bulk_pk2
    custom_pk2_jacobian = bulk_pk2_jacobian
    pk1_stress = bulk_pk1
    dpk1_d_grad_u = bulk_pk1_jacobian
    stabilize_strain = ${fbar}
  []
  # These peaks give initial normal/tangential stiffnesses of 10 and 8
  [local_traction]
    type = SalehaniIrani3DCTraction
    boundary = 'Block0_Block1'
    displacements = 'ux uy uz'
    normal_gap_at_maximum_normal_traction = 1e8
    tangential_gap_at_maximum_shear_traction = 1e8
    maximum_normal_traction = 367879441.171442
    maximum_shear_traction = 485224527.770107
  []
[]
