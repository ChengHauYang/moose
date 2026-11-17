!include mechanics_base.i

[Variables]
  [speed]
  []
[]

[Kernels]
  [speed]
    type = ADMaterialPropertyValue
    variable = speed
    prop_name = wave_speed
  []
[]

[Materials]
  [density]
    type = ADParsedMaterial
    property_name = density
    coupled_variables = eta1
    expression = '2 + eta1^2'
  []
  [stiffness]
    type = ADParsedMaterial
    property_name = effective_stiffness
    coupled_variables = eta2
    expression = '1 + eta2^2'
  []
  [wave_speed]
    type = ADWaveSpeed
  []
[]
