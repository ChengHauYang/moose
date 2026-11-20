# 3D counterpart of the grain-boundary SCZM cases: a tetrahedral cube split into
# four grains (2x2 in the x-y plane), with shifted cohesive zones on the four
# internal grain interfaces. Case (a): every grain's complete watertight boundary
# is extracted and combined into one saved boundary mesh (all grain subdomains in
# a single mesh), then fed to the interface manager via complete_interface_mesh.
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

  # Each grain's complete watertight boundary becomes a distinct subdomain in the
  # saved boundary mesh. One combined mesh holds all four grain shells.
  [g1_ss]
    type = SideSetsAroundSubdomainGenerator
    input = grain4
    block = 1
    new_boundary = 's1'
  []
  [g1_ld]
    type = LowerDBlockFromSidesetGenerator
    input = g1_ss
    sidesets = 's1'
    new_block_id = 11
  []
  [g1_s]
    type = BlockToMeshConverterGenerator
    input = g1_ld
    target_blocks = '11'
  []
  [g1_id]
    type = SubdomainIDGenerator
    input = g1_s
    subdomain_id = 1
  []
  [g2_ss]
    type = SideSetsAroundSubdomainGenerator
    input = grain4
    block = 2
    new_boundary = 's2'
  []
  [g2_ld]
    type = LowerDBlockFromSidesetGenerator
    input = g2_ss
    sidesets = 's2'
    new_block_id = 12
  []
  [g2_s]
    type = BlockToMeshConverterGenerator
    input = g2_ld
    target_blocks = '12'
  []
  [g2_id]
    type = SubdomainIDGenerator
    input = g2_s
    subdomain_id = 2
  []
  [g3_ss]
    type = SideSetsAroundSubdomainGenerator
    input = grain4
    block = 3
    new_boundary = 's3'
  []
  [g3_ld]
    type = LowerDBlockFromSidesetGenerator
    input = g3_ss
    sidesets = 's3'
    new_block_id = 13
  []
  [g3_s]
    type = BlockToMeshConverterGenerator
    input = g3_ld
    target_blocks = '13'
  []
  [g3_id]
    type = SubdomainIDGenerator
    input = g3_s
    subdomain_id = 3
  []
  [g4_ss]
    type = SideSetsAroundSubdomainGenerator
    input = grain4
    block = 4
    new_boundary = 's4'
  []
  [g4_ld]
    type = LowerDBlockFromSidesetGenerator
    input = g4_ss
    sidesets = 's4'
    new_block_id = 14
  []
  [g4_s]
    type = BlockToMeshConverterGenerator
    input = g4_ld
    target_blocks = '14'
  []
  [g4_id]
    type = SubdomainIDGenerator
    input = g4_s
    subdomain_id = 4
  []
  [boundary_mesh]
    type = CombinerGenerator
    inputs = 'g1_id g2_id g3_id g4_id'
    save_with_name = boundary_mesh
  []

  # The bulk grains already carry subdomain IDs 1-4 from the bounding boxes; break
  # the mesh along block interfaces to create the surrogate SCZM boundaries.
  [break]
    type = BreakMeshByBlockGenerator
    input = grain4
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
