# Electro-Convective Instability - Stage 2 (full NS-PNP)
# Restarts the fully-developed EDL field from the Stage 1 checkpoint
# (electro_convective_instability_stage1.i) and runs the coupled
# Navier-Stokes + PNP equations. A small tangential velocity seed breaks the
# 1D symmetry of the space-charge layer so the electro-convective instability
# can develop (Alimanestianu et al., Section 2.1).
#
# Run order:
#   combined-opt -i electro_convective_instability_stage1.i
#   combined-opt -i electro_convective_instability.i

lambda = 1e-3
permittivity = '${fparse 2 * lambda^2}'
applied_potential = 120

[Problem]
  type = FEProblem
  restart_file_base = electro_convective_instability_stage1_out_cp/LATEST
  # The velocity IC (new variable) must be allowed even though the underlying
  # restart is active; the concentration/potential fields are left untouched.
  allow_initial_conditions_with_restart = true
[]

[Mesh]
  # Must be identical to Stage 1 for the restart to map fields correctly.
  [near_wall]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = 0
    xmax = 2
    ymin = 0
    ymax = 0.01
    nx = 64
    ny = 80
  []
  [mid]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = 0
    xmax = 2
    ymin = 0.01
    ymax = 0.1
    nx = 64
    ny = 30
  []
  [bulk]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = 0
    xmax = 2
    ymin = 0.1
    ymax = 1
    nx = 64
    ny = 36
  []
  [stitch12]
    type = StitchedMeshGenerator
    inputs = 'near_wall mid'
    stitch_boundaries_pairs = 'top bottom'
  []
  [assembly]
    type = StitchedMeshGenerator
    inputs = 'stitch12 bulk'
    stitch_boundaries_pairs = 'top bottom'
  []
  [pressure_pin]
    type = ExtraNodesetGenerator
    input = assembly
    new_boundary = pressure_pin
    nodes = 0
  []
  second_order = true
[]

[Variables]
  # c_positive, c_negative, potential are loaded from the Stage 1 checkpoint.
  [c_positive]
    scaling = 1e6
  []
  [c_negative]
    scaling = 1e6
  []
  [potential]
    scaling = 1
  []
  [velocity]
    family = LAGRANGE_VEC
    order = SECOND
  []
  [pressure]
  []
[]

[ICs]
  # The EDL fields are loaded from the Stage 1 checkpoint; only the velocity
  # (a new variable) is seeded. The seed is a small tangential roll decaying
  # away from the ion-selective surface, which breaks the 1D symmetry of the
  # ESC layer so the electro-convective instability can develop. (Seeding the
  # concentrations instead would overwrite the restarted ESC field.)
  [velocity_seed]
    type = VectorFunctionIC
    variable = velocity
    function = velocity_seed
  []
[]

[Functions]
  [velocity_seed]
    type = ParsedVectorFunction
    expression_x = 'seed * sin(2*pi*x/2) * exp(-y/0.05)'
    expression_y = 'seed * cos(2*pi*x/2) * exp(-y/0.05)'
    symbol_names = 'seed'
    symbol_values = '1e-2'
  []
[]

[Kernels]
  # -------- PNP Equations --------
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
  [positive_advection]
    type = ADConservativeAdvection
    variable = c_positive
    velocity_variable = velocity
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
  [negative_advection]
    type = ADConservativeAdvection
    variable = c_negative
    velocity_variable = velocity
  []

  # Poisson equation for potential
  [potential_diffusion]
    type = ADMatDiffusion
    variable = potential
    diffusivity = ${permittivity}
  []
  [positive_charge]
    type = ADCoupledForce
    variable = potential
    v = c_positive
    coef = 1
  []
  [negative_charge]
    type = ADCoupledForce
    variable = potential
    v = c_negative
    coef = -1
  []

  # -------- Navier-Stokes Equations --------
  [mass]
    type = INSADMass
    variable = pressure
  []
  [momentum_time]
    type = INSADMomentumTimeDerivative
    variable = velocity
  []
  [momentum_advection]
    type = INSADMomentumAdvection
    variable = velocity
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
  # Electrohydrodynamic force: F = rho_e * E = (c_positive - c_negative) * (-grad potential)
  [ehd_force]
    type = INSADMomentumEHDForce
    variable = velocity
    concentrations = 'c_positive c_negative'
    valences = '1 -1'
    potential = potential
  []
[]

[BCs]
  # Top Wall (y = 1) - stationary reservoir
  [top_velocity]
    type = ADVectorFunctionDirichletBC
    variable = velocity
    boundary = top
  []
  [top_potential]
    type = ADDirichletBC
    variable = potential
    boundary = top
    value = ${applied_potential}
  []
  [top_positive]
    type = ADDirichletBC
    variable = c_positive
    boundary = top
    value = 1
  []
  [top_negative]
    type = ADDirichletBC
    variable = c_negative
    boundary = top
    value = 1
  []

  # Bottom Wall (y = 0) - ion-selective surface
  [bottom_velocity]
    type = ADVectorFunctionDirichletBC
    variable = velocity
    boundary = bottom
  []
  [bottom_potential]
    type = ADDirichletBC
    variable = potential
    boundary = bottom
    value = 0
  []
  [bottom_positive]
    type = ADDirichletBC
    variable = c_positive
    boundary = bottom
    value = 2
  []
  # c_negative has zero-flux on bottom by default (natural boundary condition)

  # Side Walls (Periodic)
  [Periodic]
    [x]
      variable = 'c_positive c_negative potential velocity pressure'
      auto_direction = x
    []
  []

  # Pressure Pin
  [pin_pressure]
    type = ADDirichletBC
    variable = pressure
    boundary = pressure_pin
    value = 0
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

[AuxVariables]
  [vel_mag]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[AuxKernels]
  [vel_mag_aux]
    type = VectorVariableMagnitudeAux
    variable = vel_mag
    vector_variable = velocity
  []
[]

[Postprocessors]
  [vel_mag_avg]
    type = ElementAverageValue
    variable = vel_mag
    execute_on = 'TIMESTEP_END'
  []
  [vel_mag_max]
    type = ElementExtremeValue
    variable = vel_mag
    value_type = max
    execute_on = 'TIMESTEP_END'
  []
  [charge_density_l2]
    type = ElementL2Norm
    variable = c_positive
    execute_on = 'TIMESTEP_END'
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  dt = 0.01
  end_time = 12
  automatic_scaling = true
  nl_rel_tol = 1e-8
  nl_abs_tol = 1e-10
  petsc_options_iname = '-pc_type -pc_factor_shift_type'
  petsc_options_value = 'lu NONZERO'
[]

[Outputs]
  exodus = true
  csv = true
[]