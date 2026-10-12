[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 2
    xmax = 2
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '1 0 0'
    top_right = '2 1 0'
    block_id = 1
  []
  [interface]
    type = SideSetsBetweenSubdomainsGenerator
    input = right_block
    primary_block = 0
    paired_block = 1
    new_boundary = interface
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

[Materials]
  [jump]
    type = KokkosSCZMComputeDisplacementJumpSmallStrain
    boundary = interface
    displacements = 'ux uy'
    sbm_distance_uo = distance
  []
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = interface
    normal_stiffness = 10
    tangent_stiffness = 8
  []
  [global_traction]
    type = KokkosCZMComputeGlobalTractionSmallStrain
    boundary = interface
  []
[]

[InterfaceKernels]
  [sczm_x]
    type = KokkosSCZMInterfaceKernelSmallStrain
    variable = ux
    neighbor_var = ux
    boundary = interface
    sbm_distance_uo = distance
    component = 0
    displacements = 'ux uy'
  []
  [sczm_y]
    type = KokkosSCZMInterfaceKernelSmallStrain
    variable = uy
    neighbor_var = uy
    boundary = interface
    sbm_distance_uo = distance
    component = 1
    displacements = 'ux uy'
  []
[]

[UserObjects]
  [distance]
    type = BoundaryShortestDistanceToSurface
    surfaces = true_interface
    boundary = interface
    execute_on = INITIAL
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = 'x - 1.1'
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
[]

[Outputs]
  exodus = false
[]
