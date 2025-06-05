
[GlobalParams]
  block = '0 2'
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    # xmax = 2
    # ymax = 2
    nx = 200
    ny = 200
    subdomain_ids = '1'
  []

  [subdomain1]
    type = SubdomainInterceptedGenerator
    input = 'gmg'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    lambda = 1
    outer_boundary = false
    function = 'sqrt((x-0.5)^2 + (y-0.5)^2) - 0.38'
  []

  use_displaced_mesh = false

  add_subdomain_ids = 2
[]

[Variables]
  [T]
    order = FIRST
    initial_condition = 8.5
  []
[]

[SpatioTemporalPaths]
  [path]
    type = CSVPiecewiseLinearSpatioTemporalPath
    file = 'concentric_circles_reverse.csv'
    verbose = true
  []
[]

[UserObjects]
  [extrapolation_patch_T]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = FIRST
    use_specific_elements = true
    var = 'T disp_x disp_y'
    execute_on = 'TIMESTEP_END'
  []
  [extrapolation_patch_disp_x]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = FIRST
    use_specific_elements = true
    var = 'disp_x'
    execute_on = 'TIMESTEP_END'
  []
  [extrapolation_patch_disp_y]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = FIRST
    use_specific_elements = true
    var = 'disp_y'
    execute_on = 'TIMESTEP_END'
  []
[]

[MeshModifiers]
  [esm]
    type = SpatioTemporalPathElementSubdomainModifier
    path = 'path'
    radius = 0.15
    target_subdomain = '2'
    block = '1 2'
    execute_on = 'TIMESTEP_END'

    # --- new for setting IC --- #
    unsolved_blocks = '1'
    ic_strategy = "IC_POLYNOMIAL"

    nodal_patch_recovery_uo = 'extrapolation_patch_disp_x extrapolation_patch_disp_y'
  []
[]

[Physics]

  [SolidMechanics]

    [QuasiStatic]
      [all]
        add_variables = true
        strain = FINITE
        automatic_eigenstrain_names = true
        generate_output = 'vonmises_stress'
      []
    []
  []
[]

[Materials]
  [thermal]
    type = HeatConductionMaterial
    thermal_conductivity = 45.0
    specific_heat = 0.5
  []
  [density]
    type = GenericConstantMaterial
    prop_names = 'density'
    prop_values = 8000.0
  []
  [elasticity]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1e9
    poissons_ratio = 0.0
  []
  [expansion1]
    type = ComputeThermalExpansionEigenstrain
    temperature = T
    thermal_expansion_coeff = 1e-7
    stress_free_temperature = 0
    eigenstrain_name = thermal_expansion
  []
  [stress]
    type = ComputeFiniteStrainElasticStress
  []
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
[]

[Kernels]
  [heat_conduction]
    type = HeatConduction
    variable = T
  []
  [time_derivative]
    type = HeatConductionTimeDerivative
    variable = T
  []
  #    type = ADMatHeatSource
  #    material_property = 'volumetric_heat'
  #    variable = T
  #  []
[]

[BCs]
  [left]
    type = DirichletBC
    variable = T
    boundary = left
    value = 10
  []

  [right]
    type = DirichletBC
    variable = T
    boundary = right
    value = 10
  []

  [top]
    type = DirichletBC
    variable = T
    boundary = top
    value = 8
  []

  [bottom]
    type = DirichletBC
    variable = T
    boundary = bottom
    value = 8
  []

  [anchor_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'left right top bottom'
    #boundary = 'left'
    value = 0.0
  []
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'left right top bottom'
    #boundary =  'bottom'
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
  dt = 5
  end_time = 1000
[]

[Outputs]
  exodus = true
[]
