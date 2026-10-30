# Verifies that an interface_only NEML2 model publishes its output and derivatives on BOTH sides of a
# split interface, not just the primary side, and that doing so coexists with a bulk material that
# declares the same property names on the blocks the interface straddles.
#
# The model is NEML2TestModel2: s = u*u - 0.1, ds/du = 2*u. The NEML2 input 'u' is a block material
# that differs across the interface (u = 0.3 on A, u = 0.7 on B), so the two sides carry distinct,
# nonzero outputs:
#   primary side of A_B (block A): s = 0.3*0.3 - 0.1 = -0.01,  ds/du = 0.6
#   neighbor side of A_B (block B): s = 0.7*0.7 - 0.1 =  0.39,  ds/du = 1.4
# s_A/dsdu_A read the primary (boundary) material data on A_B; s_B/dsdu_B read the neighbor material
# data on A_B through an InterfaceUserObject. Before the two-sided fix the neighbor material data was
# never written by any object and s_B/dsdu_B read back as zero.
#
# [Materials/bulk] declares the same names 's' and 'ds/du' on blocks A and B. It proves that the
# boundary-restricted interface retrievers do not collide with a bulk producer (no duplicate
# declaration error), that the bulk values survive untouched where the interface model does not run
# (s_bulk_elem on the elements, s_bulk_side on the external boundary 'left'), and that on the
# interface sideset itself the interface values take precedence on both sides.
#
# MaterialPropertySource additionally consumes both the primary (getMaterialProperty) and the
# neighbor (getNeighborMaterialProperty) copies of s and its derivatives, exercising the full
# primary/neighbor read path and a hand-coded four-coupling (EE/EN/NE/NN) interface Jacobian.

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 10
    ny = 3
  []
  [A]
    type = SubdomainBoundingBoxGenerator
    input = 'gmg'
    block_id = 0
    block_name = 'A'
    bottom_left = '0 0 0'
    top_right = '0.5 1 0'
  []
  [B]
    type = SubdomainBoundingBoxGenerator
    input = 'A'
    block_id = 1
    block_name = 'B'
    bottom_left = '0.5 0 0'
    top_right = '1 1 0'
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = 'B'
  []
[]

[Variables]
  [u]
  []
[]

[Kernels]
  [diff]
    type = Diffusion
    variable = u
  []
  [react]
    type = Reaction
    variable = u
  []
[]

[Materials]
  # Distinct NEML2 inputs on the two interface sides.
  [uA]
    type = GenericConstantMaterial
    prop_names = 'u'
    prop_values = 0.3
    block = 'A'
  []
  [uB]
    type = GenericConstantMaterial
    prop_names = 'u'
    prop_values = 0.7
    block = 'B'
  []
  # Bulk producer of the same property names the interface model emits.
  [bulk]
    type = GenericConstantMaterial
    prop_names = 's ds/du'
    prop_values = '10 20'
    block = 'A B'
  []
[]

[InterfaceKernels]
  # A stable primary-side consumer that makes MOOSE traverse the interface so the NEML2 retriever
  # reinitializes on it. The exact two-sided output assertion is made through the postprocessors
  # below; the neighbor-side material-data read path is exercised by the Jacobian variant of this
  # test, which swaps in MaterialPropertySource (reads getNeighborMaterialProperty) via cli_args.
  [flux]
    type = SimpleCohesiveFlux
    variable = u
    neighbor_var = u
    boundary = 'A_B'
    flux = 's'
    dflux_djump = 'ds/du'
  []
[]

[NEML2]
  input = 'models/custom_model.i'
  device = 'cpu'
  eager = true
  load = 'models/test_models.py'

  input_types = 'MATERIAL'
  inputs = 'u'

  derivatives = 's u'

  [source]
    model = 'interface_source'
    block = 'A B'
    interface = 'A_B'
    interface_only = true
  []
[]

[Executioner]
  type = Steady
  # Matrix-free action because the flux comes from a block material (matches interface_only_material.i).
  solve_type = PJFNK
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_abs_tol = 1e-10
  nl_rel_tol = 1e-08
[]

[UserObjects]
  # Reads the neighbor material data on A_B, i.e. the block-B side of the split interface.
  [s_neighbor_uo]
    type = InterfaceQpMaterialPropertyRealUO
    property = 's'
    boundary = 'A_B'
    interface_value_type = 'secondary'
    execute_on = 'TIMESTEP_END'
  []
  [dsdu_neighbor_uo]
    type = InterfaceQpMaterialPropertyRealUO
    property = 'ds/du'
    boundary = 'A_B'
    interface_value_type = 'secondary'
    execute_on = 'TIMESTEP_END'
  []
[]

[AuxVariables]
  [s_neighbor]
    order = CONSTANT
    family = MONOMIAL
  []
  [dsdu_neighbor]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [s_neighbor]
    type = InterfaceValueUserObjectAux
    variable = s_neighbor
    boundary = 'A_B'
    interface_uo_name = s_neighbor_uo
    execute_on = 'TIMESTEP_END'
  []
  [dsdu_neighbor]
    type = InterfaceValueUserObjectAux
    variable = dsdu_neighbor
    boundary = 'A_B'
    interface_uo_name = dsdu_neighbor_uo
    execute_on = 'TIMESTEP_END'
  []
[]

[Postprocessors]
  # Primary (boundary) material data on A_B: the block-A side.
  [s_A]
    type = SideAverageMaterialProperty
    property = 's'
    boundary = 'A_B'
    execute_on = 'TIMESTEP_END'
  []
  [dsdu_A]
    type = SideAverageMaterialProperty
    property = 'ds/du'
    boundary = 'A_B'
    execute_on = 'TIMESTEP_END'
  []
  # Neighbor material data on A_B: the block-B side.
  [s_B]
    type = SideAverageValue
    variable = s_neighbor
    boundary = 'A_B'
    execute_on = 'TIMESTEP_END'
  []
  [dsdu_B]
    type = SideAverageValue
    variable = dsdu_neighbor
    boundary = 'A_B'
    execute_on = 'TIMESTEP_END'
  []
  # The bulk producer of 's' is untouched where the interface model does not run: in the volume
  # (block material data) and on an external boundary (face material data off the interface).
  [s_bulk_elem]
    type = ElementAverageMaterialProperty
    mat_prop = 's'
    execute_on = 'TIMESTEP_END'
  []
  [s_bulk_side]
    type = SideAverageMaterialProperty
    property = 's'
    boundary = 'left'
    execute_on = 'TIMESTEP_END'
  []
[]

[Outputs]
  csv = true
  execute_on = 'FINAL'
[]
