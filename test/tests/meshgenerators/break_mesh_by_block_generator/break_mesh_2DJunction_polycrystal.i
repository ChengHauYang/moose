[Problem]
  solve = false
  boundary_restricted_node_integrity_check = false
[]

[Mesh]
  [fmg]
    type = FileMeshGenerator
    file = poly.msh
  []

  [breakmesh]
    type = BreakMeshByBlockGenerator
    input = fmg
    add_interface_on_two_sides = true
    split_interface = true
  []

  parallel_type = distributed
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

[AuxVariables]
  [proc]
    [AuxKernel]
      type = ProcessorIDAux
      execute_on = initial
    []
  []
  [proc_elem]
    family = MONOMIAL
    order = CONSTANT
    [AuxKernel]
      type = ProcessorIDAux
      execute_on = initial
    []
  []
[]

