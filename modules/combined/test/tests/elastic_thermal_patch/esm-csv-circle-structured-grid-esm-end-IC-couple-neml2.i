
neml2_input = rate_independent_plasticity_isoharden

[GlobalParams]
  block = '0'
  displacements = 'disp_x disp_y'
[]

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
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
    target_subdomain = '0'
    block = '0 1'
    execute_on = 'TIMESTEP_END'

    # --- new for setting IC --- #
    inactive_subdomain_ID = 1
    ic_strategy = "IC_POLYNOMIAL"

    nodal_patch_recovery_uo = 'extrapolation_patch_T extrapolation_patch_disp_x extrapolation_patch_disp_y'
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
      []
    []
  []
[]

[NEML2]
  input = 'models/${neml2_input}.i'
  verbose = true
  device = 'cpu'

  moose_input_types = 'MATERIAL     POSTPROCESSOR POSTPROCESSOR MATERIAL              MATERIAL'
  moose_inputs = '     neml2_strain time          time          plastic_strain        equivalent_plastic_strain'
  neml2_inputs = '     forces/E     forces/t      old_forces/t  old_state/internal/Ep old_state/internal/ep'

  moose_output_types = 'MATERIAL     MATERIAL          MATERIAL'
  moose_outputs = '     neml2_stress plastic_strain    equivalent_plastic_strain'
  neml2_outputs = '     state/S      state/internal/Ep state/internal/ep'

  moose_derivative_types = 'MATERIAL'
  moose_derivatives = 'neml2_jacobian'
  neml2_derivatives = 'state/S forces/E'

  [A]
    model = 'model'
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
    thermal_expansion_coeff = 1e-7
    stress_free_temperature = 0
    eigenstrain_name = thermal_expansion
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

[Postprocessors]
  [time]
    type = TimePostprocessor
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[Outputs]
  exodus = true
[]
