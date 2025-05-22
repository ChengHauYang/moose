
neml2_input = viscoplasticity_isoharden

[Problem]
  default_block = '0'
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Mesh]
  [gmg]
    type = FileMeshGenerator
    file = "weld_boundary_fitted.msh"
  []

  [subdomain1]
    type = SubdomainInterceptedGenerator
    input = 'gmg'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    lambda = 0.5
    outer_boundary = false
    function = 'sqrt((x-0.5)^2 + (y-0.5)^2) - 0.38'
  []
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
    file = 'concentric_circles_reverse.csv'
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
[]


[MeshModifiers]
  [esm]
    type = SpatioTemporalPathElementSubdomainModifier
    path = 'path'
    radius = 0.03
    target_subdomain = '0'
    block = '0 1'
    execute_on = 'TIMESTEP_BEGIN'

    # --- new for setting IC --- #
    inactive_subdomain_ID = 1
    ic_strategy = "IC_POLYNOMIAL"

    nodal_patch_recovery_uo = 'extrapolation_patch_T extrapolation_patch_disp_x extrapolation_patch_disp_y'
  []
[]

# [HeatPathAction]
#   type = SpatioTemporalHeatAction
#   path_file = 'concentric_circles_reverse.csv'
#   # for path
#   verbose = true
#   # for esm
#   block = '0 1'
#   target_subdomain = 0
#   radius = 0.03
#   execute_on_esm = 'TIMESTEP_BEGIN'
#   inactive_subdomain_ID = 1
#   ic_strategy = "IC_EXTRAPOLATE_FIRST_LAYER"
#   # for heat source
#   power = 1
#   a = 0.035
#   b = 0.01
#   efficiency = 1
#   scale = 1
#   # for kernel
#   heat_variable = T
# []

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

    moose_input_types = 'MATERIAL     MATERIAL     POSTPROCESSOR POSTPROCESSOR MATERIAL     MATERIAL'
    moose_inputs = '     neml2_strain neml2_strain time          time          neml2_stress equivalent_plastic_strain'
    neml2_inputs = '     forces/E     old_forces/E forces/t      old_forces/t  old_state/S  old_state/internal/ep'

    moose_output_types = 'MATERIAL     MATERIAL'
    moose_outputs = '     neml2_stress equivalent_plastic_strain'
    neml2_outputs = '     state/S      state/internal/ep'

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
    power = 50
    efficiency = 1
    scale = 1
    a = 0.02
    b = 0.005
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
  [left]
    type = DirichletBC
    variable = T
    boundary = left
    value = 0
  []

  [right]
    type = DirichletBC
    variable = T
    boundary = right
    value = 0
  []

  [top]
    type = DirichletBC
    variable = T
    boundary = top
    value = 0
  []

  [bottom]
    type = DirichletBC
    variable = T
    boundary = bottom
    value = 0
  []

  [anchor_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'left top bottom'
    #boundary = 'left'
    value = 0.0
  []
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'left top bottom'
    #boundary =  'bottom'
    value = 0.0
  []
  [displacement_x_right]
    #Anchors the left side against deformation in the x-direction
    type = FunctionDirichletBC
    variable = disp_x
    boundary = 'right'
    function = displacement_with_time
    preset = false
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

[Functions]
  [displacement_with_time]
    type = ParsedFunction
    expression = '1e-8*t'
  []
[]


[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu mumps'
  nl_max_its = 100
  nl_rel_tol = 1e-5
  nl_abs_tol = 1e-6
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
  interval = 1
[]
