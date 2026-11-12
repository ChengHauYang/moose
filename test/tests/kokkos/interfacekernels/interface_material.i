[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 2
    xmax = 1
    ymax = 1
  []
  [left_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '0 0 0'
    top_right = '0.5 1 0'
    block_id = 1
  []
  [interface]
    type = SideSetsBetweenSubdomainsGenerator
    input = left_block
    primary_block = 0
    paired_block = 1
    new_boundary = interface
  []
[]

[Variables]
  [u]
  []
  [v]
  []
[]

[Kernels]
  [u_diffusion]
    type = KokkosCoefDiffusion
    variable = u
    coef = 1
  []
  [v_diffusion]
    type = KokkosCoefDiffusion
    variable = v
    coef = 1
  []
[]

[InterfaceKernels]
  [linear]
    type = KokkosLinearInterfaceDiffusion
    variable = u
    neighbor_var = v
    boundary = interface
    traction = traction
    dtraction_djump = dtraction_djump
  []
[]

[Materials]
  [interface_material]
    type = KokkosLinearInterfaceMaterial
    variable = u
    neighbor_var = v
    boundary = interface
    stiffness = 10
  []
[]

[BCs]
  [u_left]
    type = KokkosDirichletBC
    variable = u
    boundary = left
    value = 1
  []
  [v_left]
    type = KokkosDirichletBC
    variable = v
    boundary = left
    value = 0
  []
  [u_right]
    type = KokkosDirichletBC
    variable = u
    boundary = right
    value = 0
  []
  [v_right]
    type = KokkosDirichletBC
    variable = v
    boundary = right
    value = 1
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  exodus = false
[]
