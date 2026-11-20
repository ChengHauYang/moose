# On the unit element, stable density = 6^2 = 36, additional density = 36-4 = 32,
# and wave speed = 6/sqrt(4) = 3.
[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 2
  xmax = 2
[]

[Problem]
  solve = false
[]

[Variables]
  [dummy]
  []
[]

[Materials]
  [constants]
    type = GenericConstantMaterial
    prop_names = 'density effective_stiffness'
    prop_values = '4 6'
  []
  [scaling]
    type = DensityScaling
    true_density = density
    scaled_density = scaled_density
    desired_time_step = 1
    safety_factor = 1
    outputs = all
  []
  [speed]
    type = WaveSpeed
    outputs = all
  []
[]

[Postprocessors]
  [scaled]
    type = ElementAverageValue
    variable = scaled_density
  []
  [additional]
    type = ElementAverageValue
    variable = additional_density
  []
  [speed]
    type = ElementAverageValue
    variable = wave_speed
  []
[]

[Executioner]
  type = Steady
[]

[Outputs]
  csv = true
  execute_on = TIMESTEP_END
[]
