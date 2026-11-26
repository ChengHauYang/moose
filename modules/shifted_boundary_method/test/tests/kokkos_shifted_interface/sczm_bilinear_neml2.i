!include sczm_bilinear_common.i

[Kernels]
  [solid_x]
    type = KokkosIsotropicElasticity
    variable = ux
    component = 0
    displacements = 'ux uy'
    youngs_modulus = ${E}
    poissons_ratio = ${nu}
  []
  [solid_y]
    type = KokkosIsotropicElasticity
    variable = uy
    component = 1
    displacements = 'ux uy'
    youngs_modulus = ${E}
    poissons_ratio = ${nu}
  []
[]

# As in sczm_bilinear_kokkos.i, this gamma reproduces the unstabilized host interface
[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  use_kokkos = true
  stabilization = weighted_traction
  stabilization_gamma = 5e8
[]

# The NEML2 gatherer evaluates the jump material on INITIAL, which needs the distance first
[UserObjects/distance]
  execution_order_group = -1
[]

[Materials]
  [elasticity]
    type = KokkosComputeIsotropicElasticity
    displacements = 'ux uy'
    youngs_modulus = ${E}
    poissons_ratio = ${nu}
  []
[]

[NEML2]
  input = 'bilinear_traction_neml2.i'
  eager = true
  [traction]
    model = 'czm'
    executor_name = 'neml2_traction'
    interface = 'Block0_Block1'
    volume = false
    input_backend = kokkos
    output_backend = kokkos
    # The damage and the old jump must be advanced from the auxiliary jump of the generalized
    # Robin solve, which only the executor's on-device state advance does
    manage_state_advance = true
    interface_material_inputs = 'interface_displacement_jump'
    derivatives = 'interface_traction interface_displacement_jump dinterface_traction_djump'
    interface_output_side = primary
  []
[]
