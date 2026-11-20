E = 1e3
poi = 0.3
interfaces = 'grain1_grain2 grain1_grain4 grain1_grain5 grain2_grain4 grain3_grain4 grain3_grain5 grain4_grain5'

ny = 6
nx = '${fparse ny*3/2}'

[GlobalParams]
  displacements = 'disp_x disp_y'
  use_displaced_mesh = false
[]

[Problem]
  extra_tag_vectors = 'ref'
[]

[Mesh]
  # Each grain's watertight shell is loaded from its own file, tagged with that
  # grain's subdomain ID, and combined into the single boundary mesh that the
  # interface manager expects. The merged result is geometrically identical to
  # grain_boundary_only.msh, so the detected interfaces match the single-file case.
  [grain1]
    type = FileMeshGenerator
    file = 'grain1_shell.msh'
  []
  [grain1_id]
    type = SubdomainIDGenerator
    input = grain1
    subdomain_id = 1
  []
  [grain2]
    type = FileMeshGenerator
    file = 'grain2_shell.msh'
  []
  [grain2_id]
    type = SubdomainIDGenerator
    input = grain2
    subdomain_id = 2
  []
  [grain3]
    type = FileMeshGenerator
    file = 'grain3_shell.msh'
  []
  [grain3_id]
    type = SubdomainIDGenerator
    input = grain3
    subdomain_id = 3
  []
  [grain4]
    type = FileMeshGenerator
    file = 'grain4_shell.msh'
  []
  [grain4_id]
    type = SubdomainIDGenerator
    input = grain4
    subdomain_id = 4
  []
  [grain5]
    type = FileMeshGenerator
    file = 'grain5_shell.msh'
  []
  [grain5_id]
    type = SubdomainIDGenerator
    input = grain5
    subdomain_id = 5
  []
  [boundary_mesh]
    type = CombinerGenerator
    inputs = 'grain1_id grain2_id grain3_id grain4_id grain5_id'
    save_with_name = boundary_mesh
  []

  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = -0.6
    xmax = 0.6
    ymin = -0.4
    ymax = 0.4
    nx = ${nx}
    ny = ${ny}
    subdomain_ids = 10
  []

  [grain_gen]
    type = SubdomainGrainIDGenerator
    input = gen
    boundary_mesh = boundary_mesh
    lambda = 1
  []

  [break7]
    input = grain_gen
    type = BreakMeshByBlockGenerator
    split_interface = true
    add_interface_on_two_sides = true
  []

  [left_bottom]
    input = break7
    type = ExtraNodesetGenerator
    coord = '-0.6 -0.4'
    new_boundary = 'left_bottom'
  []

  [right_top]
    input = left_bottom
    type = ExtraNodesetGenerator
    coord = '0.6 0.4'
    new_boundary = 'right_top'
  []

  add_subdomain_ids = '10'
  add_subdomain_names = 'block_10'
  final_generator = 'right_top'
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = SMALL
        volumetric_locking_correction = true
        extra_vector_tags = 'ref'
        use_automatic_differentiation = false
        generate_output = 'stress_xx stress_xy stress_yy stress_zz strain_xx strain_xy strain_yy strain_zz'
      []
    []
  []
[]
[Physics/SolidMechanics/ShiftedCohesiveZone]
  [czm_ik]
    use_automatic_differentiation = false
    boundary = ${interfaces}
    complete_interface_mesh = boundary_mesh
  []
[]

[Materials]
  [elastic_stress]
    type = ComputeLinearElasticStress
  []
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    poissons_ratio = ${poi}
    youngs_modulus = ${E}
  []
  [interface_traction]
    type = BiLinearMixedModeTraction
    boundary = ${interfaces}
    penalty_stiffness = 2e3
    GI_c = 1e3
    GII_c = 1e2
    normal_strength = 500
    shear_strength = 300
    displacements = 'disp_x disp_y'
    eta = 2.2
    viscosity = 1e-3
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu superlu_dist'
  automatic_scaling = true
  line_search = 'none'

  nl_max_its = 500
  nl_abs_tol = 1e-12
  nl_rel_tol = 1e-50
  dt = 1
  end_time = 5
[]

[Functions]
  [displacement_with_time]
    type = ParsedFunction
    expression = '1e-2*t'
  []
[]

[Kernels]
[]

[AuxVariables]
  [react_x]
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
[]

[BCs]
  [anchor_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'left'
    value = 0.0
  []
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'bottom'
    value = 0.0
  []
  [displacement_x_right]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = 'right'
    function = displacement_with_time
    preset = false
  []
[]

[Postprocessors]
  [react_x]
    type = NodalSum
    variable = 'react_x'
    boundary = 'right'
  []

  [area]
    type = ElementIntegralFunctorPostprocessor
    functor = 1
    block = '1 2 3 4 5 10'
    execute_on = 'final'
  []
[]

[Outputs]
  exodus = true
  [csv]
    type = CSV
    precision = 15
  []
[]
