# 3D case (c): each already-formed grain interface (the shared face between two
# grains) is loaded from its own file and drives the signed-distance user object
# directly via generate_sbm_distance + surface_meshes. No interface manager is
# needed because each mesh is already an interface, not a watertight shell.
#
# The bulk mesh is the same TET4 cube split into four grains as cases (a)/(b), so
# the solution matches them to floating-point (a different but equivalent
# distance path than the manager's).

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
  # Bulk mesh: the same TET4 cube split into four grains as cases (a)/(b).
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
  [grain1]
    type = SubdomainBoundingBoxGenerator
    input = cube
    block_id = 1
    block_name = grain1
    bottom_left = '0 0 0'
    top_right = '1 1 1'
  []
  [grain2]
    type = SubdomainBoundingBoxGenerator
    input = grain1
    block_id = 2
    block_name = grain2
    bottom_left = '1 0 0'
    top_right = '2 1 1'
  []
  [grain3]
    type = SubdomainBoundingBoxGenerator
    input = grain2
    block_id = 3
    block_name = grain3
    bottom_left = '0 1 0'
    top_right = '1 2 1'
  []
  [grain4]
    type = SubdomainBoundingBoxGenerator
    input = grain3
    block_id = 4
    block_name = grain4
    bottom_left = '1 1 0'
    top_right = '2 2 1'
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = grain4
    split_interface = true
  []

  # One saved surface mesh per grain interface (the shared face).
  [grain1_grain2]
    type = FileMeshGenerator
    file = 'grain1_grain2_3d.e'
    save_with_name = 'grain1_grain2'
  []
  [grain1_grain3]
    type = FileMeshGenerator
    file = 'grain1_grain3_3d.e'
    save_with_name = 'grain1_grain3'
  []
  [grain2_grain4]
    type = FileMeshGenerator
    file = 'grain2_grain4_3d.e'
    save_with_name = 'grain2_grain4'
  []
  [grain3_grain4]
    type = FileMeshGenerator
    file = 'grain3_grain4_3d.e'
    save_with_name = 'grain3_grain4'
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
    generate_sbm_distance = true
    surface_meshes = ${interfaces}
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
