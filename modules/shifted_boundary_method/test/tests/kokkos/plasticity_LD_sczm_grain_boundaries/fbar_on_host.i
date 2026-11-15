# F-bar ON overlay for the host steps (step1, step2) of the plasticity_LD_sczm_grain_boundaries
# benchmark. Merged on top of the step input, which keeps every other setting:
#   shifted_boundary_method-opt -i step1.i fbar_on_host.i
#
# Switches on the total-mode F-bar of plasticity_LDinSCZM in the bulk and on the grain boundaries.
# The bulk uses the 3x3 plane-strain stabilized deformation gradient B = gamma * U with U_zz = 1 and
# gamma = cbrt(det(M) / det(U)). The interface NEML2 model evaluates P = U * PK2(B) and returns
# dP/dU and dP/dB, which the total-Lagrangian SCZM (strain = FINITE) uses for its local and
# nonlocal Jacobian terms. The bulk strain/stress objects take base_name = bulk so the unprefixed
# deformation_gradient and physical_deformation_gradient below feed only the interface NEML2 model.

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        volumetric_locking_correction = true
        base_name = bulk
        strain_base_name = bulk
      []
    []
    [ShiftedCohesiveZone]
      [sczm]
        stabilized_tangent = stabilized_pk1_jacobian
        strain = FINITE
        volumetric_locking_correction = true
      []
    []
  []
[]

[NEML2]
  [interface]
    input = '../plasticity_LDinSCZM/exact_kinematics_pk1_neml2.i'
    input_types = 'MATERIAL MATERIAL'
    inputs = 'physical_deformation_gradient deformation_gradient'
    derivatives = 'pk1_stress physical_deformation_gradient pk1_jacobian; pk1_stress deformation_gradient stabilized_pk1_jacobian'
  []
[]

[Materials]
  [deformation_gradient]
    type = ComputeDeformationGradient
    stabilize_strain = true
  []
  [bulk_stress]
    base_name = bulk
  []
[]
