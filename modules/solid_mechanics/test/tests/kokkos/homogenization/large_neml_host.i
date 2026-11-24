# Host reference for large_neml.i: lagrangian/cartesian/total/homogenization/neml2/large_neml.i
# with the St. Venant-Kirchhoff model in place of the NEML2 hypoelastic stress update.

constraint_types = 'strain strain strain stress stress stress stress strain stress'
targets = 'strain11 zero zero zero zero zero zero zero zero'

[Mesh]
  [base]
    type = FileMeshGenerator
    file = '../../lagrangian/cartesian/total/homogenization/neml2/3d.exo'
  []

  [sidesets]
    type = SideSetsFromNormalsGenerator
    input = base
    normals = '-1 0 0
                1 0 0
                0 -1 0
                0 1 0
                0 0 -1
                0 0 1'
    fixed_normal = true
    new_boundary = 'left right bottom top back front'
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
  [hvar]
    family = SCALAR
    order = NINTH
  []
[]

[BCs]
  [fix1_x]
    type = DirichletBC
    boundary = fix_all
    variable = disp_x
    value = 0
  []
  [fix1_y]
    type = DirichletBC
    boundary = fix_all
    variable = disp_y
    value = 0
  []
  [fix1_z]
    type = DirichletBC
    boundary = fix_all
    variable = disp_z
    value = 0
  []
  [fix2_x]
    type = DirichletBC
    boundary = fix_xy
    variable = disp_x
    value = 0
  []
  [fix2_y]
    type = DirichletBC
    boundary = fix_xy
    variable = disp_y
    value = 0
  []
  [fix3_z]
    type = DirichletBC
    boundary = fix_z
    variable = disp_z
    value = 0
  []

  [Periodic]
    [x]
      variable = disp_x
      auto_direction = 'x y z'
    []
    [y]
      variable = disp_y
      auto_direction = 'x y z'
    []
    [z]
      variable = disp_z
      auto_direction = 'x y z'
    []
  []
[]

[Functions]
  [strain11]
    type = ParsedFunction
    expression = 't'
  []

  [zero]
    type = ConstantFunction
    value = 0
  []
[]

[Kernels]
  [TM_all0]
    type = HomogenizedTotalLagrangianStressDivergence
    large_kinematics = true
    macro_var = hvar
    component = 0
    displacements = 'disp_x disp_y disp_z'
    variable = disp_x
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
  [TM_all1]
    type = HomogenizedTotalLagrangianStressDivergence
    large_kinematics = true
    macro_var = hvar
    component = 1
    displacements = 'disp_x disp_y disp_z'
    variable = disp_y
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
  [TM_all2]
    type = HomogenizedTotalLagrangianStressDivergence
    large_kinematics = true
    macro_var = hvar
    component = 2
    displacements = 'disp_x disp_y disp_z'
    variable = disp_z
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
[]

[Materials]
  [compute_homogenization_gradient]
    type = ComputeHomogenizedLagrangianStrain
    macro_gradient = hvar
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
  [compute_strain]
    type = ComputeLagrangianStrain
    displacements = 'disp_x disp_y disp_z'
    large_kinematics = true
    homogenization_gradient_names = homogenization_gradient
  []
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 100000
    poissons_ratio = 0.3
  []
  [stress]
    type = ComputeStVenantKirchhoffStress
  []
  [mCS_xx]
    type = RankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_xx
    index_i = 0
    index_j = 0
  []
  [mCS_xy]
    type = RankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_xy
    index_i = 0
    index_j = 1
  []
  [mCS_xz]
    type = RankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_xz
    index_i = 0
    index_j = 2
  []
  [mCS_yx]
    type = RankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_yx
    index_i = 1
    index_j = 0
  []
  [mCS_yy]
    type = RankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_yy
    index_i = 1
    index_j = 1
  []
  [mCS_yz]
    type = RankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_yz
    index_i = 1
    index_j = 2
  []
  [mCS_zx]
    type = RankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_zx
    index_i = 2
    index_j = 0
  []
  [mCS_zy]
    type = RankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_zy
    index_i = 2
    index_j = 1
  []
  [mCS_zz]
    type = RankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_zz
    index_i = 2
    index_j = 2
  []
[]

[Postprocessors]
  [mCS_xx]
    type = ElementAverageMaterialProperty
    mat_prop = mCS_xx
  []
  [mCS_xy]
    type = ElementAverageMaterialProperty
    mat_prop = mCS_xy
  []
  [mCS_xz]
    type = ElementAverageMaterialProperty
    mat_prop = mCS_xz
  []
  [mCS_yx]
    type = ElementAverageMaterialProperty
    mat_prop = mCS_yx
  []
  [mCS_yy]
    type = ElementAverageMaterialProperty
    mat_prop = mCS_yy
  []
  [mCS_yz]
    type = ElementAverageMaterialProperty
    mat_prop = mCS_yz
  []
  [mCS_zx]
    type = ElementAverageMaterialProperty
    mat_prop = mCS_zx
  []
  [mCS_zy]
    type = ElementAverageMaterialProperty
    mat_prop = mCS_zy
  []
  [mCS_zz]
    type = ElementAverageMaterialProperty
    mat_prop = mCS_zz
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Transient

  residual_and_jacobian_together = true

  solve_type = 'newton'
  line_search = 'none'

  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'

  l_max_its = 2
  l_tol = 1e-14
  nl_max_its = 20
  nl_rel_tol = 1e-8
  nl_abs_tol = 1e-10

  start_time = 0.0
  dt = 0.2
  dtmin = 0.2
  end_time = 1.0
[]

[Outputs]
  file_base = large_neml_out
  exodus = true
[]
