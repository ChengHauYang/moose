!include mechanics_base.i

# Distinct phase elasticities make switching-function derivatives nonzero.
[Kernels]
  [x]
    base_name := mixed
  []
  [y]
    base_name := mixed
  []
  [z]
    base_name := mixed
  []
[]

[Materials]
  [strain_b]
    type = ADComputeSmallStrain
    base_name = phase_b
  []
  [elasticity_b]
    type = ADComputeIsotropicElasticityTensor
    base_name = phase_b
    youngs_modulus = 3
    poissons_ratio = 0.3
  []
  [stress_b]
    type = ADComputeLinearElasticStress
    base_name = phase_b
  []
  [weight_a]
    type = ADParsedMaterial
    property_name = h_a
    coupled_variables = 'eta1 eta2'
    expression = 'eta1^2/(eta1^2 + eta2^2)'
  []
  [weight_b]
    type = ADParsedMaterial
    property_name = h_b
    coupled_variables = 'eta1 eta2'
    expression = 'eta2^2/(eta1^2 + eta2^2)'
  []
  [mixed]
    type = ADMultiPhaseStressMaterial
    base_name = mixed
    phase_base = 'elastic phase_b'
    h = 'h_a h_b'
  []
[]
