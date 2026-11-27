# Linear traction-separation law, small strain

[Materials]
  [czm]
    type = PureElasticTractionSeparation
    # 1e5 times the bulk Young's modulus
    normal_stiffness = 1e8
    tangent_stiffness = 1e8
    boundary = 'Block1_Block2'
  []
[]
