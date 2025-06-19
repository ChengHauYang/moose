neml2_input = viscoplasticity_isoharden

[GlobalParams]
  displacements = 'disp_x disp_y'
  block = '0'
[]

[Problem]
  kernel_coverage_check = ONLY_LIST
  kernel_coverage_block_list = '0'
  material_coverage_check = ONLY_LIST
  material_coverage_block_list = '0'
[]

[Mesh]
  [gmg]
    type = CartesianMeshGenerator
    dim = 2
    dx = '0.7 0.5 0.8'
    dy = '1.0 0.2 0.8'
    ix = '7 5 8'
    iy = '10 2 8'
    subdomain_id = '0 0 0
                    0 1 0
                    0 0 0'
  []

  use_displaced_mesh = false
[]

[AuxVariables]
  [u]
    block = '0 1'
  []
[]

[AuxKernels]
  [cut]
    type = ParsedAux
    variable = 'u'

    expression = 'x - (0.7+0.1*t)'
    use_xyzt = true
    block = '0 1'
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[MeshModifiers]
  [cut]
    type = CoupledVarThresholdElementSubdomainModifier
    coupled_var = 'u'
    criterion_type = 'BELOW'
    threshold = 0
    subdomain_id = 0
    execute_on = 'TIMESTEP_BEGIN'
    block = '0 1'
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
[]

[BCs]
  [anchor_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'left'
    value = 0.0
  []
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'top'
    value = 0.0
  []
  [disp_y_BC]
    type = DirichletBC
    variable = disp_y
    boundary = 'bottom'
    value = 0.0001
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  automatic_scaling = true
  dt = 1
  dtmin = 1
  num_steps = 5
  residual_and_jacobian_together = true
[]

[Postprocessors]
  [time]
    type = TimePostprocessor
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[Outputs]
  interval = 1
  exodus = true
  execute_on = 'timestep_end'
[]

