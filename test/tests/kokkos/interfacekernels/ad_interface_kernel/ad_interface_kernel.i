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
[]

[ICs]
  [u]
    type = FunctionIC
    variable = u
    function = '1 + x + x*x'
  []
  [v]
    type = FunctionIC
    variable = v
    function = '2 - x + 0.5*x*x'
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
[]

[InterfaceKernels]
  [penalty]
    type = KokkosADLinearInterfaceDiffusion
    variable = u
    neighbor_var = v
    boundary = interface
    penalty = 10
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

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  exodus = false
[]
