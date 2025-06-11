
# [GlobalParams]
#   block = '1 2 3 4 5 6'
# []

[Problem]
  solve = false
[]

[Mesh]
  [gmg]
    type = FileMeshGenerator
    file = "weld_mesh_cutout_scale.msh"
  []
[]

[Variables]
  [cond]
  []
[]

[Functions]
  # [h_quench]
  #   type = ParsedFunction
  #   expression = 't < 1000 ? 16742.2 : 0.0005'
  # []

  [T_sink]
    type = ConstantFunction
    value = 373.15
  []
  [thermal_k_fn]
    type = PiecewiseLinear
    x = '20 100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400'
    y = '12.69 13.93 15.48 17.03 18.58 20.13 21.68 23.23 24.78 26.33 27.88 29.43 30.98 32.53 34.08'
  []
  [specific_heat_fn]
    type = PiecewiseLinear
    x = '20 100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400'
    y = '490 508 532 555 580 603 627 650 650 650 650 650 650 650 650'
  []

  [thermal_expansion_fn]
    type = PiecewiseLinear
    x = '20 100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400'
    y = '15.44 16.01 16.67 17.29 17.87 18.41 18.91 19.37 19.78 20.16 20.49 20.78 21.03 21.24 21.41'
  []

  [youngs_modulus_fn]
    type = PiecewiseLinear
    x = '20 100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400'
    y = '204.5 196.7 188.1 180.2 172.5 164.6 156.0 146.1 134.6 120.8 104.4 84.8 61.5 34.1 2.0' # GPa
  []
[]

# [BCs] # Boundary Conditions Start
#   # Heat transfer coefficient on outer cylinder radius and ends
#   [convective_surface] # Convective Start
#     type = ConvectiveFluxBC # Convective flux, e.g. q'' = h*(Tw - Tf)
#     boundary = 'left bottom top right interface1 interface2 interface3 interface4 interface5' # BC applied on every interfaces
#     variable = cond
#     rate = 16742.2 # h = convective heat transfer coefficient (w/m^2-K)[176000 "]
#     #         #  the above h is ~ infinity for present purposes
#     rate_final = 0.0005
#     initial = 373.15 # initial ambient temperature (K)
#     final = 293.15 # final ambient (lab or oven) temperature (K)
#     duration = 1000 # length of time in seconds that it takes the ambient
#     #     temperature to ramp from initial to final
#   [] # Convective End
# [] # BCs END

[Materials]
  # [volumetric_heat] # need to be exactly this name!
  #   type = ADMovingEllipsoidalHeatSource
  #   path = 'path'
  #   power = 1
  #   efficiency = 1
  #   scale = 1
  #   a = 0.035
  #   b = 0.01
  #   outputs = exodus
  # []
  [thermal_k]
    type = ADGenericFunctionMaterial
    prop_names = 'density thermal_conductivity specific_heat'
    prop_values = '7960.0 thermal_k_fn specific_heat_fn'
  []
[]

[Kernels]
  [udot]
    type = ADHeatConductionTimeDerivative
    variable = cond
  []
  [cond]
    type = ADHeatConduction
    variable = cond
    thermal_conductivity = thermal_conductivity
  []
  # [hsource]
  #   type = ADMatHeatSource
  #   material_property = 'volumetric_heat'
  #   variable = cond
  # []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_max_its = 100
  nl_rel_tol = 1e-6
  nl_abs_tol = 1e-10
  dt = 1
  end_time = 1
[]

[Outputs]
  exodus = true
[]
