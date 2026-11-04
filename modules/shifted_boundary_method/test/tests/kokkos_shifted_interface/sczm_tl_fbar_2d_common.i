# Plane-strain counterpart of sczm_tl_fbar_common.i: total-Lagrangian shifted cohesive zone with
# total-mode F-bar on both the bulk and the interface.
#
# The mesh is distorted by a bilinear map, so the quadrilaterals are non-affine and the average of
# the deformation gradient over an interface edge differs from its average over the element area.
# The nonuniform quadratic initial displacement makes the F-bar deformation gradient B differ from
# the physical deformation gradient U at every quadrature point, and the block-wise offset opens the
# angled true interface. All deformation gradients stay 3x3 with U_zz = 1, so B = gamma U with the
# same cube-root volume ratio gamma = (det(M) / det(U))^(1/3) as in three dimensions, and B_zz =
# gamma. The host and Kokkos inputs differ only in their object types.

# Disabling F-bar also requires the single-input NEML2 interface model:
# fbar=false interface_model=interface_model_off interface_input_types=MATERIAL
# interface_inputs=deformation_gradient
# interface_derivatives='interface_pk1 deformation_gradient interface_pk1_dU'
fbar = true
interface_model = interface_model
interface_input_types = 'MATERIAL MATERIAL'
interface_inputs = 'physical_deformation_gradient deformation_gradient'
interface_derivatives = 'interface_pk1 physical_deformation_gradient interface_pk1_dU; interface_pk1 deformation_gradient stabilized_pk1_jacobian'

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
  # The line x = 1 maps to x = 1 + 0.06 y / 1.05. Element centroids lie at least 0.2 away from it,
  # so the simpler test below separates the blocks.
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
  # Block-wise offsets that open the interface
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

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    sbm_distance_uo = distance
    strain = FINITE
    use_kokkos = ${use_kokkos}
    directional_correction = true
    volumetric_locking_correction = ${fbar}
    stress = interface_pk1
    tangent = interface_pk1_dU
    stabilized_tangent = stabilized_pk1_jacobian
    tangent_definition = pk1_wrt_deformation_gradient
  []
[]

[NEML2]
  eager = true
  input = sczm_tl_fbar_neml2.i
  device = cpu
  [interface]
    model = ${interface_model}
    executor_name = neml2_interface
    input_types = ${interface_input_types}
    inputs = ${interface_inputs}
    derivatives = ${interface_derivatives}
    output_variables = 'interface_pk1'
    interface = 'Block0_Block1'
    interface_only = true
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

[BCs]
  [ux_left]
    type = ${dirichlet_type}
    variable = ux
    boundary = left
    value = 0
  []
  [ux_right]
    type = ${dirichlet_type}
    variable = ux
    boundary = right
    value = 0.08
  []
  [uy_left]
    type = ${dirichlet_type}
    variable = uy
    boundary = left
    value = 0
  []
  [uy_right]
    type = ${dirichlet_type}
    variable = uy
    boundary = right
    value = -0.05
  []
[]

[Postprocessors]
  [ux_right_average]
    type = SideAverageValue
    variable = ux
    boundary = Block1_Block0
  []
  [uy_right_average]
    type = SideAverageValue
    variable = uy
    boundary = Block1_Block0
  []
  [ux_left_average]
    type = SideAverageValue
    variable = ux
    boundary = Block0_Block1
  []
  [uy_left_average]
    type = SideAverageValue
    variable = uy
    boundary = Block0_Block1
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

[Outputs]
  csv = true
  file_base = ${file_base}
[]
