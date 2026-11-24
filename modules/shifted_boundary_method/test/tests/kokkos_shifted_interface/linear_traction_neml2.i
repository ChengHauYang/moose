# NEML2 version of KokkosPureElasticTractionSeparation with the stiffnesses of sczm_tl_3d_common.i
[Models]
  [decompose]
    type = VecComponents
    from = 'interface_displacement_jump'
    to = 'normal_separation tangential_separation_1 tangential_separation_2'
  []
  [traction]
    type = OrthotropicLinearTraction
    traction = 'interface_traction'
    normal_stiffness = 500
    tangential_stiffness = 300
  []
  [czm]
    type = ComposedModel
    models = 'decompose traction'
  []
[]
