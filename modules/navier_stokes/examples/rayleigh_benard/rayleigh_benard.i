# Differentially heated square cavity using the linear segregated
# incompressible Navier-Stokes and heat-transfer physics.
#
# The nondimensional free-fall scaling gives
#   nu = sqrt(Pr / Ra), alpha = 1 / sqrt(Ra Pr).
# With rho = cp = 1, mu = nu and k = alpha.

Ra = 1e5
Pr = 1
nx = 64
ny = 64
mu = '${fparse sqrt(Pr / Ra)}'
k = '${fparse 1 / sqrt(Ra * Pr)}'

[Mesh]
  [mesh]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${ny}
    xmin = 0
    xmax = 1
    ymin = 0
    ymax = 1
  []
[]

[Problem]
  linear_sys_names = 'u_system v_system pressure_system energy_system'
  previous_nl_solution_required = true
[]

[Physics]
  [NavierStokes]
    [FlowSegregated/flow]
      velocity_variable = 'vel_x vel_y'
      pressure_variable = pressure
      fluid_temperature_variable = T

      density = 1
      dynamic_viscosity = ${mu}
      initial_velocity = '1e-12 1e-12 0'
      initial_pressure = 0

      boussinesq_approximation = true
      ref_temperature = 0.5
      thermal_expansion = 1
      gravity = '0 -1 0'

      wall_boundaries = 'left right top bottom'
      momentum_wall_types = 'noslip noslip noslip noslip'

      momentum_advection_interpolation = upwind
      orthogonality_correction = false
      momentum_two_term_bc_expansion = false
      pressure_two_term_bc_expansion = false
    []
    [FluidHeatTransferSegregated/energy]
      coupled_flow_physics = flow
      fluid_temperature_variable = T
      solve_for_enthalpy = false

      thermal_conductivity = ${k}
      specific_heat = 1
      initial_temperature = initial_temperature

      energy_wall_types = 'fixed-temperature fixed-temperature heatflux heatflux'
      energy_wall_functors = '1 0 0 0'
      energy_advection_interpolation = upwind
      energy_two_term_bc_expansion = false
      use_nonorthogonal_correction = false
    []
  []
[]

[Functions]
  [initial_temperature]
    type = ParsedFunction
    expression = '1 - x'
  []
[]

[VectorPostprocessors]
  # T is cell-centered, so average the rows bracketing y = 0.5.
  [centerline_lower]
    type = LineValueSampler
    variable = T
    start_point = '${fparse 0.5 / nx} ${fparse 0.5 - 0.5 / ny} 0'
    end_point = '${fparse 1 - 0.5 / nx} ${fparse 0.5 - 0.5 / ny} 0'
    num_points = ${nx}
    sort_by = x
    execute_on = FINAL
  []
  [centerline_upper]
    type = LineValueSampler
    variable = T
    start_point = '${fparse 0.5 / nx} ${fparse 0.5 + 0.5 / ny} 0'
    end_point = '${fparse 1 - 0.5 / nx} ${fparse 0.5 + 0.5 / ny} 0'
    num_points = ${nx}
    sort_by = x
    execute_on = FINAL
  []
[]

[Executioner]
  type = SIMPLE
  rhie_chow_user_object = ins_rhie_chow_interpolator
  momentum_systems = 'u_system v_system'
  pressure_system = pressure_system
  energy_system = energy_system

  momentum_equation_relaxation = 0.7
  pressure_variable_relaxation = 0.3
  energy_equation_relaxation = 0.9
  num_iterations = 5000
  momentum_absolute_tolerance = 1e-8
  pressure_absolute_tolerance = 1e-8
  energy_absolute_tolerance = 1e-8

  momentum_l_abs_tol = 1e-12
  pressure_l_abs_tol = 1e-12
  energy_l_abs_tol = 1e-12
  momentum_l_tol = 0
  pressure_l_tol = 0
  energy_l_tol = 0

  momentum_petsc_options_iname = '-pc_type -pc_hypre_type'
  momentum_petsc_options_value = 'hypre boomeramg'
  pressure_petsc_options_iname = '-pc_type -pc_hypre_type'
  pressure_petsc_options_value = 'hypre boomeramg'
  energy_petsc_options_iname = '-pc_type -pc_hypre_type'
  energy_petsc_options_value = 'hypre boomeramg'

  pin_pressure = true
  pressure_pin_value = 0
  pressure_pin_point = '0 0 0'
  print_fields = false
  continue_on_max_its = true
[]

[Outputs]
  exodus = true
  csv = true
  execute_on = FINAL
[]
