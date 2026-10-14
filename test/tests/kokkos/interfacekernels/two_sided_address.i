#
# Two-sided Kokkos property addressing fixture.
#
# The interface splits a mesh into enough faces, with more than one quadrature point per face, that a
# wrong face offset, a wrong quadrature point, or a swapped pair of property storages changes the
# solution. The primary and neighbor addresses are written by KokkosTwoSidedAddressMaterial and
# KokkosTwoSidedAddressNeighborMaterial into boundary and neighbor property storage respectively, and
# KokkosTwoSidedAddressInterfaceDiffusion reads them back into the two rows of each face.
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
  [v]
  []
[]

[Kernels]
  [u_diffusion]
    type = KokkosCoefDiffusion
    variable = u
    coef = 1
  []
  [v_diffusion]
    type = KokkosCoefDiffusion
    variable = v
    coef = 1
  []
[]

[InterfaceKernels]
  [two_sided_address]
    type = KokkosTwoSidedAddressInterfaceDiffusion
    variable = u
    neighbor_var = v
    primary_coupled_var = u
    neighbor_coupled_var = v
    boundary = interface
    side_address = side_address
    neighbor_address = neighbor_address
    dside_address_djump = dside_address_djump
    dneighbor_address_djump = dneighbor_address_djump
  []
[]

[Materials]
  [primary_address]
    type = KokkosTwoSidedAddressMaterial
    variable = u
    boundary = interface
    weight = 1
  []
  [neighbor_address]
    type = KokkosTwoSidedAddressNeighborMaterial
    neighbor_var = v
    boundary = interface
    weight = 1
  []
[]

[BCs]
  [u_left]
    type = KokkosDirichletBC
    variable = u
    boundary = left
    value = 1
  []
  [v_left]
    type = KokkosDirichletBC
    variable = v
    boundary = left
    value = 0
  []
  [u_right]
    type = KokkosDirichletBC
    variable = u
    boundary = right
    value = 0
  []
  [v_right]
    type = KokkosDirichletBC
    variable = v
    boundary = right
    value = 1
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  exodus = true
  file_base = two_sided_address_out
[]
