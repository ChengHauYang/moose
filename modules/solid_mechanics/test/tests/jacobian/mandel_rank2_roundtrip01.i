# Tests of the rank-two Mandel converter pair
#   RankTwoTensorToSymmetricRankTwoTensor and SymmetricRankTwoTensorToRankTwoTensor
# converting a mechanical strain property to and from symmetric Mandel notation
# before it reaches the stress calculation.
#
# The chain
#   disp -> mechanical_strain (RankTwoTensor)
#         -> sym_strain (SymmetricRankTwoTensor) -> foo_mechanical_strain (RankTwoTensor)
#         -> stress
# is exercised fully in AD mode with ADComputeLinearElasticStress and
# ADStressDivergenceTensors, verifying that derivatives survive the converters.
# The AD path is selected with cli_args; the plain input uses the non-AD objects.
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
[]

[Kernels]
  [grad_stress_x]
    type = StressDivergenceTensors
    displacements = 'disp_x disp_y disp_z'
    variable = disp_x
    component = 0
    base_name = foo
  []
  [grad_stress_y]
    type = StressDivergenceTensors
    displacements = 'disp_x disp_y disp_z'
    variable = disp_y
    component = 1
    base_name = foo
  []
  [grad_stress_z]
    type = StressDivergenceTensors
    displacements = 'disp_x disp_y disp_z'
    variable = disp_z
    component = 2
    base_name = foo
  []
[]

[Materials]
  [strain]
    type = ComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
  []
  [to_symmetric]
    type = RankTwoTensorToSymmetricRankTwoTensor
    from = mechanical_strain
    to = sym_strain
  []
  [to_full]
    type = SymmetricRankTwoTensorToRankTwoTensor
    from = sym_strain
    to = foo_mechanical_strain
  []
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    base_name = foo
    youngs_modulus = 10.0
    poissons_ratio = 0.3
  []
  [stress]
    type = ComputeLinearElasticStress
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
