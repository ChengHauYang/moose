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
    stabilization = weighted_traction
  []
[]

[Materials]
  # The host SCZM directional correction reads stress and Jacobian_mult.
  [strain]
    type = ComputeSmallStrain
    displacements = 'ux uy'
  []
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1
    poissons_ratio = 0.3
  []
  [stress]
    type = ComputeLinearElasticStress
  []
  # These peaks give initial normal/tangential stiffnesses of 10 and 8
  [local_traction]
    type = SalehaniIrani3DCTraction
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    normal_gap_at_maximum_normal_traction = 1e8
    tangential_gap_at_maximum_shear_traction = 1e8
    maximum_normal_traction = 367879441.171442
    maximum_shear_traction = 485224527.770107
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
    value = 0.01
  []
  [ux_right]
    type = DirichletBC
    variable = ux
    boundary = right
    value = -0.01
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
  file_base = sczm_salehani_angled_host_out
[]
