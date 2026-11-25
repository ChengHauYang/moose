# Mixed-mode opening past the bilinear peak (softening) followed by unloading of a shifted
# cohesive interface between two elastic blocks.

E = 1000
nu = 0.0
# Initial rigid offset of the right block; the Jacobian tests use it to start inside a branch of
# the law rather than at the kinks of zero opening and damage onset
opening_ic = 0
slip_ic = 0

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

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = 'Block0_Block1'
    displacements = 'ux uy'
    sbm_distance_uo = distance
    directional_correction = true
    stress = stress
    tangent = Jacobian_mult
    tangent_definition = stress_wrt_strain
  []
[]

[ICs]
  [opening]
    type = ConstantIC
    variable = ux
    block = 1
    value = ${opening_ic}
  []
  [slip]
    type = ConstantIC
    variable = uy
    block = 1
    value = ${slip_ic}
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
    expression = 'x - 1.05'
  []
  # With penalty_stiffness = 100, damage starts near an opening of normal_strength / 100 = 0.05,
  # reached at a right-side displacement of about 0.06; the last two steps unload
  [opening]
    type = PiecewiseLinear
    x = '0    0.25 0.50 0.75 1.00 1.25'
    y = '0    0.03 0.10 0.20 0.10 0.04'
  []
  # The slip makes the mode mixity, and thus the damage onset, depend on the lagged history jump
  [slip]
    type = PiecewiseLinear
    x = '0    0.25 0.50 0.75 1.00 1.25'
    y = '0    0.01 0.04 0.08 0.04 0.02'
  []
[]

[BCs]
  [ux_left]
    type = DirichletBC
    variable = ux
    boundary = left
    value = 0
  []
  [uy_left]
    type = DirichletBC
    variable = uy
    boundary = left
    value = 0
  []
  [ux_right]
    type = FunctionDirichletBC
    variable = ux
    boundary = right
    function = opening
    preset = false
  []
  [uy_right]
    type = FunctionDirichletBC
    variable = uy
    boundary = right
    function = slip
    preset = false
  []
[]

[Postprocessors]
  [ux_left_side]
    type = SideAverageValue
    variable = ux
    boundary = Block0_Block1
  []
  [ux_right_side]
    type = SideAverageValue
    variable = ux
    boundary = Block1_Block0
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  line_search = none
  nl_abs_tol = 1e-10
  nl_rel_tol = 1e-12
  dt = 0.25
  end_time = 1.25
[]

[Outputs]
  exodus = true
[]
