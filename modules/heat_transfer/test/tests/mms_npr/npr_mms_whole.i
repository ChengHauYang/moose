nx = 20
# ix0 = '${fparse 0.9 * nx}'
# ix1 = '${fparse nx - ix0}'
#elem = QUAD9
#order = SECOND
elem = QUAD4
order = FIRST
order_number = 1

number_of_element = '${fparse nx/10}' # make the extrapolation region fix
nearby_element_threshold = 1

domain_length = 1
#left_domain = '${fparse domain_length*10/11}'
left_domain = '${fparse domain_length*9/10}'

radius_search_threshold = '${fparse domain_length*number_of_element/nx}'

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

    old_subdomain_reinitialized = false
    reinitialize_subdomain_ids = '0 2'
    # ic_strategy = "IC_POLYNOMIAL"
    ic_strategy = "IC_POLYNOMIAL_WHOLE_SOLVED_DOMAIN"
    #ic_strategy = "IC_POLYNOMIAL_THRESHOLD"

    nodal_patch_recovery_uo = 'extrapolation_patch'

    nearby_element_threshold = ${nearby_element_threshold}
    radius_search_threshold = ${radius_search_threshold}
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
    # expression = 'if(${order_number} < 2, 1.1183 * x + 0.1833 * y - 0.0319, -0.1981 * x^2 - 0.1981 * y^2 - 0.4001 * x * y + 1.1183 * x + 0.1833 * y - 0.0319)'
    # expression = 'if(${order_number} < 2, 1.1079 * x + 0.2620 * y - 0.0441, -0.2153 * x^2 - 0.2153 * y^2 - 0.4415 * x * y + 1.1079 * x + 0.2620 * y - 0.0441)'
    # expression = 'if(${order_number} < 2, 3.355380839936167e-01 -3.511119826922077e-01 * x + 5.552175754015332e-01, -3.236743925655277e-01 * x^2 -3.160343803808951e-01 * y^2 -3.084956046608329e-01 * x * y + 2.347835487006705e-01 * x + 1.246724062984605e+00 * y -7.758964289738352e-02)'
    # expression = 'if(${order_number} < 2, 1.1051 * x + 0.1579 * y - 0.0255, -0.1866 * x^2 - 0.1866 * y^2 - 0.3761 * x * y + 1.1051 * x + 0.1579 * y - 0.0255)' # from python
    #expression = 'if(${order_number} < 2, 1.179581070019192e-01 -1.965718003017350e-01 * x + 7.424660370740080e-01*y, -0.1866 * x^2 - 0.1866 * y^2 - 0.3761 * x * y + 1.1051 * x + 0.1579 * y - 0.0255)'
    # c/y/x in coefficient -> p1
    # c/y/x/y^2/y*x/x^2 in coefficient -> p2
    #expression = 'if(${order_number} < 2,1.179582397365143e-01 -1.965720375559514e-01 * y + 7.424669343534810e-01 * x,-2.941472935822537e-02 +1.702804690950137e-01*y+ 1.111071580716906e+00*x -1.812109463880138e-01*y^2 -4.125369549692947e-01*y*x -1.803732289559293e-01*x^2 )'
    expression = 'if(${order_number} < 2, 0.117958283980534  -0.196572116640925  * y + 0.742467233448669* x,-2.941474961696769e-02 + 1.702804805621453e-01 * y +1.111071637989758e+00 * x -1.812109569080837e-01 * y^2 -4.125369784332500e-01 * x * y -1.803732392494518e-01 * x^2)'
  []
  [mms_force]
    type = ParsedFunction
    expression = '2 * sin(x) * cos(y)' # -(-2 * sin(x) * cos(y))
  []
  [dummy_use]
    type = ParsedFunction
    expression = '${order_number} * 0'
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
