diffusion_type = Diffusion
dirichlet_type = DirichletBC

!include sczm_pk1_common.i

[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_kokkos = false
  strain = FINITE
  use_automatic_differentiation = false
[]

[Materials/local_traction]
  type = PureElasticTractionSeparation
  boundary = 'Block0_Block1'
  displacements = 'ux uy'
  normal_stiffness = 10
  tangent_stiffness = 8
[]
