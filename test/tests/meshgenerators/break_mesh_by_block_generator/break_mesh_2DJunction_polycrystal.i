[Problem]
  solve = false
  boundary_restricted_node_integrity_check = false
[]

[Mesh]
  [fmg]
    type = FileMeshGenerator
    file = poly.msh
    #parallel_type = replicated
  []

  [breakmesh]
    type = BreakMeshByBlockGenerator
    input = fmg
    # add_interface_on_two_sides = true
  []
[]

[Outputs]
  exodus = true
[]

[Variables]
  [diffused]
    order = FIRST
  []
[]

[Executioner]
  type = Steady
[]

