# Verify that derivatives of an elasticity tensor propagate through its symmetric inverse
# when converting an initial stress to an eigenstrain.
[Mesh]
  type = GeneratedMesh
  dim = 3
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
  [c]
  []
[]

[ICs]
  [disp_x]
    type = RandomIC
    variable = disp_x
    min = -0.1
    max = 0.1
  []
  [disp_y]
    type = RandomIC
    variable = disp_y
    min = -0.1
    max = 0.1
  []
  [disp_z]
    type = RandomIC
    variable = disp_z
    min = -0.1
    max = 0.1
  []
  [c]
    type = RandomIC
    variable = c
    min = 0.1
    max = 0.9
  []
[]

[Kernels]
  [disp_x]
    type = ADStressDivergenceTensors
    variable = disp_x
    displacements = 'disp_x disp_y disp_z'
    component = 0
  []
  [disp_y]
    type = ADStressDivergenceTensors
    variable = disp_y
    displacements = 'disp_x disp_y disp_z'
    component = 1
  []
  [disp_z]
    type = ADStressDivergenceTensors
    variable = disp_z
    displacements = 'disp_x disp_y disp_z'
    component = 2
  []
  [c]
    type = ADReaction
    variable = c
  []
[]

[Materials]
  [elasticity]
    type = ADComputeConcentrationDependentElasticityTensor
    c = c
    C0_ijkl = '2 3'
    C1_ijkl = '4 5'
    fill_method0 = symmetric_isotropic
    fill_method1 = symmetric_isotropic
  []
  [initial_stress]
    type = ADComputeEigenstrainFromInitialStress
    initial_stress = '1 0 0  0 2 0  0 0 3'
    eigenstrain_name = initial_stress
  []
  [strain]
    type = ADComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
    eigenstrain_names = initial_stress
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
  type = Transient
  solve_type = Newton
  num_steps = 1
[]
