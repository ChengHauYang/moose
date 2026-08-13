[Mesh]
  parallel_type = DISTRIBUTED

  [base]
    type = FileMeshGenerator
    file = creepTest10Gr.exo
  []
  [breakmesh]
    type = BreakMeshByBlockGenerator
    input = base
  []
  [add_side_sets]
    type = SideSetsFromNormalsGenerator
    input = breakmesh
    normals = '-1  0  0
                1  0  0
                0 -1  0
                0  1  0
                0  0 -1
                0  0  1'
    new_boundary = 'x0 x1 y0 y1 z0 z1'
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[Constraints]
  [x1]
    type = EqualValueBoundaryConstraint
    variable = disp_x
    secondary = x1
    penalty = 1e7
  []
  [y1]
    type = EqualValueBoundaryConstraint
    variable = disp_y
    secondary = y1
    penalty = 1e7
  []
  [z1]
    type = EqualValueBoundaryConstraint
    variable = disp_z
    secondary = z1
    penalty = 1e7
  []
[]

[Executioner]
  type = Steady
  solve = false
[]

[Outputs]
  exodus = false
[]
