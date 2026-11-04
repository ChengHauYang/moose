# Step 2 of the plasticity_LD_sczm benchmark: Host MOOSE / CUDA NEML2 / CPU PETSc.
#
# Identical physical problem, mesh, materials, loading, and time integration as
# step1_ld_sczm_host_cpu.i. The only change is that both NEML2 executors (bulk and interface)
# run the constitutive update on the GPU (device = cuda). The outputs default to output_device =
# cpu, so the host NEML2ToMOOSEMaterialProperty bridges copy the stress/tangent/state back to the
# host for the host stress-divergence and SCZM interface kernels. MOOSE assembly and PETSc stay on
# the CPU.

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
    device = 'cuda'
    derivatives = 'neml2_stress deformation_gradient'
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
    # See step1 for the manage_state_advance rationale (seed old plastic F from identity at INITIAL).
    manage_state_advance = true
  []
  [interface]
    input = 'exact_kinematics_pk1_neml2.i'
    model = 'interface_model'
    device = 'cuda'
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

# hypre boomeramg + gmres, line_search=none: the single preconditioner shared by all six steps (see
# step1 for the rationale). Only the NEML2 device changes from step1.
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
  [max_slip_hardening]
    type = ElementExtremeMaterialProperty
    mat_prop = slip_hardening
    value_type = max
  []
  [avg_slip_hardening]
    type = ElementAverageMaterialProperty
    mat_prop = slip_hardening
  []
  [avg_disp_x_block1]
    type = ElementAverageValue
    variable = disp_x
    block = 1
  []
[]

[Outputs]
  exodus = true
[]
