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
  [T]
    type = ParsedFunction
    expression = '1 + 2*x'
  []
[]

[NEML2]
  eager = true
  input = 'thermal_neml2.i'
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
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  nl_abs_tol = 1e-12
[]

[Outputs]
  exodus = true
  file_base = neml2_action_cpu_input_out
[]