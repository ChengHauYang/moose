# Independent nonlinear variables drive the composite eigenstrain and elasticity tensor weights.
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
  [eigenstrain_a]
    type = ADGenericConstantRankTwoTensor
    tensor_name = eigenstrain_a
    tensor_values = '1 0.2 0 0.2 0.5 0 0 0 0.25'
  []
  [eigenstrain_b]
    type = ADGenericConstantRankTwoTensor
    tensor_name = eigenstrain_b
    tensor_values = '0.2 0 0.1 0 0.8 0 0.1 0 0.4'
  []
  [eigenstrain_weight_a]
    type = ADParsedMaterial
    property_name = eigenstrain_weight_a
    coupled_variables = eta1
    expression = '0.02 + 0.01*eta1^2'
  []
  [eigenstrain_weight_b]
    type = ADParsedMaterial
    property_name = eigenstrain_weight_b
    coupled_variables = eta1
    expression = '0.01 + 0.02*eta1'
  []
  [composite_eigenstrain]
    type = ADCompositeEigenstrain
    base_name = elastic
    tensors = 'eigenstrain_a eigenstrain_b'
    weights = 'eigenstrain_weight_a eigenstrain_weight_b'
    coupled_variables = eta1
    eigenstrain_name = composite_eigenstrain
  []

  [elasticity_a]
    type = ADComputeIsotropicElasticityTensor
    base_name = elasticity_a
    youngs_modulus = 10
    poissons_ratio = 0.2
  []
  [elasticity_b]
    type = ADComputeIsotropicElasticityTensor
    base_name = elasticity_b
    youngs_modulus = 20
    poissons_ratio = 0.3
  []
  [elasticity_weight_a]
    type = ADParsedMaterial
    property_name = elasticity_weight_a
    coupled_variables = eta2
    expression = '0.6 + 0.1*eta2^2'
  []
  [elasticity_weight_b]
    type = ADParsedMaterial
    property_name = elasticity_weight_b
    coupled_variables = eta2
    expression = '0.4 + 0.05*eta2'
  []
  [elasticity]
    type = ADCompositeElasticityTensor
    base_name = elastic
    tensors = 'elasticity_a elasticity_b'
    weights = 'elasticity_weight_a elasticity_weight_b'
    coupled_variables = eta2
  []

  [strain]
    type = ADComputeSmallStrain
    base_name = elastic
    eigenstrain_names = composite_eigenstrain
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
