# Two-dimensional plane-strain counterpart of `nearly_incompressible.i` for the Kokkos
# total-Lagrangian NEML2 stack. The mesh is distorted by a bilinear map, so the quadrilaterals are
# non-affine, and the nonuniform quadratic initial displacement makes the deformation gradient vary
# across the quadrature points of every element. The companion `fbar_2d_host.i` solves the same
# problem with the host `Physics/SolidMechanics/QuasiStatic` action.
#
# All tensors stay 3x3 with F_zz = 1, so the total-mode F-bar correction is the same cube-root
# volume ratio as in three dimensions. Set `fbar=false` on the command line to disable it.
fbar = true

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 4
  []
  [distort]
    type = ParsedNodeTransformGenerator
    input = gmg
    x_function = 'x + 0.1 * x * y'
    y_function = 'y + 0.08 * x * y'
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
[]

[Functions]
  [disp_x_ic]
    type = ParsedFunction
    expression = '0.004 * x * y + 0.003 * x * x'
  []
  [disp_y_ic]
    type = ParsedFunction
    expression = '-0.003 * x * y - 0.002 * y * y'
  []
[]

[ICs]
  [disp_x]
    type = FunctionIC
    variable = disp_x
    function = disp_x_ic
  []
  [disp_y]
    type = FunctionIC
    variable = disp_y
    function = disp_y_ic
  []
[]

[BCs]
  [xfix]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [yfix]
    type = DirichletBC
    variable = disp_y
    boundary = left
    value = 0
  []
  # Small uniform shear along y on the right face
  [ypull]
    type = FunctionDirichletBC
    variable = disp_y
    boundary = right
    function = t
    preset = false
  []
[]

[NEML2]
  eager = true
  input = 'nearly_incompressible_neml2.i'
  [all]
    executor_name = neml2
    model = model
    device = cpu
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
    displacements = 'disp_x disp_y'
    stabilize_strain = ${fbar}
  []
[]

[Kernels]
  [disp_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_x
    component = 0
    displacements = 'disp_x disp_y'
    stabilize_strain = ${fbar}
  []
  [disp_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_y
    component = 1
    displacements = 'disp_x disp_y'
    stabilize_strain = ${fbar}
  []
[]

[Materials]
  [f_bar_average]
    type = KokkosComputeFbarAverage
    displacements = 'disp_x disp_y'
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
    displacements = 'disp_x disp_y'
    custom_pk2_stress = neml2_stress
    custom_pk2_jacobian = dneml2_stress_ddeformation_gradient
    stabilize_strain = ${fbar}
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
  line_search = none
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  automatic_scaling = true
  dt = 5e-3
  num_steps = 2
  residual_and_jacobian_together = true
[]

[Postprocessors]
  [uy_tip]
    type = PointValue
    variable = disp_y
    point = '1.05 0.54 0'
  []
  [ux_tip]
    type = PointValue
    variable = disp_x
    point = '1.05 0.54 0'
  []
  [ux_interior]
    type = PointValue
    variable = disp_x
    point = '0.4 0.6 0'
  []
  [uy_interior]
    type = PointValue
    variable = disp_y
    point = '0.4 0.6 0'
  []
[]

[Outputs]
  csv = true
  file_base = fbar_2d_out
[]
