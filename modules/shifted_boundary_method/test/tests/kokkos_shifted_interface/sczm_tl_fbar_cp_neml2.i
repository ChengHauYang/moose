# Stateful crystal-plasticity interface model for the total-mode F-bar SCZM tests.
#
# cp_interface_model evaluates the exact-kinematics crystal-plasticity PK2 stress at the F-bar
# deformation gradient B, so the plastic state evolves with B, and wraps it with the physical
# deformation gradient U to give the interface PK1 stress P = U PK2(B).
#
# bulk_model is the St. Venant-Kirchhoff model with the elastic constants of the crystal-plasticity
# model, for the Kokkos total-Lagrangian bulk kernels.
!include ../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i

[Tensors]
  # Fixed crystal orientation (MRP), the same as in the plasticity_LD_sczm benchmark
  [orientation_value]
    type = Python
    expr = 'MRP(torch.tensor([-0.54412095, -0.34931944, 0.12600655], dtype=torch.float64))'
  []
[]

[Models]
  [bulk_strain]
    type = GreenLagrangeStrain
    deformation_gradient = 'deformation_gradient'
    strain = 'bulk_strain'
  []
  [bulk_elasticity]
    type = LinearIsotropicElasticity
    coefficients = '1e5 0.25'
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

  [orientation]
    type = MRPConstantParameter
    value = 'orientation_value'
    parameter = 'orientation'
  []
  [cp_pk1]
    type = R2Multiplication
    A = 'physical_deformation_gradient'
    B = 'neml2_stress'
    to = 'interface_pk1'
  []
  [cp_interface_model]
    type = ComposedModel
    models = 'orientation model cp_pk1'
    additional_outputs = 'plastic_deformation_gradient'
  []
[]
