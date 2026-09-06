# Linear Debye-Huckel electro-osmotic flow in a 2D periodic channel.
# The equilibrium fields are
#   phi = zeta cosh((y - 1) / lambda) / cosh(1 / lambda),
#   c_positive = 1 - phi, c_negative = 1 + phi,
#   velocity_x = kappa E (phi - zeta).

lambda = 0.1
zeta = -0.5
kappa = 0.5
electric_field = 1
permittivity = '${fparse 2 * lambda^2}'
charge_force = '${fparse kappa * electric_field / lambda^2}'

[Mesh]
  [channel]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = 0
    xmax = 2
    ymin = 0
    ymax = 2
    nx = 8
    ny = 64
  []
  second_order = true
  [pressure_pin]
    type = ExtraNodesetGenerator
    input = channel
    new_boundary = pressure_pin
    nodes = 4 # Bottom-wall midpoint for this structured mesh.
  []
[]

[Variables]
  [c_positive]
  []
  [c_negative]
  []
  [potential]
  []
  [velocity]
    family = LAGRANGE_VEC
    order = SECOND
  []
  [pressure]
  []
[]

[Kernels]
  # Linearized electrochemical equilibrium: c_positive + phi = 1 and
  # c_negative - phi = 1. These are the zero-flux Debye-Huckel limits
  # of the two steady Nernst-Planck equations.
  [positive_reaction]
    type = ADReaction
    variable = c_positive
  []
  [positive_potential]
    type = ADCoupledForce
    variable = c_positive
    v = potential
    coef = -1
  []
  [positive_bulk]
    type = ADBodyForce
    variable = c_positive
    value = 1
  []

  [negative_reaction]
    type = ADReaction
    variable = c_negative
  []
  [negative_potential]
    type = ADCoupledForce
    variable = c_negative
    v = potential
    coef = 1
  []
  [negative_bulk]
    type = ADBodyForce
    variable = c_negative
    value = 1
  []

  [potential_diffusion]
    type = ADMatDiffusion
    variable = potential
    diffusivity = ${permittivity}
  []
  [positive_poisson_charge]
    type = ADCoupledForce
    variable = potential
    v = c_positive
    coef = 1
  []
  [negative_poisson_charge]
    type = ADCoupledForce
    variable = potential
    v = c_negative
    coef = -1
  []

  [mass]
    type = INSADMass
    variable = pressure
  []
  [momentum_viscous]
    type = INSADMomentumViscous
    variable = velocity
  []
  [momentum_pressure]
    type = INSADMomentumPressure
    variable = velocity
    pressure = pressure
    integrate_p_by_parts = true
  []
  [electric_body_force]
    type = INSADMomentumCoupledForce
    variable = velocity
    vector_function = electric_body_force
  []
[]

[BCs]
  [potential_walls]
    type = ADDirichletBC
    variable = potential
    boundary = 'bottom top'
    value = ${zeta}
  []
  [positive_walls]
    type = ADDirichletBC
    variable = c_positive
    boundary = 'bottom top'
    value = '${fparse 1 - zeta}'
  []
  [negative_walls]
    type = ADDirichletBC
    variable = c_negative
    boundary = 'bottom top'
    value = '${fparse 1 + zeta}'
  []
  [no_slip]
    type = ADVectorFunctionDirichletBC
    variable = velocity
    boundary = 'bottom top'
  []
  [pin_pressure]
    type = ADDirichletBC
    variable = pressure
    boundary = pressure_pin
    value = 0
  []
  [Periodic]
    [x]
      variable = 'c_positive c_negative potential velocity pressure'
      auto_direction = x
    []
  []
[]

[Materials]
  [fluid_properties]
    type = ADGenericConstantMaterial
    prop_names = 'rho mu'
    prop_values = '1 1'
  []
  [ins]
    type = INSADMaterial
    velocity = velocity
    pressure = pressure
  []
[]

[Functions]
  [exact_potential]
    type = ParsedFunction
    expression = 'zeta * cosh((y - 1) / lambda) / cosh(1 / lambda)'
    symbol_names = 'zeta lambda'
    symbol_values = '${zeta} ${lambda}'
  []
  [exact_positive]
    type = ParsedFunction
    expression = '1 - zeta * cosh((y - 1) / lambda) / cosh(1 / lambda)'
    symbol_names = 'zeta lambda'
    symbol_values = '${zeta} ${lambda}'
  []
  [exact_negative]
    type = ParsedFunction
    expression = '1 + zeta * cosh((y - 1) / lambda) / cosh(1 / lambda)'
    symbol_names = 'zeta lambda'
    symbol_values = '${zeta} ${lambda}'
  []
  [exact_velocity_x]
    type = ParsedFunction
    expression = 'kappa * electric_field * (zeta * cosh((y - 1) / lambda) / cosh(1 / lambda) - zeta)'
    symbol_names = 'zeta lambda kappa electric_field'
    symbol_values = '${zeta} ${lambda} ${kappa} ${electric_field}'
  []
  [electric_body_force]
    type = ParsedVectorFunction
    expression_x = '-charge_force * zeta * cosh((y - 1) / lambda) / cosh(1 / lambda)'
    expression_y = 0
    symbol_names = 'charge_force zeta lambda'
    symbol_values = '${charge_force} ${zeta} ${lambda}'
  []
[]

[Postprocessors]
  [potential_l2]
    type = ElementL2Error
    variable = potential
    function = exact_potential
  []
  [positive_l2]
    type = ElementL2Error
    variable = c_positive
    function = exact_positive
  []
  [negative_l2]
    type = ElementL2Error
    variable = c_negative
    function = exact_negative
  []
  [velocity_l2]
    type = ElementVectorL2Error
    variable = velocity
    function_x = exact_velocity_x
  []
  [pressure_l2]
    type = ElementL2Norm
    variable = pressure
  []
[]

[UserObjects]
  [error_check]
    type = Terminator
    expression = 'potential_l2 > 2e-3 | positive_l2 > 2e-3 | negative_l2 > 2e-3 | velocity_l2 > 2e-3 | pressure_l2 > 1e-8'
    error_level = ERROR
    execute_on = FINAL
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  automatic_scaling = true
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-12
  petsc_options_iname = '-pc_type -pc_factor_shift_type'
  petsc_options_value = 'lu NONZERO'
[]

[Outputs]
  csv = true
  exodus = true
  execute_on = FINAL
[]
