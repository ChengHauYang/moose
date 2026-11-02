# Step 1 of the plasticity_LD_sczm_grain_boundaries benchmark: Host MOOSE / CPU NEML2 / CPU PETSc.
#
# Polycrystal version of the plasticity_LD_sczm benchmark. The grain structure, the grain-boundary
# shifted cohesive zones, the boundary conditions, and the loading follow
# grain_boundary_case/grain_boundary_sczm_coarse_lambda1.i: SubdomainGrainIDGenerator assigns the
# five grains of grain_boundary_only.msh to a 2D Cartesian mesh, BreakMeshByBlockGenerator splits
# the seven grain boundaries, and complete_interface_mesh supplies the true interfaces for the
# shifted cohesive zones. The linear-elastic small-strain bulk of that case is replaced by the
# finite-deformation total-Lagrangian crystal-plasticity bulk of plasticity_LD_sczm (plane strain),
# and the directional correction consumes the crystal-plasticity PK1 stress and its consistent
# d(PK1)/dF (emitted by NEML2, see ../plasticity_LD_sczm/exact_kinematics_pk1_neml2.i).
#
# Differences from the linear-elastic grain-boundary case, chosen so all six steps solve the same
# problem: the cohesive law is PureElasticTractionSeparation with the reference penalty stiffness
# (the Kokkos SCZM path has no bilinear mixed-mode damage law), F-bar is disabled as in every step
# of plasticity_LD_sczm, and every grain shares one crystal orientation (the Kokkos interface NEML2
# path bakes a single orientation into the interface model).
#
# Formulation: finite-deformation TL bulk + small-strain SCZM interface. Steps 2-6 solve this
# identical physical problem; only the compute backend, device, and PETSc config differ.

ny = 6
nx = '${fparse ny * 3 / 2}'
# The reference loading 1e-2 * t up to t = 5, in a quarter of the reference time step: at dt = 1
# (from t = 2) and dt = 0.5 (from t = 3.5) the NEML2 crystal-plasticity return map hits its
# iteration limit on intermediate Newton iterates, and the resulting cutbacks could make the step
# sequence differ between the six steps.
DT = 0.25
NUM_STEPS = 20

interfaces = 'grain1_grain2 grain1_grain4 grain1_grain5 grain2_grain4 grain3_grain4 grain3_grain5 grain4_grain5'

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

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
        boundary = ${interfaces}
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
    # Seed the old plastic deformation gradient from the identity through the state initializer
    # (see plasticity_LD_sczm/step1_ld_sczm_host_cpu.i for why this is required at INITIAL).
    manage_state_advance = true
  []
  [interface]
    input = '../plasticity_LD_sczm/exact_kinematics_pk1_neml2.i'
    model = 'interface_model'
    device = 'cpu'
    # orientation is a fixed constant baked into the interface model (MRPConstantParameter), so
    # only deformation_gradient is gathered from MOOSE here.
    input_types = 'MATERIAL'
    inputs = 'deformation_gradient'
    derivatives = 'pk1_stress deformation_gradient pk1_jacobian'
    interface = ${interfaces}
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
  # The same orientation as plasticity_LD_sczm and the MRP constant in its interface model.
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
  # penalty_stiffness of the reference BiLinearMixedModeTraction, used for both directions.
  [local_traction]
    type = PureElasticTractionSeparation
    boundary = ${interfaces}
    normal_stiffness = 2e3
    tangent_stiffness = 2e3
  []
[]

# The same interface manager and distance object that complete_interface_mesh generates, declared
# explicitly so the distance object runs in execution_order_group -2. The NEML2 interface executor
# evaluates the SCZM interface materials at INITIAL (group -1), and those need the surrogate-to-true
# interface distances already computed; the action-generated distance object runs in group 0.
# initialSetup also follows the group order, and the manager detects the interfaces there before
# the distance object can look them up, so the manager goes one group earlier still.
[UserObjects]
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

[Functions]
  [displacement_with_time]
    type = ParsedFunction
    expression = '1e-2 * t'
  []
[]

[BCs]
  [anchor_x]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [displacement_x_right]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = right
    function = displacement_with_time
    preset = false
  []
[]

# hypre boomeramg + gmres, line_search=none is the single preconditioner shared by all six steps so
# the CPU-vs-GPU comparison isolates the compute backend, not the solver.
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
  # Identical tight tolerances in all six steps, so cross-step differences reflect the compute
  # backend rather than where Newton stopped.
  nl_rel_tol = 1e-12
  nl_abs_tol = 1e-12
  l_tol = 1e-10
[]

[Postprocessors]
  # Bulk plasticity indicators: slip hardening grows only once crystal slip is active.
  [max_slip_hardening]
    type = ElementExtremeMaterialProperty
    mat_prop = slip_hardening
    value_type = max
  []
  [avg_slip_hardening]
    type = ElementAverageMaterialProperty
    mat_prop = slip_hardening
  []
  # Interface-sensitive measure: grain 4 lies between the loaded right edge and grains 1-3, so its
  # displacement responds to the cohesive grain boundaries.
  [avg_disp_x_grain4]
    type = ElementAverageValue
    variable = disp_x
    block = 4
  []
[]

[Outputs]
  exodus = true
  csv = true
[]
