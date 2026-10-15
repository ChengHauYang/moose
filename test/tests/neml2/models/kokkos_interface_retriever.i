[Models]
  [elasticity]
    type = LinearIsotropicElasticity
    coefficients = '2 0'
    coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
    strain = strain
    stress = stress
  []
[]
