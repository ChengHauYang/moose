# Mixed-mode bilinear traction-separation law under tension past the peak traction into
# softening, small strain

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
    boundary = 'Block1_Block2'
  []
[]

[BCs]
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'bottom'
    value = 0
  []
  [pull]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = 'right'
    function = '0.2*t'
    preset = false
  []
[]
