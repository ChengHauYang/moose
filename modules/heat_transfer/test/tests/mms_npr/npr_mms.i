nx = 20
# ix0 = '${fparse 0.9 * nx}'
# ix1 = '${fparse nx - ix0}'
#elem = QUAD9
#order = SECOND
elem = QUAD4
order = FIRST
order_number = 1

domain_length = 1
#left_domain = '${fparse domain_length*10/11}'
left_domain = '${fparse domain_length*9/10}'

[GlobalParams]
  block = '0 2'
[]

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${nx}
    elem_type = ${elem}
    xmax = ${domain_length}
    ymax = ${domain_length}
  []

  [block1]
    type = SubdomainBoundingBoxGenerator
    block_id = 0
    bottom_left = '0 0 0'
    top_right = '${left_domain} ${domain_length} 0'
    input = gmg
  []
  [block2]
    type = SubdomainBoundingBoxGenerator
    block_id = 1
    bottom_left = '${left_domain} 0 0'
    top_right = '${domain_length} ${domain_length} 0'
    input = block1
  []

  [interface_0_1]
    type = SideSetsBetweenSubdomainsGenerator
    input = block2
    primary_block = 0
    paired_block = 1
    new_boundary = 'right_original_boundary'
  []

  use_displaced_mesh = false
  add_subdomain_ids = 2
[]

[Variables]
  [diff]
    # order = FIRST
    order = ${order} #Finite element LAGRANGE on geometric element QUAD4
    # only supports FEInterface::max_order = 1, not fe_type.order = 2
  []
[]

[UserObjects]
  [extrapolation_patch]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = ${order}
    use_specific_elements = true
    var = 'diff'
    execute_on = 'TIMESTEP_END'
    verbose = true
  []
[]

[AuxVariables]
  [u]
    block = '1 2'
  []
[]

[AuxKernels]
  [cut]
    type = ParsedAux
    variable = 'u'

    expression = 'x-0.5*${domain_length}*t'
    use_xyzt = true
    block = '1 2'
    execute_on = 'INITIAL TIMESTEP_END'
  []
[]

[MeshModifiers]
  [esm]
    type = CoupledVarThresholdElementSubdomainModifier
    coupled_var = 'u'
    criterion_type = 'BELOW'
    threshold = 0
    subdomain_id = 2
    block = '1 2'
    execute_on = 'TIMESTEP_END'

    # --- new for setting IC --- #
    unsolved_blocks = '1'
    ic_strategy = "IC_POLYNOMIAL"
    # ic_strategy = "IC_POLYNOMIAL_WHOLE_SOLVED_DOMAIN"

    nodal_patch_recovery_uo = 'extrapolation_patch'
  []
[]

[Kernels]
  [heat]
    type = Diffusion
    variable = diff
  []
  [mms_forcing]
    type = BodyForce
    variable = diff
    function = mms_force
  []
[]

[BCs]
  [left]
    type = FunctionDirichletBC
    variable = diff
    boundary = left
    function = mms_bc
  []

  [right]
    type = FunctionDirichletBC
    variable = diff
    boundary = right_original_boundary
    function = mms_bc
  []

  [top]
    type = FunctionDirichletBC
    variable = diff
    boundary = top
    function = mms_bc
  []

  [bottom]
    type = FunctionDirichletBC
    variable = diff
    boundary = bottom
    function = mms_bc
  []
[]

[Functions]
  [mms_bc]
    type = ParsedFunction
    expression = 'sin(x) * cos(y)'
  []
  [mms_bc_approx]
    type = ParsedFunction
    #expression = 'if(${order_number} < 2, 3.709307137947722e-01 -3.660143390922946e-01  * y + 5.235813196693577e-01* x,3.583340447690454e-01 + 2.329959819569409e-01 * y + 2.810640261603539e-01 * x - 3.374122072254255e-01 * y^2 - 2.909172488339279e-01 * y * x + 2.157295116082268e-01 * x^2)'
    expression = 'if(${order_number} < 2, 0.117958283980534  -0.196572116640925  * y + 0.742467233448669* x,-2.941474961696769e-02 + 1.702804805621453e-01 * y +1.111071637989758e+00 * x -1.812109569080837e-01 * y^2 -4.125369784332500e-01 * x * y -1.803732392494518e-01 * x^2)'
  []
  [mms_force]
    type = ParsedFunction
    expression = '2 * sin(x) * cos(y)' # -(-2 * sin(x) * cos(y))
  []
[]

# [Functions]
#   [mms_bc]
#     type = ParsedFunction
#     expression = 'x * y^2'
#   []

#   [mms_bc_approx]
#     type = ParsedFunction
#     expression = 'if(${order_number} < 2, -0.1666 * x - 0.5 * y + 0.0833, 0.5 * y^2 + 1.0 * x * y - 0.1666 * x - 0.5 * y + 0.0833)'
#   []

#   [mms_force]
#     type = ParsedFunction
#     expression = '2 * x' # = - (∂²u/∂x² + ∂²u/∂y²)
#   []
# []

# [Functions]
#   [mms_bc]
#     type = ParsedFunction
#     expression = 'x * tan(y)'
#   []
#   [mms_force]
#     type = ParsedFunction
#     expression = '-2 * x * sec(y)^2 * tan(y)'
#   []
# []

# [Functions]
#   [mms_force]
#     type = ParsedFunction
#     expression = '-4- 6' # -Laplace(mms_bc)
#   []
#   [mms_bc]
#     type = ParsedFunction
#     expression = 'x*y + 2*x^2 + 3*y^2 -x -y + 1'
#   []
# []

# [Functions]
#   [mms_force]
#     type = ParsedFunction
#     expression = '0' # Laplacian of 2x - 3y + 1
#   []
#   [mms_bc]
#     type = ParsedFunction
#     expression = '2*x - 3*y + 1'
#   []
# []

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_max_its = 100
  nl_rel_tol = 1e-6
  nl_abs_tol = 1e-10
  dt = 1
  end_time = 3
[]

[Postprocessors]
  [l2_error_exact]
    execute_on = 'TIMESTEP_BEGIN'
    type = ElementL2Error
    variable = diff
    function = mms_bc
    block = '2'
  []
  [l2_error_approx]
    execute_on = 'TIMESTEP_BEGIN'
    type = ElementL2Error
    variable = diff
    function = mms_bc_approx
    block = '2'
  []
[]

[Outputs]
  execute_on = 'TIMESTEP_BEGIN'
  exodus = true
[]
