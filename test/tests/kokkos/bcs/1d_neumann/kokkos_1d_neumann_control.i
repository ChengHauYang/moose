# The flux of a KokkosNeumannBC is controlled by a function of time: u(1) = 2 t on a unit domain
# with u(0) = 0. Running with diffusion=Diffusion dirichlet=DirichletBC neumann=NeumannBC gives
# the host solution, which the Kokkos solution must match.
diffusion = KokkosDiffusion
dirichlet = KokkosDirichletBC
neumann = KokkosNeumannBC

[Mesh]
  type = GeneratedMesh
  dim = 1
  nx = 10
[]

[Variables]
  [u]
  []
[]

[Kernels]
  [diff]
    type = ${diffusion}
    variable = u
  []
[]

[BCs]
  [left]
    type = ${dirichlet}
    variable = u
    boundary = left
    value = 0
  []
  [right]
    type = ${neumann}
    variable = u
    boundary = right
    value = 0
  []
[]

[Functions]
  [flux]
    type = ParsedFunction
    expression = '2 * t'
  []
[]

[Controls]
  [flux]
    type = RealFunctionControl
    parameter = 'BCs/right/value'
    function = flux
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[Postprocessors]
  [u_right]
    type = PointValue
    variable = u
    point = '1 0 0'
  []
[]

[Executioner]
  type = Transient
  num_steps = 3
  solve_type = NEWTON
  nl_abs_tol = 1e-12
[]

[Outputs]
  csv = true
[]
