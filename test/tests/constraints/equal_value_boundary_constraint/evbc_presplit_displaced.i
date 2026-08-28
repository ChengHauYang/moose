[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 6
  ny = 6
  elem_type = QUAD4
  allow_renumbering = false
  displacements = 'disp_x disp_y'

  [Partitioner]
    type = GridPartitioner
    nx = 2
    ny = 1
    nz = 1
  []
[]

[Variables]
  [diffused]
    order = FIRST
    family = LAGRANGE
  []
[]

[AuxVariables]
  [disp_x]
  []
  [disp_y]
  []
[]

[Kernels]
  [diff]
    type = Diffusion
    variable = diffused
  []
[]

[BCs]
  [left]
    type = DirichletBC
    variable = diffused
    preset = false
    boundary = left
    value = 1
  []
  [right]
    type = DirichletBC
    variable = diffused
    preset = false
    boundary = right
    value = 0
  []
[]

[Constraints]
  [y_top]
    type = EqualValueBoundaryConstraint
    variable = diffused
    secondary = top
    primary_node_coord = '0.3333333333333 1 0'
    penalty = 10e6
  []
[]

[Executioner]
  type = Steady
  solve_type = PJFNK
  line_search = none
[]

[Outputs]
  execute_on = timestep_end
  exodus = true
  show = diffused
[]
