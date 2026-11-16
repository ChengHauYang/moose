# F-bar ON overlay for grain_boundaries_host.i.
# Enables total-mode F-bar volumetric locking correction in the five bulk grains and on the
# seven total-Lagrangian shifted cohesive-zone grain boundaries.

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
    input = 'exact_kinematics_pk1_fbar_neml2.i'
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
