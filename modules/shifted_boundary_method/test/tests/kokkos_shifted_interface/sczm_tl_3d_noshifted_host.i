diffusion_type = Diffusion
dirichlet_type = DirichletBC
traction_type = PureElasticTractionSeparation
use_kokkos = false
no_shifted = true
file_base = sczm_tl_3d_noshifted_host_out

!include sczm_tl_3d_common.i

[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_automatic_differentiation = false
[]

[Materials/local_traction]
  displacements = 'ux uy uz'
[]
