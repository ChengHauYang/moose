# Tests of the rank-four Mandel converter pair
#   ADRankFourTensorToSymmetricRankFourTensor and ADSymmetricRankFourTensorToRankFourTensor
# converting a concentration-dependent elasticity tensor to and from symmetric Mandel
# notation before it reaches the stress calculation.
#
# The chain
#   c -> concentration dependent elasticity tensor (RankFourTensor)
#     -> sym_C (SymmetricRankFourTensor) -> foo_elasticity_tensor (RankFourTensor)
#     -> stress -> residual
# carries the concentration derivative of the elasticity tensor into the
# displacement residual, so the off-diagonal Jacobian dR / dc is nonzero. This
# verifies that derivatives survive both rank-four converters in the AD path.
# Note: the non-AD stress chain does not consume elasticity tensor derivatives,
# so the off-diagonal Jacobian is not assembled in non-AD mode; only the AD
# variant of this test is meaningful.
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
    base_name = foo
  []
  [grad_stress_y]
    type = ADStressDivergenceTensors
    displacements = 'disp_x disp_y disp_z'
    variable = disp_y
    component = 1
    base_name = foo
  []
  [grad_stress_z]
    type = ADStressDivergenceTensors
    displacements = 'disp_x disp_y disp_z'
    variable = disp_z
    component = 2
    base_name = foo
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
  [to_symmetric]
    type = ADRankFourTensorToSymmetricRankFourTensor
    from = elasticity_tensor
    to = sym_C
  []
  [to_full]
    type = ADSymmetricRankFourTensorToRankFourTensor
    from = sym_C
    to = foo_elasticity_tensor
  []
  [strain]
    type = ADComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
    base_name = foo
  []
  [stress]
    type = ADComputeLinearElasticStress
    base_name = foo
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
