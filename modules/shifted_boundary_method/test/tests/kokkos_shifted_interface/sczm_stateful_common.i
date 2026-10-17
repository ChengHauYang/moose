[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 2
    xmax = 2
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '1 0 0'
    top_right = '2 1 0'
    block_id = 1
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = right_block
  []
[]

[Variables]
  [ux]
  []
  [uy]
  []
[]

[Kernels]
  [ux_diffusion]
    type = ${diffusion_type}
    variable = ux
  []
  [uy_diffusion]
    type = ${diffusion_type}
    variable = uy
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    sbm_distance_uo = distance
    directional_correction = true
    stress = sczm_stress
    tangent = tangent
    tangent_definition = stress_wrt_strain
  []
[]

[Materials]
  [strain]
    type = ComputeSmallStrain
    displacements = 'ux uy'
  []
[]

[NEML2]
  eager = true
  input = sczm_stateful_neml2.i
  device = cpu
  input_types = MATERIAL
  inputs = mechanical_strain
  derivatives = 'sczm_stress mechanical_strain tangent'
  [constitutive]
    model = interface_model
    interface = Block0_Block1
    interface_only = true
    executor_name = neml2
    output_variables = sczm_stress
    manage_state_advance = true
  []
[]

[UserObjects]
  [distance]
    type = BoundaryShortestDistanceToSurface
    surfaces = true_interface
    boundary = 'Block0_Block1'
    execute_on = INITIAL
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(1.047198) - (y - 0.25) * cos(1.047198)'
  []
  [loading]
    type = PiecewiseLinear
    x = '0 0.5 1'
    y = '0 0.01 0.002'
  []
[]

[BCs]
  [ux_left]
    type = DirichletBC
    variable = ux
    boundary = left
    value = 0
  []
  [ux_right]
    type = FunctionDirichletBC
    variable = ux
    boundary = right
    function = loading
    preset = false
  []
  [uy_left]
    type = DirichletBC
    variable = uy
    boundary = left
    value = 0
  []
  [uy_right]
    type = DirichletBC
    variable = uy
    boundary = right
    value = 0
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu mumps'
  dt = 0.125
  end_time = 1
  residual_and_jacobian_together = true
[]

[Outputs]
  exodus = true
[]
