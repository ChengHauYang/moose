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
    type = KokkosDiffusion
    variable = ux
  []
  [uy_diffusion]
    type = KokkosDiffusion
    variable = uy
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    sbm_distance_uo = distance
    use_kokkos = true
    directional_correction = true
    stress = neml2_stress
    tangent = tangent
    tangent_definition = stress_wrt_strain
  []
[]

[Materials]
  [strain]
    type = ComputeSmallStrain
    displacements = 'ux uy'
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
  [primary_elasticity]
    # inherit input from sczm_neml2_elasticity.i
    model = model
    output_backend = kokkos
    interface = Block0_Block1
    interface_only = true
    interface_output_side = primary
  []
  [neighbor_elasticity]
    model = model
    input = sczm_neml2_elasticity_neighbor.i
    output_backend = kokkos
    interface = Block0_Block1
    interface_only = true
    interface_output_side = neighbor
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
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  nl_abs_tol = 1e-14
  nl_rel_tol = 1e-14
[]

[Outputs]
  exodus = true
  file_base = sczm_hetero_kokkos_out
[]
