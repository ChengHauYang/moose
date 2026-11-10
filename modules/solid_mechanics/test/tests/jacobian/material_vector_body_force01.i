# Tests of the AD material vector body force kernel
#   ADMaterialVectorBodyForce
# applying a body force taken from an AD vector material property that
# depends on the nonlinear concentration variable c.
#
# The chain
#   c -> ADVectorFromComponentVariablesMaterial -> ADMaterialVectorBodyForce
# carries the concentration derivative of the body force into the
# displacement residual, so the off-diagonal Jacobian dR / dc is nonzero.
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
  [body_force_x]
    type = ADMaterialVectorBodyForce
    variable = disp_x
    displacements = 'disp_x disp_y disp_z'
    body_force = body_force_vec
  []
  [body_force_y]
    type = ADMaterialVectorBodyForce
    variable = disp_y
    displacements = 'disp_x disp_y disp_z'
    body_force = body_force_vec
  []
  [body_force_z]
    type = ADMaterialVectorBodyForce
    variable = disp_z
    displacements = 'disp_x disp_y disp_z'
    body_force = body_force_vec
  []
[]

[Materials]
  [body_force_vec_source]
    type = ADVectorFromComponentVariablesMaterial
    vector_prop_name = body_force_vec
    u = c
    v = c
    w = c
  []
  [strain]
    type = ADComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
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
