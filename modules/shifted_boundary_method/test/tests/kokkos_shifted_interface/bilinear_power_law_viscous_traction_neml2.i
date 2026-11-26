# bilinear_traction_neml2.i with the power-law mixed-mode criterion and a damage viscosity, the
# NEML2 version of BiLinearMixedModeTraction with the parameters of sczm_bilinear_traction.i,
# mixed_mode_criterion = POWER_LAW, and viscosity = 0.5
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
    exponent = 2
  []
  [macaulay_n]
    type = MacaulaySplit
    from = 'signed_normal_separation'
    to_positive = 'normal_separation'
    to_negative = 'normal_penetration'
  []
  [decompose_old]
    type = VecComponents
    from = 'interface_displacement_jump~1'
    to = 'signed_normal_separation_old tangential_separation_1_old tangential_separation_2_old'
  []
  [tangential_separation_old]
    type = ScalarPNorm
    from = 'tangential_separation_1_old tangential_separation_2_old'
    to = 'tangential_separation_old'
    exponent = 2
  []
  [macaulay_n_old]
    type = MacaulaySplit
    from = 'signed_normal_separation_old'
    to_positive = 'normal_separation_old'
    to_negative = 'normal_penetration_old'
  []
  [mode_mixity_old]
    type = ModeMixity
    normal_separation = 'normal_separation_old'
    tangential_separation = 'tangential_separation_old'
    mode_mixity = 'mode_mixity_old'
  []
  [critical_separation]
    type = CamanhoDavilaCriticalSeparation
    normal_separation = 'normal_separation_old'
    mode_mixity = 'mode_mixity_old'
    penalty_stiffness = 100
    normal_strength = 5
    shear_strength = 7
  []
  [full_separation]
    type = PowerLawFullSeparation
    normal_separation = 'normal_separation_old'
    mode_mixity = 'mode_mixity_old'
    critical_separation = 'critical_separation'
    penalty_stiffness = 100
    mode_I_fracture_toughness = 1
    mode_II_fracture_toughness = 2
    shear_strength = 7
    eta = 2.2
  []
  [effective_separation]
    type = ScalarPNorm
    from = 'normal_separation tangential_separation'
    to = 'effective_separation'
    exponent = 2
  []
  [traction]
    type = BilinearTraction
    traction = 'interface_traction'
    critical_separation = 'critical_separation'
    full_separation = 'full_separation'
    normal_penetration = 'normal_penetration'
    penalty_stiffness = 100
    # With dt = 0.25, the damage moves a third of the way to its rate-independent value per step
    viscosity = 0.5
  []
  [czm]
    type = ComposedModel
    # The NEML2 input parser does not join quoted strings split across lines
    models = 'decompose tangential_separation macaulay_n decompose_old tangential_separation_old macaulay_n_old mode_mixity_old critical_separation full_separation effective_separation traction'
    additional_outputs = 'damage'
  []
[]
