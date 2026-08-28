[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 16
  ny = 4
  xmax = 4
  ymax = 1
  parallel_type = distributed

  # Split the mesh into four vertical strips so that the top boundary spans all
  # four partitions. The EVBC primary node is on the far-right partition.
  [Partitioner]
    type = GridPartitioner
    nx = 4
    ny = 1
    nz = 1
  []
[]

[Variables]
  [u]
  []
[]

[Kernels]
  [diff]
    type = Diffusion
    variable = u
  []
[]

[BCs]
  [left]
    type = DirichletBC
    variable = u
    boundary = left
    value = 1
  []
  [right]
    type = DirichletBC
    variable = u
    boundary = right
    value = 0
  []
[]

[UserObjects]
  # Defer remote-element deletion until after the constraint is added. On the
  # pre-split mesh, this exercises retention of the distant primary element.
  [late_geometric_ghosting]
    type = TestGhostBoundarySideUserObject
    boundary = left
  []
[]

[Constraints]
  [top]
    type = EqualValueBoundaryConstraint
    variable = u
    secondary = top
    primary_node_coord = '4 1 0'
    penalty = 1e7
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  l_tol = 1e-10
  nl_rel_tol = 1e-12
[]

[Outputs]
  exodus = false
[]
