# Step 5 of the plasticity_LD_sczm_grain_boundaries benchmark: GPU NEML2 / GPU Kokkos assembly /
# GPU PETSc (AIJKOKKOS).
#
# Same polycrystal problem as steps 1-4. Identical to step4 except the PETSc linear algebra also runs
# on the GPU: the Jacobian is a MATAIJKOKKOS matrix and the vectors are -vec_type kokkos. The
# preconditioner is still hypre boomeramg, so step5 vs step6 compares the matrix backend (AIJKOKKOS
# here, MATHYPRE in step6) under one preconditioner, as in
# plasticity_LD_sczm/step5_ld_sczm_full_gpu.i. Run with: -vec_type kokkos -nl0_mat_type aijkokkos.
#
# With two displacement variables, TorchDeformationGradient and KokkosComputeDeformationGradient
# leave F_zz = 1, the same plane-strain deformation gradient the host steps use.

ny = 6
nx = '${fparse ny * 3 / 2}'
# See step1 for why the reference loading is applied in a quarter of the reference time step.
DT = 0.25
NUM_STEPS = 20

interfaces = 'grain1_grain2 grain1_grain4 grain1_grain5 grain2_grain4 grain3_grain4 grain3_grain5 grain4_grain5'

# 'displacements' is set per object rather than in [GlobalParams]: in [GlobalParams] it reaches the
# Mesh-level CreateDisplacedProblemAction and builds a displaced problem, which the Kokkos backend
# rejects. The total-Lagrangian formulation needs no displaced mesh.

[Mesh]
  [boundary_mesh]
    type = FileMeshGenerator
    file = '../../grain_boundary_case/grain_boundary_only.msh'
    save_with_name = boundary_mesh
  []
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = -0.6
    xmax = 0.6
    ymin = -0.4
    ymax = 0.4
    nx = ${nx}
    ny = ${ny}
    subdomain_ids = 10
  []
  [grain_gen]
    type = SubdomainGrainIDGenerator
    input = gen
    boundary_mesh = boundary_mesh
    lambda = 1
  []
  [break7]
    type = BreakMeshByBlockGenerator
    input = grain_gen
    split_interface = true
    add_interface_on_two_sides = true
  []
  add_subdomain_ids = '10'
  add_subdomain_names = 'block_10'
  final_generator = 'break7'
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
[]

[NEML2]
  eager = true
  [bulk]
    input = '../../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i'
    model = 'model'
    device = 'cuda'
    executor_name = 'neml2'
    input_kernels = 'deformation_gradient'
    # Bridge only the scalar slip_hardening to a Kokkos material property for the postprocessors;
    # neml2_stress and its F derivative are pulled by the explicit [Materials] retrievers below (see
    # plasticity_LD_sczm/step3_ld_sczm_kokkos_cpu_neml2.i).
    auto_output = true
    output_backend = kokkos
    output_variables = 'slip_hardening'
    manage_state_advance = true
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
  []
  [interface]
    input = '../plasticity_LD_sczm/exact_kinematics_pk1_neml2.i'
    model = 'interface_model'
    device = 'cuda'
    executor_name = 'neml2_interface'
    # orientation is baked into the interface model, so the deformation gradient from
    # KokkosComputeDeformationGradient is the only gathered input (the Kokkos input backend accepts
    # full rank-two MATERIAL inputs only).
    input_backend = kokkos
    input_types = 'MATERIAL'
    inputs = 'deformation_gradient'
    derivatives = 'pk1_stress deformation_gradient pk1_jacobian'
    output_backend = kokkos
    # The Kokkos output backend maps only rank-two/rank-four outputs, so the scalar slip_hardening
    # is left out of the interface exports.
    output_variables = 'pk1_stress plastic_deformation_gradient'
    interface = ${interfaces}
    interface_only = true
    manage_state_advance = true
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
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
    # F-bar stabilization is disabled consistently across all six steps of the benchmark.
    stabilize_strain = false
  []
  # Explicit interface manager and distance object; see step1 for the execution_order_group choice.
  [manager]
    type = SBMInterfaceManager
    complete_interface_mesh = boundary_mesh
    execution_order_group = -3
  []
  [distance]
    type = BoundaryShortestDistanceToSurface
    manager = manager
    boundary = ${interfaces}
    execute_on = INITIAL
    execution_order_group = -2
  []
[]

[Kernels]
  [stress_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_x
    component = 0
    displacements = 'disp_x disp_y'
    stabilize_strain = false
  []
  [stress_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_y
    component = 1
    displacements = 'disp_x disp_y'
    stabilize_strain = false
  []
[]

[Physics]
  [SolidMechanics]
    [ShiftedCohesiveZone]
      [sczm]
        boundary = ${interfaces}
        displacements = 'disp_x disp_y'
        sbm_distance_uo = distance
        use_kokkos = true
        directional_correction = true
        no_shifted = false
        stress = pk1_stress
        tangent = pk1_jacobian
        tangent_definition = pk1_wrt_deformation_gradient
      []
    []
  []
[]

[Materials]
  # Device-side deformation gradient read by the Kokkos interface input gatherer on the primary and
  # neighbor sides of each grain boundary.
  [interface_deformation_gradient]
    type = KokkosComputeDeformationGradient
    displacements = 'disp_x disp_y'
    deformation_gradient = deformation_gradient
  []
  [stress]
    type = NEML2ToKokkosFullRankTwoMaterialProperty
    neml2_executor = neml2
    from_neml2 = neml2_stress
    to_moose = neml2_stress
  []
  [tangent]
    type = NEML2ToKokkosFullRankFourMaterialProperty
    neml2_executor = neml2
    from_neml2 = neml2_stress
    neml2_input_derivative = deformation_gradient
    to_moose = dneml2_stress_ddeformation_gradient
  []
  [pk1]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'disp_x disp_y'
    custom_pk2_stress = neml2_stress
    custom_pk2_jacobian = dneml2_stress_ddeformation_gradient
    stabilize_strain = false
  []
  [initial_orientation]
    type = GenericConstantRealVectorValue
    vector_name = orientation
    vector_values = '-0.54412095 -0.34931944 0.12600655'
  []
  [initial_plastic_defgrad]
    type = GenericConstantRankTwoTensor
    tensor_name = initial_plastic_defgrad
    tensor_values = '1 1 1'
  []
  # penalty_stiffness of the reference BiLinearMixedModeTraction, used for both directions.
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = ${interfaces}
    normal_stiffness = 2e3
    tangent_stiffness = 2e3
  []
[]

[Functions]
  [displacement_with_time]
    type = ParsedFunction
    expression = '1e-2 * t'
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
  # KokkosDirichletBC has no function parameter; the control below drives its value.
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

# hypre boomeramg + gmres, line_search=none: the single preconditioner shared by all six steps (see
# step1). The full pipeline runs on the GPU here. The HYPRE Umpire device pool is capped at 256 MiB
# (HYPRE defaults to a 4 GiB pool on GPU, which competes with NEML2/PyTorch for device memory).
[Executioner]
  type = Transient
  solve_type = NEWTON
  line_search = none
  petsc_options_iname = '-pc_type -pc_hypre_type -ksp_type -hypre_umpire_device_pool_size'
  petsc_options_value = 'hypre boomeramg    gmres     256'
  automatic_scaling = true
  dt = ${DT}
  num_steps = ${NUM_STEPS}
  residual_and_jacobian_together = true
  # Identical tight tolerances in all six steps (see step1).
  nl_rel_tol = 1e-12
  nl_abs_tol = 1e-12
  l_tol = 1e-10
[]

[Postprocessors]
  # Kokkos counterparts of the step1 postprocessors, over the same volume quadrature points.
  [max_slip_hardening]
    type = KokkosElementExtremeMaterialProperty
    mat_prop = slip_hardening
    value_type = max
  []
  [avg_slip_hardening]
    type = KokkosElementAverageMaterialProperty
    mat_prop = slip_hardening
  []
  [avg_disp_x_grain4]
    type = KokkosElementAverageValue
    variable = disp_x
    block = 4
  []
[]

[Outputs]
  exodus = true
  csv = true
[]
