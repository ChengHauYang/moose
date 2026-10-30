diffusion_type = KokkosDiffusion
dirichlet_type = KokkosDirichletBC

!include sczm_pk1_common.i

[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_kokkos = true
  strain = FINITE
[]

[Materials/local_traction]
  type = KokkosPureElasticTractionSeparation
  boundary = 'Block0_Block1'
  normal_stiffness = 10
  tangent_stiffness = 8
[]

[NEML2/constitutive]
  output_backend = kokkos
[]
