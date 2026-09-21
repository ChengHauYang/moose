[Models]
  [elasticity]
    type = LinearIsotropicElasticity
    coefficients = '10 0.25'
    coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
    strain = 'neml2_strain'
    stress = 'elastic_stress'
  []
  [thermal]
    type = ScalarToDiagonalSR2
    input = 'T~1'
    output = 'thermal_stress'
  []
  [sum]
    type = SR2LinearCombination
    from = 'elastic_stress thermal_stress'
    to = 'neml2_stress'
    weights = '1 1'
  []
  [model]
    type = ComposedModel
    models = 'elasticity thermal sum'
  []
[]
