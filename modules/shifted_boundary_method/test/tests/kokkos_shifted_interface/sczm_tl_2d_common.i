theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 4
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
    strain = FINITE
    use_kokkos = ${use_kokkos}
    directional_correction = false
    no_shifted = ${no_shifted}
  []
[]

[Materials]
  [local_traction]
    type = ${traction_type}
    boundary = 'Block0_Block1'
    normal_stiffness = 500
    tangent_stiffness = 300
  []
[]

[UserObjects]
  [distance]
    type = BoundaryShortestDistanceToSurface
    surfaces = true_interface
    boundary = 'Block0_Block1'
    execute_on = INITIAL
    execution_order_group = -1
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad})'
  []
[]

[BCs]
  [ux_left]
    type = ${dirichlet_type}
    variable = ux
    boundary = left
    value = 0.1
  []
  [ux_right]
    type = ${dirichlet_type}
    variable = ux
    boundary = right
    value = -0.15
  []
  [uy_left]
    type = ${dirichlet_type}
    variable = uy
    boundary = left
    value = 0.2
  []
  [uy_right]
    type = ${dirichlet_type}
    variable = uy
    boundary = right
    value = -0.25
  []
[]

[Postprocessors]
  [react_left_x]
    type = SideIntegralVariablePostprocessor
    variable = ux
    boundary = left
  []
  [react_right_x]
    type = SideIntegralVariablePostprocessor
    variable = ux
    boundary = right
  []
  [react_left_y]
    type = SideIntegralVariablePostprocessor
    variable = uy
    boundary = left
  []
  [react_right_y]
    type = SideIntegralVariablePostprocessor
    variable = uy
    boundary = right
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu superlu_dist'
  nl_abs_tol = 1e-12
  nl_rel_tol = 1e-12
[]

[Outputs]
  exodus = true
  csv = true
  file_base = ${file_base}
[]
