!include sczm_bilinear_common.i

[Kernels]
  [solid_x]
    type = StressDivergenceTensors
    variable = ux
    component = 0
    displacements = 'ux uy'
  []
  [solid_y]
    type = StressDivergenceTensors
    variable = uy
    component = 1
    displacements = 'ux uy'
  []
[]

# The host weighted traction formulation is not the generalized Robin solve for a softening law,
# so the reference is the unstabilized interface
[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_automatic_differentiation = false
[]

[Materials]
  [strain]
    type = ComputeSmallStrain
    displacements = 'ux uy'
  []
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = ${E}
    poissons_ratio = ${nu}
  []
  [stress]
    type = ComputeLinearElasticStress
  []
  [local_traction]
    type = BiLinearMixedModeTraction
    displacements = 'ux uy'
  []
[]
