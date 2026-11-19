# St. Venant-Kirchhoff model with the PK2 stress as a full rank-two tensor

[Models]
  [green_lagrange_strain]
    type = GreenLagrangeStrain
    deformation_gradient = 'deformation_gradient'
    strain = 'elastic_strain'
  []
  [elasticity]
    type = LinearIsotropicElasticity
    coefficients = '100000 0.3'
    coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
    strain = 'elastic_strain'
    stress = 'sr2_stress'
  []
  [full_stress]
    type = SR2ToR2
    input = 'sr2_stress'
    output = 'neml2_stress'
  []
  [model]
    type = ComposedModel
    models = 'green_lagrange_strain elasticity full_stress'
  []
[]
