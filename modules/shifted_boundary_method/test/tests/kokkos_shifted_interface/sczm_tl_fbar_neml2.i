# St. Venant-Kirchhoff models for the total-mode F-bar SCZM tests.
#
# bulk_model maps the F-bar deformation gradient to the PK2 stress consumed by the Kokkos
# total-Lagrangian bulk kernels.
#
# interface_model evaluates the interface PK1 stress P = U PK2(B) from the physical deformation
# gradient U and the F-bar deformation gradient B = (det(M) / det(U))^(1/3) U, so that
# d(P)/d(U) at fixed B and d(P)/d(B) at fixed U are separate NEML2 derivatives.
#
# interface_model_off evaluates P = F PK2(F) from the single physical deformation gradient F for
# the tests with F-bar disabled.
[Models]
  [bulk_strain]
    type = GreenLagrangeStrain
    deformation_gradient = 'deformation_gradient'
    strain = 'bulk_strain'
  []
  [bulk_elasticity]
    type = LinearIsotropicElasticity
    coefficients = '100 0.3'
    coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
    strain = 'bulk_strain'
    stress = 'bulk_sr2_stress'
  []
  [bulk_stress]
    type = SR2ToR2
    input = 'bulk_sr2_stress'
    output = 'neml2_stress'
  []
  [bulk_model]
    type = ComposedModel
    models = 'bulk_strain bulk_elasticity bulk_stress'
  []

  [interface_strain]
    type = GreenLagrangeStrain
    deformation_gradient = 'deformation_gradient'
    strain = 'interface_strain'
  []
  [interface_elasticity]
    type = LinearIsotropicElasticity
    coefficients = '100 0.3'
    coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
    strain = 'interface_strain'
    stress = 'interface_sr2_stress'
  []
  [interface_pk2]
    type = SR2ToR2
    input = 'interface_sr2_stress'
    output = 'interface_pk2'
  []
  [interface_pk1]
    type = R2Multiplication
    A = 'physical_deformation_gradient'
    B = 'interface_pk2'
    to = 'interface_pk1'
  []
  [interface_model]
    type = ComposedModel
    models = 'interface_strain interface_elasticity interface_pk2 interface_pk1'
  []

  [interface_pk1_off]
    type = R2Multiplication
    A = 'deformation_gradient'
    B = 'interface_pk2'
    to = 'interface_pk1'
  []
  [interface_model_off]
    type = ComposedModel
    models = 'interface_strain interface_elasticity interface_pk2 interface_pk1_off'
  []
[]
