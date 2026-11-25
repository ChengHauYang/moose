# Parameters of the native bilinear law; bilinear_traction_neml2.i repeats them for the NEML2 version
[Materials]
  [local_traction]
    boundary = 'Block0_Block1'
    penalty_stiffness = 100
    GI_c = 1
    GII_c = 2
    normal_strength = 5
    shear_strength = 7
    eta = 2.2
  []
[]
