# Step 1 of the plasticity_LD_sczm benchmark: Host MOOSE / CPU NEML2 / CPU PETSc.
#
# One shifted 3D problem: finite-deformation total-Lagrangian crystal plasticity in the bulk
# (both blocks) coupled on the SAME displacement DOFs to a shifted cohesive-zone interface. The
# surrogate interface is the planar block boundary at x = 1 (Block0_Block1); the true cohesive
# interface is an angled plane, so the surrogate-to-true distance is nonzero and the surrogate
# and true normals are nonparallel, exercising the genuine shifted jump/area/directional
# correction. The directional correction consumes the crystal-plasticity PK1 stress and its
# consistent d(PK1)/dF (emitted by NEML2, see exact_kinematics_pk1_neml2.i).
#
# Formulation: finite-deformation TL bulk + existing small-strain SCZM interface. Steps 2-6 solve
# this identical physical problem; only the compute backend, device, and PETSc config differ.

N = 2
DT = 5e-3
NUM_STEPS = 5

theta = 60
theta_rad = '${fparse theta * pi / 180}'

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
[]

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

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = FINITE
        new_system = true
        add_variables = true
        formulation = TOTAL
        # F-bar stabilization is disabled consistently across all six steps of the benchmark.
        volumetric_locking_correction = false
      []
    []
    [ShiftedCohesiveZone]
      [sczm]
        boundary = 'Block0_Block1'
        sbm_distance_uo = distance
        directional_correction = true
        no_shifted = false
        use_kokkos = false
        stress = pk1_stress
        tangent = pk1_jacobian
        tangent_definition = pk1_wrt_deformation_gradient
      []
    []
  []
[]

[NEML2]
  eager = true
  [bulk]
    input = '../../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i'
    model = 'model'
    device = 'cpu'
    derivatives = 'neml2_stress deformation_gradient'
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
    # Keep the plastic state on-device and seed its initial (old) value from the identity
    # initial_plastic_defgrad through the state initializer, matching the interface block. Without
    # this the bulk old plastic deformation gradient is read via getMaterialPropertyOld from the
    # stateful NEML2-output material, whose old value at EXEC_INITIAL defaults to zero: the identity
    # initial condition is applied only to the current value at t=0 (the issue #28971 workaround in
    # NEML2ToMOOSEMaterialProperty, whose initQpStatefulProperties is empty). A singular old plastic
    # F makes the INITIAL return-map solve diverge (MAXITER). The state initializer instead reads the
    # identity initial_plastic_defgrad directly at INITIAL, so every constitutive evaluation sees a
    # nonsingular F, and the bulk plastic state stays resident on-device for the GPU steps.
    manage_state_advance = true
  []
  [interface]
    input = 'exact_kinematics_pk1_neml2.i'
    model = 'interface_model'
    device = 'cpu'
    # orientation is a fixed constant baked into the interface model (MRPConstantParameter in
    # exact_kinematics_pk1_neml2.i), so only deformation_gradient is gathered from MOOSE here.
    input_types = 'MATERIAL'
    inputs = 'deformation_gradient'
    derivatives = 'pk1_stress deformation_gradient pk1_jacobian'
    interface = 'Block0_Block1'
    interface_only = true
    executor_name = 'neml2_interface'
    manage_state_advance = true
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
  []
[]

[Materials]
  [bulk_stress]
    type = ComputeLagrangianStressCustomPK2
    custom_pk2_stress = 'neml2_stress'
    custom_pk2_jacobian = 'dneml2_stress/ddeformation_gradient'
    large_kinematics = true
  []
  [initial_orientation]
    type = GenericConstantRealVectorValue
    vector_name = 'orientation'
    vector_values = '-0.54412095 -0.34931944 0.12600655'
  []
  [initial_plastic_defgrad]
    type = GenericConstantRankTwoTensor
    tensor_name = 'initial_plastic_defgrad'
    tensor_values = '1 1 1'
  []
  [local_traction]
    type = PureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
[]

[UserObjects]
  [distance]
    type = BoundaryShortestDistanceToSurface
    surfaces = true_interface
    boundary = 'Block0_Block1'
    execute_on = INITIAL
    # Run (and fully populate the distance map) before the NEML2 interface objects
    # (execution_order_group = -1) so the stateful interface state initialization at INITIAL, which
    # reinits the cohesive-zone jump material, finds the surrogate-to-true distance already computed.
    execution_order_group = -2
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad})'
  []
[]

[BCs]
  [xfix]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [yfix]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [zfix]
    type = DirichletBC
    variable = disp_z
    boundary = back
    value = 0
  []
  [xdisp]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = right
    function = t
    preset = false
  []
[]

# hypre boomeramg + gmres, line_search=none is the single preconditioner shared by all six steps so
# the CPU-vs-GPU comparison isolates the compute backend, not the solver. boomeramg converges in the
# same Newton count as LU on this CP LD + SCZM tangent; LU (the exact direct reference) can be
# recovered with
#   Executioner/petsc_options_iname=-pc_type Executioner/petsc_options_value=lu
# when a machine-precision reference match is required. line_search=none matches the CP host
# reference: the crystal-plasticity return map already handles the material nonlinearity.
[Executioner]
  type = Transient
  solve_type = NEWTON
  line_search = none
  petsc_options_iname = '-pc_type -pc_hypre_type -ksp_type'
  petsc_options_value = 'hypre boomeramg    gmres'
  automatic_scaling = true
  dt = ${DT}
  num_steps = ${NUM_STEPS}
  residual_and_jacobian_together = true
  nl_abs_tol = 1e-10
[]

[Postprocessors]
  # Bulk plasticity indicator: slip hardening grows only once crystal slip is active.
  [max_slip_hardening]
    type = ElementExtremeMaterialProperty
    mat_prop = slip_hardening
    value_type = max
  []
  [avg_slip_hardening]
    type = ElementAverageMaterialProperty
    mat_prop = slip_hardening
  []
  # Interface-sensitive measure: block-1 displacement field responds to the cohesive interface and
  # its directional correction, so toggling directional_correction changes this value.
  [avg_disp_x_block1]
    type = ElementAverageValue
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
    type = MaterialRealAux
    variable = slip_hardening_elem
    property = slip_hardening
    execute_on = TIMESTEP_END
  []
[]

[Outputs]
  exodus = true
  csv = true
[]
