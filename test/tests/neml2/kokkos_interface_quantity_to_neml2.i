# Verify KokkosInterfaceQuantityToNEML2 two-sided gathering.
#
# Setup: 2x1 mesh split into blocks 0 and 1 at x = 0.5, with a BreakMeshByBlock interface
#        creating one cohesive interface (Block0_Block1).
#
# Primary side (block 0): KokkosConstantRankTwoMaterial with diagonal 1e-3.
# Neighbor side (block 1): KokkosConstantRankTwoMaterial with diagonal 2e-3.
#
# KokkosInterfaceQuantityToNEML2 gathers both sides into a single device buffer.
# KokkosInterfaceGatheringOracle reads gatheredData() and verifies each batch row has
# the expected diagonal.  It returns the number of checked rows as a postprocessor;
# the gold CSV asserts it equals 2 * n_qps_per_face (one QP per side per face, here 2).
[Mesh]
  [mesh]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 2
    ny = 1
  []
  [left]
    type = SubdomainBoundingBoxGenerator
    input = mesh
    bottom_left = '0 0 0'
    top_right = '0.5 1 0'
    block_id = 0
  []
  [right]
    type = SubdomainBoundingBoxGenerator
    input = left
    bottom_left = '0.5 0 0'
    top_right = '1 1 0'
    block_id = 1
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = right
  []
[]

[Problem]
  solve = false
[]

[Materials]
  [F_primary]
    type = KokkosConstantRankTwoMaterial
    block = 0
    prop_name = F
    values = '1e-3 0 0  0 1e-3 0  0 0 1e-3'
  []
  [F_neighbor]
    type = KokkosConstantRankTwoMaterial
    block = 1
    prop_name = F
    values = '2e-3 0 0  0 2e-3 0  0 0 2e-3'
  []
[]

[UserObjects]
  [batch_index_generator]
    type = NEML2BatchIndexGenerator
    interface_boundaries = Block0_Block1
    interface_only = true
    execute_on = 'initial linear nonlinear'
  []
  [F_gatherer]
    type = KokkosInterfaceQuantityToNEML2
    boundary = Block0_Block1
    primary_from_moose = F
    neighbor_from_moose = F
    to_neml2 = F
    batch_index_generator = batch_index_generator
    execute_on = 'initial linear nonlinear'
  []
[]

[Postprocessors]
  [checked_rows]
    type = KokkosInterfaceGatheringOracle
    gatherer = F_gatherer
    batch_index_generator = batch_index_generator
    expected_diag_primary = 1e-3
    expected_diag_neighbor = 2e-3
    execute_on = timestep_end
  []
[]

[Executioner]
  type = Steady
[]

[Outputs]
  csv = true
  execute_on = FINAL
[]
