# Shifted cohesive zone on a mesh that does not fit the interface of common.i

[GlobalParams]
  sbm_distance_uo = sbm_distance_uo
[]

[Mesh]
  [gen]
    # An odd element count puts x = 0.5 in the middle of an element column, so the surrogate
    # interface is half an element width away from the true one
    nx = 21
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [czm_ik]
    use_automatic_differentiation = false
    boundary = 'Block1_Block2'
  []
[]

[UserObjects]
  [sbm_distance_uo]
    type = BoundaryShortestDistanceToSurface
    surfaces = 'signed_dist_func'
    boundary = 'Block1_Block2'
    execution_order_group = 0
    execute_on = 'INITIAL'
    # The surrogate distance is half the element width by construction
    suppress_distance_warning = true
  []
[]

[Functions]
  [signed_dist_func]
    type = ParsedFunction
    expression = 'x-${x0}'
  []
[]
