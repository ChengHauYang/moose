# Minimal interface-only fixture for NEML2 Kokkos Retriever storage and Mandel conversion.
# cli_args select the output side and matching device oracle.
[Mesh]
  [mesh]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 3
    ny = 1
  []
  [middle]
    type = SubdomainBoundingBoxGenerator
    input = mesh
    bottom_left = '0.3333333333333333 0 0'
    top_right = '0.6666666666666667 1 0'
    block_id = 1
  []
  [right]
    type = SubdomainBoundingBoxGenerator
    input = middle
    bottom_left = '0.6666666666666667 0 0'
    top_right = '1 1 0'
    block_id = 2
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
  [unused_strain]
    type = GenericConstantSymmetricRankTwoTensor
    block = 2
    tensor_name = strain
    tensor_values = '7 8 0 0 0 9'
  []
[]

[NEML2]
  input = models/kokkos_interface_retriever.i
  eager = true
  device = cpu
  input_types = MATERIAL
  inputs = strain
  derivatives = 'stress strain tangent'
  [elasticity]
    model = elasticity
    executor_name = elasticity_executor
    block = '0 1'
    interface = Block0_Block1
    interface_only = true
    output_backend = kokkos
    interface_output_side = both
  []
[]

[Materials]
  [primary_oracle]
    type = KokkosNEML2PrimaryInterfaceRetrieverOracle
    boundary = Block0_Block1
    stress = stress
    tangent = tangent
    expected_normal_stress = 2
    expected_transverse_stress = 4
    expected_shear_stress = 6
    expected_shear_tangent = 1
  []
  [neighbor_oracle]
    type = KokkosNEML2NeighborInterfaceRetrieverOracle
    boundary = Block0_Block1
    stress = stress
    tangent = tangent
    expected_normal_stress = 8
    expected_transverse_stress = 10
    expected_shear_stress = 12
    expected_shear_tangent = 1
  []
  [zero_primary_oracle]
    type = KokkosNEML2PrimaryInterfaceRetrieverOracle
    boundary = Block1_Block2
    stress = zero_stress
    tangent = zero_tangent
    expected_normal_stress = 0
    expected_transverse_stress = 0
    expected_shear_stress = 0
    expected_shear_tangent = 0
  []
  [zero_neighbor_oracle]
    type = KokkosNEML2NeighborInterfaceRetrieverOracle
    boundary = Block1_Block2
    stress = zero_stress
    tangent = zero_tangent
    expected_normal_stress = 0
    expected_transverse_stress = 0
    expected_shear_stress = 0
    expected_shear_tangent = 0
  []
  [zero_stress]
    type = NEML2ToKokkosSideRankTwoMaterialProperty
    boundary = 'Block0_Block1 Block1_Block2'
    neml2_executor = elasticity_executor
    from_neml2 = stress
    to_moose = zero_stress
  []
  [zero_tangent]
    type = NEML2ToKokkosSideRankFourMaterialProperty
    boundary = 'Block0_Block1 Block1_Block2'
    neml2_executor = elasticity_executor
    from_neml2 = stress
    neml2_input_derivative = strain
    to_moose = zero_tangent
  []
  [zero_stress_neighbor]
    type = NEML2ToKokkosNeighborRankTwoMaterialProperty
    boundary = 'Block0_Block1 Block1_Block2'
    neml2_executor = elasticity_executor
    from_neml2 = stress
    to_moose = zero_stress
  []
  [zero_tangent_neighbor]
    type = NEML2ToKokkosNeighborRankFourMaterialProperty
    boundary = 'Block0_Block1 Block1_Block2'
    neml2_executor = elasticity_executor
    from_neml2 = stress
    neml2_input_derivative = strain
    to_moose = zero_tangent
  []
[]

[Executioner]
  type = Steady
[]
