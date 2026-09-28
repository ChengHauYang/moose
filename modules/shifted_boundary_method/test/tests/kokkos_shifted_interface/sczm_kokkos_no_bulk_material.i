# The Kokkos SCZM directional correction is enabled, but no
# KokkosSCZMComputeLinearElasticBulkStress material is declared. The Action must reject this during
# setup rather than letting the correction silently produce an incomplete residual.

theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 2
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
    split_interface = true
    add_interface_on_two_sides = true
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
  []
[]

[Materials]
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
  # Intentionally no KokkosSCZMComputeLinearElasticBulkStress block.
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

[Executioner]
  type = Steady
[]

[Outputs]
  exodus = false
[]
