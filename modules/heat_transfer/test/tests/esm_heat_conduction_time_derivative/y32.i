[Problem]
  default_block = '0 1 3'
[]

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 48
    ny = 32
    xmin = 0
    xmax = 3
    ymin = 0
    ymax = 2
  []
  [block_left]
    type = SubdomainBoundingBoxGenerator
    input = gmg
    block_id = 0
    block_name = material_left
    bottom_left = '0 0 0'
    top_right = '1.25 2.0 0'
  []
  [block_right]
    type = SubdomainBoundingBoxGenerator
    input = block_left
    block_id = 1
    block_name = material_right
    bottom_left = '1.75 0 0'
    top_right = '3.0 2.0 0'
  []
  [block_middle]
    type = SubdomainBoundingBoxGenerator
    input = block_right
    block_id = 2
    block_name = material_null
    bottom_left = '1.25 0 0'
    top_right = '1.75 2.0 0'
  []
  [block_middle_new]
    type = SubdomainBoundingBoxGenerator
    input = block_middle
    block_id = 3
    block_name = material_middle
    bottom_left = '1.25 1.0 0'
    top_right = '1.75 2.0 0'
  []
  use_displaced_mesh = false
[]

[MeshModifiers]
  [line_filling]
    type = RowElementModifier
    subdomain_id_change_from = 2
    subdomain_id_change_to = 3
    number_of_elements = 1
    x_min = 1.25
    x_max = 1.75
    y_min = 0
    y_max = 2
    change_one_row = true
    execute_on = 'INITIAL TIMESTEP_END'

    # --- new for setting IC --- #
    inactive_subdomain_ID = 2
    ic_strategy = "IC_EXTRAPOLATE_FIRST_LAYER"

    block = '0 1 2 3'
  []
[]

[Variables]
  [cond]
    order = FIRST
  []
[]

[Kernels]
  [diff]
    type = HeatConduction
    variable = cond
  []
[]

[Materials]
  [material_left_cond]
    type = HeatConductionMaterial
    block = 0
    specific_heat = 30
    thermal_conductivity = 20
  []
  [material_right_cond]
    type = HeatConductionMaterial
    block = 1
    specific_heat = 75
    thermal_conductivity = 50
  []
  [material_middle_cond]
    type = HeatConductionMaterial
    block = 3
    specific_heat = 150
    thermal_conductivity = 100
  []
  [density_left]
    type = GenericConstantMaterial
    prop_names = 'density'
    block = 0
    prop_values = 10
  []
  [density_right]
    type = GenericConstantMaterial
    prop_names = 'density'
    block = 1
    prop_values = 20
  []
  [density_middle]
    type = GenericConstantMaterial
    prop_names = 'density'
    block = 3
    prop_values = 50
  []
[]

[BCs]
  [left]
    type = DirichletBC
    variable = cond
    boundary = left
    value = 10
  []

  [right]
    type = DirichletBC
    variable = cond
    boundary = right
    value = 0
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_max_its = 100
  nl_rel_tol = 1e-6
  nl_abs_tol = 1e-10
  dt = 1
  end_time = 32
[]

[Postprocessors]
  [T3]
    type = ElementAverageValue
    variable = cond
    block = '3'
    execute_on = 'INITIAL TIMESTEP_END'
  []
[]

[Outputs]
  exodus = true
  csv = true
[]
