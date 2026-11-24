[Mesh]
  [generated_mesh]
    type = GeneratedMeshGenerator
    dim = 1
    nx = 4
    xmin = -0.5
    xmax = 0.5
  []
  [cnode]
    type = ExtraNodesetGenerator
    coord = '0'
    new_boundary = 100
    input = generated_mesh
  []
[]

[Variables]
  [global_strain]
    order = FIRST
    family = SCALAR
    initial_condition = 0.01
  []
[]

[ICs]
  [u_x]
    type = FunctionIC
    variable = u_x
    function = '0.03*x'
  []
[]

[GlobalParams]
  displacements = 'u_x'
  block = 0
[]

[Physics/SolidMechanics/QuasiStatic]
  [all]
    add_variables = true
    strain = SMALL
    incremental = false
    global_strain = global_strain
    use_automatic_differentiation = true
  []
[]

[ScalarKernels]
  [global_strain]
    type = ADGlobalStrain
    variable = global_strain
    global_strain_uo = global_strain_uo
  []
[]

[BCs]
  [Periodic]
    [all]
      auto_direction = 'x'
      variable = 'u_x'
    []
  []
  [centerfix_x]
    type = DirichletBC
    boundary = 100
    variable = u_x
    value = 0
  []
[]

[Materials]
  [elasticity_tensor]
    type = ADComputeIsotropicElasticityTensor
    youngs_modulus = 10
    poissons_ratio = 0.25
  []
  [global_strain]
    type = ADComputeGlobalStrain
    scalar_global_strain = global_strain
    global_strain_uo = global_strain_uo
  []
  [stress]
    type = ADComputeLinearElasticStress
  []
[]

[UserObjects]
  [global_strain_uo]
    type = ADGlobalStrainUserObject
    applied_stress_tensor = '0.1'
    execute_on = 'Initial Linear Nonlinear'
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
