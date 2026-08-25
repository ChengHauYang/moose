[Mesh]
  [generated]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 4
  []
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
[]

[Kernels]
  [stress_x]
    type = StressDivergenceTensors
    variable = disp_x
    component = 0
  []
  [stress_y]
    type = StressDivergenceTensors
    variable = disp_y
    component = 1
  []
[]

[Materials]
  [strain]
    type = ComputeSmallStrain
  []
  [elasticity]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 10
    poissons_ratio = 0.25
  []
  [stress]
    type = ComputeLinearElasticStress
  []
[]

[BCs]
  [disp_x_left]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [disp_x_right]
    type = DirichletBC
    variable = disp_x
    boundary = right
    value = 0.1
  []
  [disp_y]
    type = DirichletBC
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

[Executioner]
  type = Steady
  solve_type = NEWTON
  nl_abs_tol = 1e-12
[]

[Outputs]
  exodus = false
[]
