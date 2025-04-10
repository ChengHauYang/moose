[Problem]
  kernel_coverage_check = false
  material_coverage_check = false
  #boundary_restricted_node_integrity_check = false
  #material_dependency_check = false
  #boundary_restricted_elem_integrity_check = false
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
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

#[Problem]
#  solve = false
#[]

[Variables]
  [cond]
    #order = SECOND
    order = FIRST
    block = '0 1 3'
  []
[]

[Kernels]
  [diff]
    type = HeatConduction
    variable = cond
    block = '0 1 3'
  []

  [heat_ie]
    type = HeatConductionTimeDerivative
    variable = cond
    block = '0 1 3'
  []
  #[null]
  #  type = NullKernel
  #  variable = cond
  #  block = '2'
  #[]
[]

#[Materials]
#  [./material_left_cond]
#    type = HeatConductionMaterial
#    block = 0
#    specific_heat = 900
#    thermal_conductivity = 237 # Aluminum
#  [../]
#  [./material_right_cond]
#    type = HeatConductionMaterial
#    block = 1
#    specific_heat = 385
#    thermal_conductivity = 400 # Copper
#  [../]
#  [./material_middle_cond]
#    type = HeatConductionMaterial
#    block = 3
#    specific_heat = 380
#    thermal_conductivity = 300 # Brass
#  [../]
#
#  [density_left]
#    type = GenericConstantMaterial
#    prop_names = 'density'
#    block = 0
#    prop_values = 2700.0
#  []
#  [density_right]
#    type = GenericConstantMaterial
#    prop_names = 'density'
#    block = 1
#    prop_values = 8960.0
#  []
#  [density_middle]
#    type = GenericConstantMaterial
#    prop_names = 'density'
#    block = 3
#    prop_values = 8500.0
#  []
#[]

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

#[AuxVariables]
#  [u]
#  []
#[]

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
  petsc_options_iname = '-ksp_type -pc_type -pc_factor_mat_solver_type -ksp_max_it -snes_atol -snes_rtol'
  petsc_options_value = 'bcgs lu mumps 100 1e-12 1e-12'
  type = Transient
  dt = 1
  end_time = 36
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
  vtk = true
  csv = true
[]

