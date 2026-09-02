[Mesh]
  [generated]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 4
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
[]

[Kernels]
  [stress_x]
    type = KokkosStressDivergence
    variable = disp_x
    component = 0
    displacements = 'disp_x disp_y'
  []
  [stress_y]
    type = KokkosStressDivergence
    variable = disp_y
    component = 1
    displacements = 'disp_x disp_y'
  []
[]

[Materials]
  [elasticity]
    type = KokkosComputeIsotropicElasticity
    displacements = 'disp_x disp_y'
    youngs_modulus = 10
    poissons_ratio = 0.25
  []
[]

[BCs]
  [disp_x_left]
    type = KokkosDirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [disp_x_right]
    type = KokkosDirichletBC
    variable = disp_x
    boundary = right
    value = 0.1
  []
  [disp_y]
    type = KokkosDirichletBC
    variable = disp_y
    boundary = 'top bottom'
    value = 0
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

# Same problem as kokkos_material_linear_elasticity.i but with an explicit
# GPU-compatible linear solver (Jacobi-preconditioned CG). The default SMP
# preconditioner selects ILU in serial, whose triangular solve segfaults on
# aijkokkos matrices (needs VecGetKokkosView on CPU-side vectors).
#
# For GPU runs, also set the env var:
#   PETSC_OPTIONS="-use_gpu_aware_mpi 0 -vec_type kokkos -mat_type aijkokkos"
# so that libmesh's VecCreate + VecSetFromOptions and PetscMatrix's
# MatSetFromOptions pick up the Kokkos types. The -nl0_ prefix cannot be
# used for -mat_type / -vec_type via petsc_options_iname because MOOSE
# rejects that path with a solver-system-prefix check.
[Executioner]
  type = Steady
  solve_type = NEWTON
  nl_abs_tol = 1e-12
  petsc_options_iname = '-pc_type -ksp_type -ksp_max_it -ksp_rtol'
  petsc_options_value = 'jacobi   cg        5000        1e-8'
[]

[Outputs]
  exodus = true
  file_base = kokkos_linear_elasticity_gpu_ksp_out
[]
