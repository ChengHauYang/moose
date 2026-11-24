# Kokkos version of lagrangian/cartesian/total/homogenization/action/action_2d.i: the
# QuasiStatic Physics with use_kokkos = true adds the Kokkos kernels, kinematics, and macro
# gradient objects, compared against the host solution.

[Mesh]
  [base]
    type = FileMeshGenerator
    file = '../../lagrangian/cartesian/total/homogenization/action/2d.exo'
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

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      displacements = 'disp_x disp_y'
      [all]
        displacements = 'disp_x disp_y'
        strain = SMALL
        add_variables = true
        new_system = true
        formulation = TOTAL
        use_kokkos = true
        constraint_types = 'stress none none stress strain none none none none'
        targets = 'stress11 stress12 strain22'
      []
    []
  []
[]

[Functions]
  [stress11]
    type = ParsedFunction
    expression = '400*t'
  []
  [strain22]
    type = ParsedFunction
    expression = '-2.0e-2*t'
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
    boundary = fix1
    variable = disp_x
    value = 0
  []
  [fix1_y]
    type = KokkosDirichletBC
    boundary = fix1
    variable = disp_y
    value = 0
  []
  [fix2_y]
    type = KokkosDirichletBC
    boundary = fix2
    variable = disp_y
    value = 0
  []
[]

[Materials]
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
  [out]
    type = Exodus
    file_base = '2d'
  []
[]
