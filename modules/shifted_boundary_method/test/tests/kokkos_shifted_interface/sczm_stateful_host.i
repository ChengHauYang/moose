diffusion_type = Diffusion

!include sczm_stateful_common.i

[Materials/local_traction]
  type = PureElasticTractionSeparation
  boundary = 'Block0_Block1'
  displacements = 'ux uy'
  normal_stiffness = 10
  tangent_stiffness = 8
[]
