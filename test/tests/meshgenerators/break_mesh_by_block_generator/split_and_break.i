[Mesh]
  [gen]
    type = FileMeshGenerator
    file = 'mesh_splitted.cpa.gz'
  []


  [break]
    input = gen
    type = BreakMeshByBlockGenerator
    split_interface = true
    add_interface_on_two_sides = true
  []

[]

[Outputs]
  exodus = true
[]
