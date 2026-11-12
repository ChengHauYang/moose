theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 2
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
    type = ${diffusion_type}
    variable = ux
  []
  [uy_diffusion]
    type = ${diffusion_type}
    variable = uy
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    sbm_distance_uo = distance
    directional_correction = true
    stress = pk1_stress
    tangent = pk1_jacobian
    tangent_definition = pk1_wrt_deformation_gradient
  []
[]

[Materials]
  [deformation_gradient]
    type = ComputeLagrangianStrain
    displacements = 'ux uy'
  []
[]

[NEML2]
  eager = true
  input = sczm_pk1_neml2.i
  device = cpu
  input_types = MATERIAL
  inputs = deformation_gradient
  derivatives = 'pk1_stress deformation_gradient pk1_jacobian'
  [constitutive]
    model = model
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
    execution_order_group = -1
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.25) * cos(${theta_rad})'
  []
[]

[BCs]
  [ux_left]
    type = ${dirichlet_type}
    variable = ux
    boundary = left
    value = 0.1
  []
  [ux_right]
    type = ${dirichlet_type}
    variable = ux
    boundary = right
    value = -0.1
  []
  [uy_left]
    type = ${dirichlet_type}
    variable = uy
    boundary = left
    value = 0.2
  []
  [uy_right]
    type = ${dirichlet_type}
    variable = uy
    boundary = right
    value = -0.2
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  exodus = false
[]
