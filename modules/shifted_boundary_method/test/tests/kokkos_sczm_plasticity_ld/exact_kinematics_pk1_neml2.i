# Interface constitutive model for the shifted cohesive-zone directional correction.
#
# Reuses the bulk exact-kinematics crystal-plasticity model (which defines `model` outputting
# the PK2 stress `neml2_stress` and carrying the plastic state) and adds R2Multiplication to
# form the first Piola-Kirchhoff stress PK1 = F * PK2. The shifted cohesive-zone kernel
# consumes PK1 and its derivative d(PK1)/dF directly via tangent_definition =
# pk1_wrt_deformation_gradient.
!include ../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i

[Tensors]
  # Fixed crystal orientation (MRP) matching the bulk orientation material property.
  # Baking it into the interface model allows the Kokkos interface input gatherer
  # (KokkosInterfaceQuantityToNEML2, which gathers full rank-two MATERIAL inputs)
  # to gather only deformation_gradient.
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
    A = 'deformation_gradient'
    B = 'neml2_stress'
    to = 'pk1_stress'
  []
  [interface_model]
    type = ComposedModel
    models = 'orientation model pk1_mult'
    additional_outputs = 'plastic_deformation_gradient'
  []
[]
