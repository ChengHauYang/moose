# Verify KokkosInterfaceQuantityToNEML2 two-sided gathering.
#
# Setup: 2x1 mesh split into blocks 0 and 1 at x = 0.5, with a BreakMeshByBlock interface
#        creating one cohesive interface (Block0_Block1).
#
# Primary side (block 0): KokkosConstantRankTwoMaterial with nine distinct components 1.1e-3..1.9e-3.
# Neighbor side (block 1): KokkosConstantRankTwoMaterial with nine distinct components 2.1e-3..2.9e-3.
# Every component differs within a row and between sides, so any component transposition, dropped
# side, or mis-placed batch row changes at least one component and fails the check.
#
# NEML2Action creates KokkosInterfaceQuantityToNEML2, which gathers both sides into a
# single device buffer. KokkosInterfaceGatheringOracle verifies, on the device the buffer lives on,
# that each batch row matches the full nine-component primary- or neighbor-side pattern. It returns
# the number of matched rows as a postprocessor; the gold CSV asserts it equals 2 * n_qps_per_face
# (one QP per side per face, here 2). Running the oracle's comparison on the device keeps the test
# valid on CUDA, where gatheredData() is a device tensor that cannot be read on the host.
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
    values = '1.1e-3 1.2e-3 1.3e-3  1.4e-3 1.5e-3 1.6e-3  1.7e-3 1.8e-3 1.9e-3'
  []
  [F_neighbor]
    type = KokkosConstantRankTwoMaterial
    block = 1
    prop_name = F
    values = '2.1e-3 2.2e-3 2.3e-3  2.4e-3 2.5e-3 2.6e-3  2.7e-3 2.8e-3 2.9e-3'
  []
[]

[NEML2]
  input = models/kokkos_quantity_to_neml2.i
  load = models/kokkos_r2_scale.py
  eager = true
  input_types = MATERIAL
  inputs = F
  [weighted]
    model = weighted_model
    interface = Block0_Block1
    interface_only = true
    input_backend = kokkos
  []
[]

[Postprocessors]
  [checked_rows]
    type = KokkosInterfaceGatheringOracle
    gatherer = __moose_to_neml2_F_0_weighted__
    batch_index_generator = neml2_index_weighted_model_weighted
    expected_primary = '1.1e-3 1.2e-3 1.3e-3  1.4e-3 1.5e-3 1.6e-3  1.7e-3 1.8e-3 1.9e-3'
    expected_neighbor = '2.1e-3 2.2e-3 2.3e-3  2.4e-3 2.5e-3 2.6e-3  2.7e-3 2.8e-3 2.9e-3'
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
