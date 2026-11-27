# Makes block 2 of common.i 1000 times stiffer than block 1. This contrast is typical of a metal
# against a polymer, and it is where the two sides' bulk tractions must enter the weighted traction
# with stiffness-dependent weights.

[Materials]
  [elasticity_tensor]
    block = 1
  []
  [elasticity_tensor_stiff]
    type = ComputeIsotropicElasticityTensor
    poissons_ratio = ${poi}
    youngs_modulus = '${fparse 1000 * E}'
    block = 2
  []
[]
