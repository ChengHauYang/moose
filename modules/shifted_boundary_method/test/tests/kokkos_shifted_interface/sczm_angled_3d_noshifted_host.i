theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 4
    ny = 4
    nz = 2
    xmax = 2
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '1 0 0'
    top_right = '2 1 1'
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
  [uz]
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
  [uz_diffusion]
    type = Diffusion
    variable = uz
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy uz'
    sbm_distance_uo = distance
    directional_correction = true
    no_shifted = true
  []
[]

[Materials]
  # The host SCZM kernel requires stress and Jacobian_mult even when directional correction is disabled.
  [strain]
    type = ComputeSmallStrain
    displacements = 'ux uy uz'
  []
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1
    poissons_ratio = 0.3
  []
  [stress]
    type = ComputeLinearElasticStress
  []
  [local_traction]
    type = PureElasticTractionSeparation
    boundary = 'Block0_Block1'
    displacements = 'ux uy uz'
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
  [uz_left]
    type = DirichletBC
    variable = uz
    boundary = left
    value = 0.3
  []
  [uz_right]
    type = DirichletBC
    variable = uz
    boundary = right
    value = -0.3
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
  file_base = sczm_angled_3d_noshifted_host_out
[]
