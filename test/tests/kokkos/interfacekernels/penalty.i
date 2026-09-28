[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 1
    nx = 4
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
  [u]
    block = 0
  []
  [v]
    block = 1
  []
  [w]
    block = 0
  []
  [z]
    block = 1
  []
[]

[Kernels]
  [u_diffusion]
    type = KokkosCoefDiffusion
    variable = u
    block = 0
    coef = 1
  []
  [v_diffusion]
    type = KokkosCoefDiffusion
    variable = v
    block = 1
    coef = 1
  []
  [w_diffusion]
    type = KokkosCoefDiffusion
    variable = w
    block = 0
    coef = 1
  []
  [z_diffusion]
    type = KokkosCoefDiffusion
    variable = z
    block = 1
    coef = 1
  []
[]

[InterfaceKernels]
  [penalty]
    type = KokkosPenaltyInterfaceDiffusion
    variable = u
    neighbor_var = v
    primary_coupled_var = w
    neighbor_coupled_var = z
    boundary = interface
    penalty = 10
    primary_coefficient = primary_coefficient
    neighbor_coefficient = neighbor_coefficient
  []
[]

[Materials]
  [primary]
    type = KokkosGenericConstantMaterial
    prop_names = primary_coefficient
    prop_values = 2
    block = 0
  []
  [neighbor]
    type = KokkosGenericConstantMaterial
    prop_names = neighbor_coefficient
    prop_values = 3
    block = 1
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
  [w_left]
    type = KokkosDirichletBC
    variable = w
    boundary = left
    value = 0.25
  []
  [z_right]
    type = KokkosDirichletBC
    variable = z
    boundary = right
    value = -0.25
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  exodus = false
[]
