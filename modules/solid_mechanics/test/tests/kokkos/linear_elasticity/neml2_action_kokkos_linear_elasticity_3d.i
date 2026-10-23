# 3D linear elasticity driven through the NEML2 Kokkos Action input/output backend.
#
# Same mesh, variables, boundary conditions, and loading ramp as
# neml2_kokkos_linear_elasticity_3d.i (the Torch-input 3D fixture), but the small
# strain is gathered into NEML2 through the Kokkos input backend and the stress and
# tangent are returned through the Kokkos output backend, exercising the Action's
# six-component Mandel packing on a fully 3D stress state. The displacement solution
# is identical to the Torch 3D path (neml2_kokkos_linear_elasticity_3d.i), so this
# test's own gold file holds that same solution.
[Mesh]
  [generated]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 2
    ny = 2
    nz = 2
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[NEML2]
  eager = true
  input = 'elasticity_neml2.i'
  [all]
    executor_name = neml2
    model = model
    input_backend = kokkos
    output_backend = kokkos
    derivatives = 'neml2_stress neml2_strain Jacobian_mult'
  []
[]

[Materials]
  [strain]
    type = KokkosComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
    strain = neml2_strain
  []
[]

[Kernels]
  [stress_x]
    type = KokkosStressDivergence
    variable = disp_x
    component = 0
    displacements = 'disp_x disp_y disp_z'
    stress = neml2_stress
  []
  [stress_y]
    type = KokkosStressDivergence
    variable = disp_y
    component = 1
    displacements = 'disp_x disp_y disp_z'
    stress = neml2_stress
  []
  [stress_z]
    type = KokkosStressDivergence
    variable = disp_z
    component = 2
    displacements = 'disp_x disp_y disp_z'
    stress = neml2_stress
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
    value = 0
  []
  [disp_y]
    type = KokkosDirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [disp_z]
    type = KokkosDirichletBC
    variable = disp_z
    boundary = back
    value = 0
  []
[]

[Functions]
  [loading]
    type = ParsedFunction
    expression = t
  []
[]

[Controls]
  [loading]
    type = RealFunctionControl
    parameter = 'BCs/disp_x_right/value'
    function = loading
    execute_on = 'INITIAL TIMESTEP_BEGIN'
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
  solve_type = NEWTON
  dt = 0.001
  num_steps = 5
  nl_abs_tol = 1e-12
[]

[Outputs]
  exodus = true
  file_base = neml2_action_kokkos_linear_elasticity_3d_out
[]
