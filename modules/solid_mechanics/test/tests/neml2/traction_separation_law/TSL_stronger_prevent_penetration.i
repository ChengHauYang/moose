[Models]
  [decompose]
    type = VecComponents
    from = 'interface_displacement_jump'
    to = 'signed_normal_separation tangential_separation_1 tangential_separation_2'
  []
  [tangential_separation]
    type = ScalarPNorm
    from = 'tangential_separation_1 tangential_separation_2'
    to = 'tangential_separation'
    exponent = 2.0
  []
  [macaulay_n]
    type = MacaulaySplit
    from = 'signed_normal_separation'
    to_positive = 'normal_separation'
    to_negative = 'unscaled_normal_penetration'
  []
  [mode_mixity]
    type = ModeMixity
  []
  [critical_separation]
    type = CamanhoDavilaCriticalSeparation
    mode_mixity = 'mode_mixity'
    penalty_stiffness = 1e6
    normal_strength = 1e4
    shear_strength = 1e3
  []
  [full_separation]
    type = BenzeggaghKenaneFullSeparation
    mode_mixity = 'mode_mixity'
    critical_separation = 'critical_separation'
    penalty_stiffness = 1e6
    mode_I_fracture_toughness = 1e3
    mode_II_fracture_toughness = 1e2
    shear_strength = 1e3
    eta = 2.2
  []
  # Preserve the physical penetration as an intermediate value and amplify only
  # the penalty contribution. A factor of 1e1 gives an effective compressive
  # penalty stiffness of 1e7 while leaving the tensile/shear law unchanged.
  [scale_normal_penetration]
    type = ScalarLinearCombination
    from = 'unscaled_normal_penetration'
    to = 'normal_penetration'
    weights = 1e1
  []
  [effective_separation]
    type = ScalarPNorm
    from = 'normal_separation tangential_separation'
    to = 'effective_separation'
    exponent = 2.0
  []
  [traction]
    type = BilinearTraction
    traction = 'interface_traction'
    critical_separation = 'critical_separation'
    full_separation = 'full_separation'
    normal_penetration = 'normal_penetration'
    penalty_stiffness = 1e6
  []
  [czm]
    type = ComposedModel
    models = 'decompose tangential_separation macaulay_n scale_normal_penetration
              effective_separation traction'
    additional_outputs = 'damage'
  []
[]
