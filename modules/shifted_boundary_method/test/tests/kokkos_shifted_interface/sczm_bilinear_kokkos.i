!include sczm_bilinear_common.i
!include sczm_bilinear_traction.i

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

# As beta grows, the auxiliary jump of the generalized Robin solve tends to the interface jump, so
# this gamma, which gives beta = gamma |face| E / V = 1e12 (face-to-volume ratio 2), reproduces the
# unstabilized host interface. The Jacobian tests override it.
[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_kokkos = true
  stabilization = weighted_traction
  stabilization_gamma = 5e8
[]

[Materials]
  [elasticity]
    type = KokkosComputeIsotropicElasticity
    displacements = 'ux uy'
    youngs_modulus = ${E}
    poissons_ratio = ${nu}
  []
  [local_traction]
    type = KokkosBiLinearMixedModeTraction
  []
[]
