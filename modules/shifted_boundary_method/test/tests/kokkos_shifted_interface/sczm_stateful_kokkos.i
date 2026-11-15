diffusion_type = KokkosDiffusion

!include sczm_stateful_common.i

[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_kokkos = true
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
