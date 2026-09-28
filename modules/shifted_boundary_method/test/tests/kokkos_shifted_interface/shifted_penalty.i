[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 2
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
  [break]
    type = BreakMeshByBlockGenerator
    input = right_block
    split_interface = true
    add_interface_on_two_sides = true
  []
[]

[Variables]
  [u]
    block = 0
  []
  [v]
    block = 1
  []
[]

[Kernels]
  [u_diffusion]
    type = KokkosDiffusion
    variable = u
    block = 0
  []
  [v_diffusion]
    type = KokkosDiffusion
    variable = v
    block = 1
  []
[]

[InterfaceKernels]
  [penalty]
    type = KokkosShiftedPenaltyInterface
    variable = u
    neighbor_var = v
    boundary = 'Block0_Block1'
    sbm_distance_uo = distance
    penalty = 10
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
    expression = 'x - 1.1'
  []
[]

[BCs]
  [u_left]
    type = KokkosDirichletBC
    variable = u
    boundary = left
    value = 1
  []
  [v_right]
    type = KokkosDirichletBC
    variable = v
    boundary = right
    value = 0
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  exodus = false
[]
