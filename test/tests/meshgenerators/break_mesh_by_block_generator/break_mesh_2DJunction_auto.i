[Problem]
  solve = false
  boundary_restricted_node_integrity_check = false
[]

[Mesh]
  [./fmg]
    type = FileMeshGenerator
    file = 4ElementJunction.e
  []

  [./breakmesh]
    type = BreakMeshByBlockGenerator
    input = fmg
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

