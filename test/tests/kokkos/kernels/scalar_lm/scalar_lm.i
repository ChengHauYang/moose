# Pure Neumann diffusion problem made unique by constraining the integral of u with a Lagrange
# multiplier scalar variable. The same input runs with host objects or with their Kokkos versions
# (KOKKOS = Kokkos), and both compare against the same gold file.
KOKKOS = ''

[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 4
  ny = 3
  xmax = 2
[]

[Variables]
  [u]
  []
  [lambda]
    family = SCALAR
    order = FIRST
  []
[]

[Kernels]
  [diff]
    type = ${KOKKOS}Diffusion
    variable = u
  []
  [sk_lm]
    type = ${KOKKOS}ScalarLMKernel
    variable = u
    kappa = lambda
    pp_name = volume
    value = 3
  []
[]

[BCs]
  [flux]
    type = ${KOKKOS}NeumannBC
    variable = u
    boundary = left
    value = 1
  []
[]

[Problem]
  # The kernel coverage check cannot look up the subdomains of the scalar variable that the
  # ScalarLMKernel rows cover (as in kernels/scalar_kernel_constraint)
  kernel_coverage_check = false
[]

[Postprocessors]
  [volume]
    type = FunctionElementIntegral
    function = 1
    execute_on = initial
  []
[]

[Preconditioning]
  [pc]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_shift_type'
  petsc_options_value = 'lu       NONZERO'
  nl_rel_tol = 1e-10
[]

[Outputs]
  file_base = scalar_lm_out
  exodus = true
[]
