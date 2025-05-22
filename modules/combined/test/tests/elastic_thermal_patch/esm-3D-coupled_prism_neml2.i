neml2_input = viscoplasticity_perfect

[Problem]
  default_block = '0'
[]

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
[]

[Mesh]
  [gmg]
    type = FileMeshGenerator
    #file = "cube_cylinder.msh"
    file = "cube_cylinder_prism.msh"
  []

  [subdomain1]
    type = SubdomainInterceptedGenerator
    input = 'gmg'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    lambda = 0.5
    outer_boundary = false
    function = '(y-0.3)^2+(z-1.5)^2-0.15^2'
  []

  # add_subdomain_ids = 2 # activated elements
  use_displaced_mesh = false
[]

[Variables]
  [T]
    order = FIRST
  []
[]


[SpatioTemporalPaths]
  [path]
    type = CSVPiecewiseLinearSpatioTemporalPath
    file = 'InsideTriPt_filled_horizontal_lines.csv'
    verbose = true
  []
[]

[UserObjects]
  [extrapolation_patch_T]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = FIRST
    use_specific_elements = true
    var = 'T'
    execute_on = 'TIMESTEP_BEGIN'
  []
  [extrapolation_patch_disp_x]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = FIRST
    use_specific_elements = true
    var = 'disp_x'
    execute_on = 'TIMESTEP_BEGIN'
  []
  [extrapolation_patch_disp_y]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = FIRST
    use_specific_elements = true
    var = 'disp_y'
    execute_on = 'TIMESTEP_BEGIN'
  []
  [extrapolation_patch_disp_z]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = FIRST
    use_specific_elements = true
    var = 'disp_z'
    execute_on = 'TIMESTEP_BEGIN'
  []
[]

[MeshModifiers]
  [esm]
    type = SpatioTemporalPathElementSubdomainModifier
    path = 'path'
    radius = 0.045
    target_subdomain = '0'
    block = '0 1'
    execute_on = 'TIMESTEP_BEGIN'

    # --- new for setting IC --- #
    inactive_subdomain_ID = 1
    ic_strategy = "IC_POLYNOMIAL"

    nodal_patch_recovery_uo = 'extrapolation_patch_T extrapolation_patch_disp_x extrapolation_patch_disp_y extrapolation_patch_disp_z'
  []
[]


[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = SMALL
        new_system = true
        add_variables = true
        formulation = TOTAL
        volumetric_locking_correction = true
        automatic_eigenstrain_names = true
        generate_output = 'vonmises_cauchy_stress'
      []
    []
  []
[]


[NEML2]
  input = 'models/${neml2_input}.i'
  [all]
    model = 'model'
    verbose = true
    device = 'cpu'

    moose_input_types = 'MATERIAL     MATERIAL     POSTPROCESSOR POSTPROCESSOR MATERIAL'
    moose_inputs = '     neml2_strain neml2_strain time          time          neml2_stress'
    neml2_inputs = '     forces/E     old_forces/E forces/t      old_forces/t  old_state/S'

    moose_output_types = 'MATERIAL'
    moose_outputs = '     neml2_stress'
    neml2_outputs = '     state/S'

    moose_derivative_types = 'MATERIAL'
    moose_derivatives = 'neml2_jacobian'
    neml2_derivatives = 'state/S forces/E'
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
  [convert_strain]
    type = RankTwoTensorToSymmetricRankTwoTensor
    from = 'mechanical_strain'
    to = 'neml2_strain'
  []
  [stress]
    type = ComputeLagrangianObjectiveCustomSymmetricStress
    custom_small_stress = 'neml2_stress'
    custom_small_jacobian = 'neml2_jacobian'
  []
  [expansion1]
    type = ComputeThermalExpansionEigenstrain
    temperature = T
    thermal_expansion_coeff = 5e-7
    stress_free_temperature = 0
    eigenstrain_name = thermal_expansion
  []
  [volumetric_heat] # need to be exactly this name!
    type = ADMovingEllipsoidalHeatSource
    path = 'path'
    power = 1
    efficiency = 1
    scale = 1
    a = 0.2
    b = 0.1
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
  [hsource]
    type = ADMatHeatSource
    material_property = 'volumetric_heat'
    variable = T
  []
[]

[BCs]
  [bottom]
    type = DirichletBC
    variable = T
    boundary = bottom
    value = 0
  []

  [anchor_x]
    type = DirichletBC
    variable = disp_x
    # boundary = 'left right top bottom'
    boundary = 'bottom'
    value = 0.0
  []
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    # boundary = 'left right top bottom'
    boundary = 'bottom'
    value = 0.0
  []
  [anchor_z]
    type = DirichletBC
    variable = disp_z
    # boundary = 'left right top bottom'
    boundary = 'bottom'
    value = 0.0
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu mumps'
  nl_max_its = 100
  nl_rel_tol = 1e-6
  nl_abs_tol = 1e-7
  dt = 1
  end_time = 1000
  automatic_scaling = true
  residual_and_jacobian_together = true
  line_search = none
  abort_on_solve_fail = true
[]

[Postprocessors]
  [time]
   type = TimePostprocessor
   execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[Outputs]
  exodus = true
[]
