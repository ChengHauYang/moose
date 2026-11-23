# Verifies that ADGeneralizedMaxwellModel rejects driving_eigenstrain
# with an informative error rather than a silent wrong-Jacobian result.

[Mesh]
  type = GeneratedMesh
  dim = 3
  nx = 1
  ny = 1
  nz = 1
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[Kernels]
  [stress_div_x]
    type = ADStressDivergenceTensors
    variable = disp_x
    displacements = 'disp_x disp_y disp_z'
    component = 0
  []
  [stress_div_y]
    type = ADStressDivergenceTensors
    variable = disp_y
    displacements = 'disp_x disp_y disp_z'
    component = 1
  []
  [stress_div_z]
    type = ADStressDivergenceTensors
    variable = disp_z
    displacements = 'disp_x disp_y disp_z'
    component = 2
  []
[]

[BCs]
  [symmy]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [symmx]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [symmz]
    type = DirichletBC
    variable = disp_z
    boundary = back
    value = 0
  []
[]

[Materials]
  [eigenstrain]
    type = ADComputeEigenstrain
    eigenstrain_name = thermal
    eigen_base = '1e-4 1e-4 1e-4 0 0 0'
  []
  [maxwell]
    type = ADGeneralizedMaxwellModel
    creep_modulus = '3.333333e9'
    creep_viscosity = '1'
    poisson_ratio = 0.2
    young_modulus = 10e9
    driving_eigenstrain = thermal
  []
  [strain]
    type = ADComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
    eigenstrain_names = thermal
  []
  [stress]
    type = ADComputeLinearViscoelasticStress
  []
[]

[UserObjects]
  [update]
    type = ADLinearViscoelasticityManager
    viscoelastic_model = maxwell
  []
[]

[Executioner]
  type = Transient
  dt = 1
  num_steps = 1
[]
