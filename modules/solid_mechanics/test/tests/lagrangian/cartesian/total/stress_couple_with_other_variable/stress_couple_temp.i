# 2D Total-Lagrangian (TL) elasticity + additional-coupled Jacobian test
# Goal: verify off-diagonal Jacobian path for dPK1/dT provided as a material property.

[GlobalParams]
  displacements = 'disp_x disp_y'
  large_kinematics = false
[]

[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 64
  ny = 64
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []

  # Scalar field for the additional coupling test
  [temperature]
  []
[]

[Kernels]
  # TL momentum balance in x with additional coupling P(T)
  [sdx]
    type = TotalLagrangianStressDivergence
    variable = disp_x
    component = 0
    additional_coupled_vars = 'temperature'
    additional_coupling_jacobians = 'dpk1_dT_const'
  []

  # TL momentum balance in y with additional coupling P(T)
  [sdy]
    type = TotalLagrangianStressDivergence
    variable = disp_y
    component = 1
    additional_coupled_vars = 'temperature'
    additional_coupling_jacobians = 'dpk1_dT_const'
  []

  # Diffusion to create a temperature gradient
  [temp_diff]
    type = Diffusion
    variable = temperature
  []
[]

[Materials]
  # TL linear elasticity chain:
  #   ComputeLagrangianStrain -> deformation_gradient
  #   ComputeLagrangianLinearElasticStress -> pk1_stress + pk1_jacobian
  [elastic_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1.0e5
    poissons_ratio = 0.3
  []

  [compute_strain]
    type = ComputeLagrangianStrain
    # No eigenstrain here (isolate "additional coupling" effect only)
  []

  [compute_stress]
    type = ComputeLagrangianLinearElasticStress
  []

  # Provide dPK1/dT as a constant RankTwoTensor property.
  # IMPORTANT: RankTwoTensor needs 1, 3, 6, or 9 entries. Use 9 (3x3).
  [dpk1_dT_const]
    type = GenericConstantRankTwoTensor
    tensor_name = dpk1_dT_const
    tensor_values = '1 0 0  0 1 0  0 0 1'
  []
[]

[BCs]
  # Fix left boundary to remove rigid body motion
  [fix_left_x]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [fix_left_y]
    type = DirichletBC
    variable = disp_y
    boundary = left
    value = 0
  []

  # Temperature gradient left -> right
  [T_left]
    type = DirichletBC
    variable = temperature
    boundary = left
    value = 0
  []
  [T_right]
    type = DirichletBC
    variable = temperature
    boundary = right
    value = 1
  []
[]

[AuxVariables]
  # PK1 stress components (2D)
  [pk1_stress_xx]
    family = MONOMIAL
    order = CONSTANT
  []
  [pk1_stress_yx]
    family = MONOMIAL
    order = CONSTANT
  []
  [pk1_stress_xy]
    family = MONOMIAL
    order = CONSTANT
  []
  [pk1_stress_yy]
    family = MONOMIAL
    order = CONSTANT
  []

  # Deformation gradient components (2D)
  [deformation_gradient_xx]
    family = MONOMIAL
    order = CONSTANT
  []
  [deformation_gradient_yx]
    family = MONOMIAL
    order = CONSTANT
  []
  [deformation_gradient_xy]
    family = MONOMIAL
    order = CONSTANT
  []
  [deformation_gradient_yy]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[AuxKernels]
  # PK1 = pk1_stress (RankTwoTensor)
  [pk1_stress_xx]
    type = RankTwoAux
    variable = pk1_stress_xx
    rank_two_tensor = pk1_stress
    index_i = 0
    index_j = 0
  []
  [pk1_stress_yx]
    type = RankTwoAux
    variable = pk1_stress_yx
    rank_two_tensor = pk1_stress
    index_i = 1
    index_j = 0
  []
  [pk1_stress_xy]
    type = RankTwoAux
    variable = pk1_stress_xy
    rank_two_tensor = pk1_stress
    index_i = 0
    index_j = 1
  []
  [pk1_stress_yy]
    type = RankTwoAux
    variable = pk1_stress_yy
    rank_two_tensor = pk1_stress
    index_i = 1
    index_j = 1
  []

  # F = deformation_gradient (RankTwoTensor)
  [deformation_gradient_xx]
    type = RankTwoAux
    variable = deformation_gradient_xx
    rank_two_tensor = deformation_gradient
    index_i = 0
    index_j = 0
  []
  [deformation_gradient_yx]
    type = RankTwoAux
    variable = deformation_gradient_yx
    rank_two_tensor = deformation_gradient
    index_i = 1
    index_j = 0
  []
  [deformation_gradient_xy]
    type = RankTwoAux
    variable = deformation_gradient_xy
    rank_two_tensor = deformation_gradient
    index_i = 0
    index_j = 1
  []
  [deformation_gradient_yy]
    type = RankTwoAux
    variable = deformation_gradient_yy
    rank_two_tensor = deformation_gradient
    index_i = 1
    index_j = 1
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON

  petsc_options_iname = '-snes_type -pc_type'
  petsc_options_value = 'newtonls lu'

  nl_max_its = 30
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-12
[]

[Outputs]
  file_base = 'tl_additional_coupling_2d'
  exodus = true
[]
