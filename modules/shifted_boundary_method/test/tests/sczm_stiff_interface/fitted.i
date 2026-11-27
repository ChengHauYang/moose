# Interface-fitted cohesive zone on the mesh of common.i

[Mesh]
  [fit]
    type = ParsedNodeTransformGenerator
    input = gen
    # Shifts each row of nodes along x so that the nodes on x = 0.5 land on the inclined interface,
    # leaving the nodes on x = 0 and x = 1 in place
    x_function = 'x + (y-0.5)*cos(${theta})/sin(${theta})*(1-abs(2*x-1))'
    y_function = 'y'
    z_function = 'z'
  []
  [subdomain_intercepted]
    input = fit
  []
[]

[Physics/SolidMechanics/CohesiveZone]
  [czm_ik]
    use_automatic_differentiation = false
    boundary = 'Block1_Block2'
  []
[]
