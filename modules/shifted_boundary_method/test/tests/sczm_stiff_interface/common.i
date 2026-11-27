# Unit square split by a cohesive interface through (0.5, 0.5) inclined 60 degrees to the x axis,
# with an interface stiffness about 1e5 times the bulk Young's modulus. The left edge is clamped
# and the right edge is pulled, so the interface carries a nonuniform traction that the elements
# cut by the interface cannot represent exactly. Combined with fitted.i or shifted.i, which add the
# cohesive zone, and with a file that adds the traction-separation law.

E = 1e3
poi = 0.3
theta = '${fparse 60 * pi / 180}'
# Pull per time step: the last of the 5 steps loads the interface to about half its strength
load = 0.06

[GlobalParams]
  displacements = 'disp_x disp_y'
  use_displaced_mesh = false
[]

[Problem]
  extra_tag_vectors = 'ref'
[]

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 20
    ny = 20
    elem_type = QUAD4
  []
  [subdomain_intercepted]
    type = SubdomainInterceptedGenerator
    input = gen
    subdomain_id_inside = 1
    subdomain_id_outside = 2
    lambda = 0.5
    is_domain_inside_surface = true
    signed_dist_function = '(x-0.5)*sin(${theta}) - (y-0.5)*cos(${theta})'
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = subdomain_intercepted
    split_interface = true
    add_interface_on_two_sides = true
  []
  [ss]
    type = SideSetsFromNormalsGenerator
    input = break
    normals = '-1 0 0
                1 0 0'
    fixed_normal = true
    new_boundary = 'left right'
  []
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = SMALL
        extra_vector_tags = 'ref'
        add_variables = true
        use_automatic_differentiation = false
      []
    []
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    poissons_ratio = ${poi}
    youngs_modulus = ${E}
  []
  [elastic_stress]
    type = ComputeLinearElasticStress
  []
[]

[BCs]
  [clamp_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'left'
    value = 0
  []
  [clamp_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'left'
    value = 0
  []
  [pull]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = 'right'
    function = '${load}*t'
    preset = false
  []
[]

[AuxVariables]
  [react_x]
  []
  [normal_traction]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[AuxKernels]
  [react_x]
    type = TagVectorAux
    vector_tag = 'ref'
    v = 'disp_x'
    variable = 'react_x'
    remove_variable_scaling = true
  []
  [normal_traction]
    type = MaterialRealVectorValueAux
    property = interface_traction
    component = 0
    variable = normal_traction
    boundary = 'Block1_Block2'
    execute_on = 'TIMESTEP_END'
    # Elements at a corner of the zigzag surrogate interface have two sides on the interface
    check_boundary_restricted = false
  []
[]

[Postprocessors]
  [react_x]
    type = NodalSum
    variable = 'react_x'
    boundary = 'right'
  []
  [normal_traction_average]
    type = SideAverageValue
    variable = normal_traction
    boundary = 'Block1_Block2'
  []
  [normal_traction_max]
    type = SideExtremeValue
    variable = normal_traction
    boundary = 'Block1_Block2'
  []
  [normal_traction_min]
    type = SideExtremeValue
    variable = normal_traction
    boundary = 'Block1_Block2'
    value_type = min
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  # A direct solver, so that the differences between the cohesive zone models are those of the
  # discrete solutions rather than of the linear solver
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu superlu_dist'
  line_search = 'none'
  nl_max_its = 30
  # The residual scales with the interface stiffness, so convergence is judged relative to the
  # initial residual only
  nl_rel_tol = 1e-8
  nl_abs_tol = 1e-50
  dt = 1
  # A failed step is not cut back, so that a model that does not converge fails at once
  dtmin = 1
  end_time = 5
[]

[Outputs]
  csv = true
[]
