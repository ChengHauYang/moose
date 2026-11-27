# Linear traction-separation law under shear, small strain

[Materials]
  [czm]
    type = PureElasticTractionSeparation
    # 1e5 times the bulk Young's modulus
    normal_stiffness = 1e8
    tangent_stiffness = 1e8
    boundary = 'Block1_Block2'
  []
[]

[BCs]
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'left'
    value = 0
  []
  [right_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'right'
    value = 0
  []
  [shear]
    type = FunctionDirichletBC
    variable = disp_y
    boundary = 'right'
    function = '-1e-2*t'
    preset = false
  []
[]
