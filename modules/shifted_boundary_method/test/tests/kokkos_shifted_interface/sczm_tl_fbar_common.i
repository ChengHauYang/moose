# Total-Lagrangian shifted cohesive zone with total-mode F-bar on both the bulk and the interface.
#
# The mesh is distorted by a trilinear map, so the hexahedra are non-affine and the average of the
# deformation gradient over an interface face differs from its average over the element volume. The
# nonuniform quadratic initial displacement makes the F-bar deformation gradient B differ from the
# physical deformation gradient U at every quadrature point, and the block-wise offset opens the
# angled true interface. The host and Kokkos inputs differ only in their object types.

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
  # Block-wise offsets that open the interface
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

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy uz'
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
  [uz_left]
    type = ${dirichlet_type}
    variable = uz
    boundary = left
    value = 0
  []
  [uz_right]
    type = ${dirichlet_type}
    variable = uz
    boundary = right
    value = 0.04
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
  [uz_right_average]
    type = SideAverageValue
    variable = uz
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
  [uz_left_average]
    type = SideAverageValue
    variable = uz
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
