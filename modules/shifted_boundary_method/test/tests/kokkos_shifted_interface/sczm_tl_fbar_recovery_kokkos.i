# Total-Lagrangian shifted cohesive zone using element_linear stress recovery with
# directional_correction = true and NO interface NEML2 block or interface bulk stress.
# Exercises the bulk-only workflow on distorted HEX8 elements with total-mode F-bar.

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
  [distort]
    type = ParsedNodeTransformGenerator
    input = gen
    x_function = 'x + 0.06 * y * z'
    y_function = 'y + 0.05 * x * z'
    z_function = 'z + 0.04 * x * y'
  []
  [right_block]
    type = ParsedSubdomainMeshGenerator
    input = distort
    combinatorial_geometry = 'x > 1 + 0.06 * y * z'
    block_id = 1
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = right_block
    add_interface_on_two_sides = true
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

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad}) + 0.1 * (z - 0.5)'
  []
  [ux_ic]
    type = ParsedFunction
    expression = '0.04 * x * y + 0.03 * y * z - 0.02 * z * x + 0.03 * x * x'
  []
  [uy_ic]
    type = ParsedFunction
    expression = '-0.03 * x * y + 0.05 * y * z + 0.02 * z * x - 0.02 * y * y'
  []
  [uz_ic]
    type = ParsedFunction
    expression = '0.02 * x * y - 0.04 * y * z + 0.05 * z * x + 0.02 * z * z'
  []
  [ux_right_ic]
    type = ParsedFunction
    expression = '0.01 + f'
    symbol_names = f
    symbol_values = ux_ic
  []
  [uz_right_ic]
    type = ParsedFunction
    expression = '-0.008 + f'
    symbol_names = f
    symbol_values = uz_ic
  []
[]

[ICs]
  [ux_left]
    type = FunctionIC
    variable = ux
    function = ux_ic
    block = 0
  []
  [ux_right]
    type = FunctionIC
    variable = ux
    function = ux_right_ic
    block = 1
  []
  [uy]
    type = FunctionIC
    variable = uy
    function = uy_ic
  []
  [uz_left]
    type = FunctionIC
    variable = uz
    function = uz_ic
    block = 0
  []
  [uz_right]
    type = FunctionIC
    variable = uz
    function = uz_right_ic
    block = 1
  []
[]

[Kernels]
  [stress_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = ux
    component = 0
    displacements = 'ux uy uz'
    stabilize_strain = true
  []
  [stress_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = uy
    component = 1
    displacements = 'ux uy uz'
    stabilize_strain = true
  []
  [stress_z]
    type = KokkosTotalLagrangianStressDivergence
    variable = uz
    component = 2
    displacements = 'ux uy uz'
    stabilize_strain = true
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
    volumetric_locking_correction = true
    stabilization = weighted_traction
    # beta = gamma |face| (lambda + 2 mu) / V of about 11 (E = 100, nu = 0.3, face-to-volume ratio
    # 2), comparable to the cohesive stiffnesses, so both tractions enter the Jacobian
    stabilization_gamma = 0.04
    stress_recovery = element_linear
    tangent_definition = pk2_wrt_deformation_gradient
  []
[]

[NEML2]
  eager = true
  input = sczm_tl_fbar_neml2.i
  device = cpu
  [bulk]
    model = bulk_model
    executor_name = neml2_bulk
    input_kernels = deformation_gradient
    auto_output = false
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
  [assembly]
    type = TorchAssembly
  []
  [fe]
    type = TorchFEInterpolation
    assembly = assembly
  []
  [deformation_gradient]
    type = TorchDeformationGradient
    assembly = assembly
    fe = fe
    to_neml2 = deformation_gradient
    displacements = 'ux uy uz'
    stabilize_strain = true
  []
[]

[Materials]
  [f_bar_average]
    type = KokkosComputeFbarAverage
    displacements = 'ux uy uz'
  []
  [bulk_pk2]
    type = NEML2ToKokkosFullRankTwoMaterialProperty
    neml2_executor = neml2_bulk
    from_neml2 = neml2_stress
    to_moose = bulk_pk2
  []
  [bulk_pk2_jacobian]
    type = NEML2ToKokkosFullRankFourMaterialProperty
    neml2_executor = neml2_bulk
    from_neml2 = neml2_stress
    neml2_input_derivative = deformation_gradient
    to_moose = bulk_pk2_jacobian
  []
  [bulk_pk1]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'ux uy uz'
    custom_pk2_stress = bulk_pk2
    custom_pk2_jacobian = bulk_pk2_jacobian
    stabilize_strain = true
  []
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
[]

[BCs]
  [ux_left]
    type = KokkosDirichletBC
    variable = ux
    boundary = left
    value = 0
  []
  [ux_right]
    type = KokkosDirichletBC
    variable = ux
    boundary = right
    value = 0.08
  []
  [uy_left]
    type = KokkosDirichletBC
    variable = uy
    boundary = left
    value = 0
  []
  [uy_right]
    type = KokkosDirichletBC
    variable = uy
    boundary = right
    value = -0.05
  []
  [uz_left]
    type = KokkosDirichletBC
    variable = uz
    boundary = left
    value = 0
  []
  [uz_right]
    type = KokkosDirichletBC
    variable = uz
    boundary = right
    value = 0.04
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_abs_tol = 1e-10
  nl_rel_tol = 1e-10
[]
