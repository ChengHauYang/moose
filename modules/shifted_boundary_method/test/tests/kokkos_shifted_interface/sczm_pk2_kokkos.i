theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 2
    ny = 2
    nz = 1
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
    stress = pk2_stress
    tangent = dpk2_dF
    tangent_definition = pk2_wrt_deformation_gradient
  []
[]

[Materials]
  [deformation_gradient]
    type = ComputeLagrangianStrain
    displacements = 'ux uy uz'
  []
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
  [pk2_to_pk1_primary_oracle]
    type = KokkosPK2ToPK1PrimaryInterfaceOracle
    boundary = 'Block0_Block1'
    displacements = 'ux uy uz'
    pk2_stress = pk2_stress
    dpk2_dF = dpk2_dF
    pk1_stress = sczm_pk1_stress
    dpk1_dF = sczm_pk1_jacobian
  []
  [pk2_to_pk1_neighbor_oracle]
    type = KokkosPK2ToPK1NeighborInterfaceOracle
    boundary = 'Block0_Block1'
    displacements = 'ux uy uz'
    pk2_stress = pk2_stress
    dpk2_dF = dpk2_dF
    pk1_stress = sczm_pk1_stress
    dpk1_dF = sczm_pk1_jacobian
  []
[]

[NEML2]
  eager = true
  input = sczm_pk2_primary_neml2.i
  device = cpu
  input_types = MATERIAL
  inputs = deformation_gradient
  derivatives = 'pk2_stress deformation_gradient dpk2_dF'
  [primary]
    model = model
    output_backend = kokkos
    interface = Block0_Block1
    interface_only = true
    interface_output_side = primary
  []
  [neighbor]
    model = model
    input = sczm_pk2_neighbor_neml2.i
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
  [ux_ic]
    type = ParsedFunction
    expression = '0.03*x*y + 0.02*y*z + 0.01*z*x'
  []
  [uy_ic]
    type = ParsedFunction
    expression = '-0.02*x*y + 0.04*y*z + 0.03*z*x'
  []
  [uz_ic]
    type = ParsedFunction
    expression = '0.01*x*y - 0.03*y*z + 0.05*z*x'
  []
[]

[ICs]
  [ux]
    type = FunctionIC
    variable = ux
    function = ux_ic
  []
  [uy]
    type = FunctionIC
    variable = uy
    function = uy_ic
  []
  [uz]
    type = FunctionIC
    variable = uz
    function = uz_ic
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

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  nl_abs_tol = 1e-14
  nl_rel_tol = 1e-14
[]

[Outputs]
  exodus = false
[]
