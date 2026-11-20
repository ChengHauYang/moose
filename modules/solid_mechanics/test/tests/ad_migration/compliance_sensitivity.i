# The design density controls both the elasticity tensor and its compliance sensitivity.
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
  [rho]
    [InitialCondition]
      type = FunctionIC
      function = '0.4 + 0.1*x + 0.2*y + 0.3*z'
    []
  []
  [sensitivity]
  []
[]

[Kernels]
  [x]
    type = ADReaction
    variable = disp_x
  []
  [y]
    type = ADReaction
    variable = disp_y
  []
  [z]
    type = ADReaction
    variable = disp_z
  []
  [rho]
    type = ADReaction
    variable = rho
  []
  [sensitivity]
    type = ADMaterialPropertyValue
    variable = sensitivity
    prop_name = sensitivity
  []
[]

[Materials]
  [elastic_constants]
    type = ADGenericConstantMaterial
    prop_names = poissons_ratio
    prop_values = 0.2
  []
  [youngs_modulus]
    type = ADDerivativeParsedMaterial
    coupled_variables = rho
    f_name = youngs_modulus
    function = '5 + 2*rho + rho^2'
    derivative_order = 2
  []
  [elasticity]
    type = ADComputeVariableIsotropicElasticityTensor
    youngs_modulus = youngs_modulus
    poissons_ratio = poissons_ratio
  []
  [strain]
    type = ADComputeSmallStrain
  []
  [stress]
    type = ADComputeLinearElasticStress
  []
  [compliance_sensitivity]
    type = ADComplianceSensitivity
    design_density = rho
    youngs_modulus = youngs_modulus
    incremental = false
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
