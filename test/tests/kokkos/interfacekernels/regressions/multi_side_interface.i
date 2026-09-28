# The two central bottom elements each have a top side and an outer side on
# the interface. Both faces must participate in interface assembly.
!include ../interface_material.i

[Mesh/left_block]
  bottom_left := '0.25 0 0'
  top_right := '0.75 0.5 0'
[]

[Mesh/interface]
  primary_block := 1
  paired_block := 0
[]

[Materials/interface_material]
  # Two top faces and the two outer vertical faces of the central bottom block.
  expected_interface_faces = 4
  # This mesh keeps every interface face and its neighbor on the same rank, so the ghost-path
  # assertion that the parent input relies on cannot hold here.
  require_ghost_neighbor = false
[]
