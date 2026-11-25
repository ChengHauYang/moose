file_base = sczm_tl_3d_weighted_native_out

!include sczm_tl_3d_weighted_common.i

[Materials/local_traction]
  type = KokkosPureElasticTractionSeparation
  boundary = 'Block0_Block1'
  normal_stiffness = 500
  tangent_stiffness = 300
[]
