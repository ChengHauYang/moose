[Models]
  [model]
    type = LinearIsotropicElasticity
    coefficients = '3 0.2'
    coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
    strain = neml2_strain
    stress = neml2_stress
  []
[]
