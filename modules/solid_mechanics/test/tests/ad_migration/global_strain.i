# AD global strain: scalar residual derivatives must reach the displacement residuals.
[Mesh]
  [generated_mesh]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 4
    xmin = -0.5
    xmax = 0.5
    ymin = -0.5
    ymax = 0.5
  []
  [cnode]
    type = ExtraNodesetGenerator
    coord = '0 0'
    new_boundary = 100
    input = generated_mesh
  []
[]

[Variables]
  [global_strain]
    order = THIRD
    family = SCALAR
    initial_condition = 0.01
  []
[]

[ICs]
  [u_x]
    type = FunctionIC
    variable = u_x
    function = '0.03*x*y'
  []
  [u_y]
    type = FunctionIC
    variable = u_y
    function = '-0.02*x*y'
  []
[]

[GlobalParams]
  displacements = 'u_x u_y'
  block = 0
[]

[Physics/SolidMechanics/QuasiStatic]
  [all]
    add_variables = true
    strain = SMALL
    incremental = false
    use_automatic_differentiation = true
  []
[]

[Physics/SolidMechanics/GlobalStrain]
  [global_strain]
    scalar_global_strain = global_strain
    displacements = 'u_x u_y'
    auxiliary_displacements = 'disp_x disp_y'
    global_displacements = ''
    use_automatic_differentiation = true
    applied_stress_tensor = '0.1 0.2 0 0 0 -0.2'
  []
[]

[BCs]
  [Periodic]
    [left-right]
      auto_direction = 'x y'
      variable = 'u_x u_y'
    []
  []

  [centerfix_x]
    type = DirichletBC
    boundary = 100
    variable = u_x
    value = 0
  []
  [centerfix_y]
    type = DirichletBC
    boundary = 100
    variable = u_y
    value = 0
  []
[]

[Materials]
  [elasticity_tensor]
    type = ADComputeIsotropicElasticityTensor
    youngs_modulus = 10
    poissons_ratio = 0.25
  []
  [stress]
    type = ADComputeLinearElasticStress
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  checkpoint = false
[]
