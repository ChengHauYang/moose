# Kokkos version of lagrangian/cartesian/total/homogenization/large-tests/1d.i (large kinematics with the St. Venant-Kirchhoff model).
# The host reference replaces ComputeLagrangianLinearElasticStress by
# ComputeStVenantKirchhoffStress. The postprocessors reproduce the columns of the host CSV output.

STABILIZE = false

[Mesh]
  [base]
    type = FileMeshGenerator
    file = '../../lagrangian/cartesian/total/homogenization/large-tests/1d.exo'
  []
  [ss]
    type = SideSetsFromPointsGenerator
    input = base
    points = '-1 0 0
               7 0 0'
    new_boundary = 'left right'
  []
[]

[Variables]
  [disp_x]
  []
  [hvar]
    family = SCALAR
    order = FIRST
  []
[]

[Kernels]
  [sdx]
    type = KokkosHomogenizedTotalLagrangianStressDivergence
    variable = disp_x
    component = 0
    displacements = 'disp_x'
    large_kinematics = true
    stabilize_strain = ${STABILIZE}
    macro_var = hvar
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
[]

[Functions]
  [func_stress]
    type = ParsedFunction
    expression = '400*t'
  []
  [func_strain]
    type = ParsedFunction
    expression = '4.0e-1*t'
  []
[]

[BCs]
  [Periodic]
    [all]
      variable = disp_x
      auto_direction = 'x'
    []
  []

  [centerfix_x]
    type = KokkosDirichletBC
    boundary = "fixme"
    variable = disp_x
    value = 0
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
    displacements = 'disp_x'
    large_kinematics = true
    stabilize_strain = ${STABILIZE}
    homogenization_gradient_names = homogenization_gradient
  []
  [elastic_1]
    type = KokkosComputeStVenantKirchhoffStress
    youngs_modulus = 100000.0
    poissons_ratio = 0.3
    block = 1
  []
  [elastic_2]
    type = KokkosComputeStVenantKirchhoffStress
    youngs_modulus = 120000.0
    poissons_ratio = 0.21
    block = 2
  []
  [elastic_3]
    type = KokkosComputeStVenantKirchhoffStress
    youngs_modulus = 80000.0
    poissons_ratio = 0.4
    block = 3
  []
  [elastic_4]
    type = KokkosComputeStVenantKirchhoffStress
    youngs_modulus = 76000.0
    poissons_ratio = 0.11
    block = 4
  []
  [fbar_average]
    type = KokkosComputeFbarAverage
    displacements = 'disp_x'
  []
  [compute_stress]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'disp_x'
    custom_pk2_stress = pk2_stress
    custom_pk2_jacobian = dpk2_dF
    stabilize_strain = ${STABILIZE}
    homogenization_gradient_names = homogenization_gradient
  []

  # Components for the averages; the mechanical strain is sym(F) - I
  [prop_s11]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_s11
    index_i = 0
    index_j = 0
  []
  [prop_s21]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_s21
    index_i = 1
    index_j = 0
  []
  [prop_s31]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_s31
    index_i = 2
    index_j = 0
  []
  [prop_s12]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_s12
    index_i = 0
    index_j = 1
  []
  [prop_s22]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_s22
    index_i = 1
    index_j = 1
  []
  [prop_s32]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_s32
    index_i = 2
    index_j = 1
  []
  [prop_s13]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_s13
    index_i = 0
    index_j = 2
  []
  [prop_s23]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_s23
    index_i = 1
    index_j = 2
  []
  [prop_s33]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_s33
    index_i = 2
    index_j = 2
  []
  [prop_F11]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = prop_F11
    index_i = 0
    index_j = 0
  []
  [prop_F21]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = prop_F21
    index_i = 1
    index_j = 0
  []
  [prop_F31]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = prop_F31
    index_i = 2
    index_j = 0
  []
  [prop_F12]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = prop_F12
    index_i = 0
    index_j = 1
  []
  [prop_F22]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = prop_F22
    index_i = 1
    index_j = 1
  []
  [prop_F32]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = prop_F32
    index_i = 2
    index_j = 1
  []
  [prop_F13]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = prop_F13
    index_i = 0
    index_j = 2
  []
  [prop_F23]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = prop_F23
    index_i = 1
    index_j = 2
  []
  [prop_F33]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = prop_F33
    index_i = 2
    index_j = 2
  []
[]

[Postprocessors]
  [s11]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_s11
    execute_on = 'initial timestep_end'
  []
  [s21]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_s21
    execute_on = 'initial timestep_end'
  []
  [s31]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_s31
    execute_on = 'initial timestep_end'
  []
  [s12]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_s12
    execute_on = 'initial timestep_end'
  []
  [s22]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_s22
    execute_on = 'initial timestep_end'
  []
  [s32]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_s32
    execute_on = 'initial timestep_end'
  []
  [s13]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_s13
    execute_on = 'initial timestep_end'
  []
  [s23]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_s23
    execute_on = 'initial timestep_end'
  []
  [s33]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_s33
    execute_on = 'initial timestep_end'
  []
  [F11]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_F11
    # The host averages an aux variable that is still zero at the initial
    # execution, so this postprocessor skips it to report the same value
    execute_on = 'timestep_end'
  []
  [F21]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_F21
    # The host averages an aux variable that is still zero at the initial
    # execution, so this postprocessor skips it to report the same value
    execute_on = 'timestep_end'
  []
  [F31]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_F31
    # The host averages an aux variable that is still zero at the initial
    # execution, so this postprocessor skips it to report the same value
    execute_on = 'timestep_end'
  []
  [F12]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_F12
    # The host averages an aux variable that is still zero at the initial
    # execution, so this postprocessor skips it to report the same value
    execute_on = 'timestep_end'
  []
  [F22]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_F22
    # The host averages an aux variable that is still zero at the initial
    # execution, so this postprocessor skips it to report the same value
    execute_on = 'timestep_end'
  []
  [F32]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_F32
    # The host averages an aux variable that is still zero at the initial
    # execution, so this postprocessor skips it to report the same value
    execute_on = 'timestep_end'
  []
  [F13]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_F13
    # The host averages an aux variable that is still zero at the initial
    # execution, so this postprocessor skips it to report the same value
    execute_on = 'timestep_end'
  []
  [F23]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_F23
    # The host averages an aux variable that is still zero at the initial
    # execution, so this postprocessor skips it to report the same value
    execute_on = 'timestep_end'
  []
  [F33]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_F33
    # The host averages an aux variable that is still zero at the initial
    # execution, so this postprocessor skips it to report the same value
    execute_on = 'timestep_end'
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

  solve_type = 'newton'
  line_search = default

  automatic_scaling = true

  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'

  l_max_its = 2
  l_tol = 1e-14
  nl_max_its = 15
  nl_rel_tol = 1e-6
  nl_abs_tol = 1e-8

  start_time = 0.0
  dt = 0.2
  dtmin = 0.2
  end_time = 1.0
[]

[Outputs]
  exodus = false
  csv = true
[]
