all_blocks = 'default pass-1 pass-2 pass-3 pass-4 pass-5 pass-6 pass-7 pass-8 pass-9 pass-10 pass-11 pass-12 pass-13 pass-14 pass-15 pass-16 pass-17 pass-18 pass-19 pass-20 pass-21 pass-22 pass-23 pass-24 new'
weld_blocks = ' pass-1 pass-2 pass-3 pass-4 pass-5 pass-6 pass-7 pass-8 pass-9 pass-10 pass-11 pass-12 pass-13 pass-14 pass-15 pass-16 pass-17 pass-18 pass-19 pass-20 pass-21 pass-22 pass-23 pass-24'
#npr_order= CONSTANT
npr_order = FIRST

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Problem]
  block = 'default new'
  boundary_restricted_node_integrity_check = false
  boundary_restricted_elem_integrity_check = false
[]

[Mesh]
  [gmg]
    type = FileMeshGenerator
    file = "geometry_xy_swapped_quad_symmetric.msh"
  []

  [shift_mesh]
    type = TransformGenerator
    transform = TRANSLATE
    vector_value = '0.055 0.0 0.0' # translation in x, y, z directions
    input = gmg
  []

  coord_type = 'RZ'

  add_subdomain_ids = '26'
  add_subdomain_names = 'new'

  rz_coord_axis = y
  use_displaced_mesh = false
[]

[Variables]
  [cond]
    order = FIRST
    # initial_condition = 293.15
    block = 'default new'
  []
[]

[SpatioTemporalPaths]
  [path]
    type = CSVPiecewiseLinearSpatioTemporalPath
    file = 'weld_only_Xiong_symmetric.csv'
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
    patch_polynomial_order = ${npr_order}
    use_specific_elements = true
    block = 'default new'
    var = 'cond'
    verbose = true
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []

  [extrapolation_patch_disp_x]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = ${npr_order}
    use_specific_elements = true
    block = 'default new'
    var = 'disp_x'
    verbose = true
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
  [extrapolation_patch_disp_y]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = ${npr_order}
    use_specific_elements = true
    block = 'default new'
    var = 'disp_y'
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
  [extrapolation_patch_disp_vm]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = ${npr_order}
    use_specific_elements = true
    block = 'default new'
    var = 'vonmises_stress'
    execute_on = 'TIMESTEP_END'
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
    blocks_to = 'new new new new new new new new new new new new new new new new new new new new new new new new'
    execute_on = 'TIMESTEP_END '

    block = ${all_blocks}

    # --- new for setting IC --- #

    old_subdomain_reinitialized = false
    reinitialize_subdomain_ids = 'default new'
    # ic_strategy = "IC_DEFAULT IC_FUNC IC_POLYNOMIAL IC_POLYNOMIAL"
    # ic_variables = "cond  gaussian_weight disp_x disp_y"
    # function_for_ic = "gaussian_weight_func"
    # nodal_patch_recovery_uo = 'extrapolation_patch_disp_x extrapolation_patch_disp_y'

    ic_strategy = "IC_POLYNOMIAL  IC_FUNC IC_POLYNOMIAL IC_POLYNOMIAL"
    ic_variables = "cond gaussian_weight disp_x disp_y"
    function_for_ic = "gaussian_weight_func"
    nodal_patch_recovery_uo = 'extrapolation_patch_T extrapolation_patch_disp_x extrapolation_patch_disp_y'
  []
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        add_variables = true
        strain = FINITE
        eigenstrain_names = eigenstrain
        generate_output = 'vonmises_stress stress_xx stress_yy stress_zz stress_xy stress_yz stress_zx'
        use_automatic_differentiation = true
        volumetric_locking_correction = true
        block = 'default new'
      []
    []
  []
[]

[Materials]
  [density]
    type = ADGenericConstantMaterial
    prop_names = 'density '
    prop_values = '7960.0               ' # kg/m^3
    block = 'default new'
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
    block = 'default new'
  []

  [specific_heat]
    type = ADPiecewiseLinearInterpolationMaterial
    property = 'specific_heat'
    variable = cond
    x = '293.15 373.15 473.15 573.15 673.15 773.15 873.15 973.15 1073.15 1173.15 1273.15 1373.15 1473.15 1573.15 1673.15'
    y = '490 508 532 555 580 603 627 650 650 650 650 650 650 650 650' # J/(kg*k) -> We use J here because the convective heat transfer coefficient
    block = 'default new'
  []

  [thermalconductivity]
    type = ADPiecewiseLinearInterpolationMaterial
    property = 'thermal_conductivity'
    variable = cond
    x = '293.15 373.15 473.15 573.15 673.15 773.15 873.15 973.15 1073.15 1173.15 1273.15 1373.15 1473.15 1573.15 1673.15'
    y = '12.69 13.93 15.48 17.03 18.58 20.13 21.68 23.23 24.78 26.33 27.88 29.43 30.98 32.53 34.08'
    block = 'default new'
  []

  [parent_youngs_modulus]
    type = ADPiecewiseLinearInterpolationMaterial
    x = '293.15 373.15 473.15 573.15 673.15 773.15 873.15 973.15 1073.15 1173.15 1273.15 1373.15 1473.15 1573.15 1673.15'
    y = '204500000000.0 196700000000.0 188100000000.0 180200000000.0 172500000000.0 164600000000.0 156000000000.0 146100000000.0 134600000000.0 120800000000.0 104400000000.0 84800000000.0 61500000000.0 34100000000.0 2000000000.0' # Pa
    property = parent_youngs_modulus
    variable = cond
    block = 'default new'
  []

  [weld_youngs_modulus]
    type = ADPiecewiseLinearInterpolationMaterial
    x = '293.15 373.15 473.15 573.15 673.15 773.15 873.15 973.15 1073.15 1173.15 1273.15 1373.15 1473.15 1573.15 1673.15'
    y = '157800000000.0 151500000000.0 144400000000.0 137800000000.0 131400000000.0 124800000000.0 117700000000.0 109800000000.0 100600000000.0 89900000000.0 77400000000.0 62600000000.0 45300000000.0 25000000000.0 1600000000.0'
    property = weld_youngs_modulus
    variable = cond
    block = 'default new'
  []

  [elasticity_tensor_parent]
    type = ADComputeVariableIsotropicElasticityTensor
    youngs_modulus = parent_youngs_modulus
    poissons_ratio = 0.294
    block = 'default'
  []

  [elasticity_tensor_weld]
    type = ADComputeVariableIsotropicElasticityTensor
    youngs_modulus = weld_youngs_modulus
    poissons_ratio = 0.294
    block = 'new'
  []

  [CTE_base]
    type = ADComputeInstantaneousThermalExpansionFunctionEigenstrain
    eigenstrain_name = eigenstrain
    stress_free_temperature = 293.15 # TODO: double check
    thermal_expansion_function = thermal_expansion_fn
    temperature = cond
    outputs = exodus
    block = 'default'
  []

  [CTE_new]
    type = ADComputeInstantaneousThermalExpansionFunctionEigenstrain
    eigenstrain_name = eigenstrain
    stress_free_temperature = 923.15 # melting temperature
    thermal_expansion_function = thermal_expansion_fn
    temperature = cond
    outputs = exodus
    block = 'new'
  []

  [stress]
    type = ADComputeLinearElasticStress
    block = 'default new'
  []
[]

[Kernels]
  [time_kernel]
    type = ADHeatConductionTimeDerivative
    variable = cond
    density_name = density
    specific_heat = specific_heat
    block = 'default new'
  []
  [heat]
    type = ADHeatConduction
    variable = cond
    thermal_conductivity = "thermal_conductivity"
    block = 'default new'
  []
  [hsource]
    type = ADMatHeatSource
    material_property = 'volumetric_heat'
    variable = cond
    block = 'default new'
  []
[]

[Functions]
  [thermal_expansion_fn]
    type = PiecewiseLinear
    x = '293.15 373.15 473.15 573.15 673.15 773.15 873.15 973.15 1073.15 1173.15 1273.15 1373.15 1473.15 1573.15 1673.15'
    y = '15.44e-6 16.01e-6 16.67e-6 17.29e-6 17.87e-6 18.41e-6 18.91e-6 19.37e-6 19.78e-6 20.16e-6 20.49e-6 20.78e-6 21.03e-6 21.24e-6 21.41e-6'
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
    # x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    # y = '0.0045  0.0045  0.0050  0.0050  0.0050  0.0050 0.0055  0.0055  0.0055  0.0055  0.0055  0.0055  0.0055  0.0055 0.0065  0.0065  0.0065  0.0065  0.0065  0.0065  0.0065  0.0065  0.0065  0.0065' # mm -> m
    x = '0 500'
    y = '0 0'
  []

  [axis_centroid]
    type = PiecewiseLinear
    x = '1  2  3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19   20   21   22   23  24'
    y = '0.1 0.1 0.102933969 0.09706603 0.103418847 0.096581152 0.103851272 0.1 0.104276219 0.09572378 0.10468856 0.095311439 0.105071512 0.094928488 0.105422274 0.094577725 0.105774865 0.094225134 0.10610476 0.093895239 0.106415819 0.09358418 0.107297768 0.092702231'
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

  [anchor_axis]
    type = DirichletBC
    variable = disp_y
    boundary = 'left'
    value = 0.0
  []
  [disp_zero_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'fix_pt'
    value = 0.0
  []
  [disp_zero_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'fix_pt'
    value = 0.0
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
  dt = 0.2
  end_time = 26
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
