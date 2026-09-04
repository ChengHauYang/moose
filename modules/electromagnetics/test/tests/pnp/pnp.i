# Galerkin part of the nondimensional PNP weak form
# For each species s:
#   (w, dc_s/dt) + (grad(w), grad(c_s) + z_s c_s grad(phi)) = 0
# and for the potential:
#   (grad(w), 2 lambda^2 grad(phi)) - (w, sum_s z_s c_s) = 0.
# Natural boundaries impose zero total species flux. The DendrIon VMS terms are not included.

lambda = 0.1
permittivity = '${fparse 2 * lambda^2}'

[Mesh]
  [line]
    type = GeneratedMeshGenerator
    dim = 1
    nx = 40
    xmin = 0
    xmax = 1
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
    expression = '1 + 0.2 * sin(pi * x)'
  []
  [negative_initial]
    type = ParsedFunction
    expression = '1 - 0.2 * sin(pi * x)'
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
[]

[Kernels]
  [positive_time]
    type = ADTimeDerivative
    variable = c_positive
  []
  [positive_diffusion]
    type = ADDiffusion
    variable = c_positive
  []
  [positive_migration]
    type = ADConservativeAdvection
    variable = c_positive
    velocity_as_variable_gradient = potential
    velocity_scalar_coef = -1 # -z_positive
  []

  [negative_time]
    type = ADTimeDerivative
    variable = c_negative
  []
  [negative_diffusion]
    type = ADDiffusion
    variable = c_negative
  []
  [negative_migration]
    type = ADConservativeAdvection
    variable = c_negative
    velocity_as_variable_gradient = potential
    velocity_scalar_coef = 1 # -z_negative
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
    coef = 1 # z_positive
  []
  [negative_charge]
    type = ADCoupledForce
    variable = potential
    v = c_negative
    coef = -1 # z_negative
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
  end_time = 0.05
  dt = 0.01
  solve_type = NEWTON
  automatic_scaling = true
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-12
[]

[Outputs]
  csv = true
[]
