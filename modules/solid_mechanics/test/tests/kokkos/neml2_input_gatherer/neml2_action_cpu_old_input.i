[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Mesh]
  [generated]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 4
  []
[]

[Variables]
  [T]
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
      []
    []
  []
[]

[Functions]
  [t_fn]
    type = ParsedFunction
    expression = '1 + 2*x + 0.5*t'
  []
[]

[ICs]
  [t_ic]
    type = FunctionIC
    variable = T
    function = t_fn
  []
[]

[NEML2]
  eager = true
  input = 'thermal_old_neml2.i'
  [all]
    model = model
    device = 'cpu'
    derivatives = 'neml2_stress neml2_strain'
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
    custom_small_jacobian = 'dneml2_stress/dneml2_strain'
  []
[]

[Kernels]
  [time_T]
    type = TimeDerivative
    variable = T
  []
  [diffusion_T]
    type = Diffusion
    variable = T
  []
[]

[BCs]
  [disp_x_left]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [disp_x_right]
    type = DirichletBC
    variable = disp_x
    boundary = right
    value = 0.1
  []
  [disp_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'top bottom'
    value = 0
  []
  [T_all]
    type = FunctionDirichletBC
    variable = T
    boundary = 'left right top bottom'
    function = t_fn
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  nl_abs_tol = 1e-12
  dt = 1
  num_steps = 2
[]

[Outputs]
  exodus = true
  hide = 'T'
  file_base = neml2_action_cpu_old_input_out
[]
