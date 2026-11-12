# Interface constitutive model for the shifted cohesive-zone directional correction.
#
# Reuses the bulk exact-kinematics crystal-plasticity model unchanged (included below, which
# defines `model` outputting the PK2 stress `neml2_stress` and carrying the plastic state) and
# adds one multiplication to form the first Piola-Kirchhoff stress PK1 = F * PK2. The shifted
# cohesive-zone kernel consumes PK1 and its consistent derivative d(PK1)/dF directly through
# tangent_definition = pk1_wrt_deformation_gradient; NEML2 autodiff supplies both the geometric
# term (delta_ik * PK2) and the material term (F * d(PK2)/dF), so no PK2->PK1 bridge is needed
# on either the host or the Kokkos path.
!include ../../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i

[Tensors]
  # Fixed crystal orientation (MRP), identical across all six benchmark steps. These are the same
  # three components, in the same x,y,z order and full precision, that Steps 1-2 previously fed as a
  # MOOSE RealVectorValue 'orientation' material; the host gatherer maps a RealVectorValue to a NEML2
  # tensor of base shape [3] (NEML2Utils::Layout<RealVectorValue>), so this constant reproduces the
  # host orientation exactly through the same RotationMatrix model. Baking it in makes 'orientation'
  # an internal constant rather than a gathered input, which the Kokkos interface input backend
  # (full rank-two MATERIAL inputs only) requires for Steps 3-6. This is specific to this
  # fixed-orientation benchmark and is not a substitute for spatially varying orientation support.
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
