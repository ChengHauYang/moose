# Interface constitutive model for the total-mode F-bar shifted cohesive-zone directional correction.
#
# Reuses the bulk exact-kinematics crystal-plasticity model unchanged (included below, which
# defines `model` mapping the deformation gradient to the PK2 stress `neml2_stress` and carrying the
# plastic state). With total-mode F-bar the deformation gradient fed to `model` is the F-bar
# deformation gradient B = (det(M) / det(U))^(1/3) U, so the plastic state evolves with B, and one
# multiplication wraps the result with the physical deformation gradient U to give the first
# Piola-Kirchhoff stress PK1 = U * PK2(B). The two kinematic inputs are separate NEML2 variables, so
# NEML2 autodiff supplies d(PK1)/d(U) at fixed B (the SCZM `tangent`) and d(PK1)/d(B) at fixed U (the
# SCZM `stabilized_tangent`) independently; the SCZM kernel adds the chain rule of B through U and M.
!include ../../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i

[Tensors]
  # Fixed crystal orientation (MRP), identical across all six benchmark steps. These are the same
  # three components, in the same x,y,z order and full precision, that the bulk 'orientation'
  # material feeds the bulk model; the host gatherer maps a RealVectorValue to a NEML2 tensor of base
  # shape [3] (NEML2Utils::Layout<RealVectorValue>), so this constant reproduces the bulk orientation
  # exactly through the same RotationMatrix model. Baking it in makes 'orientation' an internal
  # constant rather than a gathered input, which the Kokkos interface input backend (full rank-two
  # MATERIAL inputs only) requires for Steps 3-6. This is specific to this fixed-orientation
  # benchmark and is not a substitute for spatially varying orientation support.
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
