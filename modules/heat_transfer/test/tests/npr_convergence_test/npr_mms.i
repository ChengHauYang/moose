nx = 20
ix0 = '${fparse 0.9 * nx}'
ix1 = '${fparse nx - ix0}'

[Problem]
  default_block = '0 2'
[]

[Mesh]
  [gmg]
    type = CartesianMeshGenerator
    dim = 2
    dx = '0.9 0.1'
    dy = '1'
    ix = '${ix0} ${ix1}'
    iy = '${nx}'
    subdomain_id = '0 1'
  []

  use_displaced_mesh = false
  add_subdomain_ids = 2
[]

[Variables]
  [diff]
    order = FIRST
  []
[]

[UserObjects]
  [extrapolation_patch]
    type = NodalPatchRecoveryVariable
    patch_polynomial_order = FIRST
    use_specific_elements = true
    var = 'diff'
    execute_on = 'TIMESTEP_END'
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

    expression = 'x-0.5*t'
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
    inactive_subdomain_ID = 1
    ic_strategy = "IC_POLYNOMIAL"

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
    boundary = right
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

# [Functions]
#   [mms_force]
#     type = ParsedFunction
#     expression = '-2*y^2 - 2*x^2' # -Laplace(x^2*y^2)
#   []
#   [mms_bc]
#     type = ParsedFunction
#     expression = 'x^2 * y^2'
#   []
# []

[Functions]
  [mms_force]
    type = ParsedFunction
    expression = '0' # Laplacian of 2x - 3y + 1
  []
  [mms_bc]
    type = ParsedFunction
    expression = '2*x - 3*y + 1'
  []
[]

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

[Outputs]
  execute_on = 'TIMESTEP_BEGIN'
  exodus = true
[]
