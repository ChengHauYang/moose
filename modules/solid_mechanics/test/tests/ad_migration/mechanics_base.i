# Nonuniform fields keep displacement and order-parameter couplings active.
[Mesh]
  type = GeneratedMesh
  dim = 3
  nx = 2
[]

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
[]

[Variables]
  [disp_x]
    [InitialCondition]
      type = FunctionIC
      function = '0.1*x + 0.2*y*z'
    []
  []
  [disp_y]
    [InitialCondition]
      type = FunctionIC
      function = '0.2*y + 0.1*x*z'
    []
  []
  [disp_z]
    [InitialCondition]
      type = FunctionIC
      function = '0.3*z + 0.2*x*y'
    []
  []
  [eta1]
    [InitialCondition]
      type = FunctionIC
      function = '0.3 + x + 0.2*y + 0.1*z + x*y'
    []
  []
  [eta2]
    [InitialCondition]
      type = FunctionIC
      function = '0.4 + 0.2*x + y + 0.3*z + y*z'
    []
  []
[]

[Kernels]
  [x]
    type = ADStressDivergenceTensors
    variable = disp_x
    component = 0
    base_name = elastic
  []
  [y]
    type = ADStressDivergenceTensors
    variable = disp_y
    component = 1
    base_name = elastic
  []
  [z]
    type = ADStressDivergenceTensors
    variable = disp_z
    component = 2
    base_name = elastic
  []
  [eta1]
    type = ADReaction
    variable = eta1
  []
  [eta2]
    type = ADReaction
    variable = eta2
  []
[]

[Materials]
  [strain]
    type = ADComputeSmallStrain
    base_name = elastic
  []
  [elasticity]
    type = ADComputeIsotropicElasticityTensor
    base_name = elastic
    youngs_modulus = 10
    poissons_ratio = 0.2
  []
  [stress]
    type = ADComputeLinearElasticStress
    base_name = elastic
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
  num_steps = 1
[]
