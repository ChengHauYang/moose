theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 4
    xmax = 2
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '1 0 0'
    top_right = '2 1 0'
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
  [ux]
  []
  [uy]
  []
[]

[Kernels]
  [ux_diffusion]
    type = KokkosDiffusion
    variable = ux
  []
  [uy_diffusion]
    type = KokkosDiffusion
    variable = uy
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
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
  # Heterogeneous bulk response. These constants match the block-restricted host materials of the
  # paired host input, primary side (block 0) and neighbor side (block 1).
  [bulk_stress]
    type = KokkosSCZMComputeLinearElasticBulkStress
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    youngs_modulus = 1
    poissons_ratio = 0.30
    neighbor_youngs_modulus = 3
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
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad})'
  []
[]

[BCs]
  [ux_left]
    type = KokkosDirichletBC
    variable = ux
    boundary = left
    value = 0.1
  []
  [ux_right]
    type = KokkosDirichletBC
    variable = ux
    boundary = right
    value = -0.1
  []
  [uy_left]
    type = KokkosDirichletBC
    variable = uy
    boundary = left
    value = 0.2
  []
  [uy_right]
    type = KokkosDirichletBC
    variable = uy
    boundary = right
    value = -0.2
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  nl_abs_tol = 1e-14
  nl_rel_tol = 1e-14
[]

[Outputs]
  exodus = true
  file_base = sczm_hetero_kokkos_out
[]
