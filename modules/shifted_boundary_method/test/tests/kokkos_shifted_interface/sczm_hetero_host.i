theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 4
    xmax = 2
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '1 0 0'
    top_right = '2 1 0'
    block_id = 1
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = right_block
  []
[]

[Variables]
  [ux]
  []
  [uy]
  []
[]

[Kernels]
  [ux_diffusion]
    type = Diffusion
    variable = ux
  []
  [uy_diffusion]
    type = Diffusion
    variable = uy
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    sbm_distance_uo = distance
    directional_correction = true
  []
[]

[Materials]
  # Heterogeneous bulk response. The primary side (block 0) and the neighbor side (block 1) carry
  # different elastic constants so that swapping the two sides changes the solution.
  [strain_primary]
    type = ComputeSmallStrain
    displacements = 'ux uy'
    block = 0
  []
  [elasticity_primary]
    type = ComputeIsotropicElasticityTensor
    block = 0
    youngs_modulus = 1
    poissons_ratio = 0.30
  []
  [stress_primary]
    type = ComputeLinearElasticStress
    block = 0
  []
  [strain_neighbor]
    type = ComputeSmallStrain
    displacements = 'ux uy'
    block = 1
  []
  [elasticity_neighbor]
    type = ComputeIsotropicElasticityTensor
    block = 1
    youngs_modulus = 3
    poissons_ratio = 0.20
  []
  [stress_neighbor]
    type = ComputeLinearElasticStress
    block = 1
  []
  [local_traction]
    type = PureElasticTractionSeparation
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
[]

[UserObjects]
  [distance]
    type = BoundaryShortestDistanceToSurface
    surfaces = true_interface
    boundary = 'Block0_Block1'
    execute_on = INITIAL
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad})'
  []
[]

[BCs]
  [ux_left]
    type = DirichletBC
    variable = ux
    boundary = left
    value = 0.1
  []
  [ux_right]
    type = DirichletBC
    variable = ux
    boundary = right
    value = -0.1
  []
  [uy_left]
    type = DirichletBC
    variable = uy
    boundary = left
    value = 0.2
  []
  [uy_right]
    type = DirichletBC
    variable = uy
    boundary = right
    value = -0.2
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  nl_abs_tol = 1e-14
  nl_rel_tol = 1e-14
[]

[Outputs]
  exodus = true
  file_base = sczm_hetero_host_out
[]
