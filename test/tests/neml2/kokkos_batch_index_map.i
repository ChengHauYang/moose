# Verify that an Action-created NEML2 batch generator publishes face-indexed maps before Kokkos UOs.
[Mesh]
  [mesh]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 6
    ny = 4
  []
  [left]
    type = SubdomainBoundingBoxGenerator
    input = mesh
    bottom_left = '0 0 0'
    top_right = '0.5 1 0'
    block_id = 1
  []
  [interface]
    type = SideSetsBetweenSubdomainsGenerator
    input = left
    primary_block = 0
    paired_block = 1
    new_boundary = interface
  []
[]

[Problem]
  solve = false
[]

[NEML2]
  eager = true
  input = models/kokkos_interface_retriever.i
  device = cpu
  input_types = MATERIAL
  inputs = strain
  [elasticity]
    model = elasticity
    executor_name = elasticity_executor
    batch_index_generator_name = batch_index_generator
    block = '0 1'
    interface = interface
    interface_only = true
    output_backend = moose
  []
[]

[Materials]
  [left_strain]
    type = GenericConstantSymmetricRankTwoTensor
    block = 0
    tensor_name = strain
    tensor_values = '1 2 0 0 0 3'
  []
  [right_strain]
    type = GenericConstantSymmetricRankTwoTensor
    block = 1
    tensor_name = strain
    tensor_values = '4 5 0 0 0 6'
  []
[]

[UserObjects]
  [batch_index_map]
    type = KokkosBatchIndexMapVerifier
    boundary = interface
    batch_index_generator = batch_index_generator
    execute_on = initial
  []
[]

[Executioner]
  type = Steady
[]

[Outputs]
  csv = true
  execute_on = INITIAL
[]
