[Mesh]
  type = GeneratedMesh
  dim = 3
[]

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
[]

[Variables]
  [temperature]
  []
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[ICs]
  [temperature]
    type = FunctionIC
    variable = temperature
    function = '1 + x*y + y*z'
  []
  [disp_x]
    type = FunctionIC
    variable = disp_x
    function = '0.2*x + 0.1*x*y'
  []
  [disp_y]
    type = FunctionIC
    variable = disp_y
    function = '-0.15*y + 0.1*y*z'
  []
  [disp_z]
    type = FunctionIC
    variable = disp_z
    function = '0.25*z + 0.1*z*x'
  []
[]

[Kernels]
  [heat]
    type = ADPlasticHeatEnergy
    variable = temperature
    coeff = 0.5
  []
  [temperature_time]
    type = ADTimeDerivative
    variable = temperature
  []
  [disp_x]
    type = ADStressDivergenceTensors
    variable = disp_x
    component = 0
  []
  [disp_y]
    type = ADStressDivergenceTensors
    variable = disp_y
    component = 1
  []
  [disp_z]
    type = ADStressDivergenceTensors
    variable = disp_z
    component = 2
  []
[]

[Materials]
  [elasticity_tensor]
    type = ADComputeIsotropicElasticityTensor
    youngs_modulus = 100
    poissons_ratio = 0.3
  []
  [strain]
    type = ADComputeIncrementalStrain
  []
  [stress]
    type = ADComputeMultipleInelasticStress
    inelastic_models = plasticity
    perform_finite_strain_rotations = false
  []
  [plasticity]
    type = ADIsotropicPlasticityStressUpdate
    yield_stress = 0.1
    hardening_constant = 1
  []
  [plastic_heat]
    type = ADComputePlasticHeatEnergy
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
  solve_type = Newton
  dt = 1
  num_steps = 1
[]
