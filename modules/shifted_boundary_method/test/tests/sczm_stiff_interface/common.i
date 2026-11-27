# Unit square split by a vertical cohesive interface at x = 0.5, with an interface stiffness about
# 1e5 times the bulk Young's modulus. Combined with fitted.i or shifted.i, which add the cohesive
# zone, and with a loading file, which adds the traction-separation law and the boundary conditions.

E = 1e3
poi = 0.3
x0 = 0.5
dt = 1

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
    # An even element count puts element edges on x = 0.5, so the interface is fitted
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
    signed_dist_function = 'x-${x0}'
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
                1 0 0
                0 -1 0'
    fixed_normal = true
    new_boundary = 'left right bottom'
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
  [anchor_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'left'
    value = 0
  []
[]

[AuxVariables]
  [react_x]
  []
  [react_y]
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
  [react_y]
    type = TagVectorAux
    vector_tag = 'ref'
    v = 'disp_y'
    variable = 'react_y'
    remove_variable_scaling = true
  []
[]

[Postprocessors]
  [react_x]
    type = NodalSum
    variable = 'react_x'
    boundary = 'right'
  []
  [react_y]
    type = NodalSum
    variable = 'react_y'
    boundary = 'right'
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  # The stiff interface makes the shifted cohesive terms dominate the Jacobian. A direct solver
  # factors it regardless, whereas GMRES with algebraic multigrid exposes how well it is conditioned.
  petsc_options_iname = '-ksp_type -pc_type -pc_hypre_type'
  petsc_options_value = 'gmres hypre boomeramg'
  line_search = 'none'
  l_max_its = 200
  l_tol = 1e-8
  nl_max_its = 30
  # The residual scales with the interface stiffness, so convergence is judged relative to the
  # initial residual only
  nl_rel_tol = 1e-8
  nl_abs_tol = 1e-50
  dt = ${dt}
  # A failed step is not cut back: GMRES breaks down for the unstabilized shifted model in every
  # Newton iteration of the first step even after cutting dt back to 1e-4
  dtmin = ${dt}
  end_time = 5
[]

[Outputs]
  csv = true
[]
