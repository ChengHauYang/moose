# Step 3 of the plasticity_LD_sczm benchmark: CPU NEML2 / GPU Kokkos assembly / CPU PETSc.
#
# Same shifted 3D problem as step1/step2 (finite-deformation total-Lagrangian crystal plasticity in
# the bulk of both blocks, coupled on the same displacement DOFs to a shifted cohesive-zone
# interface on the angled true interface). The bulk stress divergence is now assembled with the
# Kokkos total-Lagrangian path and the cohesive-zone interface with the Kokkos SCZM path, so the
# element/interface assembly runs on the GPU. The two NEML2 constitutive updates stay on the CPU
# (device = cpu) and MOOSE/PETSc assembly results are solved on the CPU; only the Kokkos assembly
# changes from step1/step2.
#
# Bulk Kokkos TL path mirrors solid_mechanics/.../kokkos/plasticity_LD/step3: TorchDeformationGradient
# feeds F = I + grad(u) to the bulk NEML2 executor; NEML2ToKokkosFullRankTwo/Four pull back PK2 and
# dS/dF; KokkosComputeLagrangianStressCustomPK2 forms (PK1, dP/dgrad u) for KokkosTotalLagrangianStress-
# Divergence. Interface Kokkos SCZM path mirrors kokkos_shifted_interface/sczm_angled_3d_kokkos.i but
# uses the crystal-plasticity PK1 interface model (exact_kinematics_pk1_neml2.i) with output_backend =
# kokkos so the directional correction consumes PK1 and d(PK1)/dF on the device.

N = 2
DT = 5e-3
NUM_STEPS = 5

theta = 60
theta_rad = '${fparse theta * pi / 180}'

# 'displacements' is set per object (kernels, SCZM physics, deformation-gradient gatherer, custom-PK2
# material) rather than in [GlobalParams]. Placing it in [GlobalParams] makes it reach the Mesh-level
# CreateDisplacedProblemAction, whose use_displaced_mesh defaults to true, which builds a displaced
# problem; the Kokkos backend rejects a displaced mesh. This is a total-Lagrangian formulation
# evaluated entirely in the reference configuration, so no displaced mesh is needed.

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 3
    nx = '${fparse 2 * N}'
    ny = ${N}
    nz = ${N}
    xmax = 2
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '1 0 0'
    top_right = '2 1 1'
    block_id = 1
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = right_block
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[NEML2]
  eager = true
  [bulk]
    input = '../../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i'
    model = 'model'
    device = 'cpu'
    executor_name = 'neml2'
    input_kernels = 'deformation_gradient'
    # Auto-retrieve exactly one output -- the scalar slip_hardening -- and bridge it to a Kokkos Real
    # material property (NEML2ToKokkosRealMaterialProperty) on the volume blocks, so the Kokkos
    # slip-hardening postprocessors read the same per-QP values the host steps read. auto_output must
    # stay true for output_variables to take effect (it is the filter on the auto-retrieved set);
    # restricting it to slip_hardening keeps neml2_stress out of the auto set, since neml2_stress and
    # its F derivative are pulled by the explicit [Materials] retrievers below instead.
    auto_output = true
    output_backend = kokkos
    output_variables = 'slip_hardening'
    manage_state_advance = true
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
  []
  [interface]
    input = 'exact_kinematics_pk1_neml2.i'
    model = 'interface_model'
    device = 'cpu'
    executor_name = 'neml2_interface'
    # orientation is a fixed constant baked into the interface model (MRPConstantParameter in
    # exact_kinematics_pk1_neml2.i), so deformation_gradient is the only gathered input. It is
    # gathered device-side from the KokkosComputeDeformationGradient block material property on the
    # interface primary/neighbor sides (KokkosInterfaceQuantityToNEML2), which the Kokkos input
    # backend requires (full rank-two MATERIAL inputs only).
    input_backend = kokkos
    input_types = 'MATERIAL'
    inputs = 'deformation_gradient'
    derivatives = 'pk1_stress deformation_gradient pk1_jacobian'
    output_backend = kokkos
    # The Kokkos output backend maps only rank-two/rank-four outputs. The crystal-plasticity model
    # also emits the scalar slip_hardening (Real), which Kokkos cannot map, so restrict the
    # auto-exported interface outputs to the two tensors the SCZM directional correction and the
    # stateful plastic-state seed need. slip_hardening is still computed inside NEML2; it is just not
    # bridged to a Kokkos material on the interface.
    output_variables = 'pk1_stress plastic_deformation_gradient'
    interface = 'Block0_Block1'
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
    displacements = 'disp_x disp_y disp_z'
    # F-bar stabilization is disabled consistently across all six steps of the benchmark.
    stabilize_strain = false
  []
  [distance]
    type = BoundaryShortestDistanceToSurface
    surfaces = true_interface
    boundary = 'Block0_Block1'
    execute_on = INITIAL
    # Run before the NEML2 interface objects (execution_order_group = -1) so the stateful interface
    # state initialization at INITIAL finds the surrogate-to-true distance already computed.
    execution_order_group = -2
  []
[]

[Kernels]
  [stress_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_x
    component = 0
    displacements = 'disp_x disp_y disp_z'
    stabilize_strain = false
  []
  [stress_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_y
    component = 1
    displacements = 'disp_x disp_y disp_z'
    stabilize_strain = false
  []
  [stress_z]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_z
    component = 2
    displacements = 'disp_x disp_y disp_z'
    stabilize_strain = false
  []
[]

[Physics]
  [SolidMechanics]
    [ShiftedCohesiveZone]
      [sczm]
        boundary = 'Block0_Block1'
        displacements = 'disp_x disp_y disp_z'
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
  # Device-side reference-configuration deformation gradient (full 3x3 rank-two) on both volume
  # blocks. The Kokkos interface input gatherer (KokkosInterfaceQuantityToNEML2) reads this block
  # material property on the interface primary and neighbor sides and feeds F = I + grad(u) to the
  # interface NEML2 executor without any host strain computation or Host<->Device copy.
  [interface_deformation_gradient]
    type = KokkosComputeDeformationGradient
    displacements = 'disp_x disp_y disp_z'
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
    displacements = 'disp_x disp_y disp_z'
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
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad})'
  []
  [loading]
    type = ParsedFunction
    expression = t
  []
[]

[BCs]
  [xfix]
    type = KokkosDirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [yfix]
    type = KokkosDirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [zfix]
    type = KokkosDirichletBC
    variable = disp_z
    boundary = back
    value = 0
  []
  [xdisp]
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
    parameter = 'BCs/xdisp/value'
    function = loading
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
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  automatic_scaling = true
  dt = ${DT}
  num_steps = ${NUM_STEPS}
  residual_and_jacobian_together = true
  nl_abs_tol = 1e-10
[]

[Postprocessors]
  # Kokkos postprocessors reading the same per-QP slip_hardening Kokkos Real material property that
  # the bulk NEML2 output backend bridges (NEML2ToKokkosRealMaterialProperty), over the same volume
  # QPs as the host steps, so avg/max match Steps 1-2 without a Host<->Device copy for the reduction.
  [max_slip_hardening]
    type = KokkosElementExtremeMaterialProperty
    mat_prop = slip_hardening
    value_type = max
  []
  [avg_slip_hardening]
    type = KokkosElementAverageMaterialProperty
    mat_prop = slip_hardening
  []
  [avg_disp_x_block1]
    type = KokkosElementAverageValue
    variable = disp_x
    block = 1
  []
[]

[AuxVariables]
  [slip_hardening_elem]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [slip_hardening_elem]
    type = KokkosMaterialRealAux
    variable = slip_hardening_elem
    property = slip_hardening
    execute_on = TIMESTEP_END
  []
[]

[Outputs]
  exodus = true
  csv = true
[]
