# Reaction balances the prescribed pressure: strain_zz = -12*(2+x).
[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 2
[]

[Variables]
  [strain_zz]
  []
[]

[Kernels]
  [reaction]
    type = Reaction
    variable = strain_zz
  []
  [pressure]
    type = OutOfPlanePressure
    variable = strain_zz
    function = '2+x'
    factor = 12
    use_displaced_mesh = false
  []
[]

[Postprocessors]
  [load]
    type = ConstantPostprocessor
    value = 4
    outputs = none
  []
  [average]
    type = ElementAverageValue
    variable = strain_zz
  []
  [error]
    type = ElementL2Error
    variable = strain_zz
    function = '-12*(2+x)'
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  csv = true
  execute_on = TIMESTEP_END
[]
