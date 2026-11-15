use_kokkos = false
dirichlet_type = DirichletBC
file_base = sczm_tl_fbar_host_out

!include sczm_tl_fbar_common.i

[Physics/SolidMechanics/QuasiStatic]
  displacements = 'ux uy uz'
  [bulk]
    displacements = 'ux uy uz'
    strain = FINITE
    formulation = TOTAL
    new_system = true
    volumetric_locking_correction = ${fbar}
    base_name = bulk
    strain_base_name = bulk
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_automatic_differentiation = false
[]

[Materials]
  # Interface kinematics: U, and with F-bar also the element-volume average M and B
  [interface_deformation_gradient]
    type = ComputeDeformationGradient
    displacements = 'ux uy uz'
    stabilize_strain = ${fbar}
  []
  [bulk_elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    base_name = bulk
    youngs_modulus = 100
    poissons_ratio = 0.3
  []
  [bulk_stress]
    type = ComputeStVenantKirchhoffStress
    base_name = bulk
    elasticity_tensor = bulk_elasticity_tensor
    large_kinematics = true
  []
  [local_traction]
    type = PureElasticTractionSeparation
    boundary = 'Block0_Block1'
    displacements = 'ux uy uz'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
[]
