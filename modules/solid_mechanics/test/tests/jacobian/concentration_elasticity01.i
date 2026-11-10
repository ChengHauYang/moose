# Tests of the concentration dependent elasticity tensor
#   ADComputeConcentrationDependentElasticityTensor
#in conjunction with the usual ADStressDivergenceTensors kernels.
#
# The elasticity tensor depends on the nonlinear concentration variable c,
# so the off-diagonal Jacobian dR_displacement / dc is nonzero and verifies
# that the concentration derivative propagates through the stress residual.
[Mesh]
  type = GeneratedMesh
  dim = 3
[]

[GlobalParams]
  block = 0
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
    min = -1
    max = 1
  []
  [disp_y]
    type = RandomIC
    variable = disp_y
    min = -1
    max = 1
  []
  [disp_z]
    type = RandomIC
    variable = disp_z
    min = -1
    max = 1
  []
  [c]
    type = RandomIC
    variable = c
    min = -1
    max = 1
  []
[]

[Kernels]
  [unimportant_c]
    type = TimeDerivative
    variable = c
  []
  [grad_stress_x]
    type = ADStressDivergenceTensors
    displacements = 'disp_x disp_y disp_z'
    variable = disp_x
    component = 0
  []
  [grad_stress_y]
    type = ADStressDivergenceTensors
    displacements = 'disp_x disp_y disp_z'
    variable = disp_y
    component = 1
  []
  [grad_stress_z]
    type = ADStressDivergenceTensors
    displacements = 'disp_x disp_y disp_z'
    variable = disp_z
    component = 2
  []
[]

[Materials]
  [elasticity_tensor]
    type = ADComputeConcentrationDependentElasticityTensor
    C0_ijkl = '1 1'
    C1_ijkl = '2 2'
    fill_method0 = symmetric_isotropic
    fill_method1 = symmetric_isotropic
    c = c
  []
  [strain]
    type = ADComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
  []
  [stress]
    type = ADComputeLinearElasticStress
  []
[]

[Preconditioning]
  [andy]
    type = SMP
    full = true
    petsc_options_iname = '-ksp_type -pc_type -snes_atol -snes_rtol -snes_max_it -snes_type'
    petsc_options_value = 'bcgs bjacobi 1E-15 1E-10 10000 test'
  []
[]

[Executioner]
  type = Transient
  solve_type = Newton
[]
