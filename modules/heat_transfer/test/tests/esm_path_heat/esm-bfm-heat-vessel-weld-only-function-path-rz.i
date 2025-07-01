all_blocks = 'default pass-1 pass-2 pass-3 pass-4 pass-5 pass-6 pass-7 pass-8 pass-9 pass-10 pass-11 pass-12 pass-13 pass-14 pass-15 pass-16 pass-17 pass-18 pass-19 pass-20 pass-21 pass-22 pass-23 pass-24'
weld_blocks = ' pass-1 pass-2 pass-3 pass-4 pass-5 pass-6 pass-7 pass-8 pass-9 pass-10 pass-11 pass-12 pass-13 pass-14 pass-15 pass-16 pass-17 pass-18 pass-19 pass-20 pass-21 pass-22 pass-23 pass-24'

[GlobalParams]
  block = 'default '
[]

[Problem]
  # block = 'default '
  boundary_restricted_node_integrity_check = false
  boundary_restricted_elem_integrity_check = false
[]

[Mesh]
  [gmg]
    type = FileMeshGenerator
    file = "geometry_xy_swapped.msh"
  []

  [shift_mesh]
    type = TransformGenerator
    transform = TRANSLATE
    vector_value = '0.055 0.0 0.0' # translation in x, y, z directions
    input = gmg
  []

  coord_type = 'RZ'

  rz_coord_axis = y
  use_displaced_mesh = false
[]

[Variables]
  [cond]
    order = FIRST
    # initial_condition = 293.15
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
    type = FunctorAux
    variable = gaussian_weight
    functor = 'gaussian_weight_func'
    execute_on = 'TIMESTEP_BEGIN'
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

[ICs]
  [temperature_IC]
    type = FunctionIC
    variable = cond
    function = melting_temperature
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
    ic_strategy = "IC_DEFAULT IC_FUNC"
    ic_variables = "cond  gaussian_weight"
    function_for_ic = "gaussian_weight_func"

    # nodal_patch_recovery_uo = 'extrapolation_patch_T'
  []
[]

[Materials]
  [density]
    type = ADGenericConstantMaterial
    prop_names = 'density '
    prop_values = '7960.0               ' # kg/m^3
  []

  [volumetric_heat]
    type = FunctionPathEllipsoidHeatSource
    heat_source_rx = "source_radius"
    heat_source_ry = "source_radius"
    heat_source_rz = "source_radius"
    function_power = "heat_source_p"
    function_efficiency = "heat_source_eff"
    function_torch_speed = "heat_source_v"
    function_weave_amp_y = "heat_source_weave_y"
    wavelength = 0.01 # m
    factor = 1
    # va_postprocess = Va_integral
    #path = 'path'
    function_x = "radial_centroid"
    function_y = "axis_centroid"
    function_z = "z_centroid"
  []

  [specific_heat]
    type = ADPiecewiseLinearInterpolationMaterial
    property = 'specific_heat'
    variable = cond
    x = '293.15 373.15 473.15 573.15 673.15 773.15 873.15 973.15 1073.15 1173.15 1273.15 1373.15 1473.15 1573.15 1673.15'
    y = '490 508 532 555 580 603 627 650 650 650 650 650 650 650 650' # J/(kg*k) -> We use J here because the convective heat transfer coefficient
  []

  [thermalconductivity]
    type = ADPiecewiseLinearInterpolationMaterial
    property = 'thermal_conductivity'
    variable = cond
    x = '293.15 373.15 473.15 573.15 673.15 773.15 873.15 973.15 1073.15 1173.15 1273.15 1373.15 1473.15 1573.15 1673.15'
    y = '12.69 13.93 15.48 17.03 18.58 20.13 21.68 23.23 24.78 26.33 27.88 29.43 30.98 32.53 34.08'
  []
[]

[Kernels]
  [time]
    type = ADHeatConductionTimeDerivative
    variable = cond
    density_name = density
    specific_heat = specific_heat
  []
  [heat]
    type = ADHeatConduction
    variable = cond
    thermal_conductivity = "thermal_conductivity"
  []
  [hsource]
    type = ADMatHeatSource
    material_property = 'volumetric_heat'
    variable = cond
  []
[]

[Functions]
  [thermal_expansion_fn]
    type = PiecewiseLinear
    x = '293.15 373.15 473.15 573.15 673.15 773.15 873.15 973.15 1073.15 1173.15 1273.15 1373.15 1473.15 1573.15 1673.15'
    y = '15.44 16.01 16.67 17.29 17.87 18.41 18.91 19.37 19.78 20.16 20.49 20.78 21.03 21.24 21.41'
  []

  [youngs_modulus_fn]
    type = PiecewiseLinear
    x = '293.15 373.15 473.15 573.15 673.15 773.15 873.15 973.15 1073.15 1173.15 1273.15 1373.15 1473.15 1573.15 1673.15'
    y = '204.5 196.7 188.1 180.2 172.5 164.6 156.0 146.1 134.6 120.8 104.4 84.8 61.5 34.1 2.0' # GPa
  []

  [source_radius]
    type = PiecewiseLinear
    x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    y = '0.0024 0.0018 0.0032 0.0032 0.0032 0.0032 0.004 0.004 0.004 0.004 0.004 0.004 0.004 0.004 0.005 0.005 0.005 0.005 0.005 0.005 0.005 0.005 0.005 0.005' # mm -> m
  []

  [heat_source_p] # power = J/s (this is from V*A)
    type = PiecewiseLinear
    x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    y = '73.1 127.5 271.04 278.4 244.26 257.04 376.83 395.28 393.6 380.01 397.67 392.84 388.01 384.79 580 582.12 577.1 576 578.16 567.15 589.04 557.2 420 436.17'
  []

  [heat_source_eff]
    type = PiecewiseLinear
    x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    y = '0.85 0.95 0.75 0.8 0.82 0.85 0.78 0.8 0.75 0.8 0.75 0.78 0.75 0.8 0.79 0.83 0.78 0.85 0.76 0.82 0.74 0.79 0.64 0.74'
  []

  [heat_source_v]
    type = PiecewiseLinear
    x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    y = '0.00085  0.00125  0.00224  0.00232  0.00207  0.00216 0.00237  0.00244  0.00246  0.00239  0.00247  0.00244 0.00241  0.00239  0.00290  0.00294  0.00290  0.00288 0.00292  0.00285  0.00296  0.00280  0.00210  0.00217' # mm/s-> m/s
  []

  [heat_source_weave_y]
    type = PiecewiseLinear
    x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    y = '0.0045  0.0045  0.0050  0.0050  0.0050  0.0050 0.0055  0.0055  0.0055  0.0055  0.0055  0.0055  0.0055  0.0055 0.0065  0.0065  0.0065  0.0065  0.0065  0.0065  0.0065  0.0065  0.0065  0.0065' # mm -> m
    # x = '0 500'
    # y = '0 0'
  []

  [axis_centroid]
    type = PiecewiseLinear
    x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    y = '0.103486395 0.103486395 0.106420364 0.100552425 0.106905242 0.100067547 0.107337667 0.099635122 0.107762614 0.099210175 0.108174955 0.098797834 0.108557907 0.098414883 0.108908669 0.09806412 0.10926126 0.097711529 0.109591155 0.097381634 0.109902214 0.097070575 0.110784163 0.096188626'
  []

  [radial_centroid_ori]
    type = PiecewiseLinear
    x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    y = '0.000266216 0.003315408 0.006161999 0.006161999 0.009976729 0.009976729 0.01337878 0.01337878 0.01672201 0.01672201 0.019966053 0.019966053 0.022978885 0.022978885 0.025738468 0.025738468 0.028512442 0.028512442 0.031107857 0.031107857 0.033555082 0.033555082 0.036103556 0.036103556'
  []

  [radial_centroid]
    type = ParsedFunction
    expression = 'radial_original + 0.055'
    symbol_names = 'radial_original'
    symbol_values = 'radial_centroid_ori'
  []

  [z_centroid]
    type = ConstantFunction
    value = 0.0
  []

  [radial_shift]
    type = ParsedFunction
    expression = 'amp*sin(2*3.14159*t*speed/0.01)'
    symbol_names = 'amp speed'
    symbol_values = 'heat_source_weave_y heat_source_v'
  []

  [gaussian_weight_func]
    type = ParsedFunction
    expression = 'exp(-( pow(x-radial_0-radial_shift_weave,2)/pow(r,2) + pow(y-axis_0,2)/pow(r,2)))' # for 2*pi, the RZ coord will take care of this
    symbol_names = 'r axis_0 radial_0 radial_shift_weave'
    symbol_values = 'source_radius axis_centroid radial_centroid radial_shift'
  []

  [melting_temperature]
    type = PiecewiseLinear
    x = '0 1  500'
    y = '293.15 923.15 923.15'
  []
[]

[BCs]
  [convective_surface] # Convective Start
    type = ConvectiveFluxBC # Convective flux, e.g. q'' = h*(Tw - Tf)
    variable = cond
    rate = 0.0005 # h = convective heat transfer coefficient (w/m^2-K)
    #         #  the above h is ~ infinity for present purposes
    rate_final = 0.0005
    initial = 293.15 # initial ambient temperature (K)
    final = 293.15 # final ambient (lab or oven) temperature (K)     # temperature to ramp from initial to final
    duration = 1000 # length of time in seconds that it takes the ambient
    boundary = 'bottom right top weld weld_interior' # BC applied on every interfaces
    #boundary = 'bottom right top'
    #according to Bipul's one, we should not consider left side
    neglect_side_btw_two_default_blocks = true
  [] # Convective End
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_max_its = 10
  nl_rel_tol = 1e-6
  nl_abs_tol = 1e-10
  # dt = 0.5
  dt = 1
  # dtmin = 1.0
  end_time = 24
[]

[Outputs]
  exodus = true
[]

[Postprocessors]
  [Va_integral]
    type = ElementIntegralVariablePostprocessor
    variable = gaussian_weight
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]
