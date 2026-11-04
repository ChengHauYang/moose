# Directly exercises current, old, and older Kokkos interface-property states. Step 2 fails once
# after convergence; the retry must retain only accepted states. The older-state contribution makes
# replacing older with old change the solution after enough accepted steps.

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 4
    ny = 2
    xmax = 1
    ymax = 1
  []
  [left_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '0 0 0'
    top_right = '0.5 1 0'
    block_id = 1
  []
  [interface]
    type = SideSetsBetweenSubdomainsGenerator
    input = left_block
    primary_block = 0
    paired_block = 1
    new_boundary = interface
  []
[]

[Variables]
  [u]
  []
  [v]
  []
  [old_state]
  []
  [old_state_neighbor]
  []
  [older_state]
  []
  [older_state_neighbor]
  []
[]

[Kernels]
  [u_diffusion]
    type = KokkosCoefDiffusion
    variable = u
    coef = 1
  []
  [v_diffusion]
    type = KokkosCoefDiffusion
    variable = v
    coef = 1
  []
  [u_time]
    type = KokkosTimeDerivative
    variable = u
  []
  [v_time]
    type = KokkosTimeDerivative
    variable = v
  []
  [old_state_diffusion]
    type = KokkosCoefDiffusion
    variable = old_state
    coef = 1
  []
  [older_state_diffusion]
    type = KokkosCoefDiffusion
    variable = older_state
    coef = 1
  []
  [old_state_reaction]
    type = KokkosReaction
    variable = old_state
  []
  [older_state_reaction]
    type = KokkosReaction
    variable = older_state
  []
  [old_state_neighbor_diffusion]
    type = KokkosCoefDiffusion
    variable = old_state_neighbor
    coef = 1
  []
  [older_state_neighbor_diffusion]
    type = KokkosCoefDiffusion
    variable = older_state_neighbor
    coef = 1
  []
  [old_state_neighbor_reaction]
    type = KokkosReaction
    variable = old_state_neighbor
  []
  [older_state_neighbor_reaction]
    type = KokkosReaction
    variable = older_state_neighbor
  []
[]

[InterfaceKernels]
  [stateful]
    type = KokkosLinearInterfaceDiffusion
    variable = u
    neighbor_var = v
    boundary = interface
    traction = traction
    dtraction_djump = dtraction_djump
  []
  [observe_old]
    type = KokkosLinearInterfaceDiffusion
    variable = old_state
    neighbor_var = old_state_neighbor
    boundary = interface
    traction = traction_at_old_state
    dtraction_djump = zero_tangent
  []
  [observe_older]
    type = KokkosLinearInterfaceDiffusion
    variable = older_state
    neighbor_var = older_state_neighbor
    boundary = interface
    traction = traction_at_older_state
    dtraction_djump = zero_tangent
  []
[]

[Materials]
  [interface_material]
    type = KokkosTractionSeparationLawInterfaceMaterial
    variable = u
    neighbor_var = v
    boundary = interface
    stiffness = 10
    state_increment = 1
  []
[]

[BCs]
  [u_left]
    type = KokkosDirichletBC
    variable = u
    boundary = left
    value = 1
  []
  [v_left]
    type = KokkosDirichletBC
    variable = v
    boundary = left
    value = 0
  []
  [u_right]
    type = KokkosDirichletBC
    variable = u
    boundary = right
    value = 0
  []
  [v_right]
    type = KokkosDirichletBC
    variable = v
    boundary = right
    value = 1
  []
  [old_state_boundary]
    type = KokkosDirichletBC
    variable = old_state
    boundary = 'left right top bottom'
    value = 0
  []
  [older_state_boundary]
    type = KokkosDirichletBC
    variable = older_state
    boundary = 'left right top bottom'
    value = 0
  []
  [old_state_neighbor_boundary]
    type = KokkosDirichletBC
    variable = old_state_neighbor
    boundary = 'left right top bottom'
    value = 0
  []
  [older_state_neighbor_boundary]
    type = KokkosDirichletBC
    variable = older_state_neighbor
    boundary = 'left right top bottom'
    value = 0
  []
[]

[Postprocessors]
  [failed_steps]
    type = NumFailedTimeSteps
  []
[]

[Problem]
  type = FailingProblem
  fail_steps = 2
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  start_time = 0
  # Three accepted steps provide two distinct history levels while making recovery's half-transient
  # run stop after step 1, so the forced step-2 rejection occurs after recovery starts.
  num_steps = 3
  [TimeStepper]
    type = ConstantDT
    dt = 0.1
    # Keep the forced-failure retry within the Exodiff tolerance of --test-restep, which retries the
    # same dt, so both modes compare accepted-state rollback rather than different time schedules.
    cutback_factor_at_failure = 0.999999
  []
[]

[Outputs]
  exodus = true
  checkpoint = true
[]
