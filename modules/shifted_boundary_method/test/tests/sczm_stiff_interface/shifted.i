# Shifted cohesive zone on the Cartesian mesh of common.i, which does not fit the interface

[GlobalParams]
  sbm_distance_uo = sbm_distance_uo
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
  []
[]

[Functions]
  [signed_dist_func]
    type = ParsedFunction
    expression = '(x-0.5)*sin(${theta}) - (y-0.5)*cos(${theta})'
  []
[]
