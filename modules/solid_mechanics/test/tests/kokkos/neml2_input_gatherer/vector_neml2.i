[Models]
  [vec_decomp]
    type = VecComponents
    from = 'v'
    to = 'vx vy vz'
  []
  [scalar_comb]
    type = ScalarLinearCombination
    from = 'vx vy vz'
    to = 'v_scalar'
    weights = '1 0 0'
  []
  [thermal]
    type = ScalarToDiagonalSR2
    input = 'v_scalar'
    output = 'thermal_stress'
  []
  [elasticity]
    type = LinearIsotropicElasticity
    coefficients = '10 0.25'
    coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
    strain = 'neml2_strain'
    stress = 'elastic_stress'
  []
  [sum]
    type = SR2LinearCombination
    from = 'elastic_stress thermal_stress'
    to = 'neml2_stress'
    weights = '1 1'
  []
  [model]
    type = ComposedModel
    models = 'vec_decomp scalar_comb thermal elasticity sum'
  []
[]
