# Control case for distributed_evbc.i: identical except that
# BreakMeshByBlockGenerator is removed, so the mesh stays conforming.
# Used to determine whether the augmentSendList() crash needs the broken
# mesh or is a plain EqualValueBoundaryConstraint + distributed mesh failure.

[Mesh]
  parallel_type = DISTRIBUTED

  [base]
    type = FileMeshGenerator
    file = creepTest10Gr.exo
  []
  [add_side_sets]
    type = SideSetsFromNormalsGenerator
    input = base
    # Required on a distributed mesh; the outer faces of this mesh are flat and
    # axis-aligned, so pinning the reference normal selects the same sides.
    fixed_normal = true
    normals = '-1  0  0
                1  0  0
                0 -1  0
                0  1  0
                0  0 -1
                0  0  1'
    new_boundary = 'x0 x1 y0 y1 z0 z1'
  []

  displacements='disp_x disp_y disp_z'
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

# No Kernels are needed: the failure happens during problem initialization.
[Problem]
  kernel_coverage_check = FALSE
[]

[Executioner]
  type = Steady
  # solve = false
[]

[Outputs]
  exodus = false
[]
