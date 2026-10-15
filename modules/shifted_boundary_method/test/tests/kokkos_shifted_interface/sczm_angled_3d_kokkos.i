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
    type = KokkosDiffusion
    variable = ux
  []
  [uy_diffusion]
    type = KokkosDiffusion
    variable = uy
  []
  [uz_diffusion]
    type = KokkosDiffusion
    variable = uz
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy uz'
    sbm_distance_uo = distance
    use_kokkos = true
    directional_correction = true
    stress = neml2_stress
    tangent = tangent
  []
[]

[Materials]
  [strain]
    type = ComputeSmallStrain
    displacements = 'ux uy uz'
  []
  [neml2_strain]
    type = RankTwoTensorToSymmetricRankTwoTensor
    from = mechanical_strain
    to = neml2_strain
  []
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
[]

[NEML2]
  eager = true
  input = sczm_neml2_elasticity.i
  device = cpu
  input_types = MATERIAL
  inputs = neml2_strain
  derivatives = 'neml2_stress neml2_strain tangent'
  [elasticity]
    model = model
    output_backend = kokkos
    interface = Block0_Block1
    interface_only = true
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
    type = KokkosDirichletBC
    variable = ux
    boundary = left
    value = 0.1
  []
  [ux_right]
    type = KokkosDirichletBC
    variable = ux
    boundary = right
    value = -0.1
  []
  [uy_left]
    type = KokkosDirichletBC
    variable = uy
    boundary = left
    value = 0.2
  []
  [uy_right]
    type = KokkosDirichletBC
    variable = uy
    boundary = right
    value = -0.2
  []
  [uz_left]
    type = KokkosDirichletBC
    variable = uz
    boundary = left
    value = 0.3
  []
  [uz_right]
    type = KokkosDirichletBC
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
  file_base = sczm_angled_3d_kokkos_out
[]
