# Tests of the variable base eigenstrain material
#   ADComputeVariableBaseEigenStrain
# computing
#   eigenstrain = prefactor * base_tensor + offset_tensor
# with both the prefactor and the base tensor depending on the nonlinear
# concentration variable c. The base tensor material is a test-only
# material (SolidMechanicsTestApp) because no other material declares an
# AD RealTensorValue property coupled to a nonlinear variable.
#
# The chain
#   c -> ADTestFunctionTensorMaterial -> ADComputeVariableBaseEigenStrain
#     -> ADComputeSmallStrain -> ADComputeLinearElasticStress
#     -> ADStressDivergenceTensors
# carries the concentration derivative of the base tensor into the
# displacement residual, so the off-diagonal Jacobian dR / dc is nonzero.
# The prefactor path is exercised through ADParsedMaterial with pref = c*c + 0.25.
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
  [base_tensor_source]
    type = ADTestFunctionTensorMaterial
    property_name = base_tensor
    tensor0 = '1 0 0 0 1 0 0 0 1'
    tensor1 = '0 1 0 0 0 0 0 0 0'
    c = c
  []
  [prefactor_source]
    type = ADParsedMaterial
    property_name = pref
    expression = 'c*c + 0.25'
    coupled_variables = 'c'
  []
  [eigenstrain]
    type = ADComputeVariableBaseEigenStrain
    base_tensor_property_name = base_tensor
    prefactor = pref
    eigenstrain_name = var_eig
    offset_tensor = '0 0 0 0 0 0 0 0 0'
  []
  [strain]
    type = ADComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
    eigenstrain_names = var_eig
  []
  [elasticity_tensor]
    type = ADComputeIsotropicElasticityTensor
    youngs_modulus = 10.0
    poissons_ratio = 0.3
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
