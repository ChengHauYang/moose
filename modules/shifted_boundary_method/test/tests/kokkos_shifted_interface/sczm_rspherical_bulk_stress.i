[Mesh]
  coord_type = RSPHERICAL
  [gen]
    type = GeneratedMeshGenerator
    dim = 1
    nx = 2
    xmin = 1
    xmax = 3
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '2 0 0'
    top_right = '3 0 0'
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
[]

[Kernels]
  [ur_diffusion]
    type = KokkosDiffusion
    variable = ur
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ur'
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
    displacements = 'ur'
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
    expression = 'x - 2.1'
  []
[]

[BCs]
  # Boundary conditions producing linear field:
  # u_r(r) = 0.01 * r  -> eps_rr = 0.01, eps_theta_theta = eps_phi_phi = u_r/r = 0.01
  # trace(eps) = 0.01 + 0.01 + 0.01 = 0.03
  #
  # Primary:
  # sigma_rr = lambda * trace + 2*mu * eps_rr = 400*0.03 + 800*0.01 = 20.0
  # sigma_tt = lambda * trace + 2*mu * eps_tt = 400*0.03 + 800*0.01 = 20.0
  # sigma_pp = lambda * trace + 2*mu * eps_pp = 400*0.03 + 800*0.01 = 20.0
  #
  # Neighbor:
  # sigma_rr = 555.55555556*0.03 + 1666.66666667*0.01 = 33.33333333
  # sigma_tt = 555.55555556*0.03 + 1666.66666667*0.01 = 33.33333333
  # sigma_pp = 555.55555556*0.03 + 1666.66666667*0.01 = 33.33333333
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
[]

[Postprocessors]
  [sigma_rr_primary]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress'
    component_i = 0
    component_j = 0
  []
  [sigma_tt_primary]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress'
    component_i = 1
    component_j = 1
  []
  [sigma_pp_primary]
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
  [sigma_tt_neighbor]
    type = KokkosSCZMBulkStressPostprocessor
    boundary = 'Block0_Block1'
    stress_property = 'sczm_bulk_stress_neighbor'
    component_i = 1
    component_j = 1
  []
  [sigma_pp_neighbor]
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
