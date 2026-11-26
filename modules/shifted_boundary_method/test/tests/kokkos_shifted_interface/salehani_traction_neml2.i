# NEML2 version of KokkosSalehaniIraniTractionSeparation with the parameters of
# sczm_salehani_weighted_kokkos.i. The native law is memoryless and takes the signed normal jump, so
# the NEML2 law is fed the signed normal jump too; the two then agree while the loading is
# monotonic, where the damage irreversibility cap of the NEML2 law is inactive.
[Models]
  [decompose]
    type = VecComponents
    from = 'interface_displacement_jump'
    to = 'normal_separation tangential_separation_1 tangential_separation_2'
  []
  [traction]
    type = SalehaniIraniTraction
    traction = 'interface_traction'
    normal_characteristic_length = 0.05
    tangential_characteristic_length = 0.05
    normal_strength = 5
    shear_strength = 7
  []
  [czm]
    type = ComposedModel
    models = 'decompose traction'
    additional_outputs = 'damage'
  []
[]
