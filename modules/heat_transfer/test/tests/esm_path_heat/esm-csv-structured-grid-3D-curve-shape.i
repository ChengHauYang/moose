[GlobalParams]
  block = '0 2'
[]

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 3
    xmax = 3
    ymax = 0.25
    zmax = 3
    nx = 60
    ny = 5
    nz = 60
    subdomain_ids = '1'
  []

  [subdomain1]
    type = SubdomainInterceptedGenerator
    input = 'gmg'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    lambda = 1
    outer_boundary = false
    function = '(y-0.3)^2+(z-1.5)^2-0.15^2'
  []

  [refine1]
    type = RefineBlockGenerator
    input = subdomain1
    block = '1'
    refinement = '1'
    enable_neighbor_refinement = false
  []

  [subdomain2]
    type = SubdomainInterceptedGenerator
    input = 'refine1'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    lambda = 1
    outer_boundary = false
    function = '(y-0.3)^2+(z-1.5)^2-0.15^2'
  []

  [refine2]
    type = RefineBlockGenerator
    input = subdomain2
    block = '1'
    refinement = '1'
    enable_neighbor_refinement = false
  []

  [subdomain3]
    type = SubdomainInterceptedGenerator
    input = 'refine2'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    lambda = 1
    outer_boundary = false
    function = '(y-0.3)^2+(z-1.5)^2-0.15^2'
  []

  # [interface]
  #   type = SideSetsBetweenSubdomainsGenerator
  #   # input = subdomain3
  #   input = subdomain2
  #   primary_block = 0
  #   paired_block = 1
  #   new_boundary = 'interface'
  # []

  # [refine_interface]
  #   type = RefineSidesetGenerator
  #   input = interface
  #   boundaries = 'interface'
  #   refinement = '1'
  #   enable_neighbor_refinement = false
  # []

  # [subdomain4]
  #   type = SubdomainInterceptedGenerator
  #   input = 'refine_interface'
  #   subdomain_id_inside = 0
  #   subdomain_id_outside = 1
  #   lambda = 1
  #   outer_boundary = false
  #   function = '(y-0.3)^2+(z-1.5)^2-0.15^2'
  # []

  # [interface2]
  #   type = SideSetsBetweenSubdomainsGenerator
  #   input = subdomain2
  #   primary_block = 0
  #   paired_block = 1
  #   new_boundary = 'interface2'
  # []

  # [refine2]
  #   type = RefineSidesetGenerator
  #   input = interface2
  #   boundaries = 'interface2'
  #   refinement = '3'
  # []

  add_subdomain_ids = 2 # activated elements
  use_displaced_mesh = false
[]

[Variables]
  [cond]
    order = FIRST
  []
[]

[SpatioTemporalHeat]
  path_file = 'filled_semicircle_horizontal_lines_y_first.csv'
  ## for path
  verbose = true
  ## for esm
  target_subdomain = 2
  radius = 0.02
  execute_on_esm = 'TIMESTEP_BEGIN'
  block = '1 2'
  # execute_on_esm = 'TIMESTEP_END'
  #
  old_subdomain_reinitialized = false
  reinitialize_subdomain_ids = '0 2'
  #ic_strategy = "IC_EXTRAPOLATE_FIRST_LAYER"
  ## for heat source
  power = 1
  a = 0.035
  b = 0.01
  efficiency = 1
  scale = 1
  ## for kernel
  heat_variable = cond
[]

[Materials]
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
[]

[BCs]
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
  end_time = 2400
[]

[Outputs]
  exodus = true
[]
