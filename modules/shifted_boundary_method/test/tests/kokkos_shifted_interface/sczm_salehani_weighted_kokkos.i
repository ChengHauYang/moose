# Native Salehani-Irani law through its peak and into softening under the loading of
# sczm_bilinear_common.i. The tests stop at end_time = 0.75, before the unloading, because the
# native law is memoryless. salehani_traction_neml2.i repeats the law parameters for the NEML2
# version.
!include sczm_bilinear_common.i

[Kernels]
  [solid_x]
    type = KokkosIsotropicElasticity
    variable = ux
    component = 0
    displacements = 'ux uy'
    youngs_modulus = ${E}
    poissons_ratio = ${nu}
  []
  [solid_y]
    type = KokkosIsotropicElasticity
    variable = uy
    component = 1
    displacements = 'ux uy'
    youngs_modulus = ${E}
    poissons_ratio = ${nu}
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_kokkos = true
  stabilization = weighted_traction
  # beta = gamma |face| E / V = 100 with E = 1000 and a face-to-volume ratio of 2, as in the
  # bilinear_mixed_mode_weighted_traction tests
  stabilization_gamma = 0.05
[]

[Materials]
  [elasticity]
    type = KokkosComputeIsotropicElasticity
    displacements = 'ux uy'
    youngs_modulus = ${E}
    poissons_ratio = ${nu}
  []
  [local_traction]
    type = KokkosSalehaniIraniTractionSeparation
    boundary = 'Block0_Block1'
    normal_gap_at_maximum_normal_traction = 0.05
    tangential_gap_at_maximum_shear_traction = 0.05
    maximum_normal_traction = 5
    maximum_shear_traction = 7
  []
[]
