# Plane-strain total-Lagrangian shifted cohesive zone using element_linear stress recovery with
# directional_correction = true and NO interface NEML2 block or interface bulk stress.
# Exercises the general least-squares affine recovery fallback on distorted QUAD4 elements.

theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 2
    xmax = 2
  []
  [distort]
    type = ParsedNodeTransformGenerator
    input = gen
    x_function = 'x + 0.06 * x * y'
    y_function = 'y + 0.05 * x * y'
  []
  [right_block]
    type = ParsedSubdomainMeshGenerator
    input = distort
    combinatorial_geometry = 'x > 1 + 0.06 * y'
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
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad})'
  []
  [ux_ic]
    type = ParsedFunction
    expression = '0.04 * x * y + 0.03 * x * x'
  []
  [uy_ic]
    type = ParsedFunction
    expression = '-0.03 * x * y - 0.02 * y * y'
  []
  [ux_right_ic]
    type = ParsedFunction
    expression = '0.01 + f'
    symbol_names = f
    symbol_values = ux_ic
  []
  [uy_right_ic]
    type = ParsedFunction
    expression = '-0.008 + f'
    symbol_names = f
    symbol_values = uy_ic
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
  [uy_left]
    type = FunctionIC
    variable = uy
    function = uy_ic
    block = 0
  []
  [uy_right]
    type = FunctionIC
    variable = uy
    function = uy_right_ic
    block = 1
  []
[]

[Kernels]
  [stress_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = ux
    component = 0
    displacements = 'ux uy'
    stabilize_strain = true
  []
  [stress_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = uy
    component = 1
    displacements = 'ux uy'
    stabilize_strain = true
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    sbm_distance_uo = distance
    strain = FINITE
    use_kokkos = true
    directional_correction = true
    volumetric_locking_correction = true
    stabilization = weighted_traction
    stabilization_beta = 10
    stabilization_normal_stiffness = 10
    stabilization_tangential_stiffness = 30
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
    displacements = 'ux uy'
    stabilize_strain = true
  []
[]

[Materials]
  [f_bar_average]
    type = KokkosComputeFbarAverage
    displacements = 'ux uy'
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
    displacements = 'ux uy'
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
