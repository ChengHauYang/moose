# Interface constitutive model for the total-mode F-bar shifted cohesive-zone directional correction.
# Evaluates PK1 = U * PK2(B), where U is physical_deformation_gradient and B is the F-bar
# stabilized deformation_gradient, so NEML2 autodiff supplies d(PK1)/dU at fixed B and
# d(PK1)/dB at fixed U independently.
!include ../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i

[Tensors]
  # Fixed crystal orientation (MRP) matching the bulk orientation material property.
  [orientation_value]
    type = Python
    expr = 'MRP(torch.tensor([-0.54412095, -0.34931944, 0.12600655], dtype=torch.float64))'
  []
[]

[Models]
  [orientation]
    type = MRPConstantParameter
    value = 'orientation_value'
    parameter = 'orientation'
  []
  [pk1_mult]
    type = R2Multiplication
    A = 'physical_deformation_gradient'
    B = 'neml2_stress'
    to = 'pk1_stress'
  []
  [interface_model]
    type = ComposedModel
    models = 'orientation model pk1_mult'
    additional_outputs = 'plastic_deformation_gradient'
  []
[]
