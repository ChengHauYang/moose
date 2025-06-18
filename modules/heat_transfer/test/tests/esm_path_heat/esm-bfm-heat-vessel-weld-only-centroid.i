all_blocks = 'default pass-1 pass-2 pass-3 pass-4 pass-5 pass-6 pass-7 pass-8 pass-9 pass-10 pass-11 pass-12 pass-13 pass-14 pass-15 pass-16 pass-17 pass-18 pass-19 pass-20 pass-21 pass-22 pass-23 pass-24'

[Mesh]
  [gmg]
    type = FileMeshGenerator
    file = "partition_symmetric_shape.msh"
  []

  [walls]
    type = SideSetsFromPointsGenerator
    input = gmg
    new_boundary = 'left bottom top right'
    points = '0 0.011 0
              0.13 0 0
              0.13 0.035 0
              0.2 0.011 0'
  []
  use_displaced_mesh = false
  # add_subdomain_ids = 'default' # subdomain change to
[]

[Variables]
  [cond]
    order = FIRST
  []
[]

[AuxVariables]
  [u]
    block = ${all_blocks}
  []
  [gaussian_weight]
    family = MONOMIAL
    order = CONSTANT
  []
  [x_coord]
    family = MONOMIAL
    order = CONSTANT
    block = ${all_blocks}
  []
  [y_coord]
    family = MONOMIAL
    order = CONSTANT
    block = ${all_blocks}
  []
[]

[AuxKernels]
  [assign_x]
    type = ParsedAux
    variable = x_coord
    expression = 'x'
    use_xyzt = true
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
  [assign_y]
    type = ParsedAux
    variable = y_coord
    expression = 'y'
    use_xyzt = true
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[Materials]
  # [volumetric_heat] # need to be exactly this name!
  #   type = ADMovingEllipsoidalHeatSource
  #   path = 'path'
  #   power = 1
  #   efficiency = 1
  #   scale = 1
  #   a = 0.035
  #   b = 0.01
  #   outputs = exodus
  # []
  [density]
    type = ADGenericConstantMaterial
    prop_names = 'density  thermal_conductivity'
    prop_values = '10431.0 3.0                 '
  []
[]

[Kernels]
  [heat]
    type = ADHeatConduction
    variable = cond
    thermal_conductivity = thermal_conductivity
  []
  # [hsource]
  #   type = ADMatHeatSource
  #   material_property = 'volumetric_heat'
  #   variable = cond
  # []
[]

[BCs]
  [left]
    type = DirichletBC
    variable = cond
    boundary = left
    value = 0
  []

  [right]
    type = DirichletBC
    variable = cond
    boundary = right
    value = 0
  []

  [top]
    type = DirichletBC
    variable = cond
    boundary = top
    value = 0
  []

  [bottom]
    type = DirichletBC
    variable = cond
    boundary = bottom
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
  end_time = 1
[]

[Outputs]
  exodus = true
  csv = true
[]

[Postprocessors]
  [Va_integral]
    type = ElementIntegralVariablePostprocessor
    variable = gaussian_weight
  []

  [centroid_x_p1]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-1'
  []
  [centroid_y_p1]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-1'
  []
  [centroid_x_p2]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-2'
  []
  [centroid_y_p2]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-2'
  []
  [centroid_x_p3]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-3'
  []
  [centroid_y_p3]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-3'
  []
  [centroid_x_p4]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-4'
  []
  [centroid_y_p4]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-4'
  []
  [centroid_x_p5]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-5'
  []
  [centroid_y_p5]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-5'
  []
  [centroid_x_p6]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-6'
  []
  [centroid_y_p6]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-6'
  []
  [centroid_x_p7]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-7'
  []
  [centroid_y_p7]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-7'
  []
  [centroid_x_p8]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-8'
  []
  [centroid_y_p8]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-8'
  []
  [centroid_x_p9]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-9'
  []
  [centroid_y_p9]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-9'
  []
  [centroid_x_p10]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-10'
  []
  [centroid_y_p10]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-10'
  []
  [centroid_x_p11]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-11'
  []
  [centroid_y_p11]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-11'
  []
  [centroid_x_p12]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-12'
  []
  [centroid_y_p12]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-12'
  []
  [centroid_x_p13]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-13'
  []
  [centroid_y_p13]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-13'
  []
  [centroid_x_p14]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-14'
  []
  [centroid_y_p14]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-14'
  []
  [centroid_x_p15]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-15'
  []
  [centroid_y_p15]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-15'
  []
  [centroid_x_p16]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-16'
  []
  [centroid_y_p16]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-16'
  []
  [centroid_x_p17]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-17'
  []
  [centroid_y_p17]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-17'
  []
  [centroid_x_p18]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-18'
  []
  [centroid_y_p18]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-18'
  []
  [centroid_x_p19]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-19'
  []
  [centroid_y_p19]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-19'
  []
  [centroid_x_p20]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-20'
  []
  [centroid_y_p20]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-20'
  []
  [centroid_x_p21]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-21'
  []
  [centroid_y_p21]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-21'
  []
  [centroid_x_p22]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-22'
  []
  [centroid_y_p22]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-22'
  []
  [centroid_x_p23]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-23'
  []
  [centroid_y_p23]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-23'
  []
  [centroid_x_p24]
    type = ElementAverageValue
    variable = x_coord
    block = 'pass-24'
  []
  [centroid_y_p24]
    type = ElementAverageValue
    variable = y_coord
    block = 'pass-24'
  []
[]

