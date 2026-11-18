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
  [sed]
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
  [sed_kernel]
    type = ADMaterialPropertyValue
    variable = sed
    prop_name = strain_energy_density
  []
[]

[Materials]
  [elasticity]
    type = ADComputeIsotropicElasticityTensor
    youngs_modulus = 10
    poissons_ratio = 0.2
  []
  [strain]
    type = ADComputeSmallStrain
  []
  [stress]
    type = ADComputeLinearElasticStress
  []
  [strain_energy_density]
    type = ADStrainEnergyDensity
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