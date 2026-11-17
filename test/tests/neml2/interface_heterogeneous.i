# Verifies that two NEML2 sub-blocks with different models and disjoint 'block's share an interface
# between their blocks. Each sub-block lists the same interface sideset, evaluates it only on the
# side in its own block, with that side's inputs, and publishes only to that side.
#
# The models are NEML2TestModel: sum = p1*A + p2*B, dsum/dA = p1. The NEML2 inputs A and B are block
# materials that differ across the interface:
#   block A side, model_A (p1 = 2, p2 = 3), A = 1, B = 2: sum = 8,  dsum/dA = 2
#   block B side, model_B (p1 = 4, p2 = 5), A = 3, B = 4: sum = 32, dsum/dA = 4
# Evaluating a side with the other side's inputs or model would give 18 or 14 instead of 8 or 32.
#
# The interface sideset is A_B (block A on the primary side) by default; the flipped test selects
# B_A (block B on the primary side) and reproduces the same values.

interface = A_B
# Which side of 'interface' block A is on, as an InterfaceQpMaterialPropertyRealUO value type
A_side = primary
B_side = secondary

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 2
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
[]

[Materials]
  [inputs_A]
    type = GenericConstantMaterial
    prop_names = 'A B'
    prop_values = '1 2'
    block = 'A'
  []
  [inputs_B]
    type = GenericConstantMaterial
    prop_names = 'A B'
    prop_values = '3 4'
    block = 'B'
  []
[]

[NEML2]
  input = 'models/custom_model.i'
  device = 'cpu'
  eager = true
  load = 'models/test_models.py'

  inputs = 'A B'
  input_types = 'MATERIAL MATERIAL'
  derivatives = 'sum A'

  [left]
    model = 'model_A'
    block = 'A'
    interface = ${interface}
    volume = false
  []
  [right]
    model = 'model_B'
    block = 'B'
    interface = ${interface}
    volume = false
  []
[]

[UserObjects]
  [sum_A_uo]
    type = InterfaceQpMaterialPropertyRealUO
    property = 'sum'
    boundary = ${interface}
    interface_value_type = ${A_side}
    execute_on = 'TIMESTEP_END'
  []
  [dsum_A_uo]
    type = InterfaceQpMaterialPropertyRealUO
    property = 'dsum/dA'
    boundary = ${interface}
    interface_value_type = ${A_side}
    execute_on = 'TIMESTEP_END'
  []
  [sum_B_uo]
    type = InterfaceQpMaterialPropertyRealUO
    property = 'sum'
    boundary = ${interface}
    interface_value_type = ${B_side}
    execute_on = 'TIMESTEP_END'
  []
  [dsum_B_uo]
    type = InterfaceQpMaterialPropertyRealUO
    property = 'dsum/dA'
    boundary = ${interface}
    interface_value_type = ${B_side}
    execute_on = 'TIMESTEP_END'
  []
[]

[AuxVariables]
  [sum_A]
    order = CONSTANT
    family = MONOMIAL
  []
  [dsum_A]
    order = CONSTANT
    family = MONOMIAL
  []
  [sum_B]
    order = CONSTANT
    family = MONOMIAL
  []
  [dsum_B]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [sum_A]
    type = InterfaceValueUserObjectAux
    variable = sum_A
    boundary = ${interface}
    interface_uo_name = sum_A_uo
    execute_on = 'TIMESTEP_END'
  []
  [dsum_A]
    type = InterfaceValueUserObjectAux
    variable = dsum_A
    boundary = ${interface}
    interface_uo_name = dsum_A_uo
    execute_on = 'TIMESTEP_END'
  []
  [sum_B]
    type = InterfaceValueUserObjectAux
    variable = sum_B
    boundary = ${interface}
    interface_uo_name = sum_B_uo
    execute_on = 'TIMESTEP_END'
  []
  [dsum_B]
    type = InterfaceValueUserObjectAux
    variable = dsum_B
    boundary = ${interface}
    interface_uo_name = dsum_B_uo
    execute_on = 'TIMESTEP_END'
  []
[]

[Postprocessors]
  [sum_A]
    type = SideAverageValue
    variable = sum_A
    boundary = ${interface}
    execute_on = 'TIMESTEP_END'
  []
  [dsum_A]
    type = SideAverageValue
    variable = dsum_A
    boundary = ${interface}
    execute_on = 'TIMESTEP_END'
  []
  [sum_B]
    type = SideAverageValue
    variable = sum_B
    boundary = ${interface}
    execute_on = 'TIMESTEP_END'
  []
  [dsum_B]
    type = SideAverageValue
    variable = dsum_B
    boundary = ${interface}
    execute_on = 'TIMESTEP_END'
  []
[]

[Executioner]
  type = Steady
[]

[Outputs]
  csv = true
[]
