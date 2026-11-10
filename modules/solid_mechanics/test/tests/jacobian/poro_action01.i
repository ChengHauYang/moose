# The PetscJacobianTester verifies the Jacobian of the PoroMechanics action, including the
# pore-pressure coupling and stress-divergence kernels. The AD test variant selects the AD kernels
# through cli_args and compares the assembled AD Jacobian with the numerical Jacobian.
[Mesh]
  type = GeneratedMesh
  dim = 3
[]

[GlobalParams]
  block = 0
  displacements = 'disp_x disp_y disp_z'
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
  [p]
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
  [p]
    type = RandomIC
    variable = p
    min = -1
    max = 1
  []
[]

[Kernels]
  [PoroMechanics]
    porepressure = p
  []
  [unimportant_p]
    type = TimeDerivative
    variable = p
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeElasticityTensor
    C_ijkl = '1 1'
    fill_method = symmetric_isotropic
  []
  [strain]
    type = ComputeSmallStrain
  []
  [stress]
    type = ComputeLinearElasticStress
  []
  [biot]
    type = GenericConstantMaterial
    prop_names = biot_coefficient
    prop_values = 0.54
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
