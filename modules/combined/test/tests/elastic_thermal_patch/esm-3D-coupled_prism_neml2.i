neml2_input = voce_isotropic_hardening

[GlobalParams]
  block = '0'
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
    radius = 0.05
    target_subdomain = '0'
    block = '0 1'
    execute_on = 'TIMESTEP_BEGIN'

    # --- new for setting IC --- #

    old_subdomain_reinitialized = false
    reinitialize_subdomain_ids = '0'
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

    moose_input_types = 'MATERIAL     POSTPROCESSOR POSTPROCESSOR MATERIAL                  MATERIAL'
    moose_inputs = '     neml2_strain time          time          equivalent_plastic_strain plastic_strain'
    neml2_inputs = '     forces/E     forces/t      old_forces/t  old_state/internal/ep     old_state/internal/Ep'

    moose_output_types = 'MATERIAL MATERIAL MATERIAL'
    moose_outputs = '     neml2_stress equivalent_plastic_strain plastic_strain'
    neml2_outputs = '     state/S state/internal/ep state/internal/Ep'

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
    thermal_expansion_coeff = 1e-6
    stress_free_temperature = 0
    eigenstrain_name = thermal_expansion
  []
  [volumetric_heat] # need to be exactly this name!
    type = ADMovingEllipsoidalHeatSource
    path = 'path'
    power = 2000
    efficiency = 1
    scale = 1
    a = 0.05
    b = 0.02
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

[AuxVariables]
  [ep]
    order = CONSTANT
    family = MONOMIAL
    [AuxKernel]
      type = MaterialRealAux
      property = equivalent_plastic_strain
      execute_on = 'INITIAL TIMESTEP_END'
    []
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  #petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  #petsc_options_value = 'lu mumps'
  #petsc_options_iname = '-ksp_type -pc_type -ksp_rtol -pc_gamg_threshold -mg_levels_ksp_type'
  #petsc_options_value = 'gmres gamg 1e-6 0.02 richardson'
  petsc_options_iname = "-ksp_type -pc_type -pc_gamg_asm_use_agg -mg_levels_ksp_type "
                        "-mg_levels_pc_type -mg_levels_sub_pc_type -mg_levels_ksp_max_it -ksp_rtol "
                        "-ksp_max_it"
  petsc_options_value = "fgmres gamg true gmres lu lu 5 1e-6 100"
  nl_max_its = 100
  nl_rel_tol = 1e-6
  nl_abs_tol = 1e-7
  dt = 1
  end_time = 1000
  automatic_scaling = true
  residual_and_jacobian_together = true
  line_search = 'bt'
  abort_on_solve_fail = true
[]

[Postprocessors]
  [time]
    type = TimePostprocessor
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
  [max_T]
    type = NodalExtremeValue
    variable = T
    value_type = max
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [max_ep]
    type = ElementExtremeMaterialProperty
    mat_prop = equivalent_plastic_strain
    value_type = MAX
    execute_on = 'INITIAL TIMESTEP_END'
  []
[]

[Outputs]
  interval = 1
  exodus = true
[]
