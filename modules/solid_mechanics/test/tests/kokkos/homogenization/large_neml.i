# Kokkos version of lagrangian/cartesian/total/homogenization/neml2/large_neml.i with a NEML2
# St. Venant-Kirchhoff model fed by TorchDeformationGradient, including the macro gradient. The
# host reference is large_neml_host.i.

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
    type = KokkosDirichletBC
    boundary = fix_all
    variable = disp_x
    value = 0
  []
  [fix1_y]
    type = KokkosDirichletBC
    boundary = fix_all
    variable = disp_y
    value = 0
  []
  [fix1_z]
    type = KokkosDirichletBC
    boundary = fix_all
    variable = disp_z
    value = 0
  []
  [fix2_x]
    type = KokkosDirichletBC
    boundary = fix_xy
    variable = disp_x
    value = 0
  []
  [fix2_y]
    type = KokkosDirichletBC
    boundary = fix_xy
    variable = disp_y
    value = 0
  []
  [fix3_z]
    type = KokkosDirichletBC
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

[NEML2]
  eager = true
  input = 'svk_neml2.i'
  [all]
    executor_name = neml2
    model = model
    input_kernels = deformation_gradient
    auto_output = false
  []
[]

[UserObjects]
  [assembly]
    type = TorchAssembly
  []
  [fe]
    type = TorchFEInterpolation
    assembly = assembly
  []
  [deformation_gradient]
    type = TorchDeformationGradient
    assembly = assembly
    fe = fe
    to_neml2 = deformation_gradient
    displacements = 'disp_x disp_y disp_z'
    macro_gradient = hvar
    constraint_types = ${constraint_types}
  []
[]

[Kernels]
  [TM_all0]
    type = KokkosHomogenizedTotalLagrangianStressDivergence
    large_kinematics = true
    macro_var = hvar
    component = 0
    displacements = 'disp_x disp_y disp_z'
    variable = disp_x
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
  [TM_all1]
    type = KokkosHomogenizedTotalLagrangianStressDivergence
    large_kinematics = true
    macro_var = hvar
    component = 1
    displacements = 'disp_x disp_y disp_z'
    variable = disp_y
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
  [TM_all2]
    type = KokkosHomogenizedTotalLagrangianStressDivergence
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
    type = KokkosComputeHomogenizedLagrangianStrain
    macro_gradient = hvar
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
  [compute_strain]
    type = KokkosComputeDeformationGradient
    displacements = 'disp_x disp_y disp_z'
    homogenization_gradient_names = homogenization_gradient
  []
  [stress]
    type = NEML2ToKokkosFullRankTwoMaterialProperty
    neml2_executor = neml2
    from_neml2 = neml2_stress
    to_moose = neml2_stress
  []
  [tangent]
    type = NEML2ToKokkosFullRankFourMaterialProperty
    neml2_executor = neml2
    from_neml2 = neml2_stress
    neml2_input_derivative = deformation_gradient
    to_moose = dneml2_stress_ddeformation_gradient
  []
  [pk1]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'disp_x disp_y disp_z'
    custom_pk2_stress = neml2_stress
    custom_pk2_jacobian = dneml2_stress_ddeformation_gradient
    homogenization_gradient_names = homogenization_gradient
  []
  [mCS_xx]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_xx
    index_i = 0
    index_j = 0
  []
  [mCS_xy]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_xy
    index_i = 0
    index_j = 1
  []
  [mCS_xz]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_xz
    index_i = 0
    index_j = 2
  []
  [mCS_yx]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_yx
    index_i = 1
    index_j = 0
  []
  [mCS_yy]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_yy
    index_i = 1
    index_j = 1
  []
  [mCS_yz]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_yz
    index_i = 1
    index_j = 2
  []
  [mCS_zx]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_zx
    index_i = 2
    index_j = 0
  []
  [mCS_zy]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_zy
    index_i = 2
    index_j = 1
  []
  [mCS_zz]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = mCS_zz
    index_i = 2
    index_j = 2
  []
[]

[Postprocessors]
  [mCS_xx]
    type = KokkosElementAverageMaterialProperty
    mat_prop = mCS_xx
  []
  [mCS_xy]
    type = KokkosElementAverageMaterialProperty
    mat_prop = mCS_xy
  []
  [mCS_xz]
    type = KokkosElementAverageMaterialProperty
    mat_prop = mCS_xz
  []
  [mCS_yx]
    type = KokkosElementAverageMaterialProperty
    mat_prop = mCS_yx
  []
  [mCS_yy]
    type = KokkosElementAverageMaterialProperty
    mat_prop = mCS_yy
  []
  [mCS_yz]
    type = KokkosElementAverageMaterialProperty
    mat_prop = mCS_yz
  []
  [mCS_zx]
    type = KokkosElementAverageMaterialProperty
    mat_prop = mCS_zx
  []
  [mCS_zy]
    type = KokkosElementAverageMaterialProperty
    mat_prop = mCS_zy
  []
  [mCS_zz]
    type = KokkosElementAverageMaterialProperty
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
  file_base = large_neml_kokkos_out
  exodus = true
[]
