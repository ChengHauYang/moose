!include ../../../../solid_mechanics/test/tests/neml2/viscoplasticity/radial_return_neml2.i

[Models]
  [symmetric_strain]
    type = R2ToSR2
    input = mechanical_strain
    output = neml2_strain
  []
  [full_stress]
    type = SR2ToR2
    input = neml2_stress
    output = sczm_stress
  []
  [interface_model]
    type = ComposedModel
    models = 'symmetric_strain model full_stress'
  []
[]
