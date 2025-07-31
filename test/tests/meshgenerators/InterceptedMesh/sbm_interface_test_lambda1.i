[Mesh]

  [gen]
    type = CartesianMeshGenerator
    dim = 2
    dx = '3'
    dy = '1'
    ix = '90'
    iy = '30'
    subdomain_id = '1'
  []

  [subdomain]
    type = SubdomainInterceptedGeneratorWaterTightGeo
    input = 'gen'
    subdomain_id_inside = '1'
    subdomain_id_outside = '2'
    subdomain_id_intercepted = 3
    subdomain_id_false_intercected = 4
    subdomain_id_neighbor_intercepted = 5
    mark_neighbor_of_intercepted = false
    threshold = 1
    lambda = 1 
    outer_boundary = false
    #outer_boundary = true
    water_tight_geo_path = 'half_mesh.msh'
    x_shift = -1.0
    y_shift = 0.0
    qrule_order = 2
  []
[]

[Functions]
  [mms_force]
    type = ParsedFunction
    #expression = '1'
    expression = '2*pi^2*sin(pi*x)*sin(pi*y)'
  []
  [mms_bc]
    type = ParsedFunction
    #expression = "0.25*(0.25 - (x-1)^2 - (y-1)^2)"
    expression = "sin(pi*x) * sin(pi*y)"
  []
  [zero_position_function]
    type = ParsedFunction
    expression = '0'
  []
[]

[Variables]
  [diffused]
    #order = SECOND
    order = FIRST
  []
[]

[Kernels]
  [diff]
    type = Diffusion
    variable = diffused
  []
  [mms_forcing]
    type = BodyForce
    variable = diffused
    function = mms_force
  []
[]

# [BCs]
#   [sbm_bc]
#     type = SbmBC
#     variable = diffused
#     boundary = SBMinterface
#     alpha = 2e2
#     dirichlet_function = mms_bc
#     water_tight_geo_path = 'star.msh'
#     x_shift = 2.0
#     y_shift = 2.0
#   []
# []

[Postprocessors]
  [l2_error]
    type = ElementL2Error
    variable = diffused
    function = mms_bc
  []
  [total_area]
    type = ElementIntegralFunctorPostprocessor
    functor = 1
  []
[]

#[Executioner]
#  type = Steady
#  solve_type = 'PJFNK'
#  #solve_type = 'LDU'
#[]

#[Executioner]
#  type = Steady
#
#  petsc_options_iname = '-ksp_type -pc_type -pc_factor_mat_solver_type -ksp_max_it -snes_atol -snes_rtol'
#  petsc_options_value = 'bcgs lu mumps 100 1e-12 1e-12'
#
#  nl_max_its = 50  # Allow more nonlinear iterations
#  nl_abs_tol = 1e-12
#  nl_rel_tol = 1e-12  # Ensure relative tolerance is also strict
#[]

[Executioner]
  type = Steady
  solve_type = 'LINEAR'

  petsc_options_iname = '-ksp_type -pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'preonly lu mumps'
[]

[Outputs]
  execute_on = 'timestep_end'
  vtk = true
  csv = true
  #exodus = true
[]
