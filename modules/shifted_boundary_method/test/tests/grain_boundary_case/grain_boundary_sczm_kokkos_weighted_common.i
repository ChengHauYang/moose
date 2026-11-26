# Kokkos total-Lagrangian shifted cohesive zones on the grain boundaries of
# grain_boundary_sczm_mesh_file.i with weighted traction stabilization (automatic beta),
# element-local stress recovery, F-bar, and a stateful NEML2 traction-separation law loaded through
# softening. The including input supplies the true interfaces.

interfaces = 'grain1_grain2 grain1_grain4 grain1_grain5 grain2_grain4 grain3_grain4 grain3_grain5 grain4_grain5'

[Mesh]
  [boundary_mesh]
    type = FileMeshGenerator
    file = 'grain_boundary_only.msh'
    save_with_name = boundary_mesh
  []
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = -0.6
    xmax = 0.6
    ymin = -0.4
    ymax = 0.4
    nx = 9
    ny = 6
    subdomain_ids = 10
  []
  [grain_gen]
    type = SubdomainGrainIDGenerator
    input = gen
    boundary_mesh = boundary_mesh
    lambda = 1
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = grain_gen
  []
  add_subdomain_ids = '10'
  add_subdomain_names = 'block_10'
  final_generator = 'break'
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone]
  [czm]
    boundary = ${interfaces}
    displacements = 'disp_x disp_y'
    strain = FINITE
    use_kokkos = true
    volumetric_locking_correction = true
    stabilization = weighted_traction
    stress_recovery = element_linear
    tangent_definition = pk2_wrt_deformation_gradient
  []
[]

[NEML2]
  eager = true
  [bulk]
    input = '../kokkos_shifted_interface/sczm_tl_fbar_neml2.i'
    model = bulk_model
    executor_name = neml2_bulk
    input_kernels = deformation_gradient
    auto_output = false
  []
  [traction]
    input = '../kokkos_shifted_interface/bilinear_traction_neml2.i'
    model = czm
    executor_name = neml2_traction
    interface = ${interfaces}
    volume = false
    input_backend = kokkos
    output_backend = kokkos
    # The damage and the old jump are advanced from the auxiliary jump of the generalized Robin
    # solve, which only the executor's on-device state advance does
    manage_state_advance = true
    interface_material_inputs = 'interface_displacement_jump'
    derivatives = 'interface_traction interface_displacement_jump dinterface_traction_djump'
    interface_output_side = primary
  []
[]

[UserObjects]
  [assembly]
    type = TorchAssembly
  []
  [fe]
    type = TorchFEInterpolation
    assembly = assembly
  []
  [deformation_gradient]
    type = TorchDeformationGradient
    assembly = assembly
    fe = fe
    to_neml2 = deformation_gradient
    displacements = 'disp_x disp_y'
    stabilize_strain = true
  []
[]

[Kernels]
  [stress_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_x
    component = 0
    displacements = 'disp_x disp_y'
    stabilize_strain = true
  []
  [stress_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_y
    component = 1
    displacements = 'disp_x disp_y'
    stabilize_strain = true
  []
[]

[Materials]
  [f_bar_average]
    type = KokkosComputeFbarAverage
    displacements = 'disp_x disp_y'
  []
  [bulk_pk2]
    type = NEML2ToKokkosFullRankTwoMaterialProperty
    neml2_executor = neml2_bulk
    from_neml2 = neml2_stress
    to_moose = bulk_pk2
  []
  [bulk_pk2_jacobian]
    type = NEML2ToKokkosFullRankFourMaterialProperty
    neml2_executor = neml2_bulk
    from_neml2 = neml2_stress
    neml2_input_derivative = deformation_gradient
    to_moose = bulk_pk2_jacobian
  []
  [bulk_pk1]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'disp_x disp_y'
    custom_pk2_stress = bulk_pk2
    custom_pk2_jacobian = bulk_pk2_jacobian
    stabilize_strain = true
  []
[]

[Functions]
  [displacement_with_time]
    type = ParsedFunction
    # The interfaces soften from the second step
    expression = '6e-2 * t'
  []
[]

[BCs]
  [anchor_x]
    type = KokkosDirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [anchor_y]
    type = KokkosDirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [displacement_x_right]
    type = KokkosDirichletBC
    variable = disp_x
    boundary = right
    preset = false
    value = 0
  []
[]

[Controls]
  [loading]
    type = RealFunctionControl
    parameter = 'BCs/displacement_x_right/value'
    function = displacement_with_time
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  line_search = none
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-10
  dt = 1
  num_steps = 4
[]

[Outputs]
  exodus = true
[]
