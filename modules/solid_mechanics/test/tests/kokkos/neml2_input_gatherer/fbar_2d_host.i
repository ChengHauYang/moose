# Host reference for `fbar_2d.i`: the same plane-strain problem through the host
# `Physics/SolidMechanics/QuasiStatic` action, whose `volumetric_locking_correction = true` selects
# the default total-mode F-bar correction. Set `fbar=false` on the command line to disable it.
fbar = true

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

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

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = FINITE
        formulation = TOTAL
        new_system = true
        add_variables = true
        volumetric_locking_correction = ${fbar}
      []
    []
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

[NEML2]
  eager = true
  input = 'nearly_incompressible_neml2.i'
  output_device = cpu
  [all]
    model = model
    device = cpu
    derivatives = 'neml2_stress deformation_gradient'
  []
[]

[Materials]
  [stress]
    type = ComputeLagrangianStressCustomPK2
    custom_pk2_stress = neml2_stress
    custom_pk2_jacobian = 'dneml2_stress/ddeformation_gradient'
    large_kinematics = true
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
  [ypull]
    type = FunctionDirichletBC
    variable = disp_y
    boundary = right
    function = t
    preset = false
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
