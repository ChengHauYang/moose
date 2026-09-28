[Mesh]
  coord_type = RZ
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 2
    ny = 2
    xmin = 1
    xmax = 3
    ymin = 0
    ymax = 1
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '2 0 0'
    top_right = '3 1 0'
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
  # Primary side (Block 0): E = 1000, nu = 0.25 -> lambda = 400, mu = 400
  # Neighbor side (Block 1): E = 2000, nu = 0.20 -> lambda = 555.55555556, mu = 833.33333333
  [bulk_stress]
    type = KokkosSCZMComputeLinearElasticBulkStress
    boundary = 'Block0_Block1'
    displacements = 'ur uz'
    youngs_modulus = 1000
    poissons_ratio = 0.25
    neighbor_youngs_modulus = 2000
    neighbor_poissons_ratio = 0.20
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
    expression = '(x - 2.1) * sin(1.047198) - (y - 0.5) * cos(1.047198)'
  []
[]

[BCs]
  # Impose exact linear displacement field u_r(r) = 0.01 * r, u_z = 0:
  # On Block 0 (r in [1, 2]): left (r=1) is 0.01, interface (r=2) is 0.02 -> eps_rr = 0.01
  # On Block 1 (r in [2, 3]): interface (r=2) is 0.02, right (r=3) is 0.03 -> eps_rr = 0.01
  # At interface r = 2: u_r = 0.02 -> eps_tt = u_r/r = 0.01
  # eps_zz = 0, trace(eps) = 0.01 + 0.0 + 0.01 = 0.02
  #
  # Analytical stress values:
  # Primary:
  # sigma_rr = lambda * trace + 2*mu * eps_rr = 400*0.02 + 800*0.01 = 16.0
  # sigma_zz = lambda * trace + 2*mu * eps_zz = 400*0.02 + 0 = 8.0
  # sigma_tt = lambda * trace + 2*mu * eps_tt = 400*0.02 + 800*0.01 = 16.0
  #
  # Neighbor:
  # sigma_rr = 555.55555556*0.02 + 1666.66666667*0.01 = 27.77777778
  # sigma_zz = 555.55555556*0.02 + 0 = 11.11111111
  # sigma_tt = 555.55555556*0.02 + 1666.66666667*0.01 = 27.77777778
  [ur_left]
    type = KokkosDirichletBC
    variable = ur
    boundary = left
    value = 0.01
  []
  [ur_interface]
    type = KokkosDirichletBC
    variable = ur
    boundary = 'Block0_Block1 Block1_Block0'
    value = 0.02
  []
  [ur_right]
    type = KokkosDirichletBC
    variable = ur
    boundary = right
    value = 0.03
  []
  [uz_all]
    type = KokkosDirichletBC
    variable = uz
    boundary = 'left right bottom top Block0_Block1 Block1_Block0'
    value = 0.0
  []
[]

[Postprocessors]
  [sigma_rr_primary]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress'
    component_i = 0
    component_j = 0
  []
  [sigma_zz_primary]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress'
    component_i = 1
    component_j = 1
  []
  [sigma_tt_primary]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress'
    component_i = 2
    component_j = 2
  []
  [sigma_rr_neighbor]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress_neighbor'
    component_i = 0
    component_j = 0
  []
  [sigma_zz_neighbor]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress_neighbor'
    component_i = 1
    component_j = 1
  []
  [sigma_tt_neighbor]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress_neighbor'
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
