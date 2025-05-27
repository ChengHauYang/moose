[GlobalParams]
  block = '0'
[]

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 3
    xmax = 5
    ymax = 0.25
    zmax = 3
    nx = 100
    ny = 5
    nz = 60
    subdomain_ids = '1'
  []

  [subdomain1]
    type = SubdomainInterceptedGenerator
    input = 'gmg'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    keep_inside_as_inside = true
    multi_geo = true
    lambda = 0.5
    outer_boundary = true
    function = 'z-1.45'
  []

  [subdomain2]
    type = SubdomainInterceptedGenerator
    input = 'subdomain1'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    keep_inside_as_inside = true
    multi_geo = true
    lambda = 0.5
    outer_boundary = true
    function = '1.55-z'
  []

  [subdomain3]
    type = SubdomainInterceptedGenerator
    input = 'subdomain2'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    keep_inside_as_inside = true
    multi_geo = true
    lambda = 0.5
    outer_boundary = true
    function = 'y-0.15'
  []

  use_displaced_mesh = false
[]

[Variables]
  [cond]
    order = FIRST
  []
[]

[SpatioTemporalPaths]
  [path]
    type = CSVPiecewiseLinearSpatioTemporalPath
    file = 'horizontal_lines_with_time.csv'
    verbose = true
  []
[]

[MeshModifiers]
  [esm]
    type = SpatioTemporalPathElementSubdomainModifier
    path = 'path'
    radius = 0.03
    target_subdomain = '0'
    block = '0 1'
    execute_on = 'TIMESTEP_BEGIN'
  []
[]

[Materials]
  [volumetric_heat] # need to be exactly this name!
    type = ADMovingEllipsoidalHeatSource
    path = 'path'
    power = 1
    efficiency = 1
    scale = 1
    a = 0.035
    b = 0.01
    outputs = exodus
  []
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
  [hsource]
    type = ADMatHeatSource
    material_property = 'volumetric_heat'
    variable = cond
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
  end_time = 400
[]

[Outputs]
  exodus = true
[]
