# Kokkos version of lagrangian/cartesian/total/homogenization/small-tests/1d.i (small kinematics).
# The postprocessors reproduce the columns of the host CSV output.

STABILIZE = false

[Mesh]
  [base]
    type = FileMeshGenerator
    file = '../../lagrangian/cartesian/total/homogenization/small-tests/1d.exo'
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
    large_kinematics = false
    stabilize_strain = ${STABILIZE}
    macro_var = hvar
    constraint_types = ${constraint_types}
    targets = ${targets}
  []
[]

[Functions]
  [func_stress]
    type = ParsedFunction
    expression = '1800*t'
  []
  [func_strain]
    type = ParsedFunction
    expression = '1.0e-2*t'
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
  [elastic_4]
    type = KokkosComputeIsotropicLinearElasticStress
    youngs_modulus = 76000.0
    poissons_ratio = 0.11
    block = 4
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
  [F11]
    type = KokkosRankTwoCartesianComponent
    rank_two_tensor = deformation_gradient
    property_name = F11
    index_i = 0
    index_j = 0
  []
[]

[Postprocessors]
  [sxx]
    type = KokkosElementAverageMaterialProperty
    mat_prop = prop_sxx
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
