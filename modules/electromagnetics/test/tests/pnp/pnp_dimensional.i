# Dimensional Galerkin PNP weak form without fluid advection.
# For each species s:
#   (w, dc_s/dt) + (grad(w), D_s grad(c_s) + D_s z_s F c_s grad(phi) / (R T)) = 0
# and for the potential:
#   (grad(w), epsilon grad(phi)) - (w, F sum_s z_s c_s) = 0.
# Concentration is in mol/m^3. Natural boundaries impose zero total species flux.

faraday = 96485.33
gas_constant = 8.314
temperature = 298
positive_diffusivity = 1e-9
negative_diffusivity = 2e-9
permittivity = 6.95e-10
positive_mobility = '${fparse positive_diffusivity * faraday / (gas_constant * temperature)}'
negative_mobility = '${fparse negative_diffusivity * faraday / (gas_constant * temperature)}'
potential_amplitude = '${fparse faraday * 2e-4 / (permittivity * (pi / 1e-6)^2)}'

[Mesh]
  [line]
    type = GeneratedMeshGenerator
    dim = 1
    nx = 40
    xmin = 0
    xmax = 1e-6
  []
[]

[Variables]
  [c_positive]
  []
  [c_negative]
  []
  [potential]
  []
[]

[Functions]
  [positive_initial]
    type = ParsedFunction
    expression = '1 + 1e-4 * sin(pi * x / 1e-6)'
  []
  [negative_initial]
    type = ParsedFunction
    expression = '1 - 1e-4 * sin(pi * x / 1e-6)'
  []
  [potential_initial]
    type = ParsedFunction
    expression = '${potential_amplitude} * (sin(pi * x / 1e-6) + pi * x / 1e-6)'
  []
[]

[ICs]
  [positive]
    type = FunctionIC
    variable = c_positive
    function = positive_initial
  []
  [negative]
    type = FunctionIC
    variable = c_negative
    function = negative_initial
  []
  [potential]
    type = FunctionIC
    variable = potential
    function = potential_initial
  []
[]

[Kernels]
  [positive_time]
    type = ADTimeDerivative
    variable = c_positive
  []
  [positive_diffusion]
    type = ADMatDiffusion
    variable = c_positive
    diffusivity = ${positive_diffusivity}
  []
  [positive_migration]
    type = ADConservativeAdvection
    variable = c_positive
    velocity_as_variable_gradient = potential
    velocity_scalar_coef = '${fparse -positive_mobility}' # -D_positive z_positive F / (R T)
  []

  [negative_time]
    type = ADTimeDerivative
    variable = c_negative
  []
  [negative_diffusion]
    type = ADMatDiffusion
    variable = c_negative
    diffusivity = ${negative_diffusivity}
  []
  [negative_migration]
    type = ADConservativeAdvection
    variable = c_negative
    velocity_as_variable_gradient = potential
    velocity_scalar_coef = ${negative_mobility} # -D_negative z_negative F / (R T)
  []

  [potential_diffusion]
    type = ADMatDiffusion
    variable = potential
    diffusivity = ${permittivity}
  []
  [positive_charge]
    type = ADCoupledForce
    variable = potential
    v = c_positive
    coef = ${faraday} # F z_positive
  []
  [negative_charge]
    type = ADCoupledForce
    variable = potential
    v = c_negative
    coef = '${fparse -faraday}' # F z_negative
  []
[]

[BCs]
  [potential_reference]
    type = ADDirichletBC
    variable = potential
    boundary = left
    value = 0
  []
[]

[Postprocessors]
  [positive_inventory]
    type = ElementIntegralVariablePostprocessor
    variable = c_positive
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [negative_inventory]
    type = ElementIntegralVariablePostprocessor
    variable = c_negative
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [potential_l2]
    type = ElementL2Norm
    variable = potential
    execute_on = 'INITIAL TIMESTEP_END'
  []
[]

[Executioner]
  type = Transient
  scheme = bdf2
  start_time = 0
  end_time = 1e-4
  dt = 2e-5
  solve_type = NEWTON
  automatic_scaling = true
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-12
[]

[Outputs]
  file_base = pnp_dimensional_out
  csv = true
[]
