# 3D case (b), C++ list form: each grain's watertight shell is loaded from its
# own file and handed directly to the interface manager via the SCZM action's
# complete_interface_meshes + interface_subdomain_ids parameters. The manager
# stamps each shell with its grain ID and stitches them internally, so no
# SubdomainIDGenerator + CombinerGenerator boilerplate is needed and the result
# is identical to the single-file case (a).
#
# The manager's surface element set accepts only TRI3 surface elements, so the
# volume mesh is TET4 (its boundary faces are TRI3).

E = 1e3
poi = 0.3
interfaces = 'grain1_grain2 grain1_grain3 grain2_grain4 grain3_grain4'

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
  use_displaced_mesh = false
[]

[Problem]
  extra_tag_vectors = 'ref'
[]

[Mesh]
  # Watertight grain shells, each saved for the interface manager to consume
  # directly via the action. No in-input tagging or combining.
  [grain1_shell]
    type = FileMeshGenerator
    file = 'grain1_shell_3d.e'
    save_with_name = 'grain1_shell'
  []
  [grain2_shell]
    type = FileMeshGenerator
    file = 'grain2_shell_3d.e'
    save_with_name = 'grain2_shell'
  []
  [grain3_shell]
    type = FileMeshGenerator
    file = 'grain3_shell_3d.e'
    save_with_name = 'grain3_shell'
  []
  [grain4_shell]
    type = FileMeshGenerator
    file = 'grain4_shell_3d.e'
    save_with_name = 'grain4_shell'
  []

  # Bulk mesh: the same TET4 cube split into four grains as case (a).
  [cube]
    type = GeneratedMeshGenerator
    dim = 3
    xmin = 0
    xmax = 2
    ymin = 0
    ymax = 2
    zmin = 0
    zmax = 1
    nx = 4
    ny = 4
    nz = 2
    elem_type = TET4
  []
  [bgrain1]
    type = SubdomainBoundingBoxGenerator
    input = cube
    block_id = 1
    block_name = grain1
    bottom_left = '0 0 0'
    top_right = '1 1 1'
  []
  [bgrain2]
    type = SubdomainBoundingBoxGenerator
    input = bgrain1
    block_id = 2
    block_name = grain2
    bottom_left = '1 0 0'
    top_right = '2 1 1'
  []
  [bgrain3]
    type = SubdomainBoundingBoxGenerator
    input = bgrain2
    block_id = 3
    block_name = grain3
    bottom_left = '0 1 0'
    top_right = '1 2 1'
  []
  [bgrain4]
    type = SubdomainBoundingBoxGenerator
    input = bgrain3
    block_id = 4
    block_name = grain4
    bottom_left = '1 1 0'
    top_right = '2 2 1'
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = bgrain4
    split_interface = true
  []

  final_generator = 'break'
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = SMALL
        extra_vector_tags = 'ref'
        use_automatic_differentiation = false
        generate_output = 'stress_xx stress_yy stress_zz'
      []
    []
  []
[]
[Physics/SolidMechanics/ShiftedCohesiveZone]
  [czm_ik]
    use_automatic_differentiation = false
    boundary = ${interfaces}
    complete_interface_meshes = 'grain1_shell grain2_shell grain3_shell grain4_shell'
    interface_subdomain_ids = '1 2 3 4'
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
    type = PureElasticTractionSeparation
    boundary = ${interfaces}
    normal_stiffness = 2e3
    tangent_stiffness = 2e3
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu superlu_dist'
  automatic_scaling = true
  line_search = 'none'
  nl_max_its = 100
  nl_abs_tol = 1e-11
  nl_rel_tol = 1e-50
  dt = 1
  end_time = 3
[]

[Functions]
  [displacement_with_time]
    type = ParsedFunction
    expression = '1e-2*t'
  []
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
  [anchor_z]
    type = DirichletBC
    variable = disp_z
    boundary = 'back'
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
[]

[Outputs]
  [csv]
    type = CSV
    precision = 15
  []
[]
