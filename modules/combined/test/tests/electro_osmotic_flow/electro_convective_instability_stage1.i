# Electro-Convective Instability - Stage 1 (PNP-only, develop the EDL)
# Solves the transient Poisson-Nernst-Planck equations (no flow) with the
# ion-selective-surface boundary conditions until the electric double layer is
# fully developed. Stage 2 restarts from the checkpoint, perturbs the developed
# electric double layer, and turns on the coupled Navier-Stokes transport.

lambda = 1e-3
permittivity = '${fparse 2 * lambda^2}'
applied_potential = 120

[Mesh]
  # Three stripes: a very fine layer resolving the Debye length (lambda=1e-3),
  # a transition band for the space-charge/diffusion layer, and a coarse bulk.
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
  second_order = true
[]

[Variables]
  [c_positive]
    scaling = 1e6
  []
  [c_negative]
    scaling = 1e6
  []
  [potential]
    scaling = 1
  []
[]

[ICs]
  # Uniform c=1 with linear phi is an exact discrete steady state of PNP (zero
  # residual every step; nothing evolves). Tiny bounded noise breaks that
  # equilibrium so the space-charge/diffusion layer can develop.
  [positive_noise]
    type = RandomIC
    variable = c_positive
    min = 0.99
    max = 1.01
  []
  [negative_noise]
    type = RandomIC
    variable = c_negative
    min = 0.99
    max = 1.01
  []
  [potential_ic]
    type = FunctionIC
    variable = potential
    function = initial_potential
  []
[]

[Functions]
  [initial_potential]
    type = ParsedFunction
    expression = 'applied_potential * y'
    symbol_names = 'applied_potential'
    symbol_values = '${applied_potential}'
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
    coef = 1
  []
  [negative_charge]
    type = ADCoupledForce
    variable = potential
    v = c_negative
    coef = -1
  []
[]

[BCs]
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

  # Ion-selective surface: grounded potential, fixed cation concentration,
  # zero anion flux is the natural boundary condition of the weak form.
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

  [Periodic]
    [x]
      variable = 'c_positive c_negative potential'
      auto_direction = x
    []
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  dt = 0.01
  end_time = 10
  automatic_scaling = true
  nl_rel_tol = 1e-8
  nl_abs_tol = 1e-10
  petsc_options_iname = '-pc_type -pc_factor_shift_type'
  petsc_options_value = 'lu NONZERO'
[]

[Outputs]
  exodus = true
  csv = true
  checkpoint = true
[]