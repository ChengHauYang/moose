[Mesh]
  coord_type = RZ
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 2
    ny = 2
    xmin = -1
    xmax = 1
    ymin = 0
    ymax = 1
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '0 0 0'
    top_right = '1 1 0'
    block_id = 1
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = right_block
    split_interface = true
    add_interface_on_two_sides = true
  []
[]

[Variables]
  [ur]
  []
  [uz]
  []
[]

[Kernels]
  [ur_diffusion]
    type = KokkosDiffusion
    variable = ur
  []
  [uz_diffusion]
    type = KokkosDiffusion
    variable = uz
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ur uz'
    sbm_distance_uo = distance
    use_kokkos = true
    directional_correction = true
  []
[]

[Materials]
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
  # Primary: E = 1000, nu = 0.25 -> lambda = 400, mu = 400
  [bulk_stress]
    type = KokkosSCZMComputeLinearElasticBulkStress
    boundary = 'Block0_Block1'
    displacements = 'ur uz'
    youngs_modulus = 1000
    poissons_ratio = 0.25
  []
[]

[UserObjects]
  [distance]
    type = BoundaryShortestDistanceToSurface
    surfaces = true_interface
    boundary = 'Block0_Block1'
    execute_on = INITIAL
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 0.1) * sin(1.047198) - (y - 0.5) * cos(1.047198)'
  []
[]

[BCs]
  # Displacement field with non-zero radial displacement at r = 0:
  # u_r(r) = 0.01 * r + 0.05
  # At r = -1: u_r = 0.04
  # At r = 1:  u_r = 0.06
  # At r = 0 (interface): u_r = 0.05 != 0
  # On-axis handling must set hoop strain to 0 without division by zero.
  # eps_rr = 0.01, eps_zz = 0.0, eps_tt = 0.0 (on axis)
  # trace(eps) = 0.01 + 0.0 + 0.0 = 0.01
  # sigma_rr = 400 * 0.01 + 800 * 0.01 = 12.0
  # sigma_zz = 400 * 0.01 + 0 = 4.0
  # sigma_tt = 400 * 0.01 + 0 = 4.0
  [ur_left]
    type = KokkosDirichletBC
    variable = ur
    boundary = left
    value = 0.04
  []
  [ur_interface]
    type = KokkosDirichletBC
    variable = ur
    boundary = 'Block0_Block1 Block1_Block0'
    value = 0.05
  []
  [ur_right]
    type = KokkosDirichletBC
    variable = ur
    boundary = right
    value = 0.06
  []
  [uz_all]
    type = KokkosDirichletBC
    variable = uz
    boundary = 'left right bottom top Block0_Block1 Block1_Block0'
    value = 0.0
  []
[]

[Postprocessors]
  [sigma_rr]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress'
    component_i = 0
    component_j = 0
  []
  [sigma_zz]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress'
    component_i = 1
    component_j = 1
  []
  [sigma_tt]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress'
    component_i = 2
    component_j = 2
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  csv = true
  exodus = false
[]
