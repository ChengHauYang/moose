#weld_blocks = '0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32'

weld_blocks_and_change_to = '0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 37'

[GlobalParams]
  block = '33 34 35 36'
[]

[Mesh]
  [gmg]
    type = FileMeshGenerator
    file = "weld_mesh_cutout.exo"
    #file = "vessel_40mm_coarse_v05.exo"
  []

  [walls]
    type = SideSetsFromPointsGenerator
    input = gmg
    new_boundary = 'left bottom top right'
    points = '0 10 0
              1 0 0
              1 35 0
              1000 10 0'
  []
  use_displaced_mesh = false
  add_subdomain_ids = 37
[]

[Variables]
  [cond]
    order = FIRST
  []
[]

# [SpatioTemporalPaths]
#   [path]
#     type = CSVPiecewiseLinearSpatioTemporalPath
#     # file = 'concentric_circles.csv'
#     file = 'concentric_circles_reverse.csv'
#     verbose = true
#   []
# []

# [MeshModifiers]
#   [esm]
#     type = SpatioTemporalPathElementSubdomainModifier
#     path = 'path'
#     radius = 0.03
#     target_subdomain = '0'
#     block = '0 1'
#     execute_on = 'TIMESTEP_END'

#     # --- new for setting IC --- #
#     unsolved_blocks = '1'
#     ic_strategy = "IC_EXTRAPOLATE_FIRST_LAYER"
#   []
# []

# [SpatioTemporalHeat]
#   path_file = 'concentric_circles_reverse.csv'
#   ## for path
#   verbose = true
#   ## for esm
#   block = '0 1'
#   target_subdomain = 0
#   radius = 0.03
#   execute_on_esm = 'TIMESTEP_BEGIN'
#   # execute_on_esm = 'TIMESTEP_END'
#   unsolved_blocks = '1'
#   ic_strategy = "IC_EXTRAPOLATE_FIRST_LAYER"
#   ## for heat source
#   power = 1
#   a = 0.035
#   b = 0.01
#   efficiency = 1
#   scale = 1
#   ## for kernel
#   heat_variable = cond
# []

[AuxVariables]
  [u]
    block = ${weld_blocks_and_change_to}
  []
[]

[AuxKernels]
  [cut]
    type = ParsedAux
    variable = 'u'
    # expression = 'y-(0.1+0.025*t)'
    # expression = 'x-0.1*t'
    constant_names = 'y0 x0 Ly vy dx'
    constant_expressions = '0.0 500.0 35.0 3.18182 200.0'

    # expression = '
    #   max(
    #     x - (x0 +dx+ dx * floor(t / (Ly / vy))),
    #     y - (y0 + vy + vy * (t - (Ly / vy) * floor(t / (Ly / vy))))
    #   )'
    expression = '
      max(
        x - (x0 +dx*0.8+ dx * floor(t / (Ly / vy))),
        y - (y0 + vy + vy * (t - (Ly / vy) * floor(t / (Ly / vy))))
      )'

    use_xyzt = true
    block = ${weld_blocks_and_change_to}
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[MeshModifiers]
  [cut_esm]
    type = CoupledVarThresholdElementSubdomainModifier
    coupled_var = 'u'
    criterion_type = 'BELOW'
    threshold = 0
    subdomain_id = 37
    execute_on = 'TIMESTEP_BEGIN'
    block = ${weld_blocks_and_change_to}

    # --- new for setting IC --- #
    # unsolved_blocks = ${weld_blocks}
    # ic_strategy = "IC_POLYNOMIAL"

    # nodal_patch_recovery_uo = 'extrapolation_patch_T extrapolation_patch_disp_x extrapolation_patch_disp_y extrapolation_patch_disp_z'
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
  end_time = 33
[]

[Outputs]
  exodus = true
[]
