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
      function = '0.2 + 0.3*x + 0.1*y*z'
    []
  []
  [eta2]
    [InitialCondition]
      type = FunctionIC
      function = '0.4 + 0.2*y + 0.15*x*z'
    []
  []
[]

[Kernels]
  [x]
    type = ADStressDivergenceTensors
    variable = disp_x
    component = 0
  []
  [y]
    type = ADStressDivergenceTensors
    variable = disp_y
    component = 1
  []
  [z]
    type = ADStressDivergenceTensors
    variable = disp_z
    component = 2
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
  [prefactor1]
    type = ADParsedMaterial
    property_name = prefactor1
    coupled_variables = eta1
    expression = '1 + eta1^2'
  []
  [prefactor2]
    type = ADParsedMaterial
    property_name = prefactor2
    coupled_variables = eta2
    expression = '2 + eta2^3'
  []
  [eigenstrain1]
    type = ADComputeEigenstrain
    eigen_base = '0.1 0 0 0 0.1 0 0 0 0.1'
    prefactor = prefactor1
    eigenstrain_name = increment1
  []
  [eigenstrain2]
    type = ADComputeEigenstrain
    eigen_base = '0.2 0 0 0 0.05 0 0 0 0.15'
    prefactor = prefactor2
    eigenstrain_name = increment2
  []
  [sum_tensor]
    type = ADSumTensorIncrements
    tensor_name = stress
    coupled_tensor_increment_names = 'increment1 increment2'
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