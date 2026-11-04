#
# Kokkos InterfaceUserObject dispatch fixture. The primary interface has four faces with two
# quadrature points each, so the counter must report eight mapped primary/neighbor QP pairs.
#
[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 6
    ny = 4
    xmax = 1
    ymax = 1
  []
  [left_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '0 0 0'
    top_right = '0.5 1 0'
    block_id = 1
  []
  [interface]
    type = SideSetsBetweenSubdomainsGenerator
    input = left_block
    primary_block = 0
    paired_block = 1
    new_boundary = interface
  []
[]

[Variables]
  [u]
  []
[]

[UserObjects]
  [interface_qp_count]
    type = KokkosInterfaceUserObjectCounter
    boundary = interface
    execute_on = initial
  []
[]

[Problem]
  solve = false
[]

[Executioner]
  type = Steady
[]

[Outputs]
  csv = true
  execute_on = INITIAL
[]
