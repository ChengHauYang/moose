# 3D total-Lagrangian weighted-traction SCZM problem shared by native and NEML2 traction tests.
theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 4
    ny = 2
    nz = 2
    xmax = 2
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '1 0 0'
    top_right = '2 1 1'
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
  [uz]
  []
[]

[Kernels]
  [stress_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = ux
    component = 0
    displacements = 'ux uy uz'
    large_kinematics = true
    stabilize_strain = false
  []
  [stress_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = uy
    component = 1
    displacements = 'ux uy uz'
    large_kinematics = true
    stabilize_strain = false
  []
  [stress_z]
    type = KokkosTotalLagrangianStressDivergence
    variable = uz
    component = 2
    displacements = 'ux uy uz'
    large_kinematics = true
    stabilize_strain = false
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy uz'
    sbm_distance_uo = distance
    strain = FINITE
    use_kokkos = true
    directional_correction = true
    stabilization = weighted_traction
    stress = pk1_stress
    tangent = dpk1_d_grad_u
    volume_tangent = dpk1_d_grad_u
    tangent_definition = pk1_wrt_deformation_gradient
  []
[]

[Materials]
  [def_grad]
    type = KokkosComputeDeformationGradient
    displacements = 'ux uy uz'
    stabilize_strain = false
  []
  [pk2]
    type = KokkosComputeStVenantKirchhoffStress
    youngs_modulus = 200
    poissons_ratio = 0.3
  []
  [pk1]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'ux uy uz'
    custom_pk2_stress = pk2_stress
    custom_pk2_jacobian = dpk2_dF
    stabilize_strain = false
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
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad}) + 0.1 * (z - 0.5)'
  []
  [ux_left_fn]
    type = ParsedFunction
    expression = '0.05 * t'
  []
  [ux_right_fn]
    type = ParsedFunction
    expression = '-0.075 * t'
  []
  [uy_left_fn]
    type = ParsedFunction
    expression = '0.075 * t'
  []
  [uy_right_fn]
    type = ParsedFunction
    expression = '-0.1 * t'
  []
  [uz_left_fn]
    type = ParsedFunction
    expression = '0.04 * t'
  []
  [uz_right_fn]
    type = ParsedFunction
    expression = '-0.06 * t'
  []
[]

[BCs]
  [ux_left]
    type = FunctionDirichletBC
    variable = ux
    boundary = left
    function = ux_left_fn
    preset = false
  []
  [ux_right]
    type = FunctionDirichletBC
    variable = ux
    boundary = right
    function = ux_right_fn
    preset = false
  []
  [uy_left]
    type = FunctionDirichletBC
    variable = uy
    boundary = left
    function = uy_left_fn
    preset = false
  []
  [uy_right]
    type = FunctionDirichletBC
    variable = uy
    boundary = right
    function = uy_right_fn
    preset = false
  []
  [uz_left]
    type = FunctionDirichletBC
    variable = uz
    boundary = left
    function = uz_left_fn
    preset = false
  []
  [uz_right]
    type = FunctionDirichletBC
    variable = uz
    boundary = right
    function = uz_right_fn
    preset = false
  []
[]

[Postprocessors]
  [ux_left_side]
    type = SideAverageValue
    variable = ux
    boundary = Block0_Block1
  []
  [ux_right_side]
    type = SideAverageValue
    variable = ux
    boundary = Block1_Block0
  []
  [uy_left_side]
    type = SideAverageValue
    variable = uy
    boundary = Block0_Block1
  []
  [uy_right_side]
    type = SideAverageValue
    variable = uy
    boundary = Block1_Block0
  []
  [uz_left_side]
    type = SideAverageValue
    variable = uz
    boundary = Block0_Block1
  []
  [uz_right_side]
    type = SideAverageValue
    variable = uz
    boundary = Block1_Block0
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_abs_tol = 1e-12
  nl_rel_tol = 1e-12
  dt = 0.5
  num_steps = 2
[]

[Outputs]
  csv = true
  file_base = ${file_base}
[]
