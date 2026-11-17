!include sczm_stateful_neml2.i

[Models]
  # Stiffer than the radial-return elasticity (E = 1e5) so the two sides differ even before yield
  [bulk_elasticity]
    type = LinearIsotropicElasticity
    coefficients = '2e5 0.3'
    coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
    strain = 'neml2_strain'
    stress = 'neml2_stress'
  []
  [elastic_interface_model]
    type = ComposedModel
    models = 'symmetric_strain bulk_elasticity full_stress'
  []
[]
