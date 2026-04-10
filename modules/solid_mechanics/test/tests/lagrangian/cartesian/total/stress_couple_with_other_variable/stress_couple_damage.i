# Minimal total-Lagrangian example showing how to couple stress divergence to
# an additional scalar variable through a material-provided dPK1/ddamage term.
#
# Material contract demonstrated here:
#   P  = (1 - d) P0
#   dP/dd = -P0
#
# The stress-divergence kernel only assembles the off-diagonal Jacobian term.
# The material must provide both:
#   pk1_stress
#   dpk1_ddamage

[GlobalParams]
  displacements = 'disp_x disp_y'
  large_kinematics = false
[]

[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 6
  ny = 6
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [damage]
  []
[]

[AuxVariables]
  # Aux variables only to make the relationship visible in output.
  [pk1_xx]
    family = MONOMIAL
    order = CONSTANT
  []
  [undamaged_pk1_xx]
    family = MONOMIAL
    order = CONSTANT
  []
  [dpk1_ddamage_xx]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[Kernels]
  [mechanics_x]
    type = TotalLagrangianStressDivergence
    variable = disp_x
    component = 0
    additional_coupled_vars = 'damage'
    additional_coupling_jacobians = 'dpk1_ddamage'
  []
  [mechanics_y]
    type = TotalLagrangianStressDivergence
    variable = disp_y
    component = 1
    additional_coupled_vars = 'damage'
    additional_coupling_jacobians = 'dpk1_ddamage'
  []

  # Simple scalar equation so "damage" is part of the nonlinear system.
  [damage_diffusion]
    type = Diffusion
    variable = damage
  []
[]

[AuxKernels]
  [pk1_xx]
    type = RankTwoAux
    variable = pk1_xx
    rank_two_tensor = pk1_stress
    index_i = 0
    index_j = 0
  []
  [undamaged_pk1_xx]
    type = RankTwoAux
    variable = undamaged_pk1_xx
    rank_two_tensor = undamaged_pk1_stress
    index_i = 0
    index_j = 0
  []
  [dpk1_ddamage_xx]
    type = RankTwoAux
    variable = dpk1_ddamage_xx
    rank_two_tensor = dpk1_ddamage
    index_i = 0
    index_j = 0
  []
[]

[Materials]
  # First build the undamaged elastic PK1 stress P0.
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1e5
    poissons_ratio = 0.3
  []
  [lagrangian_strain]
    type = ComputeLagrangianStrain
    base_name = undamaged
  []
  [undamaged_elastic_stress]
    type = ComputeLagrangianLinearElasticStress
    base_name = undamaged
  []

  # Then convert P0 -> (1 - damage) P0 and provide dP/ddamage = -P0.
  [damage_scaled_pk1]
    type = DamageScaledPK1Stress
    damage = damage
    source_base_name = undamaged
    dpk1_ddamage_name = dpk1_ddamage
  []
[]

[BCs]
  # Mechanics: impose a small extension so P0 is nonzero.
  [fix_left_x]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [fix_bottom_y]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [pull_right_x]
    type = DirichletBC
    variable = disp_x
    boundary = right
    value = 1e-3
  []

  # Damage field used as the additional coupled variable.
  [damage_left]
    type = DirichletBC
    variable = damage
    boundary = left
    value = 0.2
  []
  [damage_right]
    type = DirichletBC
    variable = damage
    boundary = right
    value = 0.4
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
  exodus = true
[]
