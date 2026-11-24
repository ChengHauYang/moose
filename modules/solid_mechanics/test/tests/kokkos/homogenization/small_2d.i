# Kokkos version of lagrangian/cartesian/total/homogenization/small-tests/2d.i (small kinematics).
# The postprocessors reproduce the columns of the host CSV output.

STABILIZE = false

[Mesh]
  [base]
    type = FileMeshGenerator
    file = '../../lagrangian/cartesian/total/homogenization/small-tests/2d.exo'
  []

  [sidesets]
    type = SideSetsFromNormalsGenerator
    input = base
    normals = '-1 0 0
                1 0 0
                0 -1 0
                0 1 0'
    fixed_normal = true
    new_boundary = 'left right bottom top'
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [hvar]
    family = SCALAR
    order = THIRD
  []
[]

[Kernels]
  [sdx]
    type = KokkosHomogenizedTotalLagrangianStressDivergence
    variable = disp_x
    component = 0
    displacements = 'disp_x disp_y'
    large_kinematics = false
    stabilize_strain = ${STABILIZE}
    macro_var = hvar
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
  [sdy]
    type = KokkosHomogenizedTotalLagrangianStressDivergence
    variable = disp_y
    component = 1
    displacements = 'disp_x disp_y'
    large_kinematics = false
    stabilize_strain = ${STABILIZE}
    macro_var = hvar
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
[]

[Functions]
  [strain11]
    type = ParsedFunction
    expression = '4.0e-2*t'
  []
  [strain22]
    type = ParsedFunction
    expression = '-2.0e-2*t'
  []
  [strain12]
    type = ParsedFunction
    expression = '1.0e-2*t'
  []
  [stress11]
    type = ParsedFunction
    expression = '400*t'
  []
  [stress22]
    type = ParsedFunction
    expression = '-200*t'
  []
  [stress12]
    type = ParsedFunction
    expression = '100*t'
  []
[]

[BCs]
  [Periodic]
    [x]
      variable = disp_x
      auto_direction = 'x y'
    []
    [y]
      variable = disp_y
      auto_direction = 'x y'
    []
  []

  [fix1_x]
    type = KokkosDirichletBC
    boundary = "fix1"
    variable = disp_x
    value = 0
  []
  [fix1_y]
    type = KokkosDirichletBC
    boundary = "fix1"
    variable = disp_y
    value = 0
  []

  [fix2_y]
    type = KokkosDirichletBC
    boundary = "fix2"
    variable = disp_y
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
    displacements = 'disp_x disp_y'
    large_kinematics = false
    stabilize_strain = ${STABILIZE}
    homogenization_gradient_names = homogenization_gradient
  []
  [elastic_1]
    type = KokkosComputeIsotropicLinearElasticStress
    youngs_modulus = 100000.0
    poissons_ratio = 0.3
    block = 1
  []
  [elastic_2]
    type = KokkosComputeIsotropicLinearElasticStress
    youngs_modulus = 120000.0
    poissons_ratio = 0.21
    block = 2
  []
  [elastic_3]
    type = KokkosComputeIsotropicLinearElasticStress
    youngs_modulus = 80000.0
    poissons_ratio = 0.4
    block = 3
  []
  [compute_stress]
    type = KokkosComputeLagrangianStressCustomSmall
    custom_small_stress = stress
    custom_small_jacobian = Jacobian_mult
    stabilize_strain = ${STABILIZE}
  []

  # Components for the averages; the mechanical strain is sym(F) - I
  [prop_sxx]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_sxx
    index_i = 0
    index_j = 0
  []
  [prop_syy]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_syy
    index_i = 1
    index_j = 1
  []
  [prop_sxy]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = pk1_stress
    property_name = prop_sxy
    index_i = 0
    index_j = 1
  []
  [F11]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = F11
    index_i = 0
    index_j = 0
  []
  [F22]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = F22
    index_i = 1
    index_j = 1
  []
  [F12]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = F12
    index_i = 0
    index_j = 1
  []
  [F21]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = F21
    index_i = 1
    index_j = 0
  []
[]

[Postprocessors]
  [sxx]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_sxx
    execute_on = 'initial timestep_end'
  []
  [syy]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_syy
    execute_on = 'initial timestep_end'
  []
  [sxy]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_sxy
    execute_on = 'initial timestep_end'
  []
  [F11]
    type = KokkosElementAverageMaterialProperty
    mat_prop = F11
    execute_on = 'initial timestep_end'
    outputs = none
  []
  [exx]
    type = ParsedPostprocessor
    expression = 'F11 - 1'
    pp_names = 'F11'
    execute_on = 'initial timestep_end'
  []
  [F22]
    type = KokkosElementAverageMaterialProperty
    mat_prop = F22
    execute_on = 'initial timestep_end'
    outputs = none
  []
  [eyy]
    type = ParsedPostprocessor
    expression = 'F22 - 1'
    pp_names = 'F22'
    execute_on = 'initial timestep_end'
  []
  [F12]
    type = KokkosElementAverageMaterialProperty
    mat_prop = F12
    execute_on = 'initial timestep_end'
    outputs = none
  []
  [F21]
    type = KokkosElementAverageMaterialProperty
    mat_prop = F21
    execute_on = 'initial timestep_end'
    outputs = none
  []
  [exy]
    type = ParsedPostprocessor
    expression = '(F12 + F21) / 2'
    pp_names = 'F12 F21'
    execute_on = 'initial timestep_end'
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
  line_search = none

  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'

  l_max_its = 2
  l_tol = 1e-14
  nl_max_its = 30
  nl_rel_tol = 1e-8
  nl_abs_tol = 1e-10

  start_time = 0.0
  dt = 0.2
  dtmin = 0.2
  end_time = 1.0
[]

[Outputs]
  csv = true
[]
