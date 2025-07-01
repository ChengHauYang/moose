all_blocks = 'default pass-1 pass-2 pass-3 pass-4 pass-5 pass-6 pass-7 pass-8 pass-9 pass-10 pass-11 pass-12 pass-13 pass-14 pass-15 pass-16 pass-17 pass-18 pass-19 pass-20 pass-21 pass-22 pass-23 pass-24'
weld_blocks = ' pass-1 pass-2 pass-3 pass-4 pass-5 pass-6 pass-7 pass-8 pass-9 pass-10 pass-11 pass-12 pass-13 pass-14 pass-15 pass-16 pass-17 pass-18 pass-19 pass-20 pass-21 pass-22 pass-23 pass-24'
rl = 0.003 # e.g., 3 mm
rv = 0.004 # e.g., 4 mm
ra = 0.006 # e.g., 6 mm

[GlobalParams]
  block = 'default '
[]

[Problem]
  boundary_restricted_node_integrity_check = false
  boundary_restricted_elem_integrity_check = false
[]

[Mesh]
  [gmg]
    type = FileMeshGenerator
    file = "partition_symmetric_shape.msh"
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
    file = 'weld_only_Xiong.csv'
    verbose = true
  []
[]

[AuxVariables]
  [gaussian_weight]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[AuxKernels]
  [assign_gaussian_weight]
    type = ParsedAux
    variable = gaussian_weight
    expression = 'exp(-( pow(x,2) / pow(${rl},2) + pow(y,2) / pow(${rv},2) + pow(z,2) / pow(${ra},2)))'
    use_xyzt = true
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[UserObjects]
  [extrapolation_patch_T]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = FIRST
    use_specific_elements = true
    var = 'cond'
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[MeshModifiers]
  [cut_esm]
    type = TimedSubdomainModifier
    times = '1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24'
    blocks_from = 'pass-1 pass-2 pass-3 pass-4 pass-5 pass-6 pass-7 pass-8 pass-9 pass-10 pass-11 pass-12 pass-13 pass-14 pass-15 pass-16 pass-17 pass-18 pass-19 pass-20 pass-21 pass-22 pass-23 pass-24' # this is block "1" but block ID = "2"
    blocks_to = 'default default default default default default default default default default default default default default default default default default default default default default default default'
    execute_on = 'INITIAL TIMESTEP_BEGIN'

    block = ${all_blocks}

    # --- new for setting IC --- #

    old_subdomain_reinitialized = false
    reinitialize_subdomain_ids = 'default'
    ic_strategy = "IC_POLYNOMIAL"

    nodal_patch_recovery_uo = 'extrapolation_patch_T'
  []
[]

[Materials]
  [density]
    type = ADGenericConstantMaterial
    prop_names = 'density  thermal_conductivity'
    prop_values = '10431.0 3.0                 '
  []

  [volumetric_heat]
    type = FunctionPathEllipsoidHeatSource
    rx = 0.001
    ry = 0.001
    rz = 0.001
    power = 1
    efficiency = 1
    factor = 1
    path = 'path'
  []
[]

[Kernels]
  [time]
    type = ADTimeDerivative
    variable = cond
  []
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

[Functions]
  [thermal_k_fn]
    type = PiecewiseLinear
    x = '20 100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400'
    y = '12.69 13.93 15.48 17.03 18.58 20.13 21.68 23.23 24.78 26.33 27.88 29.43 30.98 32.53 34.08'
  []
  [specific_heat_fn]
    type = PiecewiseLinear
    x = '20 100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400'
    y = '490 508 532 555 580 603 627 650 650 650 650 650 650 650 650'
  []

  [thermal_expansion_fn]
    type = PiecewiseLinear
    x = '20 100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400'
    y = '15.44 16.01 16.67 17.29 17.87 18.41 18.91 19.37 19.78 20.16 20.49 20.78 21.03 21.24 21.41'
  []

  [youngs_modulus_fn]
    type = PiecewiseLinear
    x = '20 100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400'
    y = '204.5 196.7 188.1 180.2 172.5 164.6 156.0 146.1 134.6 120.8 104.4 84.8 61.5 34.1 2.0' # GPa
  []

  [source_radius]
    type = PiecewiseLinear
    x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    y = '2.4 1.8 3.2 3.2 3.2 3.2 4   4   4   4   4   4   4   4   5   5   5   5   5   5   5   5   5   5'
  []
[]

[BCs]
  [convective_surface] # Convective Start
    type = ConvectiveFluxBC # Convective flux, e.g. q'' = h*(Tw - Tf)
    boundary = 'left bottom right top weld weld_interior' # BC applied on every interfaces
    variable = cond
    rate = 0.0005 # h = convective heat transfer coefficient (w/m^2-K)[176000 "]
    #         #  the above h is ~ infinity for present purposes
    rate_final = 0.0005
    initial = 293.15 # initial ambient temperature (K)
    final = 293.15 # final ambient (lab or oven) temperature (K)
    duration = 1000 # length of time in seconds that it takes the ambient
    neglect_side_btw_two_default_blocks = true
    #     temperature to ramp from initial to final
  [] # Convective End
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
  end_time = 24
[]

[Outputs]
  exodus = true
[]

[Postprocessors]
  # [Va_integral]
  #   type = ElementIntegralVariablePostprocessor
  #   variable = gaussian_weight
  # []
[]
