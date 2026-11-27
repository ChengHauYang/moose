# Mixed-mode bilinear traction-separation law, small strain. The interface-fitted model stays
# below the strengths, so damage in a shifted model is spurious.

[Materials]
  [czm]
    type = BiLinearMixedModeTraction
    # 1e5 times the bulk Young's modulus
    penalty_stiffness = 1e8
    GI_c = 1e3
    GII_c = 1e2
    normal_strength = 500
    shear_strength = 300
    eta = 2.2
    viscosity = 0
    # The derivative of the effective jump is singular at zero jump, which makes Newton cycle even
    # for the interface-fitted model unless the damage uses the jump of the previous step
    lag_displacement_jump = true
    boundary = 'Block1_Block2'
  []
[]
